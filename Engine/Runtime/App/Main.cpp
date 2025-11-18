#include <cstdlib>

#include "Core.h"
#include "AppModule.h"
#include "Platform/Platform.h"
#include "Utility/Utility.h"
#include "Utility/LibraryLoader.h"
#include "Module/ModuleManager.h"
#include "Engine.h" 

namespace tyr
{
    int Run(int windowShowFlag = 1)
    {
        Engine* engine = new tyr::Engine();
        engine->Initialize([]() {
            // Application modules must be registered here
            TYR_REGISTER_MODULE(AppModule);
        });
        engine->Run();
        // All modules will be shutdown and unregistered here
        engine->Shutdown();
        delete engine;

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

    return tyr::Run(nCmdShow);
}
#else
int main(int argc, char* argv[])
{
    return tyr::Run(nCmdShow);
}
#endif
	
