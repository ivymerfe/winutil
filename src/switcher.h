#pragma once

enum WindowGroup { Other, Browser, Explorer, Terminal, Code, System };

class Switcher {
  std::vector<HWND> m_windowsMru;
  std::vector<HWND> m_windowsOrdered;
  std::unordered_map<HWND, WindowGroup> m_windowGroups;

public:
  void OnCreated(HWND window);
  void OnActivated(HWND window);
  void OnDestroyed(HWND window);

  bool HasWindow(HWND window);

  void SwitchToLast();
  void SwitchToGroup(WindowGroup group);
  void SwitchToIndex(int index);

  void ShowWindowList();

  static void ActivateLauncher();
  static void SendWinHotkey(int key);
  static void SwitchLayout();

private:
  static bool CanSwitchToWindow(HWND window);
  static void ActivateWindow(HWND window);

  static std::wstring GetWindowProcName(HWND window);
};
