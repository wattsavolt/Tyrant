#pragma once

#include "Core.h"
#include "Math/Vector3.h"

namespace tyr
{
	struct NavmeshConstants
	{
		static constexpr uint c_MaxNavmeshes = 4;
		// Polygons a path can cross, and the corners kept of it. A path with more corners keeps its
		// first ones, and whoever walks it finds the rest once there.
		static constexpr uint c_MaxPathPolys = 512;
		static constexpr uint c_MaxPathPoints = 24;
		// Written first in navmesh data, and bumped whenever its layout changes.
		static constexpr uint c_DataVersion = 0x4E415601;
	};

	// What a navmesh polygon is, and what can use it. Beside a door, where its leaf swings to, walking
	// costs more and nothing picks a spot to stand. Stairs are only walked by those allowed to change
	// floors, and nothing walks into a safe room.
	struct NavmeshAreas
	{
		static constexpr uint8 c_Door = 1;
		static constexpr uint8 c_Stairs = 2;
		static constexpr uint8 c_Safe = 3;
		static constexpr uint16 c_WalkFlag = 1;
		static constexpr uint16 c_DoorFlag = 2;
		static constexpr uint16 c_StairsFlag = 4;
		static constexpr uint16 c_SafeFlag = 8;
		// How many times dearer a door zone is to walk through than open floor.
		static constexpr float c_DoorCost = 6.0f;
	};

	// A four-cornered patch of floor marked as one of the NavmeshAreas, such as a door zone, between
	// two heights, in world space. The corners go round it in order.
	struct NavmeshZone
	{
		Vector3 corners[4];
		float minY = 0.0f;
		float maxY = 0.0f;
		uint8 area = NavmeshAreas::c_Door;
	};

	// The size of what walks the navmesh, and how finely it's built, in metres and degrees.
	struct NavmeshBuildSettings
	{
		// Narrow enough to fit through doorways, a metre wide, once kept clear of their frames.
		float agentRadius = 0.3f;
		float agentHeight = 1.8f;
		// The tallest step it walks up.
		float agentMaxClimb = 0.4f;
		float agentMaxSlope = 45.0f;
		// The size of the grid the walkable area is found on, across and up.
		float cellSize = 0.15f;
		float cellHeight = 0.1f;
		// Grid cells along each side of a tile. The navmesh is built and stored a tile at a time.
		uint tileSize = 64;
		// Walkable areas smaller than this, in square metres, are dropped, such as tabletops.
		float minRegionArea = 2.0f;
		// The longest a polygon edge gets along a wall, and how far it can stray from the wall.
		float maxEdgeLength = 12.0f;
		float maxEdgeError = 1.3f;
	};

	// A box the navmesh is built in, in world space. Floors outside every box aren't walkable.
	struct NavmeshBounds
	{
		Vector3 min;
		Vector3 max;
	};

	// A walkable route, from the start to the end, as the corners to walk to in turn.
	struct NavPath
	{
		LocalArray<Vector3, NavmeshConstants::c_MaxPathPoints> points;
		// The end couldn't be reached, so the path goes as close to it as it can.
		bool partial = false;
		// It had more corners than are kept, so it stops at the last one kept.
		bool truncated = false;
	};
}
