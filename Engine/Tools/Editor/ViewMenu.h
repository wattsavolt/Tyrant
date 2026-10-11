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
	};

	// Draws the View menu, which opens and closes the editor's tabs and viewport aids.
	class ViewMenu final
	{
	public:
		void Draw(ViewSettings& settings);
	};
}
