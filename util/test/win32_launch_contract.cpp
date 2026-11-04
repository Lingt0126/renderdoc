#include <windows.h>
#include <cstdio>
#define RENDERDOC_PLATFORM_WIN32
#include "renderdoc/api/replay/renderdoc_replay.h"
REPLAY_PROGRAM_MARKER();

int main(int argc, char **argv)
{
  if(argc != 4) return 2;
  RENDERDOC_InitialiseReplay(GlobalEnvironment(), {});
  RENDERDOC_SetDebugLogFile(argv[3]);
  CaptureOptions opts;
  opts.hookIntoChildren = false;
  const bool bootstrap = strcmp(argv[2], "bootstrap") == 0;
  const bool invalid = strcmp(argv[2], "invalid-pid") == 0;
  ExecuteResult result = invalid
      ? RENDERDOC_InjectIntoProcess(0, {}, "", opts, false)
      : RENDERDOC_ExecuteAndInject(argv[1], "", bootstrap ? "bootstrap" : "ordinary",
                                  {}, "", opts, false);
  bool ok = bootstrap || invalid
      ? result.result.code == ResultCode::InjectionFailed && result.ident == 0
      : result.result.code == ResultCode::Succeeded && result.ident != 0;
  printf("mode=%s result=%u ident=%u %s\n", argv[2], unsigned(result.result.code),
         result.ident, ok ? "PASS" : "FAIL");
  // RenderDoc removes its temporary log at clean DLL unload. Preserve the evidence
  // before shutdown rather than treating the expected cleanup as missing diagnostics.
  rdcstr saved = rdcstr(argv[3]) + ".saved";
  if(!CopyFileA(argv[3], saved.c_str(), FALSE))
  {
    printf("LOG_SAVE_FAILED=%lu\n", GetLastError());
    ok = false;
  }
  RENDERDOC_ShutdownReplay();
  return ok ? 0 : 1;
}
