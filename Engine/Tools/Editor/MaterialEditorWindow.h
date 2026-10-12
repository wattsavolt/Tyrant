#pragma once

#include "EditorMacros.h"
#include "PropertiesReflector.h"
#include "AssetSystem/AssetID.h"
#include "RenderResource/MaterialDesc.h"

namespace tyr
{
	class AssetManager;

	// A floating window for changing a material's parameters, opened from the asset browser.
	// Changes show straight away and are saved once a drag or edit finishes.
	class TYR_EDITOR_EXPORT MaterialEditorWindow final
	{
	public:
		explicit MaterialEditorWindow(AssetManager& assetManager);
		~MaterialEditorWindow();

		// Shows the material in the window, keeping it loaded while it's shown.
		void Open(AssetID material);
		void Draw();

	private:
		void Close();

		AssetManager& m_AssetManager;
		PropertiesReflector m_Reflector;
		AssetID m_Material;
		// The values being edited, copied from the material once it has loaded.
		MaterialParams m_Params;
		// Changed here but not saved yet, so the values above are newer than the material's.
		bool m_Unsaved = false;
		bool m_FocusPending = false;
	};
}
