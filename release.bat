@echo off
setlocal
rem Veroeffentlicht die Version aus src\game\Version.h: pusht den Stand und das Tag "v<Version>".
rem GitHub Actions baut und testet dann und erstellt das Release - die Spieler bekommen es per Auto-Update.
cd /d "%~dp0"
set "VER="
for /f "tokens=3" %%v in ('findstr /c:"#define KEK_VERSION_STR" src\game\Version.h') do set "VER=%%~v"
if not defined VER (
  echo Version in src\game\Version.h nicht gefunden.
  exit /b 1
)
git diff --quiet HEAD --
if errorlevel 1 (
  echo Es gibt nicht committete Aenderungen - bitte erst committen.
  exit /b 1
)
git rev-parse -q --verify "refs/tags/v%VER%" >nul
if not errorlevel 1 (
  echo Tag v%VER% gibt es schon - Version in src\game\Version.h erhoehen.
  exit /b 1
)
git push origin HEAD || exit /b 1
git tag -a "v%VER%" -m "Kek Me If You Can %VER%" || exit /b 1
git push origin "v%VER%" || exit /b 1
echo.
echo v%VER% ist unterwegs: https://github.com/GG2EZBoOm/kek-me-if-u-can/actions
