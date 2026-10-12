#include "GltfModelLoader.h"
#include "RenderResource/MeshUtil.h"
#include "Utility/PathUtil.h"
#include "Memory/StackAllocation.h"
#include <cmath>
#include <cstdio>

#include <fastgltf/tools.hpp>
#include <simdjson.h>
#include <limits>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>

namespace tyr
{
	namespace
	{
		// glTF's material factors. Without a roughness/metallic texture, the default texture's values
		// are replaced by the factors alone, so their range collapses to the factor.
		void SetMaterialParams(const fastgltf::Material& material, MaterialParams& params)
		{
			const fastgltf::PBRData& pbr = material.pbrData;
			params.albedoTint = Vector4(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]);

			const bool hasRoughnessMetallic = pbr.metallicRoughnessTexture.has_value();
			params.roughnessMin = hasRoughnessMetallic ? 0.0f : pbr.roughnessFactor;
			params.roughnessMax = pbr.roughnessFactor;
			params.metallicMin = hasRoughnessMetallic ? 0.0f : pbr.metallicFactor;
			params.metallicMax = pbr.metallicFactor;
			params.normalStrength = material.normalTexture.has_value() ? material.normalTexture->scale : 1.0f;
			params.occlusionStrength = material.occlusionTexture.has_value() ? material.occlusionTexture->strength : 1.0f;

			params.alphaCutoff = material.alphaCutoff;
			MaterialBlendMode blendMode = MaterialBlendMode::Opaque;
			switch (material.alphaMode)
			{
			case fastgltf::AlphaMode::Mask:
				blendMode = MaterialBlendMode::Masked;
				break;
			case fastgltf::AlphaMode::Blend:
				blendMode = MaterialBlendMode::Translucent;
				break;
			default:
				break;
			}
			params.blendMode = static_cast<uint8>(blendMode);

			// One transform covers every texture, so the albedo texture's is used.
			if (pbr.baseColorTexture.has_value() && pbr.baseColorTexture->transform)
			{
				const fastgltf::TextureTransform& transform = *pbr.baseColorTexture->transform;
				params.uvScale = Vector2(transform.uvScale[0], transform.uvScale[1]);
				params.uvOffset = Vector2(transform.uvOffset[0], transform.uvOffset[1]);
				if (transform.rotation != 0.0f)
				{
					TYR_LOG_WARNING("The material %s rotates its textures, which isn't supported. It's imported without the rotation.", material.name.c_str());
				}
			}
		}

		// Copies a source name, keeping only characters that are safe in a file name.
		template<typename NameType>
		void CopyFileSafeName(const NameType& source, char* outName, uint capacity)
		{
			uint length = 0;
			for (char c : source)
			{
				if (length + 1 >= capacity)
				{
					break;
				}
				outName[length++] = (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') ? c : '_';
			}
			outName[length] = '\0';
		}

		// Locates this file's JSON content within its own already-loaded bytes - the whole
		// buffer for a plain .gltf (it's just JSON text), or the JSON chunk specifically for a
		// .glb (walking its chunk headers; a GLB's separate binary chunk, if present, is
		// skipped). Returns false if the bytes don't look like a well-formed glTF/GLB.
		bool FindGltfJsonBytes(const std::byte* fileData, size_t fileSize, const std::byte*& outJsonData, size_t& outJsonSize)
		{
			constexpr size_t c_GlbHeaderSize = 12;
			constexpr uint32_t c_GlbMagic = 0x46546C67; // "glTF", little-endian
			constexpr uint32_t c_JsonChunkType = 0x4E4F534A; // "JSON", little-endian

			if (fileSize < 4)
			{
				return false;
			}

			uint32_t magic;
			memcpy(&magic, fileData, sizeof(magic));
			if (magic != c_GlbMagic)
			{
				// Not a GLB container - the whole file is plain glTF JSON text.
				outJsonData = fileData;
				outJsonSize = fileSize;
				return true;
			}

			if (fileSize < c_GlbHeaderSize)
			{
				return false;
			}

			size_t offset = c_GlbHeaderSize;
			while (offset + 8 <= fileSize)
			{
				uint32_t chunkLength, chunkType;
				memcpy(&chunkLength, fileData + offset, sizeof(chunkLength));
				memcpy(&chunkType, fileData + offset + 4, sizeof(chunkType));
				offset += 8;

				if (offset + chunkLength > fileSize)
				{
					break;
				}

				if (chunkType == c_JsonChunkType)
				{
					outJsonData = fileData + offset;
					outJsonSize = chunkLength;
					return true;
				}

				offset += chunkLength;
			}

			return false;
		}

		// Returns true if name ends in "lod" (case-insensitive) followed by one or more digits -
		// e.g. "Prop_LOD1", "TableLod4", "propLOD12". outBaseName gets the prefix with the LOD
		// suffix (and a single immediately-preceding '_' or '-', if there is one) stripped,
		// outLodIndex gets the parsed number.
		bool TryParseLodSuffix(const std::string& name, std::string& outBaseName, uint& outLodIndex)
		{
			const size_t end = name.size();
			size_t digitsStart = end;
			while (digitsStart > 0 && std::isdigit(static_cast<unsigned char>(name[digitsStart - 1])))
			{
				--digitsStart;
			}
			if (digitsStart == end || digitsStart < 3)
			{
				return false;
			}

			const size_t lodStart = digitsStart - 3;
			if (_strnicmp(name.c_str() + lodStart, "lod", 3) != 0)
			{
				return false;
			}

			size_t baseEnd = lodStart;
			if (baseEnd > 0 && (name[baseEnd - 1] == '_' || name[baseEnd - 1] == '-'))
			{
				--baseEnd;
			}
			if (baseEnd == 0)
			{
				// The whole name is just "lod123" - no base name left to group other LODs by.
				return false;
			}

			outBaseName = name.substr(0, baseEnd);
			outLodIndex = static_cast<uint>(std::stoul(name.substr(digitsStart)));
			return true;
		}

		// Figures out where a glTF texture's actual image bytes live and points a
		// TextureSource at them - either a resolved absolute path for a loose file, or the
		// bytes directly for an embedded (GLB) image. Returns false if the texture has no
		// usable image or an image source we don't understand.
		bool ResolveTextureSource(const fastgltf::Asset& asset, const char* baseDirectory,
			size_t textureIndex, Array<Path>& resolvedPaths, TextureSource& outSource)
		{
			const fastgltf::Texture& texture = asset.textures[textureIndex];
			if (!texture.imageIndex.has_value())
			{
				TYR_LOG_ERROR("A glTF texture has no image - skipping it.");
				return false;
			}

			const fastgltf::Image& image = asset.images[*texture.imageIndex];

			// Import-time-only, runs once per texture - std::visit is fine here (its cost
			// is one indirect call through a compile-time dispatch table, same class as a
			// virtual call, and no RTTI is involved either way).
			return std::visit([&](const auto& source) -> bool
			{
				using SourceType = std::decay_t<decltype(source)>;

				if constexpr (std::is_same_v<SourceType, fastgltf::sources::URI>)
				{
					// A loose file on disk, referenced relative to the model's own folder. The URI's
					// path already has its percent-encoded characters decoded.
					const std::string_view uriPath = source.uri.path();
					char resolved[TYR_MAX_PATH_TOTAL_SIZE];
					const int length = baseDirectory[0] != '\0'
						? snprintf(resolved, sizeof(resolved), "%s/%.*s", baseDirectory, static_cast<int>(uriPath.size()), uriPath.data())
						: snprintf(resolved, sizeof(resolved), "%.*s", static_cast<int>(uriPath.size()), uriPath.data());
					if (length < 0 || length >= static_cast<int>(sizeof(resolved)))
					{
						TYR_LOG_ERROR("A texture's path is too long - skipping it.");
						return false;
					}
					Path& stored = resolvedPaths.ExpandOne();
					stored = resolved;
					outSource.path = stored.CStr();
					return true;
				}
				else if constexpr (std::is_same_v<SourceType, fastgltf::sources::Array> || std::is_same_v<SourceType, fastgltf::sources::Vector>)
				{
					// Already-decoded bytes sitting in memory - this is what an embedded GLB
					// image comes through as. These bytes are owned by the fastgltf::Asset,
					// which is why the loader that owns this asset has to stay alive for as
					// long as this TextureSource is going to be read.
					outSource.data = reinterpret_cast<const uchar*>(source.bytes.data());
					outSource.dataSize = source.bytes.size();
					return true;
				}
				else if constexpr (std::is_same_v<SourceType, fastgltf::sources::BufferView>)
				{
					// The image points at a bufferView instead of holding its own bytes -
					// walk through to the buffer and slice out the right range ourselves.
					const fastgltf::BufferView& bufferView = asset.bufferViews[source.bufferViewIndex];
					const fastgltf::Buffer& buffer = asset.buffers[bufferView.bufferIndex];

					return std::visit([&](const auto& bufferSource) -> bool
					{
						using BufferSourceType = std::decay_t<decltype(bufferSource)>;

						if constexpr (std::is_same_v<BufferSourceType, fastgltf::sources::Array> || std::is_same_v<BufferSourceType, fastgltf::sources::Vector>)
						{
							outSource.data = reinterpret_cast<const uchar*>(bufferSource.bytes.data()) + bufferView.byteOffset;
							outSource.dataSize = bufferView.byteLength;
							return true;
						}
						else
						{
							TYR_LOG_ERROR("A texture's buffer isn't loaded into memory in a way we know how to read.");
							return false;
						}
					}, buffer.data);
				}
				else
				{
					TYR_LOG_ERROR("A texture has an image data source we don't support yet.");
					return false;
				}
			}, image.data);
		}
	}

