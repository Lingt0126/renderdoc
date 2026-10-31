@echo off
if not exist "%~dp0x64\Development\qrenderdoc.exe" (
  echo Build the x64 Development configuration first. See docs/how/genshin_capture.rst.
  exit /b 1
)
start "" "%~dp0x64\Development\qrenderdoc.exe" "%~dp0Genshin.cap"
