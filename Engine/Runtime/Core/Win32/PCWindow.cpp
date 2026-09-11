#include "PCWindow.h"
#include "Window/WindowDesc.h"
#include "Window/Window.h"
#include "resource.h"
#include <Utility/Utility.h>
#include <String/StringUtil.h>

namespace tyr
{
    HINSTANCE PCWindow::m_HInstance;
    LPCSTR PCWindow::m_WindowClass = "Tyrant";

    void PCWindow::InitializeWindow(const WindowDesc& desc, Window& window)
    {
        uint16 iconResourceId = desc.iconResourceId;
        if (!iconResourceId)
        {
            iconResourceId = IDI_ENGINE;
        }

        // Following should just be executed for the first/main window
        if (!m_HInstance)
        {
            // Any static or global function within the dll will do for below.
            // Could also get hInstance of dll from DllMain
            // If we were in exe, we would call GetModuleHandle(NULL) instead.
            if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)RegisterWindowClass, &m_HInstance) == 0)
            {
                DWORD e = GetLastError();
                StringStream ss;
                ss << "Failed to load module handle: " << e << ".";
                TYR_LOG_FATAL(ss.str().c_str());
            }

            ATOM result = RegisterWindowClass(iconResourceId);
            if (!result)
            {
                DWORD e = GetLastError();
                StringStream ss;
                ss << "Failed to register window class with error code: " << e << ".";
                TYR_LOG_FATAL(ss.str().c_str());
            }
        }

        // The title bar text
        //CHAR szTitle[MAX_LOADSTRING];                  
        // Switch from char* to LPCWSTR
        //mbstowcs(szTitle, desc.name, strlen(desc.name) + 1);//Plus null

        HWND hwnd = CreateWindow(m_WindowClass, desc.name, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, 0, desc.width, desc.height, nullptr, nullptr, m_HInstance, &window);

        if (!hwnd)
        {
            DWORD e = GetLastError();
            StringStream ss;
            ss << "Failed to create window with error code: " << e << ".";
            TYR_LOG_FATAL(ss.str().c_str());
        }
        
        ShowWindow(hwnd, desc.showFlag);
        UpdateWindow(hwnd);

        window.handle = static_cast<void*>(hwnd);
    }

    //
    //  FUNCTION: RegisterWindowClass()
    //
    //  PURPOSE: Registers the window class.
    //
    ATOM PCWindow::RegisterWindowClass(uint16 iconResourceId)
    {
        WNDCLASSEX wcex = { 0 };
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = &PCWindow::WndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = m_HInstance; 
        wcex.hIcon = LoadIcon(m_HInstance, MAKEINTRESOURCE(iconResourceId));
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wcex.lpszMenuName = 0;
        wcex.lpszClassName = m_WindowClass;
        wcex.hIconSm = wcex.hIcon;

        return RegisterClassEx(&wcex);
    }

    //
    //  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
    //
    //  PURPOSE: Processes messages for the main window.
    //
    //  WM_COMMAND  - process the application menu
    //  WM_PAINT    - Paint the main window
    //  WM_DESTROY  - post a quit message and return
    //
    //
    LRESULT CALLBACK PCWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        if (message == WM_NCCREATE)
        {
            CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            window = static_cast<Window*>(createStruct->lpCreateParams);

            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }

        if (window)
        {
            return HandleMessage(hWnd, message, wParam, lParam, *window);
        }

        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    // Message handler for about box.
    INT_PTR CALLBACK PCWindow::About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
    {
        UNREFERENCED_PARAMETER(lParam);
        switch (message)
        {
        case WM_INITDIALOG:
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
        }
        return (INT_PTR)FALSE;
    }

    LRESULT PCWindow::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, Window& window)
    {
        switch (message)
        {
        case WM_SIZE:
        {
            window.width = LOWORD(lParam);
            window.height = HIWORD(lParam);
            break;
        }
        case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);

            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(m_HInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // TODO: Add any drawing code that uses hdc here...

            EndPaint(hWnd, &ps);
        }
        break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }

        return 0;
    }

    void PCWindow::PollEvents(Window& window)
    {
        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                window.handle = nullptr;
            }
            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    bool PCWindow::IsWindowActive(const Window& window)
    {
        return window.handle != nullptr;
    }
}

