@echo off
setlocal
cd /d "%~dp0"
dotnet publish TyrianSaveEditor.csproj -c Release -r win-x64 --self-contained false -o publish
if errorlevel 1 exit /b %errorlevel%
echo.
echo TyrianSaveEditor built at:
echo   %CD%\publish\TyrianSaveEditor.exe
endlocal
