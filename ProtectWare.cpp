#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <fstream>
#include <string>
#include <iostream>
#include <ctime>
#include <sstream>
#include <filesystem>
#include <direct.h>

using namespace std;

// Global variables
static TCHAR szWindowClass[] = _T("DesktopApp");
static TCHAR szTitle[] = _T("ProtectWare V.1.");
static string curDir = _getcwd(NULL, 0);
HINSTANCE hInst;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Function to handle the custom drawing of buttons (making them rounded)
void DrawRoundedButton(HDC hdc, RECT* rc, const TCHAR* text, COLORREF fillColor, COLORREF borderColor);

// Function prototype for button drawing
void SetButtonRounded(HWND hWndButton);

string runPythonScriptWithOutput(const string& command) {
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        return "Error: Failed to execute script.";
    }

    char buffer[256];
    string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    int returnCode = _pclose(pipe);
    if (returnCode != 0 && returnCode!=1) {
        result += "\nError: Script returned a non-zero exit code.";
    }
    return result;
}

string openFileDialog(HWND hwnd) {
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        std::wstring ws(fileName);
        return std::string(ws.begin(), ws.end());
    }
    return "";
}

string openFolderDialog(HWND hwnd) {
    BROWSEINFO bi = { 0 };
    bi.lpszTitle = _T("Select Folder");
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl != NULL) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path)) {
            std::wstring ws(path);
            return std::string(ws.begin(), ws.end());
        }
    }
    return "";
}

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(wcex.hInstance, IDI_INFORMATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(0, 102, 204));
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_INFORMATION);

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, _T("Call to RegisterClassEx failed!"), _T("Error"), NULL);
        return 1;
    }

    hInst = hInstance;

    HWND hWnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 400,
        NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        MessageBox(NULL, _T("Call to CreateWindow failed!"), _T("Error"), NULL);
        return 1;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HFONT hFont;
    static HFONT hLargeFont;
    static bool splashScreen = true;
    static HWND hStatic1, hStatic2, hScanButton, hScanAllButton, hExitButton, hUpdateButton, hOutputDisplay;
    static UINT_PTR splashTimer = 0;
    string lastDate = "python.exe dateTeller.py";

    switch (message) {
    case WM_CREATE:
        // Create a font for the default size
        hFont = CreateFont(
            20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        // Create a larger font for hStatic1 (ProtectWare title)
        hLargeFont = CreateFont(
            36, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        // Splash screen controls
        hStatic1 = CreateWindow(_T("STATIC"), _T("ProtectWare"), WS_VISIBLE | WS_CHILD | SS_CENTER, 150, 100, 300, 50, hWnd, NULL, NULL, NULL);
        hStatic2 = CreateWindow(_T("STATIC"), _T("Version 1.0"), WS_VISIBLE | WS_CHILD | SS_CENTER, 150, 160, 300, 50, hWnd, NULL, NULL, NULL);

        // Set the fonts
        SendMessage(hStatic1, WM_SETFONT, (WPARAM)hLargeFont, TRUE);  // Set larger font for hStatic1
        SendMessage(hStatic2, WM_SETFONT, (WPARAM)hFont, TRUE);       // Set default font for hStatic2

        // Set timer to transition from splash screen
        splashTimer = SetTimer(hWnd, 1, 3000, NULL);

        // Main UI controls (initially hidden)
        hScanButton = CreateWindow(_T("BUTTON"), _T("SCAN FILE"), WS_CHILD | BS_OWNERDRAW, 50, 50, 120, 40, hWnd, (HMENU)5, NULL, NULL);
        hScanAllButton = CreateWindow(_T("BUTTON"), _T("SCAN FOLDER"), WS_CHILD | BS_OWNERDRAW, 200, 50, 120, 40, hWnd, (HMENU)6, NULL, NULL);

        // Adjust the size of the Update Version button
        hUpdateButton = CreateWindow(_T("BUTTON"), _T("UPDATE VER."), WS_CHILD | BS_OWNERDRAW, 50, 300, 300, 10, hWnd, (HMENU)8, NULL, NULL);  // Increased size

        hExitButton = CreateWindow(_T("BUTTON"), _T("EXIT"), WS_CHILD | BS_OWNERDRAW, 400, 300, 200, 40, hWnd, (HMENU)7, NULL, NULL);

        hOutputDisplay = CreateWindow(_T("STATIC"), _T("Scanned files will show here.."), WS_CHILD | SS_LEFT, 50, 140, 500, 150, hWnd, NULL, NULL, NULL);

        // Set fonts for buttons and output display
        SendMessage(hScanButton, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hScanAllButton, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hExitButton, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hUpdateButton, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hOutputDisplay, WM_SETFONT, (WPARAM)hFont, TRUE);
        break;


    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        // Adjust button positions after swapping
        MoveWindow(hExitButton, width - 210, height - 50, 200, 40, TRUE);  // Exit button moved
        MoveWindow(hUpdateButton, 10, height - 50, 120, 40, TRUE);          // Update button moved
        break;
    }

    case WM_TIMER:
        if (splashTimer && splashScreen) {
            splashScreen = false;
            KillTimer(hWnd, splashTimer);
            splashTimer = 0;
            ShowWindow(hStatic1, SW_HIDE);
            ShowWindow(hStatic2, SW_HIDE);
            ShowWindow(hScanButton, SW_SHOW);
            ShowWindow(hScanAllButton, SW_SHOW);
            ShowWindow(hExitButton, SW_SHOW);
            ShowWindow(hUpdateButton, SW_SHOW);
            ShowWindow(hOutputDisplay, SW_SHOW);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 5: {
            string command = "python.exe dateTeller.py";
            SetWindowTextA(hOutputDisplay, string(command).c_str());
            string filePath = openFileDialog(hWnd);
            if (!filePath.empty()) {
                string PYTHON_SCRIPT = string("python.exe ").append(curDir).append("\\virusChecker.py");
                string command = PYTHON_SCRIPT + " " + filePath;
                SetWindowTextA(hOutputDisplay, string(command).c_str());
                string output = runPythonScriptWithOutput(command);
                SetWindowTextA(hOutputDisplay, output.c_str());
            }
            break;
        }
        case 6: {
            string folderPath = openFolderDialog(hWnd);
            if (!folderPath.empty()) {
                string PYTHON_SCRIPT = string("python.exe ").append(curDir).append("\\virusChecker.py");
                string command = PYTHON_SCRIPT + " " + folderPath;
                string output = runPythonScriptWithOutput(command);
                SetWindowTextA(hOutputDisplay, output.c_str());
            }
            break;
        }
        case 7:
            PostQuitMessage(0);
            break;
        case 8: {  // "UPDATE VERSION" button clicked
            MessageBox(hWnd, _T("Latest version installed"), _T("Update Status"), MB_OK | MB_ICONINFORMATION);
            break;
        }
        }
        break;

    case WM_DESTROY:
        if (splashTimer) KillTimer(hWnd, splashTimer);
        DeleteObject(hFont);
        PostQuitMessage(0);
        break;

    case WM_DRAWITEM:
        if (wParam == 5 || wParam == 6 || wParam == 7 || wParam == 8) {
            DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
            if (pdis->CtlType == ODT_BUTTON) {
                // Get the button text dynamically
                TCHAR buttonText[256];
                GetWindowText(pdis->hwndItem, buttonText, sizeof(buttonText) / sizeof(buttonText[0]));

                // Custom draw button with rounded edges and correct text
                if (pdis->hwndItem == hScanButton || pdis->hwndItem == hScanAllButton || pdis->hwndItem == hExitButton || pdis->hwndItem == hUpdateButton) {
                    DrawRoundedButton(pdis->hDC, &pdis->rcItem, buttonText, RGB(0, 122, 204), RGB(0, 102, 204));
                }
            }
        }
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Custom button drawing function (rounded corners)
void DrawRoundedButton(HDC hdc, RECT* rc, const TCHAR* text, COLORREF fillColor, COLORREF borderColor) {
    HPEN hPen = CreatePen(PS_SOLID, 2, borderColor);
    HBRUSH hBrush = CreateSolidBrush(fillColor);
    SelectObject(hdc, hPen);
    SelectObject(hdc, hBrush);

    // Draw rounded rectangle
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, 20, 20);

    // Draw text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawText(hdc, text, -1, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DeleteObject(hPen);
    DeleteObject(hBrush);
}
