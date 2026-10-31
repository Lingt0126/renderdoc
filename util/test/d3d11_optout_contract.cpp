// Standalone Windows regression: load RenderDoc in this test process only.
#include <windows.h>
#include <d3d11.h>
#include <cstdio>

template <typename T> void Release(T *p) { if(p) p->Release(); }

static HMODULE Owner(IUnknown *object)
{
  if(!object) return NULL;
  HMODULE module = NULL;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    (LPCWSTR)(*(void ***)object)[0], &module);
  return module;
}

int wmain(int argc, wchar_t **argv)
{
  setvbuf(stdout, NULL, _IONBF, 0);
  const bool captureOptOut = argc == 3 && !wcscmp(argv[2], L"--capture-optout");
  // Isolate the test from the caller's launch profile.
  SetEnvironmentVariableA("RENDERDOC_D3D11_CAPTURE_OPTOUT", captureOptOut ? "1" : NULL);
  HMODULE capture = NULL;
  if(argc >= 2)
  {
    capture = LoadLibraryW(argv[1]);
    if(!capture) { printf("LOAD_FAILED %lu\n", GetLastError()); return 2; }
  }
  HMODULE d3d11 = LoadLibraryW(L"d3d11.dll");
  auto create = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(d3d11, "D3D11CreateDevice");
  auto createSwap = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(
      d3d11, "D3D11CreateDeviceAndSwapChain");
  if(!create || !createSwap) return 3;

  int failures = 0;
  // All combinations of optional device/context outputs, for native opt-out and normal capture.
  for(int phase = 0; phase < 3; phase++)
  {
    // Repeat ordinary creation after opt-out to detect a leaked recursion guard.
    const int optout = phase == 1;
    const UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED |
        (optout ? D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY : 0);
    for(int mask = 1; mask < 4; mask++)
    {
      ID3D11Device *device = NULL;
      ID3D11DeviceContext *context = NULL;
      const HRESULT hr = create(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, NULL, 0,
                                D3D11_SDK_VERSION, (mask & 1) ? &device : NULL,
                                NULL, (mask & 2) ? &context : NULL);
      bool ok = SUCCEEDED(hr) && (!(mask & 1) || device) && (!(mask & 2) || context);
      ID3D11Device *contextDevice = NULL;
      if(context) { context->Flush(); context->GetDevice(&contextDevice); }
      if(context && !contextDevice) ok = false;
      if(device && contextDevice && device != contextDevice) ok = false;
      ID3D11Device *checkDevice = device ? device : contextDevice;
      if(checkDevice && checkDevice->GetCreationFlags() != flags) ok = false;
      if(capture && checkDevice &&
         (Owner(checkDevice) == capture) != (!optout || captureOptOut)) ok = false;
      printf("optout=%d outputs=%d hr=%08lx device=%d context=%d wrapped=%d %s\n",
             optout, mask, hr, device != NULL, context != NULL,
             checkDevice && Owner(checkDevice) == capture, ok ? "PASS" : "FAIL");
      if(!ok) failures++;
      Release(contextDevice); Release(context); Release(device);
    }
  }

  // An invalid SDK version must not become success or leave non-null outputs.
  ID3D11Device *device = NULL;
  ID3D11DeviceContext *context = NULL;
  const UINT optoutFlags = D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY;
  HRESULT hr = create(NULL, D3D_DRIVER_TYPE_WARP, NULL, optoutFlags, NULL, 0,
                      0, &device, NULL, &context);
  bool ok = FAILED(hr) && !device && !context;
  printf("invalid_sdk hr=%08lx %s\n", hr, ok ? "PASS" : "FAIL");
  if(!ok) failures++;
  Release(context); Release(device);

  // Exercise the combined entry point without presenting or making a visible window.
  HWND wnd = CreateWindowExW(0, L"STATIC", L"D3D11 contract", WS_OVERLAPPEDWINDOW,
                             0, 0, 64, 64, NULL, NULL, GetModuleHandleW(NULL), NULL);
  DXGI_SWAP_CHAIN_DESC desc = {};
  desc.BufferDesc.Width = desc.BufferDesc.Height = 64;
  desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 1;
  desc.OutputWindow = wnd;
  desc.Windowed = TRUE;
  IDXGISwapChain *swap = NULL;
  device = NULL; context = NULL;
  hr = createSwap(NULL, D3D_DRIVER_TYPE_WARP, NULL, optoutFlags, NULL, 0,
                  D3D11_SDK_VERSION, &desc, &swap, &device, NULL, &context);
  ok = SUCCEEDED(hr) && swap && device && context;
  if(capture && ((Owner(device) == capture) != captureOptOut ||
                 (Owner(context) == capture) != captureOptOut ||
                 (Owner(swap) == capture) != captureOptOut)) ok = false;
  if(device && device->GetCreationFlags() != optoutFlags) ok = false;
  printf("optout_swapchain hr=%08lx swap=%d context=%d %s\n", hr, swap != NULL,
         context != NULL, ok ? "PASS" : "FAIL");
  if(!ok) failures++;
  Release(swap); Release(context); Release(device);

  // The returned swapchain/context must keep a temporary wrapped device alive even
  // when the caller did not request the device itself.
  for(int optout = 0; optout < 2; optout++)
  {
    for(int wantContext = 0; wantContext < 2; wantContext++)
    {
      const UINT flags = optout ? optoutFlags : 0;
      swap = NULL; context = NULL;
      hr = createSwap(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, NULL, 0,
                      D3D11_SDK_VERSION, &desc, &swap, NULL, NULL,
                      wantContext ? &context : NULL);
      ID3D11Device *swapDevice = NULL;
      ID3D11Device *contextDevice = NULL;
      if(swap) swap->GetDevice(__uuidof(ID3D11Device), (void **)&swapDevice);
      if(context) context->GetDevice(&contextDevice);
      ok = SUCCEEDED(hr) && swap && swapDevice && (!wantContext || (context && contextDevice));
      if(contextDevice && swapDevice != contextDevice) ok = false;
      if(swapDevice && swapDevice->GetCreationFlags() != flags) ok = false;
      const bool expectWrapped = !optout || captureOptOut;
      if(capture && ((Owner(swap) == capture) != expectWrapped ||
                     (Owner(swapDevice) == capture) != expectWrapped)) ok = false;
      Release(swapDevice); Release(contextDevice); Release(swap);
      if(context) context->Flush();
      Release(context);
      printf("swap_without_device optout=%d context=%d hr=%08lx %s\n",
             optout, wantContext, hr, ok ? "PASS" : "FAIL");
      if(!ok) failures++;
    }
  }
  DestroyWindow(wnd);

  // Failure and swapchain opt-out returns must also clear the recursion guard.
  device = NULL; context = NULL;
  hr = create(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
              &device, NULL, &context);
  ok = SUCCEEDED(hr) && device && context && (!capture || Owner(device) == capture);
  printf("capture_after_optout hr=%08lx %s\n", hr, ok ? "PASS" : "FAIL");
  if(!ok) failures++;
  Release(context); Release(device);
  printf("CONTRACT_COMPLETE failures=%d\n", failures);
  return failures ? 1 : 0;
}
