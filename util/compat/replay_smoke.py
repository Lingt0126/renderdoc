"""Run with qrendertest --python; input/output paths are supplied in child-only env vars.

This is a basic independent replay check, not Wuthering Waves scene acceptance.
"""
import ctypes
import json
import os
import sys
import traceback
import renderdoc as rd
import qrenderdoc


def main():
    dll = ctypes.WinDLL('rendertest.dll')
    module = ctypes.create_unicode_buffer(32768)
    get_module_path = ctypes.windll.kernel32.GetModuleFileNameW
    get_module_path.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_uint]
    assert get_module_path(dll._handle, module, len(module))
    assert rd.GetLogFile(), 'Public replay initialisation did not initialise the runtime log'
    # The renamed runtime still serves public API names and Python modules.
    assert dll.RENDERDOC_GetAPI
    settings = qrenderdoc.CaptureSettings()
    assert settings.compatibilityProfile == 'auto'
    cap = rd.OpenCaptureFile()
    controller = None
    try:
        result = cap.OpenFile(os.environ['COMPAT_SMOKE_RDC'], '', None)
        assert result, str(result)
        result, controller = cap.OpenCapture(rd.ReplayOptions(), None)
        assert result and controller, str(result)
        def flatten(actions):
            result = []
            for action in actions:
                result.append(action)
                result.extend(flatten(action.children))
            return result
        actions = flatten(controller.GetRootActions())
        assert actions, 'No replay actions'
        textures = controller.GetTextures()
        assert textures, 'No replay textures'
        controller.SetFrameEvent(actions[-1].eventId, True)
        # A present action's copyDestination is the final backbuffer in RenderDoc's API.
        presents = [a for a in actions if a.flags & rd.ActionFlags.Present]
        assert presents, 'No present action'
        backbuffer = presents[-1].copyDestination
        assert backbuffer != rd.ResourceId.Null(), 'No final backbuffer resource'
        save = rd.TextureSave()
        save.resourceId = backbuffer
        save.destType = rd.FileType.PNG
        save.mip = 0
        result = controller.SaveTexture(save, os.environ['COMPAT_SMOKE_PNG'])
        assert result, str(result)
        return {'result': 'PASS', 'runtime': module.value, 'replay_log': rd.GetLogFile(),
                'capture': os.environ['COMPAT_SMOKE_RDC'], 'driver': cap.DriverName(),
                'actions': len(actions), 'textures': len(textures),
                'final_event': actions[-1].eventId, 'png': os.environ['COMPAT_SMOKE_PNG']}
    finally:
        if controller:
            controller.Shutdown()
        cap.Shutdown()


try:
    report = main()
except BaseException:
    report = {'result': 'FAIL', 'error': traceback.format_exc()}
with open(os.environ['COMPAT_SMOKE_RESULT'], 'w') as output:
    json.dump(report, output, indent=2)
sys.exit(0 if report['result'] == 'PASS' else 1)
