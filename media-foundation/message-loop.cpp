#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <commctrl.h>

#include "media-foundation.cpp"

namespace {

template <typename T>
class Scope {
private:
  T* ptr_;

public:
  explicit Scope(T* ptr = nullptr) noexcept
    : ptr_{ptr} {}

  Scope(const Scope&) noexcept = delete;

  Scope(Scope&&) noexcept = delete;

  ~Scope() {
    delete ptr_;
  }

  operator T*() const noexcept {
    return ptr_;
  }

  T* operator=(T* ptr) noexcept {
    delete ptr_;
    ptr_ = ptr;
    return ptr_;
  }

  T* operator->() const noexcept {
    return ptr_;
  }

  T& operator*() const noexcept {
    return *ptr_;
  }
};

const TCHAR* kWindowTitle = _T("Demo");
const TCHAR* kApplicationName = _T("Demo 1.0");
const TCHAR* kApplicationClass = _T("Demo-4ca7770ba7544336ad9eec208baf3001");

Scope<MediaFoundation> mf{};
Scope<MediaFoundation::Camera> camera{};
Scope<MediaFoundation::Camera::Names<1024>> cameraNames{};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE: {
      if (!(mf = MediaFoundation::create())) return -1;
      if (!(cameraNames = MediaFoundation::Camera::Names<1024>::create())) return -1;
      if (!(mf->initNames(*cameraNames, _T("Unknown Camera")))) return -1;

      HWND hComboBox = CreateWindow(WC_COMBOBOX, _T(""), WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP, 10, 11, 200, 200, hWnd, nullptr, nullptr, nullptr);
      if (!hComboBox) return -1;
      for (MediaFoundation::Camera::Names<1024>::Iter iter = cameraNames->begin(); iter != cameraNames->end(); ++iter) {
        SendMessage(hComboBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(*iter));
      }
      break;
    }
    case WM_CLOSE:
      if (MessageBox(hWnd, _T("Do you really want to close?"), kWindowTitle, MB_YESNO | MB_ICONQUESTION) == IDYES) {
        DestroyWindow(hWnd);
      }
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    case WM_COMMAND: {
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        HWND hComboBox = reinterpret_cast<HWND>(lParam);
        int index = static_cast<int>(SendMessage(hComboBox, CB_GETCURSEL, 0, 0));
        if (index != CB_ERR) {
          camera = nullptr;
          camera = mf->createCamera(index);
          InvalidateRect(hWnd, nullptr, TRUE);
        }
      }
      break;
    }
    case WM_PAINT: {
      if (camera) {
        if (HBITMAP hBitmap = camera->grab()) {
          PAINTSTRUCT ps;
          if (HDC hdc = BeginPaint(hWnd, &ps)) {
            if (HDC hMemDC = CreateCompatibleDC(hdc)) {
              SelectObject(hMemDC, hBitmap);
              int x = (ps.rcPaint.right - ps.rcPaint.left - camera->getWidth()) / 2;
              int y = (ps.rcPaint.bottom - ps.rcPaint.top + 36 - camera->getHeight()) / 2;
              BitBlt(hdc, x, y, camera->getWidth(), camera->getHeight(), hMemDC, 0, 0, SRCCOPY);
              DeleteDC(hMemDC);
            }
            EndPaint(hWnd, &ps);
          }
          DeleteObject(hBitmap);
        }
      }
      break;
    }
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
}

} // namespace

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPTSTR lpCmdLine, int nCmdShow) {
  WNDCLASSEX wc{};
  if (!GetClassInfoEx(nullptr, MAKEINTRESOURCE(32770), &wc)) return 1;

  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
  wc.lpszClassName = kApplicationClass;
  if (!RegisterClassEx(&wc)) return 1;

  HWND hWnd = CreateWindow(kApplicationClass, kApplicationName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInst, nullptr);
  if (!hWnd) return 1;

  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  MSG msg{};
  for (;;) {
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        break;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      InvalidateRect(hWnd, nullptr, FALSE);
    }
  }
  return static_cast<int>(msg.wParam);
}
