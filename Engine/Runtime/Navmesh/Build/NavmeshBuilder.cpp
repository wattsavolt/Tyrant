#include "NavmeshBuilder.h"
#include "Private/NavmeshFormat.h"
#include "Math/Math.h"
#include <algorithm>
#include <cstring>

#include <Recast.h>
#include <RecastAlloc.h>
#include <DetourAlloc.h>
#include <DetourNavMeshBuilder.h>

namespace tyr
{
	namespace
	{
		// Polygons get up to this many corners.
		constexpr int c_MaxVertsPerPoly = 6;
		// The flag every walkable polygon has, which queries include.
		constexpr unsigned short c_WalkFlag = 1;
		// Detour's polygon references have this many bits for the tile and the polygon within it.
		constexpr uint c_TileAndPolyBits = 22;
		constexpr uint c_MaxTileBits = 14;

		void* Allocate(size_t size, rcAllocHint /*hint*/)
		{
			return MemAlloc(size);
		}

		void FreeBlock(void* block)
		{
			if (block)
			{
				Free(block);
			}
		}

		uint NextPowerOfTwo(uint value)
		{
			uint power = 1;
			while (power < value)
			{
				power <<= 1;
			}
			return power;
		}

		uint Log2(uint value)
		{
			uint bits = 0;
			while (value > 1)
			{
				value >>= 1;
				++bits;
			}
			return bits;
		}

		void Append(Array<uint8>& data, const void* bytes, size_t size)
		{
			const uint offset = data.Size();
			data.Resize(offset + static_cast<uint>(size));
			memcpy(data.Data() + offset, bytes, size);
		}

		// What one tile is built from: its triangles, turned to face up, and whether each is walkable.
		struct TileInput
		{
			Array<float> vertices;
			Array<unsigned char> areas;
		};

		// The Recast steps for one tile. Each frees what it made, so a tile that fails part way
		// leaves nothing behind.
		class TileBuild final
		{
		public:
			~TileBuild()
			{
				rcFreeHeightField(heightfield);
				rcFreeCompactHeightfield(compact);
				rcFreeContourSet(contours);
				rcFreePolyMesh(polyMesh);
				rcFreePolyMeshDetail(detailMesh);
			}

			rcHeightfield* heightfield = nullptr;
			rcCompactHeightfield* compact = nullptr;
			rcContourSet* contours = nullptr;
			rcPolyMesh* polyMesh = nullptr;
			rcPolyMeshDetail* detailMesh = nullptr;
		};

		bool Inside(const NavmeshBounds* bounds, uint boundsCount, float x, float y, float z)
		{
			for (uint i = 0; i < boundsCount; ++i)
			{
				const NavmeshBounds& box = bounds[i];
				if (x >= box.min.x && x <= box.max.x && y >= box.min.y && y <= box.max.y && z >= box.min.z && z <= box.max.z)
				{
					return true;
				}
			}
			return false;
		}

		// Floors outside every box can't be walked on. Done before the walkable area is shrunk away
		// from edges, so the boxes' sides become edges too.
		void ClearOutsideBounds(rcCompactHeightfield& compact, const NavmeshBounds* bounds, uint boundsCount)
		{
			if (boundsCount == 0)
			{
				return;
			}
			for (int z = 0; z < compact.height; ++z)
			{
				for (int x = 0; x < compact.width; ++x)
				{
					const rcCompactCell& cell = compact.cells[x + z * compact.width];
					const float worldX = compact.bmin[0] + (x + 0.5f) * compact.cs;
					const float worldZ = compact.bmin[2] + (z + 0.5f) * compact.cs;
					for (unsigned int i = cell.index; i < cell.index + cell.count; ++i)
					{
						const float worldY = compact.bmin[1] + compact.spans[i].y * compact.ch;
						if (!Inside(bounds, boundsCount, worldX, worldY, worldZ))
						{
							compact.areas[i] = RC_NULL_AREA;
						}
					}
				}
			}
		}

