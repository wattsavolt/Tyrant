#include "GltfModelLoader.h"
#include "RenderResource/MeshUtil.h"

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
		bool ResolveTextureSource(const fastgltf::Asset& asset, const std::filesystem::path& baseDirectory,
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
					// A loose file on disk, referenced relative to the model's own folder.
					const std::filesystem::path resolved = baseDirectory / source.uri.fspath();
					Path& stored = resolvedPaths.ExpandOne();
					stored = resolved.string().c_str();
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

		fastgltf::Expected<fastgltf::GltfDataBuffer> bufferExpected = fastgltf::GltfDataBuffer::FromPath(filePath);
		if (!bufferExpected)
		{
			TYR_LOG_ERROR("Failed to open model file %s.", filePath);
			return false;
		}
		m_DataBuffer = std::move(bufferExpected.get());

		const std::filesystem::path path(filePath);
		m_BaseDirectory = path.parent_path();

		fastgltf::Parser parser;

		// LoadExternalBuffers so loose .bin files actually get read (a GLB's own embedded
		// binary chunk is always available regardless of this flag). GenerateMeshIndices so
		// every primitive has real indices, even ones authored without any. We deliberately
		// don't set LoadExternalImages - we want loose texture files to stay as plain paths,
		// which ImageLoader/MaterialImporter can read directly, rather than fastgltf reading
		// them into memory for us just so we hand the bytes straight back as a TextureSource.
		constexpr fastgltf::Options options = fastgltf::Options::LoadExternalBuffers
			| fastgltf::Options::GenerateMeshIndices
			| fastgltf::Options::DecomposeNodeMatrices;

		fastgltf::Expected<fastgltf::Asset> assetExpected = parser.loadGltf(m_DataBuffer, m_BaseDirectory, options);
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
			ModelImportMesh* mesh = LoadMesh(*node.meshIndex, outResult, m_MaterialIndexMap);
			if (!mesh)
			{
				continue;
			}
			anyMeshLoaded = true;

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

		return LoadMeshPrimitives(meshIndex, lod.vertices, lod.indices, lod.submeshes, outResult, materialIndexMap);
	}

	ModelImportMesh* GltfModelLoader::LoadMesh(size_t meshIndex, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap)
	{
		ModelImportMesh& mesh = outResult.AddMesh();

		if (!LoadMeshPrimitives(meshIndex, mesh.vertices, mesh.indices, mesh.submeshes, outResult, materialIndexMap))
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

		// A simple, not-tightest-possible bounding sphere - centered on the AABB, radius
		// reaching its farthest corner. Good enough for now.
		const Vector3 centre = (aabbMin + aabbMax) * 0.5f;
		const float radius = (aabbMax - centre).Length();
		mesh.sphere = BoundingSphere(centre, radius);

		return &mesh;
	}

	bool GltfModelLoader::LoadMeshPrimitives(size_t meshIndex, Array<Vertex>& outVertices, Array<uint>& outIndices,
		Array<ModelImportSubmesh>& outSubmeshes, ModelImportResult& outResult, HashMap<uint, uint>& materialIndexMap)
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

		if (gltfMaterial.pbrData.baseColorTexture.has_value())
		{
			ResolveTextureSource(m_Asset, m_BaseDirectory, gltfMaterial.pbrData.baseColorTexture->textureIndex, m_ResolvedImagePaths, desc.albedoSource);
		}

		const bool hasRoughnessMetallic = gltfMaterial.pbrData.metallicRoughnessTexture.has_value();
		if (hasRoughnessMetallic)
		{
			ResolveTextureSource(m_Asset, m_BaseDirectory, gltfMaterial.pbrData.metallicRoughnessTexture->textureIndex, m_ResolvedImagePaths, desc.roughnessMetallicSource);
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
				ResolveTextureSource(m_Asset, m_BaseDirectory, gltfMaterial.occlusionTexture->textureIndex, m_ResolvedImagePaths, desc.occlusionSource);
			}
		}

		if (gltfMaterial.normalTexture.has_value())
		{
			ResolveTextureSource(m_Asset, m_BaseDirectory, gltfMaterial.normalTexture->textureIndex, m_ResolvedImagePaths, desc.normalSource);
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
}
