#include <cstdlib>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

namespace {

const TCHAR* kWindowTitle = _T("Demo");
const TCHAR* kApplicationName = _T("Demo 1.0");
const TCHAR* kApplicationClass = _T("Demo-4ca7770ba7544336ad9eec208baf3001");

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CLOSE:
      if (MessageBox(hWnd, _T("Do you really want to close the application?"), kWindowTitle, MB_YESNO | MB_ICONQUESTION) == IDYES) {
        DestroyWindow(hWnd);
      }
      break;
    case WM_DESTROY:
      PostQuitMessage(EXIT_SUCCESS);
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

} // namespace

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPTSTR lpCmdLine, int nCmdShow) {
  WNDCLASSEX wc{};
  if (!GetClassInfoEx(nullptr, MAKEINTRESOURCE(32770), &wc)) return EXIT_FAILURE;

  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
  wc.lpszClassName = kApplicationClass;
  if (!RegisterClassEx(&wc)) return EXIT_FAILURE;

  HWND hWnd = CreateWindow(kApplicationClass, kApplicationName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInst, nullptr);
  if (!hWnd) return EXIT_FAILURE;

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  MSG msg{};
  while (BOOL status = GetMessage(&msg, nullptr, 0, 0) != 0) {
    if (status == -1) {
      return EXIT_FAILURE;
    }

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return static_cast<int>(msg.wParam);
}
