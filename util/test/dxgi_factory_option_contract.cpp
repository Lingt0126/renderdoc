#include <windows.h>
#include <dxgi1_3.h>
#include <cstdio>
#include <cstring>

static HMODULE Owner(const void *address)
{
  HMODULE result = NULL;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    (LPCWSTR)address, &result);
  return result;
}

int wmain(int argc, wchar_t **argv)
{
  if(argc != 3) return 2;
  const bool legacy = wcscmp(argv[2], L"modern") != 0;
  SetEnvironmentVariableA("RENDERDOC_DXGI_HOOK_FACTORY0", legacy ? NULL : "0");
  HMODULE capture = LoadLibraryW(argv[1]);
  HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
  if(!capture || !dxgi) return 3;
  int failures = 0;
  const char *names[] = {"CreateDXGIFactory", "CreateDXGIFactory1", "CreateDXGIFactory2"};
  for(unsigned i = 0; i < 3; i++)
  {
    FARPROC entry = GetProcAddress(dxgi, names[i]);
    bool ok = entry && (Owner(entry) == capture) == (legacy || i != 0);
    IDXGIFactory *factory = NULL;
    const IID &riid = i == 0 ? __uuidof(IDXGIFactory) : __uuidof(IDXGIFactory1);
    HRESULT hr = E_FAIL;
    if(entry)
    {
      if(i == 2)
        hr = ((HRESULT(WINAPI *)(UINT, REFIID, void **))entry)(0, riid, (void **)&factory);
      else
        hr = ((HRESULT(WINAPI *)(REFIID, void **))entry)(riid, (void **)&factory);
    }
    ok &= SUCCEEDED(hr) && factory;
    bool wrapped = factory && Owner((*(void ***)factory)[0]) == capture;
    if((legacy || i != 0) && !wrapped) ok = false;
    if(factory) factory->Release();
    printf("legacy=%d %s entry_wrapped=%d object_wrapped=%d hr=%08lx %s\n",
           legacy, names[i], entry && Owner(entry) == capture, wrapped, hr, ok ? "PASS" : "FAIL");
    if(!ok) failures++;
  }
  printf("DXGI_CONTRACT_COMPLETE failures=%d\n", failures);
  return failures ? 1 : 0;
}
