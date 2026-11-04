#include <windows.h>
#include <cstdio>

int wmain(int argc, wchar_t **argv)
{
  if(argc != 4)
    return 2;

  HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
  FARPROC nativeResolver = GetProcAddress(kernel, "GetProcAddress");
  HMODULE capture = LoadLibraryW(argv[1]);
  if(!capture)
    return 3;

  HMODULE overlay = LoadLibraryW(argv[2]);
  HMODULE ordinary = LoadLibraryW(argv[3]);
  typedef FARPROC (*Resolve)(HMODULE, LPCSTR);
  Resolve overlayResolve = (Resolve)GetProcAddress(overlay, "ResolveForClient");
  Resolve ordinaryResolve = (Resolve)GetProcAddress(ordinary, "ResolveForClient");
  if(!overlayResolve || !ordinaryResolve)
    return 4;

  FARPROC hookedResolver = GetProcAddress(kernel, "GetProcAddress");
  bool applicationHooked = hookedResolver && hookedResolver != nativeResolver;
  bool overlayNative = overlayResolve(kernel, "GetProcAddress") == nativeResolver;
  bool ordinaryHooked = ordinaryResolve(kernel, "GetProcAddress") == hookedResolver;
  bool missingExport = overlayResolve(kernel, "RenderDocMissingExportContract") == NULL &&
                       ordinaryResolve(kernel, "RenderDocMissingExportContract") == NULL;
  bool pass = applicationHooked && overlayNative && ordinaryHooked && missingExport;
  printf("application_hooked=%d overlay_native=%d ordinary_hooked=%d missing_export=%d %s\n",
         applicationHooked, overlayNative, ordinaryHooked, missingExport, pass ? "PASS" : "FAIL");
  return pass ? 0 : 1;
}