	bool GltfModelLoader::Load(const char* filePath, ModelImportResult& outResult)
	{
		// This loader gets reused across imports (see the class comment), so every field
		// below needs to start this call in a known, empty state rather than whatever the
		// previous file left behind.
		m_ResolvedImagePaths.Clear();
		m_MeshNodeIndices.Clear();
		m_MaterialIndexMap.Clear();
		m_LodChains.Clear();
		m_ConsumedLodNodeIndices.Clear();
		m_HasDefaultMaterial = false;
		m_DefaultMaterialIndex = 0;
		m_SkinIndex = -1;
		m_NodeParents.Clear();
		m_NodeToBone.Clear();
		m_BoneNodes.Clear();
		m_BoneParentOffsets.Clear();
		m_BoneHasParentOffset.Clear();

		fastgltf::Expected<fastgltf::GltfDataBuffer> bufferExpected = fastgltf::GltfDataBuffer::FromPath(filePath);
		if (!bufferExpected)
		{
			TYR_LOG_ERROR("Failed to open model file %s.", filePath);
			return false;
		}
		m_DataBuffer = std::move(bufferExpected.get());

		char baseDirectory[TYR_MAX_PATH_TOTAL_SIZE];
		PathUtil::GetDirectoryPathFromFilePath(filePath, baseDirectory);
		m_BaseDirectory = baseDirectory;

		fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform);

		// LoadExternalBuffers so loose .bin files actually get read (a GLB's own embedded
		// binary chunk is always available regardless of this flag). GenerateMeshIndices so
		// every primitive has real indices, even ones authored without any. We deliberately
		// don't set LoadExternalImages - we want loose texture files to stay as plain paths,
		// which ImageLoader/MaterialImporter can read directly, rather than fastgltf reading
		// them into memory for us just so we hand the bytes straight back as a TextureSource.
		constexpr fastgltf::Options options = fastgltf::Options::LoadExternalBuffers
			| fastgltf::Options::GenerateMeshIndices
			| fastgltf::Options::DecomposeNodeMatrices;

		// fastgltf only takes std::filesystem paths, here and in FromPath above.
		fastgltf::Expected<fastgltf::Asset> assetExpected = parser.loadGltf(m_DataBuffer, std::filesystem::path(baseDirectory), options);
		if (!assetExpected)
		{
			TYR_LOG_ERROR("Failed to parse model file %s.", filePath);
			return false;
		}
		m_Asset = std::move(assetExpected.get());

		// Reserved up front so the pointers we hand out via TextureSource::path later stay
		// valid - if this Array ever had to grow, it would move its existing entries and
		// invalidate any pointer already taken into them.
		m_ResolvedImagePaths.Reserve(static_cast<uint>(m_Asset.images.size()));

		// TODO: node transforms aren't baked into the vertices yet, so every mesh is
		// imported as if its owning node were at the origin with no rotation or scale. Fine
		// for a single mesh sitting at the root, wrong for a multi-node scene - that needs
		// either baking the accumulated transform into each mesh's vertices, or importing
		// each node as its own entity so the transforms can live there instead.
		if (!m_Asset.scenes.empty())
		{
			const size_t sceneIndex = m_Asset.defaultScene.value_or(0);
			const fastgltf::Scene& scene = m_Asset.scenes[sceneIndex];
			for (size_t nodeIndex : scene.nodeIndices)
			{
				CollectMeshNodes(nodeIndex, m_MeshNodeIndices);
			}
		}

		if (m_MeshNodeIndices.IsEmpty())
		{
			TYR_LOG_ERROR("Model file %s has no meshes reachable from its default scene.", filePath);
			return false;
		}

		m_NodeParents.Resize(static_cast<uint>(m_Asset.nodes.size()), -1);
		for (size_t nodeIndex = 0; nodeIndex < m_Asset.nodes.size(); ++nodeIndex)
		{
			for (size_t childIndex : m_Asset.nodes[nodeIndex].children)
			{
				m_NodeParents[static_cast<uint>(childIndex)] = static_cast<int>(nodeIndex);
			}
		}

		// The first skinned mesh's skin becomes the model's skeleton.
		for (size_t nodeIdx : m_MeshNodeIndices)
		{
			const fastgltf::Node& node = m_Asset.nodes[nodeIdx];
			if (node.skinIndex.has_value())
			{
				if (!BuildSkeleton(*node.skinIndex, outResult.skeleton))
				{
					return false;
				}
				m_SkinIndex = static_cast<int>(*node.skinIndex);
				break;
			}
		}

