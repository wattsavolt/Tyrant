#pragma once

// Nuklear, set up the same way GUIModule builds it, for game code drawing its HUD and menus with
// GUIModule's context and fonts. The engine builds Nuklear and exports it to games.
#include "EngineMacros.h"
#define NK_API extern TYR_ENGINE_API
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "nuklear.h"