		// Builds one tile's Detour data, or returns false when the tile has nothing walkable.
		bool BuildTile(rcContext& context, const rcConfig& config, const NavmeshBuildSettings& settings, const TileInput& input,
			const NavmeshBounds* bounds, uint boundsCount, int tileX, int tileZ, unsigned char*& outData, int& outSize)
		{
			const int triangleCount = static_cast<int>(input.areas.Size());
			if (triangleCount == 0)
			{
				return false;
			}

			TileBuild build;
			build.heightfield = rcAllocHeightfield();
			if (!rcCreateHeightfield(&context, *build.heightfield, config.width, config.height, config.bmin, config.bmax, config.cs, config.ch)
				|| !rcRasterizeTriangles(&context, input.vertices.Data(), input.areas.Data(), triangleCount, *build.heightfield, config.walkableClimb))
			{
				return false;
			}

			// Low obstacles can be stepped over, ledges can't be stood on, and low gaps can't be stood in.
			rcFilterLowHangingWalkableObstacles(&context, config.walkableClimb, *build.heightfield);
			rcFilterLedgeSpans(&context, config.walkableHeight, config.walkableClimb, *build.heightfield);
			rcFilterWalkableLowHeightSpans(&context, config.walkableHeight, *build.heightfield);

			build.compact = rcAllocCompactHeightfield();
			if (!rcBuildCompactHeightfield(&context, config.walkableHeight, config.walkableClimb, *build.heightfield, *build.compact))
			{
				return false;
			}
			ClearOutsideBounds(*build.compact, bounds, boundsCount);
			if (!rcErodeWalkableArea(&context, config.walkableRadius, *build.compact)
				|| !rcBuildDistanceField(&context, *build.compact)
				|| !rcBuildRegions(&context, *build.compact, config.borderSize, config.minRegionArea, config.mergeRegionArea))
			{
				return false;
			}

			build.contours = rcAllocContourSet();
			if (!rcBuildContours(&context, *build.compact, config.maxSimplificationError, config.maxEdgeLen, *build.contours)
				|| build.contours->nconts == 0)
			{
				return false;
			}

			build.polyMesh = rcAllocPolyMesh();
			build.detailMesh = rcAllocPolyMeshDetail();
			if (!rcBuildPolyMesh(&context, *build.contours, config.maxVertsPerPoly, *build.polyMesh)
				|| !rcBuildPolyMeshDetail(&context, *build.polyMesh, *build.compact, config.detailSampleDist, config.detailSampleMaxError, *build.detailMesh)
				|| build.polyMesh->npolys == 0)
			{
				return false;
			}

			for (int i = 0; i < build.polyMesh->npolys; ++i)
			{
				build.polyMesh->flags[i] = build.polyMesh->areas[i] == RC_WALKABLE_AREA ? c_WalkFlag : 0;
			}

			dtNavMeshCreateParams params = {};
			params.verts = build.polyMesh->verts;
			params.vertCount = build.polyMesh->nverts;
			params.polys = build.polyMesh->polys;
			params.polyAreas = build.polyMesh->areas;
			params.polyFlags = build.polyMesh->flags;
			params.polyCount = build.polyMesh->npolys;
			params.nvp = build.polyMesh->nvp;
			params.detailMeshes = build.detailMesh->meshes;
			params.detailVerts = build.detailMesh->verts;
			params.detailVertsCount = build.detailMesh->nverts;
			params.detailTris = build.detailMesh->tris;
			params.detailTriCount = build.detailMesh->ntris;
			params.walkableHeight = settings.agentHeight;
			params.walkableRadius = settings.agentRadius;
			params.walkableClimb = settings.agentMaxClimb;
			params.tileX = tileX;
			params.tileY = tileZ;
			params.tileLayer = 0;
			rcVcopy(params.bmin, build.polyMesh->bmin);
			rcVcopy(params.bmax, build.polyMesh->bmax);
			params.cs = config.cs;
			params.ch = config.ch;
			params.buildBvTree = true;
			return dtCreateNavMeshData(&params, &outData, &outSize);
		}
	}

