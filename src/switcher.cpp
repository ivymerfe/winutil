#include "Switcher.h"

#include <sstream>

const std::unordered_map<std::wstring, WindowGroup> APP_GROUPS = {
    {L"brave.exe", Browser},         {L"firefox.exe", Browser},
    {L"chrome.exe", Browser},        {L"msedge.exe", Browser},
    {L"zen.exe", Browser},           {L"WindowsTerminal.exe", Terminal},
    {L"explorer.exe", Explorer},     {L"Code.exe", Code},
    {L"idea64.exe", Code},           {L"pycharm64.exe", Code},
    {L"SystemInformer.exe", System}, {L"Taskmgr.exe", System}};

void Switcher::OnActivated(HWND window) {
  if (CanSwitchToWindow(window)) {
    std::erase(m_windowsMru, window);
    m_windowsMru.push_back(window);
    if (!HasWindow(window)) {
      m_windowsOrdered.push_back(window);
      std::wstring proc_name = GetWindowProcName(window);
      if (auto it = APP_GROUPS.find(proc_name); it != APP_GROUPS.end()) {
        m_windowGroups[window] = it->second;
      } else {
        m_windowGroups[window] = Other;
      }
    }
  }
}

void Switcher::OnDestroyed(HWND window) {
  std::erase(m_windowsMru, window);
  std::erase(m_windowsOrdered, window);
  m_windowGroups.erase(window);
}

bool Switcher::HasWindow(HWND window) {
  for (HWND win : m_windowsOrdered) {
    if (win == window) {
      return true;
    }
  }
  return false;
}

void Switcher::SwitchToLast() {
  if (m_windowsMru.empty()) {
    return;
  }
  HWND current = m_windowsMru.back();
  if (GetForegroundWindow() == current) {
    if (m_windowsMru.size() < 2) {
      return;
    }
    ActivateWindow(m_windowsMru[m_windowsMru.size() - 2]);
  } else {
    ActivateWindow(current);
  }
}

void Switcher::SwitchToGroup(WindowGroup group) {
  for (int i = m_windowsMru.size() - 1; i >= 0; i--) {
    HWND window = m_windowsMru[i];
    if (m_windowGroups[window] == group) {
      ActivateWindow(window);
      break;
    }
  }
}

void Switcher::SwitchToIndex(int index) {
  int current_index = -1;
  WindowGroup group = m_windowGroups[GetForegroundWindow()];
  for (HWND window : m_windowsOrdered) {
    if (m_windowGroups[window] == group) {
      current_index += 1;
      if (current_index == index) {
        ActivateWindow(window);
        break;
      }
    }
  }
}

void Switcher::ShowWindowList() {
  std::ostringstream oss;

  for (HWND hwnd : m_windowsMru) {
    char title[256] = {};
    GetWindowTextA(hwnd, title, sizeof(title));

    char className[256] = {};
    GetClassNameA(hwnd, className, sizeof(className));

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);

    oss << "0x" << std::hex << std::uppercase
        << reinterpret_cast<uintptr_t>(hwnd) << std::dec << " " << title
        << " PID=" << processId << " Class=" << className << "\n";
  }

  HWND fgHwnd = GetForegroundWindow();
  char fgTitle[256] = {};
  GetWindowTextA(fgHwnd, fgTitle, sizeof(fgTitle));
  char fgClassName[256] = {};
  GetClassNameA(fgHwnd, fgClassName, sizeof(fgClassName));
  DWORD fgProcessId = 0;
  GetWindowThreadProcessId(fgHwnd, &fgProcessId);

  oss << "\nForeground Window:\n"
      << "0x" << std::hex << std::uppercase
      << reinterpret_cast<uintptr_t>(fgHwnd) << std::dec << " " << fgTitle
      << " PID=" << fgProcessId << " Class=" << fgClassName << "\n";

  std::string result = oss.str();
  MessageBoxA(NULL, result.c_str(), "Windows", MB_OK);
}

void Switcher::ActivateLauncher() {
  INPUT input[2] = {};
  input[0].type = input[1].type = INPUT_KEYBOARD;
  input[0].ki.wVk = VK_LWIN;
  input[1].ki.wVk = VK_LWIN;
  input[1].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(2, input, sizeof(INPUT));
}

void Switcher::SendWinHotkey(int key) {
  INPUT input[4] = {};
  input[0].type = input[1].type = input[2].type = input[3].type =
      INPUT_KEYBOARD;
  input[0].ki.wVk = VK_LWIN;
  input[1].ki.wVk = key;
  input[2].ki.wVk = key;
  input[2].ki.dwFlags = KEYEVENTF_KEYUP;
  input[3].ki.wVk = VK_LWIN;
  input[3].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(4, input, sizeof(INPUT));
}

void Switcher::SwitchLayout() {
  HWND fg = GetForegroundWindow();
  if (!fg) {
    return;
  }
  DWORD threadId = GetWindowThreadProcessId(fg, NULL);
  HKL current = GetKeyboardLayout(threadId);
  LANGID curLang = LOWORD((DWORD_PTR)current);
  WORD curPrimary = PRIMARYLANGID(curLang);
  if (curPrimary == LANG_RUSSIAN) {
    PostMessage(GetForegroundWindow(), WM_INPUTLANGCHANGEREQUEST, 1,
                0x04090409);
  } else {
    PostMessage(GetForegroundWindow(), WM_INPUTLANGCHANGEREQUEST, 1,
                0x04190419);
  }
}

bool Switcher::CanSwitchToWindow(HWND window) {
  if (!IsWindow(window)) {
    return false;
  }
  if (!IsWindowVisible(window)) {
    return false;
  }
  if (window == GetShellWindow()) {
    return false;
  }
  if (GetAncestor(window, GA_ROOT) != window) {
    return false;
  }
  LONG_PTR style = GetWindowLongPtr(window, GWL_STYLE);
  LONG_PTR exStyle = GetWindowLongPtr(window, GWL_EXSTYLE);
  if ((exStyle & WS_EX_TOOLWINDOW) && !(exStyle & WS_EX_APPWINDOW)) {
    return false;
  }
  HWND owner = GetWindow(window, GW_OWNER);
  if (owner && IsWindow(owner)) {
    return false;
  }
  BOOL cloaked = FALSE;
  HRESULT hr =
      DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
  if (SUCCEEDED(hr) && cloaked) {
    return false;
  }
  wchar_t title[256];
  GetWindowTextW(window, title, 256);
  if (wcscmp(title, L"Start") == 0) {
    return false;
  }
  return true;
}

void Switcher::ActivateWindow(HWND window) {
  HWND foreground_window = GetForegroundWindow();
  if (window == foreground_window) {
    return;
  }
  if (IsIconic(window)) {
    ShowWindow(window, SW_RESTORE);
  }
  INPUT input{};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = 0;
  input.ki.wScan = 0;
  SendInput(1, &input, sizeof(INPUT));
  SetForegroundWindow(window);
}

std::wstring Switcher::GetWindowProcName(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd)) {
    return L"";
  }
  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);

  if (processId == 0) {
    return L"";
  }
  HANDLE hProcess =
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);

  if (hProcess == NULL) {
    return L"";
  }
  wchar_t imagePath[MAX_PATH] = {0};
  DWORD pathSize = MAX_PATH;

  std::wstring processName = L"";
  if (QueryFullProcessImageNameW(hProcess, 0, imagePath, &pathSize)) {
    std::wstring fullPath(imagePath);
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
      processName = fullPath.substr(lastSlash + 1);
    } else {
      processName = fullPath;
    }
  }
  CloseHandle(hProcess);
  return processName;
}
