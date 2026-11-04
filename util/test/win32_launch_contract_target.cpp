// Controlled target for launch-result regression tests. No game or third-party process is used.
#include <windows.h>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR args, int)
{
  if(wcscmp(args, L"bootstrap") != 0)
  {
    Sleep(12000);
    return 0;
  }

  wchar_t path[MAX_PATH] = {};
  GetModuleFileNameW(NULL, path, MAX_PATH);
  std::wstring command = L"\"" + std::wstring(path) + L"\" child";
  STARTUPINFOEXW si = {};
  si.StartupInfo.cb = sizeof(si);
  SIZE_T bytes = 0;
  InitializeProcThreadAttributeList(NULL, 1, 0, &bytes);
  si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytes);
  if(!si.lpAttributeList || !InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytes))
    return 10;
  // A test-owned child intentionally rejects non-Microsoft DLLs. This exercises
  // truthful failure reporting; the injector must not weaken or bypass this policy.
  DWORD64 policy = PROCESS_CREATION_MITIGATION_POLICY_BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_ON;
  BOOL configured = UpdateProcThreadAttribute(si.lpAttributeList, 0,
      PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &policy, sizeof(policy), NULL, NULL);
  PROCESS_INFORMATION pi = {};
  BOOL created = configured && CreateProcessW(path, &command[0], NULL, NULL, FALSE,
      EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW, NULL, NULL, &si.StartupInfo, &pi);
  DeleteProcThreadAttributeList(si.lpAttributeList);
  HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
  if(!created)
    return 11;
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return 0;
}