		// Pre-authored LOD detection - MSFT_lod first (only attempted if the file actually
		// declares using it, via the asset-level list fastgltf already parses for us), then a
		// naming-convention fallback for any mesh node MSFT_lod didn't already account for.
		for (const auto& extensionName : m_Asset.extensionsUsed)
		{
			if (extensionName == "MSFT_lod")
			{
				ParseMsftLodChains();
				break;
			}
		}
		BuildNameBasedLodChains();

		bool anyMeshLoaded = false;
		for (size_t nodeIdx : m_MeshNodeIndices)
		{
			const uint nodeIndex = static_cast<uint>(nodeIdx);
			if (m_ConsumedLodNodeIndices.Find(nodeIndex) != nullptr)
			{
				// Already pulled in as another node's own alternate LOD below - not a
				// standalone mesh in its own right.
				continue;
			}

			const fastgltf::Node& node = m_Asset.nodes[nodeIdx];
			bool skinned = false;
			if (node.skinIndex.has_value())
			{
				const fastgltf::Skin& skin = m_Asset.skins[*node.skinIndex];
				const fastgltf::Skin& skeletonSkin = m_Asset.skins[m_SkinIndex];
				skinned = static_cast<int>(*node.skinIndex) == m_SkinIndex
					|| (skin.joints.size() == skeletonSkin.joints.size() && std::equal(skin.joints.begin(), skin.joints.end(), skeletonSkin.joints.begin()));
				if (!skinned)
				{
					TYR_LOG_WARNING("Mesh %s uses a different skin to the rest of the model, so it's imported without skinning.", m_Asset.meshes[*node.meshIndex].name.c_str());
				}
			}

			ModelImportMesh* mesh = LoadMesh(*node.meshIndex, skinned, outResult, m_MaterialIndexMap);
			if (!mesh)
			{
				continue;
			}
			anyMeshLoaded = true;

			// Skinned meshes only use LOD0, since simplifying needs to respect their weights.
			if (skinned)
			{
				continue;
			}

			if (LocalArray<uint, MeshConstants::c_MaxLods - 1>* alternates = m_LodChains.Find(nodeIndex))
			{
				for (uint altNodeIndex : *alternates)
				{
					const fastgltf::Node& altNode = m_Asset.nodes[altNodeIndex];
					if (altNode.meshIndex.has_value())
					{
						LoadPreAuthoredLod(*altNode.meshIndex, *mesh, outResult, m_MaterialIndexMap);
					}
				}
			}
		}

		if (anyMeshLoaded && m_SkinIndex >= 0)
		{
			LoadAnimations(outResult);
		}

