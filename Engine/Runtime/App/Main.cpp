#include <cstdlib>
#include <cstring>

#include "Core.h"
#include "AppModule.h"
#include "Platform/Platform.h"
#include "Utility/Utility.h"
#include "Utility/LibraryLoader.h"
#include "Module/ModuleManager.h"
#include "Config/CommandLine.h"
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

namespace
{
    constexpr size_t c_MaxCmdLineLength = 1024;
    constexpr int c_MaxArgs = 32;

    // Splits lpCmdLine (space/tab separated, no quoting support - not needed for switch-style
    // args like "-editor") into an argv-shaped array of null-terminated tokens, written in
    // place into "buffer".
    void TokenizeCommandLine(const char* cmdLine, const char* argv[], int& argc, char* buffer, size_t bufferSize)
    {
        argc = 0;

        const size_t len = std::strlen(cmdLine);
        const size_t copyLen = std::min(len, bufferSize - 1);
        std::memcpy(buffer, cmdLine, copyLen);
        buffer[copyLen] = '\0';

        char* p = buffer;
        while (*p != '\0' && argc < c_MaxArgs)
        {
            while (*p == ' ' || *p == '\t')
            {
                ++p;
            }
            if (*p == '\0')
            {
                break;
            }

            argv[argc++] = p;

            while (*p != '\0' && *p != ' ' && *p != '\t')
            {
                ++p;
            }
            if (*p != '\0')
            {
                *p = '\0';
                ++p;
            }
        }
    }
}

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

    char cmdLineBuffer[c_MaxCmdLineLength];
    const char* argv[c_MaxArgs];
    int argc = 0;
    TokenizeCommandLine(lpCmdLine, argv, argc, cmdLineBuffer, sizeof(cmdLineBuffer));
    tyr::CommandLine::Create(argc, argv);

    return tyr::Run(nCmdShow);
}
#else
int main(int argc, char* argv[])
{
    // argv[0] is the program name, not a real argument - skip it so both entry points feed
    // CommandLine identically-shaped input (see WinMain, where lpCmdLine never includes it).
    tyr::CommandLine::Create(argc - 1, argv + 1);
    return tyr::Run();
}
#endif

