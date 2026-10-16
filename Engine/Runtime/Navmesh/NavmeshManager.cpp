#include "NavmeshManager.h"
#include "Private/NavmeshFormat.h"
#include <cstring>

#include <DetourAlloc.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

namespace tyr
{
	namespace
	{
		// Search nodes a query can use. Enough for paths across the whole level.
		constexpr int c_MaxQueryNodes = 4096;
		// How far from the start and end, along each axis, the nearest navmesh is looked for.
		const float c_PathSearchExtents[3] = { 2.0f, 4.0f, 2.0f };
		// Debug lines float this far over the polygons, so they aren't hidden in the floor.
		constexpr float c_DebugLineLift = 0.05f;

		// Detour asks for random numbers from 0 to 1 through a plain function, so they come from here.
		uint g_RandomState = 0x2545F491u;

		float Random01()
		{
			g_RandomState = g_RandomState * 1664525u + 1013904223u;
			return static_cast<float>(g_RandomState >> 8) * (1.0f / 16777216.0f);
		}

		// Detour allocates through the engine's heap.
		void* Allocate(size_t size, dtAllocHint /*hint*/)
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
	}

	// A navmesh and the query that searches it.
	struct NavmeshData
	{
		dtNavMesh* mesh = nullptr;
		dtNavMeshQuery* query = nullptr;
		dtQueryFilter filter;
	};

	NavmeshManager::NavmeshManager()
	{
		dtAllocSetCustom(&Allocate, &FreeBlock);
	}

	NavmeshManager::~NavmeshManager()
	{
		// Whatever made each navmesh destroys it, before this goes.
	}

	Handle NavmeshManager::CreateNavmesh(const uint8* data, size_t size)
	{
		NavmeshFormat::Header header;
		if (size < sizeof(header))
		{
			return {};
		}
		memcpy(&header, data, sizeof(header));
		if (header.version != NavmeshConstants::c_DataVersion)
		{
			TYR_LOG_ERROR("The navmesh was built by another version of the engine. Build it again.");
			return {};
		}

		dtNavMeshParams params;
		memcpy(params.orig, header.origin, sizeof(params.orig));
		params.tileWidth = header.tileWidth;
		params.tileHeight = header.tileHeight;
		params.maxTiles = static_cast<int>(header.maxTiles);
		params.maxPolys = static_cast<int>(header.maxPolysPerTile);

		NavmeshData* navmesh = new NavmeshData();
		navmesh->mesh = dtAllocNavMesh();
		navmesh->query = dtAllocNavMeshQuery();
		bool failed = dtStatusFailed(navmesh->mesh->init(&params));

		size_t offset = sizeof(header);
		for (uint i = 0; i < header.tileCount && !failed; ++i)
		{
			uint tileSize = 0;
			if (offset + sizeof(tileSize) > size)
			{
				failed = true;
				break;
			}
			memcpy(&tileSize, data + offset, sizeof(tileSize));
			offset += sizeof(tileSize);
			if (offset + tileSize > size)
			{
				failed = true;
				break;
			}
			// Detour keeps the tile's data and frees it with the navmesh.
			unsigned char* tileData = static_cast<unsigned char*>(dtAlloc(tileSize, DT_ALLOC_PERM));
			memcpy(tileData, data + offset, tileSize);
			offset += tileSize;
			if (dtStatusFailed(navmesh->mesh->addTile(tileData, static_cast<int>(tileSize), DT_TILE_FREE_DATA, 0, nullptr)))
			{
				dtFree(tileData);
				failed = true;
			}
		}

		failed = failed || dtStatusFailed(navmesh->query->init(navmesh->mesh, c_MaxQueryNodes));
		if (failed)
		{
			TYR_LOG_ERROR("The navmesh is damaged and couldn't be loaded. Build it again.");
			dtFreeNavMeshQuery(navmesh->query);
			dtFreeNavMesh(navmesh->mesh);
			delete navmesh;
			return {};
		}

		const Handle handle = m_Navmeshes.Create();
		m_Navmeshes[handle] = navmesh;
		return handle;
	}

	void NavmeshManager::DestroyNavmesh(Handle navmesh)
	{
		NavmeshData* data = m_Navmeshes[navmesh];
		dtFreeNavMeshQuery(data->query);
		dtFreeNavMesh(data->mesh);
		delete data;
		m_Navmeshes.Delete(navmesh);
	}

	bool NavmeshManager::FindNearestPoint(Handle navmesh, const Vector3& point, const Vector3& searchExtents, Vector3& outPoint) const
	{
		const NavmeshData& data = *m_Navmeshes[navmesh];
		dtPolyRef polygon = 0;
		float nearest[3];
		if (dtStatusFailed(data.query->findNearestPoly(&point.x, &searchExtents.x, &data.filter, &polygon, nearest)) || polygon == 0)
		{
			return false;
		}
		outPoint = Vector3(nearest[0], nearest[1], nearest[2]);
		return true;
	}

