#include <Windows.h>
#include <stdio.h>

#define ID_BEEP 1
#define ID_QUIT 2

const wchar_t* ButtonText1 = L"Beep";
const wchar_t* ButtonText2 = L"Ring";
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) { switch(msg) { case WM_CREATE: CreateWindowW(L"Button", ButtonText1, WS_VISIBLE | WS_CHILD , 20, 50, 800, 100, hwnd, (HMENU) ID_BEEP, NULL, NULL); CreateWindowW(L"Button", ButtonText2, WS_VISIBLE | WS_CHILD , 20, 300, 800, 100, hwnd, (HMENU) ID_QUIT, NULL, NULL); break; case WM_COMMAND: if (LOWORD(wParam) == ID_BEEP) { MessageBeep(MB_OK); wprintf(L"%ls\n", ButtonText1); } if (LOWORD(wParam) == ID_QUIT) { MessageBeep(MB_ICONWARNING); wprintf(L"%ls\n", ButtonText2); } break; case WM_DESTROY: PostQuitMessage(0); break; } return DefWindowProcW(hwnd, msg, wParam, lParam); }
int runWindows() { MSG  msg; WNDCLASSW wc = {0}; auto hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"Buttons"; wc.hInstance     = hInstance; wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE); wc.lpfnWndProc   = WndProc; wc.hCursor       = LoadCursor(0, IDC_ARROW); RegisterClassW(&wc); auto Win = CreateWindowW(wc.lpszClassName, L"Buttons", WS_POPUP  | WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 1600, 800, 0, 0, hInstance, 0); while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); } DestroyWindow(Win); return 0; }

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, 
    WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE:
            CreateWindowW(L"Button", ButtonText1,
                WS_VISIBLE | WS_CHILD ,
                20, 50, 800, 100, hwnd, (HMENU) ID_BEEP, NULL, NULL);

            CreateWindowW(L"Button", ButtonText2,
                WS_VISIBLE | WS_CHILD ,
                20, 300, 800, 100, hwnd, (HMENU) ID_QUIT, NULL, NULL);
            break;
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BEEP) {
                MessageBeep(MB_OK);
                wprintf(L"%ls\n", ButtonText1);
            }
            if (LOWORD(wParam) == ID_QUIT) {
                MessageBeep(MB_ICONWARNING);
                wprintf(L"%ls\n", ButtonText2);
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int runWindows() {
    MSG  msg;
    WNDCLASSW wc = {0};
    auto hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"Buttons";
    wc.hInstance     = hInstance;
    wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
    wc.lpfnWndProc   = WndProc;
    wc.hCursor       = LoadCursor(0, IDC_ARROW);

    RegisterClassW(&wc);
    auto Win = CreateWindowW(wc.lpszClassName, L"Buttons",
                  WS_POPUP  | WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                  0, 0, 1600, 800, 0, 0, hInstance, 0);

    while (GetMessage(&msg, NULL, 0, 0)) {
    
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
