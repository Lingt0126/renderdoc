#include <windows.h>

// Model the resolver import used by an overlay's forwarding hook. This must
// remain native for overlays, while ordinary application imports are hooked.
extern "C" __declspec(dllexport) FARPROC ResolveForClient(HMODULE module, LPCSTR name)
{
  return GetProcAddress(module, name);
}
