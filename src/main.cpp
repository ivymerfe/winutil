#include "Switcher.h"

#include "icon.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SUSPEND 1001
#define ID_TRAY_EXIT 1002

#define WM_WIMAXIMIZE (WM_USER + 2)
#define WM_WIMINIMIZE (WM_USER + 3)
#define WM_WICLOSE (WM_USER + 4)
#define WM_WILASTWINDOW (WM_USER + 5)
#define WM_WITOPMOST (WM_USER + 7)
#define WM_WIDEBUGLIST (WM_USER + 8)

HINSTANCE g_hInstance;
HWND g_window;
HMENU g_hTrayMenu;
NOTIFYICONDATA g_notifyIconData = {};
UINT g_uTaskbarCreatedMsg;
UINT g_uShellMsg;

Switcher *g_switcher;

static void InitTray(HWND hwnd);
static void CleanupTray();

bool g_suspended = false;

bool g_winDown = false;
bool g_capsDown = false;
bool g_ctrlDown = false;
bool g_shiftDown = false;

int g_lastKeyDown = 0;
int g_lastKeyPressed = 0;

static bool HandleKey(int vk, bool down, bool injected) {
  if (g_suspended) {
    return false;
  }
  if (injected) {
    if (vk == VK_LCONTROL && down && !g_ctrlDown) {
      return true;
    }
    return false;
  }
  bool repeat = down && g_lastKeyDown == vk;
  if (down) {
    g_lastKeyPressed = vk;
    g_lastKeyDown = vk;
  } else {
    g_lastKeyDown = 0;
  }
  switch (vk) {
  case VK_LWIN: {
    g_winDown = down;
    if (!down && g_lastKeyPressed == VK_LWIN) {
      PostMessage(g_window, WM_WILASTWINDOW, 0, 0);
    }
    if (!(GetAsyncKeyState(VK_LWIN) & 0x8000)) {
      return true;
    }
    break;
  }
  case VK_CAPITAL: {
    if (down && !g_capsDown) {
      g_switcher->SwitchLayout();
    }
    g_capsDown = down;
    if (!(GetAsyncKeyState(VK_CAPITAL) & 0x8000)) {
      return true;
    }
    break;
  }
  case VK_LCONTROL: {
    g_ctrlDown = down;
    break;
  }
  case VK_LSHIFT: {
    g_shiftDown = down;
    break;
  }
  case VK_TAB: {
    if (g_ctrlDown && !g_shiftDown) {
      if (down) {
        if (repeat) {
          INPUT input = {};
          input.type = INPUT_KEYBOARD;
          input.ki.wVk = VK_ESCAPE;
          SendInput(1, &input, sizeof(INPUT));
        } else {
          INPUT input[2] = {};
          input[0].type = input[1].type = INPUT_KEYBOARD;
          input[0].ki.wVk = VK_LCONTROL;
          input[0].ki.dwFlags = KEYEVENTF_KEYUP;
          input[1].ki.wVk = VK_ESCAPE;
          SendInput(2, input, sizeof(INPUT));
        }
      } else {
        INPUT input[2] = {};
        input[0].type = input[1].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_ESCAPE;
        input[0].ki.dwFlags = KEYEVENTF_KEYUP;
        input[1].ki.wVk = VK_LCONTROL;
        SendInput(2, input, sizeof(INPUT));
      }
      return true;
    }
    break;
  }
  }
  if (g_winDown) {
    if (down && !repeat) {
      switch (vk) {
      case 'W': {
        g_switcher->SwitchToGroup(Browser);
        break;
      }
      case 'E': {
        g_switcher->SwitchToGroup(Code);
        break;
      }
      case 'Q': {
        g_switcher->SwitchToGroup(Terminal);
        break;
      }
      case VK_TAB: {
        g_switcher->SwitchToGroup(Explorer);
        break;
      }
      case 'S': {
        g_switcher->SwitchToGroup(System);
        break;
      }
      case VK_OEM_3: {
        g_switcher->SwitchToGroup(Other);
        break;
      }
      case '1': {
        g_switcher->SwitchToIndex(0);
        break;
      }
      case '2': {
        g_switcher->SwitchToIndex(1);
        break;
      }
      case '3': {
        g_switcher->SwitchToIndex(2);
        break;
      }
      case '4': {
        g_switcher->SwitchToIndex(3);
        break;
      }
      case '5': {
        g_switcher->SwitchToIndex(4);
        break;
      }
      case '6': {
        g_switcher->SwitchToIndex(5);
        break;
      }
      case 'D': {
        if (g_shiftDown) {
          PostMessage(g_window, WM_WITOPMOST, 0, 0);
        } else {
          g_switcher->ActivateLauncher();
        }
        break;
      }
      case 'F': {
        PostMessage(g_window, WM_WIMAXIMIZE, 0, 0);
        break;
      }
      case 'C': {
        PostMessage(g_window, WM_WICLOSE, 0, 0);
        break;
      }
      case 'J': {
        PostMessage(g_window, WM_WIDEBUGLIST, 0, 0);
        break;
      }
      default: {
        if (vk >= 'A' && vk <= 'Z') {
          g_switcher->SendWinHotkey(vk);
        }
        break;
      }
      }
    }
    return true;
  }
  return false;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
  if (msg == g_uTaskbarCreatedMsg) {
    InitTray(hwnd);
    return 0;
  }
  if (msg == g_uShellMsg) {
    switch (wParam) {
    case HSHELL_RUDEAPPACTIVATED:
    case HSHELL_WINDOWACTIVATED:
    case HSHELL_WINDOWCREATED: {
      g_switcher->OnActivated((HWND)lParam);
      break;
    }
    case HSHELL_WINDOWDESTROYED: {
      g_switcher->OnDestroyed((HWND)lParam);
      break;
    }
    }
    return 0;
  }
  switch (msg) {
  case WM_WIMAXIMIZE: {
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
      if (IsZoomed(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
      } else {
        ShowWindow(hwnd, SW_MAXIMIZE);
      }
    }
    break;
  }
  case WM_WIMINIMIZE: {
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
      ShowWindow(hwnd, SW_MINIMIZE);
    }
    break;
  }
  case WM_WICLOSE: {
    HWND hwnd = GetForegroundWindow();
    if (hwnd) {
      PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    break;
  }
  case WM_WILASTWINDOW: {
    g_switcher->SwitchToLast();
    break;
  }
  case WM_WITOPMOST: {
    HWND hwnd = GetForegroundWindow();
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOPMOST) {
      SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    break;
  }
  case WM_WIDEBUGLIST: {
    g_switcher->ShowWindowList();
    break;
  }
  case WM_CREATE:
    g_uShellMsg = RegisterWindowMessageW(L"SHELLHOOK");
    RegisterShellHookWindow(hwnd);
    return 0;
  case WM_TRAYICON:
    if (lParam == WM_RBUTTONUP) {
      POINT pt;
      GetCursorPos(&pt);
      SetForegroundWindow(hwnd);
      TrackPopupMenu(g_hTrayMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y,
                     0, hwnd, NULL);
    }
    break;
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case ID_TRAY_SUSPEND: {
      g_suspended = !g_suspended;
      ModifyMenu(g_hTrayMenu, ID_TRAY_SUSPEND, MF_BYCOMMAND | MF_STRING,
                 ID_TRAY_SUSPEND, g_suspended ? L"Resume" : L"Suspend");
      break;
    }
    case ID_TRAY_EXIT:
      PostQuitMessage(0);
      break;
    }
    break;
  case WM_DESTROY:
    DeregisterShellHookWindow(hwnd);
    CleanupTray();
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode < 0) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }
  bool keyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
  bool keyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
  if (!keyDown && !keyUp) {
    return CallNextHookEx(NULL, nCode, wParam, lParam);
  }
  KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;
  DWORD vk = p->vkCode;
  bool injected = p->flags & LLKHF_INJECTED;
  if (HandleKey(vk, keyDown, injected)) {
    return 1;
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
  g_hInstance = hInstance;
  g_uTaskbarCreatedMsg = RegisterWindowMessage(L"TaskbarCreated");

  const wchar_t CLASS_NAME[] = L"wi";
  WNDCLASSEX windowClass = {};
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.lpfnWndProc = WndProc;
  windowClass.hInstance = hInstance;
  windowClass.lpszClassName = CLASS_NAME;
  windowClass.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON),
                                       IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR);
  windowClass.hIconSm =
      (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON), IMAGE_ICON, 64,
                       64, LR_DEFAULTCOLOR);
  RegisterClassEx(&windowClass);

  g_window = CreateWindowEx(0, CLASS_NAME, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                            NULL, hInstance, NULL);

  g_switcher = new Switcher();
  InitTray(g_window);

  HHOOK keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
  if (!keyboard_hook) {
    return 2;
  }
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  UnhookWindowsHookEx(keyboard_hook);
  CleanupTray();
  return 0;
}

static void InitTray(HWND hwnd) {
  g_hTrayMenu = CreatePopupMenu();
  AppendMenu(g_hTrayMenu, MF_STRING, ID_TRAY_SUSPEND, L"Suspend");
  AppendMenu(g_hTrayMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

  g_notifyIconData.cbSize = sizeof(g_notifyIconData);
  g_notifyIconData.hWnd = hwnd;
  g_notifyIconData.uID = 1;
  g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  g_notifyIconData.uCallbackMessage = WM_TRAYICON;
  g_notifyIconData.hIcon =
      (HICON)LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_APP_ICON), IMAGE_ICON,
                       32, 32, LR_DEFAULTCOLOR);
  wcscpy_s(g_notifyIconData.szTip, L"Wi");
  Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);
}

static void CleanupTray() {
  Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData);
  DestroyMenu(g_hTrayMenu);
}