	namespace NavmeshBuilder
	{
		bool Build(const Vector3* triangles, uint triangleCount, const NavmeshBounds* bounds, uint boundsCount,
			const NavmeshBuildSettings& settings, Array<uint8>& outData)
		{
			outData.Clear();
			if (triangleCount == 0)
			{
				return false;
			}
			rcAllocSetCustom(&Allocate, &FreeBlock);

			float boundsMin[3] = { triangles[0].x, triangles[0].y, triangles[0].z };
			float boundsMax[3] = { triangles[0].x, triangles[0].y, triangles[0].z };
			for (uint i = 1; i < triangleCount * 3; ++i)
			{
				const float point[3] = { triangles[i].x, triangles[i].y, triangles[i].z };
				rcVmin(boundsMin, point);
				rcVmax(boundsMax, point);
			}

			// Only the area the bounds cover needs tiles, plus a margin for walls just outside them.
			if (boundsCount > 0)
			{
				float coveredMin[3] = { bounds[0].min.x, bounds[0].min.y, bounds[0].min.z };
				float coveredMax[3] = { bounds[0].max.x, bounds[0].max.y, bounds[0].max.z };
				for (uint i = 1; i < boundsCount; ++i)
				{
					rcVmin(coveredMin, &bounds[i].min.x);
					rcVmax(coveredMax, &bounds[i].max.x);
				}
				const float margin = 1.0f;
				for (int axis = 0; axis < 3; ++axis)
				{
					boundsMin[axis] = std::max(boundsMin[axis], coveredMin[axis] - margin);
					boundsMax[axis] = std::min(boundsMax[axis], coveredMax[axis] + margin);
				}
				if (boundsMin[0] >= boundsMax[0] || boundsMin[1] >= boundsMax[1] || boundsMin[2] >= boundsMax[2])
				{
					return false;
				}
			}

			rcConfig config = {};
			config.cs = settings.cellSize;
			config.ch = settings.cellHeight;
			config.walkableSlopeAngle = settings.agentMaxSlope;
			config.walkableHeight = static_cast<int>(Math::Ceil(settings.agentHeight / config.ch));
			config.walkableClimb = static_cast<int>(settings.agentMaxClimb / config.ch);
			config.walkableRadius = static_cast<int>(Math::Ceil(settings.agentRadius / config.cs));
			config.maxEdgeLen = static_cast<int>(settings.maxEdgeLength / config.cs);
			config.maxSimplificationError = settings.maxEdgeError;
			config.minRegionArea = static_cast<int>(settings.minRegionArea / (config.cs * config.cs));
			config.mergeRegionArea = config.minRegionArea * 10;
			config.maxVertsPerPoly = c_MaxVertsPerPoly;
			config.tileSize = static_cast<int>(settings.tileSize);
			config.borderSize = config.walkableRadius + 3;
			config.width = config.tileSize + config.borderSize * 2;
			config.height = config.tileSize + config.borderSize * 2;
			config.detailSampleDist = config.cs * 6.0f;
			config.detailSampleMaxError = config.ch;

			int gridWidth = 0;
			int gridDepth = 0;
			rcCalcGridSize(boundsMin, boundsMax, config.cs, &gridWidth, &gridDepth);
			const int tilesX = (gridWidth + config.tileSize - 1) / config.tileSize;
			const int tilesZ = (gridDepth + config.tileSize - 1) / config.tileSize;
			const uint tileBits = std::min(Log2(NextPowerOfTwo(static_cast<uint>(tilesX * tilesZ))), c_MaxTileBits);
			const float tileWorldSize = config.tileSize * config.cs;

			NavmeshFormat::Header header;
			header.version = NavmeshConstants::c_DataVersion;
			memcpy(header.origin, boundsMin, sizeof(header.origin));
			header.tileWidth = tileWorldSize;
			header.tileHeight = tileWorldSize;
			header.maxTiles = 1u << tileBits;
			header.maxPolysPerTile = 1u << (c_TileAndPolyBits - tileBits);
			Append(outData, &header, sizeof(header));

			rcContext context(false);
			TileInput input;
			const float border = config.borderSize * config.cs;
			const float walkableNormalY = Math::Cos(settings.agentMaxSlope * Math::c_DegToRad);
			uint tileCount = 0;
			for (int tileZ = 0; tileZ < tilesZ; ++tileZ)
			{
				for (int tileX = 0; tileX < tilesX; ++tileX)
				{
					// The tile and the border around it, which keeps walls just outside it in mind.
					config.bmin[0] = boundsMin[0] + tileX * tileWorldSize - border;
					config.bmin[1] = boundsMin[1];
					config.bmin[2] = boundsMin[2] + tileZ * tileWorldSize - border;
					config.bmax[0] = boundsMin[0] + (tileX + 1) * tileWorldSize + border;
					config.bmax[1] = boundsMax[1];
					config.bmax[2] = boundsMin[2] + (tileZ + 1) * tileWorldSize + border;

					input.vertices.Clear();
					input.areas.Clear();
					for (uint t = 0; t < triangleCount; ++t)
					{
						const Vector3& a = triangles[t * 3];
						const Vector3& b = triangles[t * 3 + 1];
						const Vector3& c = triangles[t * 3 + 2];
						if (std::max(a.x, std::max(b.x, c.x)) < config.bmin[0] || std::min(a.x, std::min(b.x, c.x)) > config.bmax[0]
							|| std::max(a.z, std::max(b.z, c.z)) < config.bmin[2] || std::min(a.z, std::min(b.z, c.z)) > config.bmax[2])
						{
							continue;
						}

						// Flat enough either way up is walkable, so it doesn't matter which way triangles wind.
						const Vector3 normal = (b - a).Cross(c - a);
						const float length = normal.Length();
						const bool walkable = length > 0.0f && Math::Abs(normal.y) / length >= walkableNormalY;
						const float points[9] = { a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z };
						for (float value : points)
						{
							input.vertices.Add(value);
						}
						input.areas.Add(walkable ? RC_WALKABLE_AREA : RC_NULL_AREA);
					}

					unsigned char* tileData = nullptr;
					int tileSize = 0;
					if (!BuildTile(context, config, settings, input, bounds, boundsCount, tileX, tileZ, tileData, tileSize))
					{
						continue;
					}
					const uint size = static_cast<uint>(tileSize);
					Append(outData, &size, sizeof(size));
					Append(outData, tileData, size);
					dtFree(tileData);
					++tileCount;
				}
			}

			if (tileCount == 0)
			{
				outData.Clear();
				return false;
			}
			header.tileCount = tileCount;
			memcpy(outData.Data(), &header, sizeof(header));
			return true;
		}
	}
}
