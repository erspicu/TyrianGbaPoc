@echo off
setlocal
pushd "%~dp0"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass ^
  -File "%~dp0tools\build_release.ps1" %*
set "BUILD_EXIT=%ERRORLEVEL%"

if not "%BUILD_EXIT%"=="0" (
  echo.
  echo Build failed. See the error above.
  pause
)

popd
exit /b %BUILD_EXIT%
