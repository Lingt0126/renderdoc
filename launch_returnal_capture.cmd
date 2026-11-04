@echo off
if not exist "C:\RenderDocCustom\qrenderdoc.exe" (
  echo Deploy the latest build with deploy_custom_runtime.ps1 first. See docs/how/returnal_capture.rst.
  exit /b 1
)
start "" "C:\RenderDocCustom\qrenderdoc.exe" "%~dp0Returnal.cap"
