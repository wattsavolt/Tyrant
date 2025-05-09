


#include <cstdlib>

#include "Memory/MemoryOverrides.h"
#include "EngineManager.h"
#include "Core.h"
#include "App/AppBase.h"
#include "Platform/Platform.h"
#include "Utility/Utility.h"
#include "Utility/LibraryLoader.h"
#include "Engine.h" 
#if TYR_IS_EDITOR
#include "Editor.h"
#endif


typedef TYR_IMPORT tyr::AppBase* (TYR_STDCALL *CreateApp)();

tyr::AppBase* LoadApp(const tyr::LibraryLoader& loader)
{
    CreateApp createApp = static_cast<CreateApp>(loader.GetProcessAddress("CreateApp"));
    return createApp();
}

int Run(int windowShowFlag = 1)
{
    tyr::EngineParams engineParams;
#if TYR_IS_EDITOR
    engineParams.app = new tyr::Editor();
#else
    clr::LibraryLoader loader;
    const char* libName = TYR_TO_LITERAL(TYR_APP_LIB_NAME);
    if (!loader.Load(libName))
    {
        return EXIT_FAILURE;
    }
    engineParams.app = LoadApp(loader);
#endif

    int result;

    if (engineParams.app)
    {
        engineParams.appName = TYR_TO_LITERAL(TYR_APP_NAME);
        engineParams.windowShowFlag = 1;
        tyr::EngineManager::RunEngine(engineParams);
        delete engineParams.app;
        result = EXIT_SUCCESS;
    }
    else
    {
        result = EXIT_FAILURE;
    } 

#if !TYR_IS_EDITOR
    loader.Unload();
#endif

    return result;
}

#if TYR_PLATFORM == TYR_PLATFORM_WINDOWS
#include <windows.h>
int APIENTRY WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    return Run(nCmdShow);
}
#else
int main(int argc, char* argv[])
{
    return Run(nCmdShow);
}
#endif
	