	bool NavmeshManager::FindRandomPoint(Handle navmesh, const Vector3& centre, float radius, Vector3& outPoint) const
	{
		const NavmeshData& data = *m_Navmeshes[navmesh];
		dtPolyRef startPolygon = 0;
		float start[3];
		data.query->findNearestPoly(&centre.x, c_PathSearchExtents, &data.filter, &startPolygon, start);
		if (startPolygon == 0)
		{
			return false;
		}
		dtPolyRef randomPolygon = 0;
		float point[3];
		if (dtStatusFailed(data.query->findRandomPointAroundCircle(startPolygon, start, radius, &data.filter, &Random01, &randomPolygon, point)))
		{
			return false;
		}
		outPoint = Vector3(point[0], point[1], point[2]);
		return true;
	}

	bool NavmeshManager::FindPath(Handle navmesh, const Vector3& start, const Vector3& end, NavPath& outPath) const
	{
		const NavmeshData& data = *m_Navmeshes[navmesh];
		outPath.points.Clear();
		outPath.partial = false;
		outPath.truncated = false;

		dtPolyRef startPolygon = 0;
		dtPolyRef endPolygon = 0;
		float startPoint[3];
		float endPoint[3];
		data.query->findNearestPoly(&start.x, c_PathSearchExtents, &data.filter, &startPolygon, startPoint);
		data.query->findNearestPoly(&end.x, c_PathSearchExtents, &data.filter, &endPolygon, endPoint);
		if (startPolygon == 0 || endPolygon == 0)
		{
			return false;
		}

		dtPolyRef polygons[NavmeshConstants::c_MaxPathPolys];
		int polygonCount = 0;
		if (dtStatusFailed(data.query->findPath(startPolygon, endPolygon, startPoint, endPoint, &data.filter, polygons, &polygonCount,
			static_cast<int>(NavmeshConstants::c_MaxPathPolys))) || polygonCount == 0)
		{
			return false;
		}

		// A path that stops short ends at the closest point of the last polygon it reached.
		if (polygons[polygonCount - 1] != endPolygon)
		{
			outPath.partial = true;
			data.query->closestPointOnPoly(polygons[polygonCount - 1], endPoint, endPoint, nullptr);
		}

		float corners[NavmeshConstants::c_MaxPathPoints * 3];
		int cornerCount = 0;
		const dtStatus status = data.query->findStraightPath(startPoint, endPoint, polygons, polygonCount, corners, nullptr, nullptr, &cornerCount,
			static_cast<int>(NavmeshConstants::c_MaxPathPoints));
		outPath.truncated = dtStatusDetail(status, DT_BUFFER_TOO_SMALL);
		for (int i = 0; i < cornerCount; ++i)
		{
			outPath.points.Add(Vector3(corners[i * 3], corners[i * 3 + 1], corners[i * 3 + 2]));
		}
		return cornerCount > 0;
	}

	void NavmeshManager::AddDebugLines(Handle navmesh, const Vector3& centre, float radius, Array<Vector3>& outLines) const
	{
		const dtNavMesh& mesh = *m_Navmeshes[navmesh]->mesh;
		const float radiusSquared = radius * radius;
		for (int t = 0; t < mesh.getMaxTiles(); ++t)
		{
			const dtMeshTile* tile = mesh.getTile(t);
			if (!tile || !tile->header)
			{
				continue;
			}
			// Tiles entirely out of reach are skipped.
			const float* tileMin = tile->header->bmin;
			const float* tileMax = tile->header->bmax;
			const float dx = centre.x < tileMin[0] ? tileMin[0] - centre.x : (centre.x > tileMax[0] ? centre.x - tileMax[0] : 0.0f);
			const float dz = centre.z < tileMin[2] ? tileMin[2] - centre.z : (centre.z > tileMax[2] ? centre.z - tileMax[2] : 0.0f);
			if (dx * dx + dz * dz > radiusSquared)
			{
				continue;
			}

			for (int p = 0; p < tile->header->polyCount; ++p)
			{
				const dtPoly& polygon = tile->polys[p];
				if (polygon.getType() == DT_POLYTYPE_OFFMESH_CONNECTION)
				{
					continue;
				}
				for (int v = 0; v < polygon.vertCount; ++v)
				{
					const float* a = &tile->verts[polygon.verts[v] * 3];
					const float* b = &tile->verts[polygon.verts[(v + 1) % polygon.vertCount] * 3];
					const Vector3 start(a[0], a[1] + c_DebugLineLift, a[2]);
					const Vector3 offset = start - centre;
					if (offset.Dot(offset) > radiusSquared)
					{
						continue;
					}
					outLines.Add(start);
					outLines.Add(Vector3(b[0], b[1] + c_DebugLineLift, b[2]));
				}
			}
		}
	}
}
