#include <iostream>
#include <windows.h>
#include <map>
#include <tuple>
#include <vector>
#include <thread>
#include <mutex>
#include <utility>

#define THREADS             6
#define COLOR_TOLERANCE     5

std::mutex mtx;
std::tuple<BYTE, BYTE, BYTE> desiredRGB = { 12, 12, 12 };
std::map<int, std::tuple<unsigned short, unsigned short>> detectedPositions;
HWND g_hwnd;
LRESULT CALLBACK WindowProcess(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void CreateConsole() {
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);

    std::ios::sync_with_stdio();
    std::cout.clear(); std::clog.clear(); std::cerr.clear();
    std::cin.clear();
}

int scanRegion(HDC hdc, int minX, int maxX, int screenHeight) {
    std::cout << "Starting pixelscan thread at " << minX << std::endl;
    while (true) {
        for (int y = 0; y < screenHeight; y++) {
            for (int x = minX; x < maxX; x++) {
                COLORREF pixelStruct = GetPixel(hdc, x, y);

                if (abs(GetRValue(pixelStruct) - std::get<0>(desiredRGB)) <= COLOR_TOLERANCE &&
                    abs(GetGValue(pixelStruct) - std::get<1>(desiredRGB)) <= COLOR_TOLERANCE &&
                    abs(GetBValue(pixelStruct) - std::get<2>(desiredRGB)) <= COLOR_TOLERANCE) {

                    std::lock_guard<std::mutex> lock(mtx);
                    int index = detectedPositions.size();
                    detectedPositions[index] = std::make_tuple(x, y);

                    std::cout << "Match at: " << x << "," << y << std::endl;
                    InvalidateRect(g_hwnd, NULL, TRUE); // trigger WM_PAINT
                }
            }
        }
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProcess;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OverlayWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, L"OverlayWindowClass", L"overlay", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 1;
    g_hwnd = hwnd;

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(hwnd, nCmdShow);

    CreateConsole();

    RECT desktop;
    HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktop);
    int screenWidth = desktop.right;
    int screenHeight = desktop.bottom;
    HDC screen = GetDC(HWND_DESKTOP);

    int sectionWidth = screenWidth / THREADS;

    for (int i = 0; i < THREADS; i++) {
        std::thread t_obj(scanRegion, screen, i * sectionWidth, (i == THREADS - 1) ? screenWidth : (i + 1) * sectionWidth, screenHeight);
        t_obj.detach();
    }

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

LRESULT CALLBACK WindowProcess(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        MessageBox(hwnd, L"Pixel scanning threads are starting!", L"Pixel scanning", MB_OK);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH green = CreateSolidBrush(RGB(0, 255, 0));
        SelectObject(hdc, green);

        std::lock_guard<std::mutex> lock(mtx);
        for (const auto& entry : detectedPositions) {
            int x = std::get<0>(entry.second);
            int y = std::get<1>(entry.second);
            Ellipse(hdc, x - 5, y - 5, x + 5, y + 5);
        }

        DeleteObject(green);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
}
