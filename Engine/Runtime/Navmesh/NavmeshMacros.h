#pragma once

#include "CoreMacros.h"

#if defined(TYR_NAVMESH_EXPORTS)
#		define TYR_NAVMESH_API TYR_EXPORT
#else
#		define TYR_NAVMESH_API TYR_IMPORT
#endif
