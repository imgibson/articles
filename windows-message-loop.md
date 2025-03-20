# Windows Message Dispatching

## Introduction

Irregardless of whether you are writing a Windows application using WinRT, UWP, WinForms, MFC (with the exception of WPF) or a native WIN32 application, all of them rely on the Windows API for message dispatch in some form. With the oncoming of higher level frameworks and libraries that abstract the Windows API, it is easy to forget that at the core of every Windows application is a message loop and it is essential to understand how it works. This may lay as a foundation to other articles that build on top of this and to some degree also the evolution of the GUI applications that we use today, from cross-platform frameworks, GTK, QT, HTML5 and similar but let us start from the beginning.

## Prerequisites

Before we begin, it is assumed that you have a basic understanding of C++ and have a recent compiler installed that can compiler Windows binaries, most conveniently Microsoft Visual Studio but also MINGW will suffice.

## The Windows API

The Windows API, commonly refered to as the 'Win32 API' is not limited to 32-bit platforms even though it initially may appear so, it was coined when migrating from the 16-bit Windows API to the 32-bit Windows API. The Windows API is a set of functions and data structures that are used to interact with the Windows operating system. The Windows API provides methods to create windows, dialogs, menus, and other graphical elements as part of its GUI subset. In addition to this it also provides methods to interact with the file system, the registry, and other parts of the operating system. The Windows API shall not be confused with STL (the C++ Standard Library) or the .NET Framework, these are higher level libraries and wrappers that abstract the Windows API, uses it and provides a subset of its features.

## Prepare the Application and Create an Entry Point

As a start, let us define some variables that will be used throughout our application. The application name is the name that will be displayed in the title bar of the window. In addition to this we may need version inforation and a unique identifier for the window class, typically a GUID/UUID. This can be used to prevent class name conflicts with other applications or to refocus the window and detect when it is already running. In order to gain access to the Window API we need to include the windows.h header file. Defining the WIN32_LEAN_AND_MEAN preprocessor directive will reduce the size of the Windows.h header file and speed up compilation.

```c++
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

const TCHAR* kWindowTitle = _T("Application");
const TCHAR* kApplicationName = _T("Application 1.0");
const TCHAR* kApplicationClass = _T("Application-4ca7770ba7544336ad9eec208baf3001");

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPTSTR lpCmdLine, int nCmdShow) {
  // code goes here...
}
```

## Creating a Window

To create a window, Windows uses template classes to derive its properties, looks and behavior. It is possible to use existing templates or create a custom template. To simplify, instead of creating a completely new template class for our dialog we can retrieve the standard dialog template resource which is using the the identifier 32770, it is used by the MessageBox function and gives us a familiar look and feel that we can modify to our needs.

```c++
  WNDCLASSEX wc{};
  if (!GetClassInfoEx(nullptr, MAKEINTRESOURCE(32770), &wc)) {
    return 1;
  }

  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1);
  wc.lpszClassName = kApplicationClass;
  if (!RegisterClassEx(&wc)) {
    return 1;
  }

  HWND hWnd = CreateWindow(kApplicationClass, kApplicationName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInst, nullptr);
  if (!hWnd) {
    return 1;
  }
```

## The Message Loop

In our example above we have specified a custom window procedure called `WndProc` and it needs to be defined. If we have no custom behavior we could equally well have set the default window procedure `DefWindowProc` and omit this function altogether. However, in order to make this example more interesting we will customize its behavior and pass the generic messages to the default window procedure manually while intercepting the messages that we would like to customize. Here is a summary of the more common window messages that are neccessary to understand:

- `WM_CREATE`
    The window is being created (not yet visible) and the application may be initialized.
- `WM_COMMAND`
    A command message was sent to the window, this could be a button press, menu item selection or similar.
- `WM_CLOSE`
    The window is being closed and the application may be terminated, prompt the user to save or cancel the operation.
- `WM_DESTROY`
    The window is being destroyed and the application may be terminated.

Let's make sure that our window procedure can handle requests to close the window by asking the user for confirmation and also make sure that the message loop exists as a response to our window being destroyed by posting a quit message.

```c++
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CLOSE:
      if (MessageBox(hWnd, _T("Do you really want to close the application?"), kWindowTitle, MB_YESNO | MB_ICONQUESTION) == IDYES) {
        DestroyWindow(hWnd);
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
```

## Running the Application

The final step is to show the window and run the message loop. The `ShowWindow` function displays the window on the screen, `UpdateWindow` sends a `WM_PAINT` message to the window procedure. The `GetMessage` function retrieves a message from the message queue and the `DispatchMessage` function sends the message to the window procedure, that means that `WndProc` that we defined above will be called as part of this function call and one or more window messages will be processed.

```c++
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
```

## Further Reading

### Unicode and ANSI

You may have noticed that we have used the `_T` macro in our code, this is to support both Unicode and ANSI character sets. The Windows API is built on Unicode (Fixed-Width UCS-2 / Variable-Width UTF-16) and it is recommended to use that API until UTF-8 becomes the standard for Windows development. The `_T` will prefix the string with an `L` if the `UNICODE` preprocessor directive is defined, otherwise it will leave the string as is. The `TCHAR` type is defined as `char` if `UNICODE` is not defined and `wchar_t` if it is defined. Note that including the `tchar.h` header file is necessary to use these macros. In recent years support for UTF-8 has been added to the Windows API and it is possible to pass UTF-8 encoded strings to the Windows API functions but it is required that the application informs this upfront by indicating usage of the `CP_UTF8` code page.

### The Windows API and RAII

The Windows API does not use RAII (Resource Acquisition Is Initialization) and it is up to the developer to manage resources manually. This is especially important when dealing with GDI objects, file handles, memory and similar. Note that windows resources such as HWNDs are destroyed by calling `DestroyWindow` within the window procedure itself and are thus implicitly released, it is thus uncommon to wrap a window resource in an RAII class. However, it is common to wrap other resources in RAII classes to ensure that they are released when they go out of scope. With STL, the `std::unique_ptr` class can be used but below are some custom simple wrapper examples just to demonstrate the concept.

```c++
class DeviceContext {
  HDC hdc_;
public:
  explicit DeviceContext(HDC hdc) noexcept : hdc_(hdc) {}
  ~DeviceContext() { ReleaseDC(nullptr, hdc_);
};

class Icon {
  HICON hicon_;
public:
  explicit Icon(HICON hicon) noexcept : hicon_(hicon) {}
  ~Icon() { DestroyIcon(hicon_);
};

class File {
  HFILE hfile_;
public:
  explicit File(HFILE hfile) noexcept : hfile_(hfile) {}
  ~File() { CloseHandle(hfile_);
};
```
