#pragma once

namespace tyr
{
	// What the View menu turns on and off.
	struct ViewSettings
	{
		bool hierarchyOpen = true;
		bool actorsOpen = true;
		bool showGrid = true;
		bool snapToGrid = true;
		// Debug drawing over the level while editing.
		bool showActorBounds = false;
		bool showLightRanges = false;
		bool showActorNames = false;
		// Drawn while playing too, where they follow the physics.
		bool showColliders = false;
		// Noises enemies can hear, as rings out to how far they carry.
		bool showNoise = false;
		// The navmesh's polygons around the camera.
		bool showNavmesh = false;
		// The frame rate in the viewport's corner, while editing and playing.
		bool showStats = false;
	};

	// Draws the View menu, which opens and closes the editor's tabs and viewport aids.
	class ViewMenu final
	{
	public:
		void Draw(ViewSettings& settings);
	};
}
