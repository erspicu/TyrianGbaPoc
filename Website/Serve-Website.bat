@echo off
setlocal
pushd "%~dp0"

set "SITE_PORT=8080"
set "PROJECT_PYTHON=%~dp0..\.venv\Scripts\python.exe"

if exist "%PROJECT_PYTHON%" (
  set "PYTHON_CMD=%PROJECT_PYTHON%"
) else (
  set "PYTHON_CMD=python"
)

echo TyrianGbaPoc local website
echo URL: http://127.0.0.1:%SITE_PORT%/
echo Press Ctrl+C to stop.
echo.

start "" "http://127.0.0.1:%SITE_PORT%/"
"%PYTHON_CMD%" -m http.server %SITE_PORT% --bind 127.0.0.1

popd
endlocal
