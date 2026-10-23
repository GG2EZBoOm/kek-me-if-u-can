@echo off
setlocal
rem Baut Kek Me If You Can mit dem MSVC-Compiler aus Visual Studio (CMake + Ninja werden mitgeliefert).
set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS (
  for /d %%v in ("%ProgramFiles%\Microsoft Visual Studio\*") do for /d %%e in ("%%v\*") do if exist "%%e\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%e\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS (
  echo Visual Studio mit C++-Werkzeugen wurde nicht gefunden.
  exit /b 1
)
call "%VCVARS%" >nul || exit /b 1
set "CFG=%1"
if "%CFG%"=="" set "CFG=Release"
cmake -S "%~dp0." -B "%~dp0build\%CFG%" -G Ninja -DCMAKE_BUILD_TYPE=%CFG% || exit /b 1
cmake --build "%~dp0build\%CFG%" || exit /b 1
copy /y "%~dp0build\%CFG%\KekMeIfYouCan.exe" "%~dp0KekMeIfYouCan.exe" >nul
copy /y "%~dp0build\%CFG%\KekMeIfYouCanCLI.exe" "%~dp0KekMeIfYouCanCLI.exe" >nul
echo.
echo Fertig: %~dp0KekMeIfYouCan.exe
