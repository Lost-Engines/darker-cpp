@echo off
setlocal DisableDelayedExpansion
rem Usage: fetch-assets.bat [destination]
if "%~1"=="--help" goto help
if not "%~2"=="" goto help
set "script_dir=%~dp0"
set "manifest="
rem Default to the package directory, beside darker.exe.
set "destination=%~dp0.."
if not "%~1"=="" set "destination=%~1"
if not exist "%destination%" mkdir "%destination%"
if not exist "%destination%" exit /b 1
pushd "%destination%" || exit /b 1

rem 1. Fetch missing manuals and map.
set "folder="
set "warning=Please find a copy of the manual or map yourself."
for /f "usebackq delims=" %%U in ("%script_dir%manual_urls.txt") do (
  set "url=%%U"
  call :fetch
)

rem 2. Keep valid game packs; replace missing or damaged ones.
set "manifest=darker-retail-packs.sha256"
set "warning=Rerun this script to retry, or supply a full game installation."
for /f "usebackq delims=" %%U in ("%script_dir%game_urls.txt") do (
  set "url=%%U"
  call :fetch
)
call :check darker-retail-packs.sha256
set "manifest="

rem 3. Fetch missing Roland ROMs.
set "warning=Roland emulation will not be available; other sound engines will work."
for /f "usebackq delims=" %%U in ("%script_dir%roland_rom_urls.txt") do (
  set "url=%%U"
  call :fetch
)

rem 4. Fetch missing SC-55 v1.21 ROMs.
set "manifest=sc55-v121.sha256"
set "warning=SC-55 emulation needs all five ROMs; other music options remain available."
for /f "usebackq delims=" %%U in ("%script_dir%sc55_rom_urls.txt") do (
  set "url=%%U"
  call :fetch
)
call :check sc55-v121.sha256

rem 5. Fetch the AWE32 sample ROM.
set "manifest=awe32.sha256"
set "warning=AWE32 ROM unavailable; other music options remain available."
for /f "usebackq delims=" %%U in ("%script_dir%awe32_rom_urls.txt") do (
  set "url=%%U"
  call :fetch
)
call :check awe32.sha256

rem 6. UltraMID and its Gravis preload patch sets.
set "manifest=gravis.sha256"
if not exist ULTRASND\MIDI mkdir ULTRASND\MIDI
if not exist ULTRASND\MIDI (
  popd
  exit /b 1
)
set "folder=ULTRASND\MIDI\"
set "warning=Please supply the original Gravis patch set; other music options still work."
for /f "usebackq delims=" %%U in ("%script_dir%gravis_urls.txt") do (
  set "url=%%U"
  call :fetch
)
call :check gravis.sha256
popd
exit /b 0

:fetch
rem Keep URLs in a variable: passing them through CALL would expand percent escapes.
for %%F in ("%url%") do set "file=%folder%%%~nxF"
if "%file%"=="ULTRASND\MIDI\ULTRAMID.EXE" set "file=ULTRASND\ULTRAMID.EXE"
if not defined manifest if exist "%file%" exit /b 0
if defined manifest (
  call :matches_checksum
  if defined matched (
    echo %file%: OK
    exit /b 0
  )
)
echo Fetching %file%
curl.exe --fail --location --retry 3 --retry-all-errors --retry-delay 2 --connect-timeout 30 --output "%file%" "%url%"
if errorlevel 1 echo Warning: %file% is unavailable. %warning%
exit /b 0

:matches_checksum
rem Manifest paths use forward slashes, including on Windows.
set "matched="
for /f "usebackq tokens=1,*" %%H in ("%script_dir%%manifest%") do if /I "%%I"=="%file:\=/%" (
  for /f "delims=" %%D in ('certutil.exe -hashfile "%file%" SHA256 2^>nul') do (
    if /I "%%D"=="%%H" set "matched=1"
  )
)
exit /b 0

:check
rem CertUtil is included with Windows; manifests are shared with the Bash helper.
for /f "usebackq tokens=1,*" %%H in ("%script_dir%%~1") do (
  set "matched="
  for /f "delims=" %%D in ('certutil.exe -hashfile "%%I" SHA256 2^>nul') do (
    if /I "%%D"=="%%H" set "matched=1"
  )
  if not defined matched echo Warning: %%I does not match the known checksum. File left in place.
)
exit /b 0

:help
echo Usage: %~nx0 [destination]
exit /b 0
