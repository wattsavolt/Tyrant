#include <cstdlib>

#include "Core.h"
#include "AppModule.h"
#include "Platform/Platform.h"
#include "Utility/Utility.h"
#include "Utility/LibraryLoader.h"
#include "Module/ModuleManager.h"
#include "EngineLoop.h" 

namespace tyr
{
    int Run(int windowShowFlag = 1)
    {
        EngineLoop engineLoop;
        engineLoop.Initialize([]() {
           
            TYR_REGISTER_MODULE(AppModule);
        });
        AppModule* appModule;
        TYR_GET_MODULE(AppModule, appModule);
        engineLoop.Run([&appModule]() -> bool {
            return appModule->WantsExit();
        });
        engineLoop.Shutdown();
        return EXIT_SUCCESS;
    }
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

    // TODO: Support command line arguments and allow game to be launched independently when in editor mode (always launched independently when not in editor mode)
    return tyr::Run(nCmdShow);
}
#else
int main(int argc, char* argv[])
{
    return tyr::Run(nCmdShow);
}
#endif
	