		return anyMeshLoaded;
	}

	void GltfModelLoader::CollectMeshNodes(size_t nodeIndex, Array<size_t>& outMeshNodeIndices) const
	{
		const fastgltf::Node& node = m_Asset.nodes[nodeIndex];
		if (node.meshIndex.has_value())
		{
			outMeshNodeIndices.Add(nodeIndex);
		}

		for (size_t childIndex : node.children)
		{
			CollectMeshNodes(childIndex, outMeshNodeIndices);
		}
	}

	void GltfModelLoader::ParseMsftLodChains()
	{
		const fastgltf::span<std::byte> fileBytes = static_cast<fastgltf::span<std::byte>>(m_DataBuffer);

		const std::byte* jsonData = nullptr;
		size_t jsonSize = 0;
		if (!FindGltfJsonBytes(fileBytes.data(), fileBytes.size(), jsonData, jsonSize))
		{
			TYR_LOG_WARNING("Model declares using MSFT_lod but its JSON content couldn't be located for a direct scan - falling back to naming-convention LOD detection only.");
			return;
		}

		simdjson::dom::parser jsonParser;
		simdjson::dom::element doc;
		if (jsonParser.parse(reinterpret_cast<const char*>(jsonData), jsonSize).get(doc) != simdjson::SUCCESS)
		{
			TYR_LOG_WARNING("Failed to re-parse model JSON for MSFT_lod - falling back to naming-convention LOD detection only.");
			return;
		}

		simdjson::dom::array nodes;
		if (doc["nodes"].get(nodes) != simdjson::SUCCESS)
		{
			return;
		}

		uint nodeIndex = 0;
		for (simdjson::dom::element nodeElem : nodes)
		{
			simdjson::dom::object extensions;
			if (nodeElem["extensions"].get(extensions) == simdjson::SUCCESS)
			{
				simdjson::dom::object msftLod;
				if (extensions["MSFT_lod"].get(msftLod) == simdjson::SUCCESS)
				{
					simdjson::dom::array ids;
					if (msftLod["ids"].get(ids) == simdjson::SUCCESS)
					{
						LocalArray<uint, MeshConstants::c_MaxLods - 1> alternates;
						for (simdjson::dom::element idElem : ids)
						{
							int64_t idValue;
							if (idElem.get(idValue) == simdjson::SUCCESS && idValue >= 0
								&& alternates.Size() < MeshConstants::c_MaxLods - 1)
							{
								alternates.Add(static_cast<uint>(idValue));
							}
						}

						if (!alternates.IsEmpty())
						{
							m_LodChains.Insert(nodeIndex, alternates);
							for (uint altNodeIndex : alternates)
							{
								m_ConsumedLodNodeIndices.Insert(altNodeIndex, true);
							}
						}
					}
				}
			}
			++nodeIndex;
		}
	}

	void GltfModelLoader::BuildNameBasedLodChains()
	{
		struct NameGroupEntry
		{
			uint lodIndex;
			uint nodeIndex;
		};
		std::unordered_map<std::string, std::vector<NameGroupEntry>> groups;

		for (size_t nodeIdx : m_MeshNodeIndices)
		{
			const uint nodeIndex = static_cast<uint>(nodeIdx);
			if (m_ConsumedLodNodeIndices.Find(nodeIndex) != nullptr || m_LodChains.Find(nodeIndex) != nullptr)
			{
				// Already handled by MSFT_lod, either as a host (has its own entry in
				// m_LodChains) or as an alternate (already consumed) - the naming convention
				// only needs to cover whatever that extension didn't.
				continue;
			}

			const fastgltf::Node& node = m_Asset.nodes[nodeIdx];
			const fastgltf::Mesh& gltfMesh = m_Asset.meshes[*node.meshIndex];

			std::string baseName;
			uint lodIndex;
			if (!TryParseLodSuffix(std::string(gltfMesh.name.c_str()), baseName, lodIndex))
			{
				continue;
			}

			groups[baseName].push_back({ lodIndex, nodeIndex });
		}

		for (auto& [baseName, entries] : groups)
		{
			bool hasLod0 = false;
			for (const NameGroupEntry& entry : entries)
			{
				if (entry.lodIndex == 0)
				{
					hasLod0 = true;
					break;
				}
			}
			if (!hasLod0 || entries.size() < 2)
			{
				// No LOD0 to anchor the chain, or only one member (nothing to chain to) - leave
				// these nodes to be imported as independent, standalone meshes instead.
				continue;
			}

			std::sort(entries.begin(), entries.end(), [](const NameGroupEntry& a, const NameGroupEntry& b)
			{
				return a.lodIndex < b.lodIndex;
			});

			const uint hostNodeIndex = entries[0].nodeIndex;
			LocalArray<uint, MeshConstants::c_MaxLods - 1> alternates;
			for (size_t i = 1; i < entries.size() && alternates.Size() < MeshConstants::c_MaxLods - 1; ++i)
			{
				alternates.Add(entries[i].nodeIndex);
				m_ConsumedLodNodeIndices.Insert(entries[i].nodeIndex, true);
			}

			m_LodChains.Insert(hostNodeIndex, alternates);
		}
	}

	bool GltfModelLoader::LoadPreAuthoredLod(size_t meshIndex, ModelImportMesh& mesh, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap)
	{
		if (mesh.preAuthoredLods.Size() >= MeshConstants::c_MaxLods - 1)
		{
			return false;
		}

		ModelImportMeshLod& lod = mesh.preAuthoredLods.ExpandOne();
		// Guards against stale content from a previous, larger import that used this same
		// reused slot - see ModelImportMeshLod's own comment.
		lod.Reset();

		return LoadMeshPrimitives(meshIndex, lod.vertices, lod.indices, lod.submeshes, nullptr, outResult, materialIndexMap);
	}

	ModelImportMesh* GltfModelLoader::LoadMesh(size_t meshIndex, bool skinned, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap)
	{
		ModelImportMesh& mesh = outResult.AddMesh();

		Array<ModelImportSkinWeights>* skinWeights = skinned ? &mesh.skinWeights : nullptr;
		if (!LoadMeshPrimitives(meshIndex, mesh.vertices, mesh.indices, mesh.submeshes, skinWeights, outResult, materialIndexMap))
		{
			return nullptr;
		}

		Vector3 aabbMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		Vector3 aabbMax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
		for (const Vertex& vertex : mesh.vertices)
		{
			aabbMin.x = std::min(aabbMin.x, vertex.position.x);
			aabbMin.y = std::min(aabbMin.y, vertex.position.y);
			aabbMin.z = std::min(aabbMin.z, vertex.position.z);
			aabbMax.x = std::max(aabbMax.x, vertex.position.x);
			aabbMax.y = std::max(aabbMax.y, vertex.position.y);
			aabbMax.z = std::max(aabbMax.z, vertex.position.z);
		}

		mesh.aabbMin = aabbMin;
		mesh.aabbMax = aabbMax;

		char name[64];
		CopyFileSafeName(m_Asset.meshes[meshIndex].name, name, sizeof(name));
		mesh.name = name;

		// A simple, not-tightest-possible bounding sphere - centered on the AABB, radius
		// reaching its farthest corner. Good enough for now.
		const Vector3 centre = (aabbMin + aabbMax) * 0.5f;
		const float radius = (aabbMax - centre).Length();
		mesh.sphere = BoundingSphere(centre, radius);

		return &mesh;
	}

	bool GltfModelLoader::LoadMeshPrimitives(size_t meshIndex, Array<Vertex>& outVertices, Array<uint>& outIndices,
		Array<ModelImportSubmesh>& outSubmeshes, Array<ModelImportSkinWeights>* outSkinWeights,
		ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap)
	{
		const fastgltf::Mesh& gltfMesh = m_Asset.meshes[meshIndex];
		bool anyPrimitiveLoaded = false;

		for (const fastgltf::Primitive& primitive : gltfMesh.primitives)
		{
			if (primitive.type != fastgltf::PrimitiveType::Triangles)
			{
				TYR_LOG_WARNING("Skipping a non-triangle primitive.");
				continue;
			}

			const fastgltf::Attribute* posAttribute = primitive.findAttribute("POSITION");
			if (posAttribute == primitive.attributes.end())
			{
				TYR_LOG_WARNING("Skipping a primitive with no POSITION attribute.");
				continue;
			}

			if (!primitive.indicesAccessor.has_value())
			{
				// Shouldn't happen - we asked fastgltf to generate indices for us - but
				// better to skip than to read a missing accessor.
				TYR_LOG_WARNING("Skipping a primitive with no indices.");
				continue;
			}

			const fastgltf::Accessor& posAccessor = m_Asset.accessors[posAttribute->accessorIndex];
			const uint vertexCount = static_cast<uint>(posAccessor.count);
			const uint baseVertex = outVertices.Size();
			outVertices.Resize(baseVertex + vertexCount);

			// RH -> LH: flip Z on position, normal and tangent, and flip the tangent's
			// handedness sign to match. UVs are unchanged - glTF's top-left origin already
			// matches what our renderer expects.
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(m_Asset, posAccessor, [&](fastgltf::math::fvec3 v, size_t i)
			{
				outVertices[baseVertex + static_cast<uint>(i)].position = Vector3(v[0], v[1], -v[2]);
			});

			if (outSkinWeights)
			{
				outSkinWeights->Resize(baseVertex + vertexCount);
				LoadSkinWeights(primitive, vertexCount, outSkinWeights->Data() + baseVertex);
			}

			bool hasNormals = false;
			const fastgltf::Attribute* normalAttribute = primitive.findAttribute("NORMAL");
			if (normalAttribute != primitive.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(m_Asset, m_Asset.accessors[normalAttribute->accessorIndex], [&](fastgltf::math::fvec3 v, size_t i)
				{
					outVertices[baseVertex + static_cast<uint>(i)].normal = Vector3(v[0], v[1], -v[2]);
				});
				hasNormals = true;
			}
			// else: no authored normals - glTF explicitly allows this (recommending flat
			// face normals as the fallback), so it's generated below rather than treated as
			// an error, once indices are available.

			const fastgltf::Attribute* uvAttribute = primitive.findAttribute("TEXCOORD_0");
			if (uvAttribute != primitive.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(m_Asset, m_Asset.accessors[uvAttribute->accessorIndex], [&](fastgltf::math::fvec2 v, size_t i)
				{
					outVertices[baseVertex + static_cast<uint>(i)].uv = Vector2(v[0], v[1]);
				});
			}

			bool hasTangents = false;
			const fastgltf::Attribute* tangentAttribute = primitive.findAttribute("TANGENT");
			if (tangentAttribute != primitive.attributes.end())
			{
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(m_Asset, m_Asset.accessors[tangentAttribute->accessorIndex], [&](fastgltf::math::fvec4 v, size_t i)
				{
					outVertices[baseVertex + static_cast<uint>(i)].tangent = Vector4(v[0], v[1], -v[2], -v[3]);
				});
				hasTangents = true;
			}
			// else: no authored tangents - generated below, once indices are available.

			const fastgltf::Accessor& indexAccessor = m_Asset.accessors[*primitive.indicesAccessor];
			const uint indexCount = static_cast<uint>(indexAccessor.count);
			const uint baseIndex = outIndices.Size();
			outIndices.Resize(baseIndex + indexCount);

			// Written as primitive-local indices for now (0-based into this primitive's own
			// vertex range, not the mesh-wide one) - rebased to the mesh-wide absolute form
			// once tangent generation below (which needs them local) is done.
			fastgltf::iterateAccessorWithIndex<std::uint32_t>(m_Asset, indexAccessor, [&](std::uint32_t index, size_t i)
			{
				outIndices[baseIndex + static_cast<uint>(i)] = index;
			});

			// Reverse winding per triangle - needed because we flipped Z above. Works the
			// same whether the indices are local or absolute, since it's just swapping
			// pairs of whatever values are there.
			for (uint i = 0; i + 2 < indexCount; i += 3)
			{
				std::swap(outIndices[baseIndex + i + 1], outIndices[baseIndex + i + 2]);
			}

			// Fill in whatever wasn't authored. If normals are missing, tangents get
			// generated alongside them regardless of whether TANGENT was present - a
			// tangent that was computed against normals we're now replacing can't be
			// trusted either. Both need local indices and a vertex pointer offset to this
			// primitive's own range, which is exactly what we have right now, before the
			// rebase below.
			if (!hasNormals)
			{
				MeshUtil::CreateNormalsAndTangents(outVertices.Data() + baseVertex, vertexCount, outIndices.Data() + baseIndex, indexCount);
			}
			else if (!hasTangents)
			{
				MeshUtil::CreateTangents(outVertices.Data() + baseVertex, vertexCount, outIndices.Data() + baseIndex, indexCount);
			}

			// Rebase this primitive's indices from local to the mesh-wide absolute form the
			// rest of the pipeline (and any later primitives sharing this same index buffer) expects.
			for (uint i = 0; i < indexCount; ++i)
			{
				outIndices[baseIndex + i] += baseVertex;
			}

			const uint materialIndex = primitive.materialIndex.has_value()
				? ResolveMaterial(*primitive.materialIndex, outResult, materialIndexMap)
				: GetOrCreateDefaultMaterial(outResult);

			ModelImportSubmesh& submesh = outSubmeshes.ExpandOne();
			submesh.indexOffset = baseIndex;
			submesh.indexCount = indexCount;
			submesh.materialIndex = materialIndex;

			anyPrimitiveLoaded = true;
		}

		return anyPrimitiveLoaded;
	}

	uint GltfModelLoader::ResolveMaterial(size_t gltfMaterialIndex, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap)
	{
		const uint key = static_cast<uint>(gltfMaterialIndex);
		if (uint* existing = materialIndexMap.Find(key))
		{
			return *existing;
		}

		const fastgltf::Material& gltfMaterial = m_Asset.materials[gltfMaterialIndex];

		PbrMaterialImportDesc& desc = outResult.materials.ExpandOne();
		desc.isSRGB = true;
		SetMaterialParams(gltfMaterial, desc.params);

		if (gltfMaterial.pbrData.baseColorTexture.has_value())
		{
			ResolveTextureSource(m_Asset, m_BaseDirectory.CStr(), gltfMaterial.pbrData.baseColorTexture->textureIndex, m_ResolvedImagePaths, desc.albedoSource);
		}

		const bool hasRoughnessMetallic = gltfMaterial.pbrData.metallicRoughnessTexture.has_value();
		if (hasRoughnessMetallic)
		{
			ResolveTextureSource(m_Asset, m_BaseDirectory.CStr(), gltfMaterial.pbrData.metallicRoughnessTexture->textureIndex, m_ResolvedImagePaths, desc.roughnessMetallicSource);
		}

		if (gltfMaterial.occlusionTexture.has_value())
		{
			// Occlusion and metallicRoughness very commonly point at the very same glTF
			// texture (the standard ORM packing). When that's true, reuse the source we
			// already resolved above instead of resolving - and later decoding - it twice.
			if (hasRoughnessMetallic && gltfMaterial.occlusionTexture->textureIndex == gltfMaterial.pbrData.metallicRoughnessTexture->textureIndex)
			{
				desc.occlusionSource = desc.roughnessMetallicSource;
			}
			else
			{
				ResolveTextureSource(m_Asset, m_BaseDirectory.CStr(), gltfMaterial.occlusionTexture->textureIndex, m_ResolvedImagePaths, desc.occlusionSource);
			}
		}

		if (gltfMaterial.normalTexture.has_value())
		{
			ResolveTextureSource(m_Asset, m_BaseDirectory.CStr(), gltfMaterial.normalTexture->textureIndex, m_ResolvedImagePaths, desc.normalSource);
		}
		// heightSource is left empty - glTF's core material model has no height/displacement map.

		const uint newIndex = outResult.materials.Size() - 1;
		materialIndexMap.Insert(key, newIndex);
		return newIndex;
	}

	uint GltfModelLoader::GetOrCreateDefaultMaterial(ModelImportResult& outResult)
	{
		if (m_HasDefaultMaterial)
		{
			return m_DefaultMaterialIndex;
		}

		// TODO: this should point at a real, hard-coded default material (with an actual
		// texture set) once one exists. Right now it produces a PbrMaterialImportDesc with
		// no textures at all, which MaterialImporter cannot actually import yet - a glTF
		// file with a material-less primitive will fail here rather than silently look
		// wrong, until a real default material is wired up.
		PbrMaterialImportDesc& desc = outResult.materials.ExpandOne();
		desc.isSRGB = true;

		m_DefaultMaterialIndex = outResult.materials.Size() - 1;
		m_HasDefaultMaterial = true;
		return m_DefaultMaterialIndex;
	}

	namespace
	{
		constexpr float c_KeyEpsilon = 1e-5f;

		// glTF is right-handed, so Z is flipped. Mirroring a rotation flips its X and Y.
		Vector3 ToEngineTranslation(float x, float y, float z)
		{
			return Vector3(x, y, -z);
		}

		Quaternion ToEngineRotation(float x, float y, float z, float w)
		{
			return Quaternion(w, -x, -y, z);
		}

		bool IsIdentity(const Matrix4& matrix)
		{
			for (uint row = 0; row < 4; ++row)
			{
				for (uint col = 0; col < 4; ++col)
				{
					const float expected = row == col ? 1.0f : 0.0f;
					if (Math::Abs(matrix[row][col] - expected) > c_KeyEpsilon)
					{
						return false;
					}
				}
			}
			return true;
		}

		bool NearlyEqual(const Vector3& a, const Vector3& b)
		{
			return Math::Abs(a.x - b.x) <= c_KeyEpsilon && Math::Abs(a.y - b.y) <= c_KeyEpsilon && Math::Abs(a.z - b.z) <= c_KeyEpsilon;
		}

		// q and -q are the same rotation.
		bool NearlyEqual(const Quaternion& a, const Quaternion& b)
		{
			return Math::Abs(Math::Abs(a.Dot(b)) - 1.0f) <= c_KeyEpsilon;
		}

		BoneTransform ApplyParentOffset(const BoneTransform& local, const Matrix4& parentOffset)
		{
			const Matrix4 matrix = Matrix4::CreateTRS(local.translation, local.rotation, local.scale) * parentOffset;
			BoneTransform result;
			matrix.DecomposeTRS(result.translation, result.rotation, result.scale);
			return result;
		}

		// Appends a channel's keys to the clip, as one key if they never change and none if
		// they also match the bind pose.
		template<typename T>
		void AddTrackKeys(const Array<T>& keys, const T& bindValue, Array<T>& clipKeys, uint& outOffset, uint& outCount)
		{
			bool constant = true;
			for (uint i = 1; i < keys.Size() && constant; ++i)
			{
				constant = NearlyEqual(keys[i], keys[0]);
			}

			outOffset = clipKeys.Size();
			if (constant && NearlyEqual(keys[0], bindValue))
			{
				outCount = 0;
				return;
			}

			outCount = constant ? 1 : keys.Size();
			for (uint i = 0; i < outCount; ++i)
			{
				clipKeys.Add(keys[i]);
			}
		}
	}

	Matrix4 GltfModelLoader::GetNodeLocalMatrix(size_t nodeIndex) const
	{
		// DecomposeNodeMatrices means every node's transform comes back as TRS.
		const fastgltf::TRS& trs = std::get<fastgltf::TRS>(m_Asset.nodes[nodeIndex].transform);
		return Matrix4::CreateTRS(ToEngineTranslation(trs.translation[0], trs.translation[1], trs.translation[2]),
			ToEngineRotation(trs.rotation[0], trs.rotation[1], trs.rotation[2], trs.rotation[3]),
			Vector3(trs.scale[0], trs.scale[1], trs.scale[2]));
	}

	bool GltfModelLoader::BuildSkeleton(size_t skinIndex, Skeleton& outSkeleton)
	{
		const fastgltf::Skin& skin = m_Asset.skins[skinIndex];
		const uint jointCount = static_cast<uint>(skin.joints.size());
		if (jointCount == 0 || jointCount > SkeletonConstants::c_MaxBones)
		{
			TYR_LOG_ERROR("Skin %s has %u joints - between 1 and %u are supported.", skin.name.c_str(), jointCount, SkeletonConstants::c_MaxBones);
			return false;
		}

		// Joints are marked first so each one's nearest joint ancestor can be found.
		constexpr uint16 c_UnorderedJoint = SkeletonConstants::c_NoParent - 1;
		m_NodeToBone.Resize(static_cast<uint>(m_Asset.nodes.size()), SkeletonConstants::c_NoParent);
		for (size_t jointNode : skin.joints)
		{
			m_NodeToBone[static_cast<uint>(jointNode)] = c_UnorderedJoint;
		}

		// Bones are added once their parent bone has been, so parents always come first.
		m_BoneNodes.Clear();
		while (m_BoneNodes.Size() < jointCount)
		{
			const uint orderedBefore = m_BoneNodes.Size();
			for (size_t jointNode : skin.joints)
			{
				const uint node = static_cast<uint>(jointNode);
				if (m_NodeToBone[node] != c_UnorderedJoint)
				{
					continue;
				}

				int ancestor = m_NodeParents[node];
				while (ancestor >= 0 && m_NodeToBone[static_cast<uint>(ancestor)] == SkeletonConstants::c_NoParent)
				{
					ancestor = m_NodeParents[static_cast<uint>(ancestor)];
				}
				if (ancestor >= 0 && m_NodeToBone[static_cast<uint>(ancestor)] == c_UnorderedJoint)
				{
					continue;
				}

				m_NodeToBone[node] = static_cast<uint16>(m_BoneNodes.Size());
				m_BoneNodes.Add(node);
			}

			if (m_BoneNodes.Size() == orderedBefore)
			{
				TYR_LOG_ERROR("Skin %s's joints don't form a hierarchy.", skin.name.c_str());
				return false;
			}
		}

		outSkeleton.parents.Resize(jointCount);
		outSkeleton.names.Resize(jointCount);
		outSkeleton.bindPose.Resize(jointCount);
		outSkeleton.inverseBindMatrices.Resize(jointCount);
		m_BoneParentOffsets.Resize(jointCount);
		m_BoneHasParentOffset.Resize(jointCount);

		for (uint bone = 0; bone < jointCount; ++bone)
		{
			const uint node = m_BoneNodes[bone];

			// Non-bone nodes above the bone are folded into it, up to its parent bone.
			Matrix4 parentOffset = Matrix4::c_Identity;
			int ancestor = m_NodeParents[node];
			while (ancestor >= 0 && m_NodeToBone[static_cast<uint>(ancestor)] == SkeletonConstants::c_NoParent)
			{
				parentOffset = parentOffset * GetNodeLocalMatrix(static_cast<size_t>(ancestor));
				ancestor = m_NodeParents[static_cast<uint>(ancestor)];
			}
			m_BoneParentOffsets[bone] = parentOffset;
			m_BoneHasParentOffset[bone] = !IsIdentity(parentOffset);

			outSkeleton.parents[bone] = ancestor >= 0 ? m_NodeToBone[static_cast<uint>(ancestor)] : SkeletonConstants::c_NoParent;
			outSkeleton.names[bone] = m_Asset.nodes[node].name.c_str();

			const fastgltf::TRS& trs = std::get<fastgltf::TRS>(m_Asset.nodes[node].transform);
			BoneTransform& bind = outSkeleton.bindPose[bone];
			bind.translation = ToEngineTranslation(trs.translation[0], trs.translation[1], trs.translation[2]);
			bind.rotation = ToEngineRotation(trs.rotation[0], trs.rotation[1], trs.rotation[2], trs.rotation[3]);
			bind.scale = Vector3(trs.scale[0], trs.scale[1], trs.scale[2]);
			if (m_BoneHasParentOffset[bone])
			{
				bind = ApplyParentOffset(bind, parentOffset);
			}
		}

		if (skin.inverseBindMatrices.has_value())
		{
			// glTF stores column-major matrices for column vectors, which is the same memory
			// layout as this engine's row-major matrices for row vectors. Z is then mirrored.
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fmat4x4>(m_Asset, m_Asset.accessors[*skin.inverseBindMatrices],
				[&](const fastgltf::math::fmat4x4& source, size_t joint)
				{
					const uint16 bone = m_NodeToBone[static_cast<uint>(skin.joints[joint])];
					Matrix4& matrix = outSkeleton.inverseBindMatrices[bone];
					for (uint row = 0; row < 4; ++row)
					{
						for (uint col = 0; col < 4; ++col)
						{
							const bool mirrored = (row == 2) != (col == 2);
							matrix[row][col] = mirrored ? -source[row][col] : source[row][col];
						}
					}
				});
		}
		else
		{
			// Without them, the mesh is bound in the skeleton's rest pose.
			SmartStack<Matrix4> modelStack = SmartStackAlloc<Matrix4>(jointCount);
			Matrix4* model = modelStack;
			for (uint bone = 0; bone < jointCount; ++bone)
			{
				const BoneTransform& bind = outSkeleton.bindPose[bone];
				const Matrix4 local = Matrix4::CreateTRS(bind.translation, bind.rotation, bind.scale);
				const uint16 parent = outSkeleton.parents[bone];
				model[bone] = parent == SkeletonConstants::c_NoParent ? local : local * model[parent];
				outSkeleton.inverseBindMatrices[bone] = model[bone].Inverse();
			}
		}

		return true;
	}

	void GltfModelLoader::LoadSkinWeights(const fastgltf::Primitive& primitive, uint vertexCount, ModelImportSkinWeights* outWeights) const
	{
		// Up to eight influences are read, and the four largest kept.
		constexpr uint c_MaxSourceInfluences = 8;
		SmartStack<uint16> sourceBonesStack = SmartStackAlloc<uint16>(vertexCount * c_MaxSourceInfluences);
		SmartStack<float> sourceWeightsStack = SmartStackAlloc<float>(vertexCount * c_MaxSourceInfluences);
		uint16* sourceBones = sourceBonesStack;
		float* sourceWeights = sourceWeightsStack;
		memset(sourceWeights, 0, sizeof(float) * vertexCount * c_MaxSourceInfluences);
		memset(sourceBones, 0, sizeof(uint16) * vertexCount * c_MaxSourceInfluences);

		const fastgltf::Skin& skin = m_Asset.skins[m_SkinIndex];
		bool hasWeights = false;
		for (uint set = 0; set < 2; ++set)
		{
			char jointsName[16];
			char weightsName[16];
			snprintf(jointsName, sizeof(jointsName), "JOINTS_%u", set);
			snprintf(weightsName, sizeof(weightsName), "WEIGHTS_%u", set);
			const fastgltf::Attribute* jointsAttribute = primitive.findAttribute(jointsName);
			const fastgltf::Attribute* weightsAttribute = primitive.findAttribute(weightsName);
			if (jointsAttribute == primitive.attributes.end() || weightsAttribute == primitive.attributes.end())
			{
				break;
			}
			hasWeights = true;

			const uint base = set * 4;
			fastgltf::iterateAccessorWithIndex<fastgltf::math::u16vec4>(m_Asset, m_Asset.accessors[jointsAttribute->accessorIndex],
				[&](fastgltf::math::u16vec4 joints, size_t vertex)
				{
					for (uint i = 0; i < 4; ++i)
					{
						const uint16 joint = joints[i];
						sourceBones[vertex * c_MaxSourceInfluences + base + i] = joint < skin.joints.size()
							? m_NodeToBone[static_cast<uint>(skin.joints[joint])] : 0;
					}
				});
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(m_Asset, m_Asset.accessors[weightsAttribute->accessorIndex],
				[&](fastgltf::math::fvec4 weights, size_t vertex)
				{
					for (uint i = 0; i < 4; ++i)
					{
						sourceWeights[vertex * c_MaxSourceInfluences + base + i] = weights[i];
					}
				});
		}

		if (!hasWeights)
		{
			TYR_LOG_WARNING("A skinned primitive has no bone weights, so it follows the first bone.");
		}

		for (uint vertex = 0; vertex < vertexCount; ++vertex)
		{
			const uint16* bones = sourceBones + vertex * c_MaxSourceInfluences;
			float* weights = sourceWeights + vertex * c_MaxSourceInfluences;
			ModelImportSkinWeights& out = outWeights[vertex];

			float total = 0.0f;
			for (uint slot = 0; slot < 4; ++slot)
			{
				// Takes the largest remaining influence each time.
				uint best = 0;
				for (uint i = 1; i < c_MaxSourceInfluences; ++i)
				{
					best = weights[i] > weights[best] ? i : best;
				}
				const float weight = weights[best] > 0.0f ? weights[best] : 0.0f;
				out.bones[slot] = weight > 0.0f ? static_cast<uint8>(bones[best]) : 0;
				out.weights[slot] = weight;
				total += weight;
				weights[best] = 0.0f;
			}

			if (total <= 0.0f)
			{
				out.bones[0] = 0;
				out.weights[0] = 1.0f;
				continue;
			}
			for (float& weight : out.weights)
			{
				weight /= total;
			}
		}
	}

	void GltfModelLoader::LoadAnimations(ModelImportResult& outResult)
	{
		for (const fastgltf::Animation& animation : m_Asset.animations)
		{
			ModelImportAnimation& imported = outResult.AddAnimation();
			imported.name.Reset();
			imported.clip.Reset();

			if (!LoadAnimation(animation, outResult.skeleton, imported.clip))
			{
				TYR_LOG_WARNING("Animation %s doesn't move the skeleton, so it's skipped.", animation.name.c_str());
				outResult.animationCount--;
				continue;
			}

			char name[64];
			if (animation.name.empty())
			{
				snprintf(name, sizeof(name), "Animation%u", outResult.animationCount - 1);
			}
			else
			{
				CopyFileSafeName(animation.name, name, sizeof(name));
			}
			imported.name = name;
		}
	}

	bool GltfModelLoader::LoadAnimation(const fastgltf::Animation& animation, const Skeleton& skeleton, AnimationClip& outClip)
	{
		const uint boneCount = skeleton.BoneCount();

		float duration = 0.0f;
		bool movesSkeleton = false;
		for (const fastgltf::AnimationChannel& channel : animation.channels)
		{
			if (!channel.nodeIndex.has_value() || m_NodeToBone[static_cast<uint>(*channel.nodeIndex)] == SkeletonConstants::c_NoParent
				|| channel.path == fastgltf::AnimationPath::Weights)
			{
				continue;
			}
			movesSkeleton = true;

			const fastgltf::Accessor& input = m_Asset.accessors[animation.samplers[channel.samplerIndex].inputAccessor];
			if (input.count > 0)
			{
				duration = std::max(duration, fastgltf::getAccessorElement<float>(m_Asset, input, input.count - 1));
			}
		}

		if (!movesSkeleton)
		{
			return false;
		}

		outClip.duration = duration;
		outClip.frameCount = duration > 0.0f ? static_cast<uint>(std::ceil(duration * AnimationConstants::c_SampleRate)) + 1 : 1;
		// Keys are spread evenly from 0 to the duration, so the last lands exactly on it.
		outClip.sampleRate = outClip.frameCount > 1 ? static_cast<float>(outClip.frameCount - 1) / duration : AnimationConstants::c_SampleRate;
		outClip.tracks.Resize(boneCount);

		m_TrackTranslations.Resize(outClip.frameCount);
		m_TrackRotations.Resize(outClip.frameCount);
		m_TrackScales.Resize(outClip.frameCount);

		for (uint bone = 0; bone < boneCount; ++bone)
		{
			const fastgltf::AnimationChannel* channels[3] = {};
			const uint node = m_BoneNodes[bone];
			for (const fastgltf::AnimationChannel& channel : animation.channels)
			{
				if (!channel.nodeIndex.has_value() || *channel.nodeIndex != node)
				{
					continue;
				}
				if (channel.path == fastgltf::AnimationPath::Translation)
				{
					channels[0] = &channel;
				}
				else if (channel.path == fastgltf::AnimationPath::Rotation)
				{
					channels[1] = &channel;
				}
				else if (channel.path == fastgltf::AnimationPath::Scale)
				{
					channels[2] = &channel;
				}
			}

			AnimationTrack& track = outClip.tracks[bone];
			track = {};
			if (!channels[0] && !channels[1] && !channels[2])
			{
				continue;
			}

			// Unanimated channels hold the node's own rest value.
			const fastgltf::TRS& rest = std::get<fastgltf::TRS>(m_Asset.nodes[node].transform);
			const Vector3 restTranslation = ToEngineTranslation(rest.translation[0], rest.translation[1], rest.translation[2]);
			const Quaternion restRotation = ToEngineRotation(rest.rotation[0], rest.rotation[1], rest.rotation[2], rest.rotation[3]);
			const Vector3 restScale(rest.scale[0], rest.scale[1], rest.scale[2]);
			for (uint frame = 0; frame < outClip.frameCount; ++frame)
			{
				m_TrackTranslations[frame] = restTranslation;
				m_TrackRotations[frame] = restRotation;
				m_TrackScales[frame] = restScale;
			}

			for (uint path = 0; path < 3; ++path)
			{
				if (!channels[path])
				{
					continue;
				}

				const fastgltf::AnimationSampler& sampler = animation.samplers[channels[path]->samplerIndex];
				ReadChannelKeys(sampler, channels[path]->path);
				for (uint frame = 0; frame < outClip.frameCount; ++frame)
				{
					const float time = std::min(static_cast<float>(frame) / outClip.sampleRate, duration);
					const Vector4 value = SampleChannelKeys(sampler.interpolation, path == 1, time);
					if (path == 0)
					{
						m_TrackTranslations[frame] = Vector3(value.x, value.y, value.z);
					}
					else if (path == 1)
					{
						m_TrackRotations[frame] = Quaternion(value.w, value.x, value.y, value.z);
					}
					else
					{
						m_TrackScales[frame] = Vector3(value.x, value.y, value.z);
					}
				}
			}

			if (m_BoneHasParentOffset[bone])
			{
				for (uint frame = 0; frame < outClip.frameCount; ++frame)
				{
					const BoneTransform local{ m_TrackTranslations[frame], m_TrackRotations[frame], m_TrackScales[frame] };
					const BoneTransform offset = ApplyParentOffset(local, m_BoneParentOffsets[bone]);
					m_TrackTranslations[frame] = offset.translation;
					m_TrackRotations[frame] = offset.rotation;
					m_TrackScales[frame] = offset.scale;
				}
			}

			// Neighbouring keys are kept on the same side, so blending between them takes the short way.
			for (uint frame = 1; frame < outClip.frameCount; ++frame)
			{
				if (m_TrackRotations[frame].Dot(m_TrackRotations[frame - 1]) < 0.0f)
				{
					m_TrackRotations[frame] = -m_TrackRotations[frame];
				}
			}

			const BoneTransform& bind = skeleton.bindPose[bone];
			AddTrackKeys(m_TrackTranslations, bind.translation, outClip.translations, track.translationOffset, track.translationCount);
			AddTrackKeys(m_TrackRotations, bind.rotation, outClip.rotations, track.rotationOffset, track.rotationCount);
			AddTrackKeys(m_TrackScales, bind.scale, outClip.scales, track.scaleOffset, track.scaleCount);
		}

		return true;
	}

	void GltfModelLoader::ReadChannelKeys(const fastgltf::AnimationSampler& sampler, fastgltf::AnimationPath path)
	{
		const fastgltf::Accessor& input = m_Asset.accessors[sampler.inputAccessor];
		const fastgltf::Accessor& output = m_Asset.accessors[sampler.outputAccessor];

		m_KeyTimes.Resize(static_cast<uint>(input.count));
		fastgltf::iterateAccessorWithIndex<float>(m_Asset, input, [&](float time, size_t i)
		{
			m_KeyTimes[static_cast<uint>(i)] = time;
		});

		m_KeyValues.Resize(static_cast<uint>(output.count));
		if (path == fastgltf::AnimationPath::Rotation)
		{
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(m_Asset, output, [&](fastgltf::math::fvec4 v, size_t i)
			{
				const Quaternion rotation = ToEngineRotation(v[0], v[1], v[2], v[3]);
				m_KeyValues[static_cast<uint>(i)] = Vector4(rotation.x, rotation.y, rotation.z, rotation.w);
			});
		}
		else
		{
			const bool isTranslation = path == fastgltf::AnimationPath::Translation;
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(m_Asset, output, [&](fastgltf::math::fvec3 v, size_t i)
			{
				m_KeyValues[static_cast<uint>(i)] = Vector4(v[0], v[1], isTranslation ? -v[2] : v[2], 0.0f);
			});
		}
	}

	Vector4 GltfModelLoader::SampleChannelKeys(fastgltf::AnimationInterpolation interpolation, bool isRotation, float time) const
	{
		const uint keyCount = m_KeyTimes.Size();
		const bool cubic = interpolation == fastgltf::AnimationInterpolation::CubicSpline;
		// Cubic spline keys are stored as in-tangent, value, out-tangent.
		const uint stride = cubic ? 3 : 1;
		const uint valueOffset = cubic ? 1 : 0;
		if (keyCount == 0 || m_KeyValues.Size() < keyCount * stride)
		{
			return isRotation ? Vector4(0.0f, 0.0f, 0.0f, 1.0f) : Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		}

		if (time <= m_KeyTimes[0] || keyCount == 1)
		{
			return m_KeyValues[valueOffset];
		}
		if (time >= m_KeyTimes[keyCount - 1])
		{
			return m_KeyValues[(keyCount - 1) * stride + valueOffset];
		}

		uint key = 0;
		while (key + 1 < keyCount - 1 && m_KeyTimes[key + 1] <= time)
		{
			++key;
		}

		const float keyDuration = m_KeyTimes[key + 1] - m_KeyTimes[key];
		const float t = keyDuration > 0.0f ? (time - m_KeyTimes[key]) / keyDuration : 0.0f;
		const Vector4& a = m_KeyValues[key * stride + valueOffset];
		const Vector4& b = m_KeyValues[(key + 1) * stride + valueOffset];

		if (interpolation == fastgltf::AnimationInterpolation::Step)
		{
			return a;
		}

		if (cubic)
		{
			const Vector4& outTangent = m_KeyValues[key * stride + 2];
			const Vector4& inTangent = m_KeyValues[(key + 1) * stride];
			const float t2 = t * t;
			const float t3 = t2 * t;
			Vector4 value = a * (2.0f * t3 - 3.0f * t2 + 1.0f) + outTangent * ((t3 - 2.0f * t2 + t) * keyDuration)
				+ b * (-2.0f * t3 + 3.0f * t2) + inTangent * ((t3 - t2) * keyDuration);
			if (isRotation)
			{
				const Quaternion rotation = Quaternion::SafeNormalize(Quaternion(value.w, value.x, value.y, value.z));
				value = Vector4(rotation.x, rotation.y, rotation.z, rotation.w);
			}
			return value;
		}

		if (isRotation)
		{
			const Quaternion rotation = Quaternion::Slerp(t, Quaternion(a.w, a.x, a.y, a.z), Quaternion(b.w, b.x, b.y, b.z));
			const Quaternion normalized = Quaternion::SafeNormalize(rotation);
			return Vector4(normalized.x, normalized.y, normalized.z, normalized.w);
		}
		return a + (b - a) * t;
	}
}
