@echo off
setlocal EnableDelayedExpansion

rem ============================================================
rem  AVSMeter release build (Windows binaries)
rem
rem  Builds the release variants and assembles them, with the
rem  documentation, into a release folder and a 7-Zip archive:
rem    x86\AVSMeter.exe       Release Win32, msvc\AVSMeter.slnx, v143 (MSVC 14.44:
rem    x64\AVSMeter64.exe     Release x64      the last toolset that targets Windows 7)
rem    ARM64\AVSMeter64.exe   Windows on ARM64, cross-compiled with llvm-mingw
rem                           (CMake + Ninja; MinGW C++ ABI, for llvm-mingw built
rem                           AviSynth+)
rem    doc\                   AVSMeter.html, *.png, history.txt
rem    CHANGELOG.md, LICENSE
rem  No sources: they are on GitHub.
rem
rem  Switches, 1 = do it (default), 0 = skip; set them before the run,
rem  e.g. "set BUILD_ARM64=0 && build_release.bat":
rem    BUILD_MSVC  BUILD_ARM64  RUN_TESTS
rem  RUN_TESTS runs tests\run-characterization.ps1 on the x64 and x86 exe
rem  (needs PowerShell 7 and an installed AviSynth+ of both bitnesses).
rem  A missing llvm-mingw skips the ARM64 build with a warning, unless
rem  BUILD_ARM64 was set explicitly.
rem
rem  Detected, can be overridden from the environment:
rem    VER              VERSION_STR of source\version.h ("4.0")
rem    BUILD_DATE       today, YYYYMMDD
rem    LLVM_MINGW_ROOT  llvm-mingw (UCRT flavour) installation
rem  Output: %DEST_ROOT%\<VER>\ and %DEST_ROOT%\AVSMeter-<VER>-<BUILD_DATE>.7z
rem          (DEST_ROOT default: build\release)
rem
rem  The Visual Studio builds are Rebuilds into the usual project output
rem  folder (msvc\bin\<platform>\Release); the ARM64 build uses a fresh
rem  build\release-arm64-mingw folder.
rem ============================================================

set "SRC=%~dp0"
set "SRC=%SRC:~0,-1%"
if not defined DEST_ROOT set "DEST_ROOT=%SRC%\build\release"
if not defined LLVM_MINGW_ROOT set "LLVM_MINGW_ROOT=C:\avsplus_build_deps\llvm-mingw-20261020-ucrt-x86_64"
set "SEVENZIP=%ProgramFiles%\7-Zip\7z.exe"

set "_OVR_ARM64=%BUILD_ARM64%"
if not defined BUILD_MSVC  set "BUILD_MSVC=1"
if not defined BUILD_ARM64 set "BUILD_ARM64=1"
if not defined RUN_TESTS   set "RUN_TESTS=1"
if not "%BUILD_MSVC%"=="1" set "RUN_TESTS=0"

rem ============================================================
rem  DETECTION
rem ============================================================

rem  Version: the line #define VERSION_STR "4.0" of source\version.h
if not defined VER (
  for /f tokens^=2^ delims^=^" %%A in ('findstr /r /c:"^#define VERSION_STR " "%SRC%\source\version.h"') do set "VER=%%A"
)
if not defined VER (
  echo ERROR: no VERSION_STR found in source\version.h
  goto :fatal_exit
)
if not defined BUILD_DATE (
  for /f %%D in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd"') do set "BUILD_DATE=%%D"
)

rem  MSBuild of the newest Visual Studio 2022 or 2026 (v143 is selectable in both).
rem  vswhere writes into a temp file: its version range argument has a comma
rem  and a parenthesis, which a for /f command line does not take unescaped.
set "MSBUILD="
if not "%BUILD_MSVC%"=="1" goto :skip_find_msbuild
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo ERROR: vswhere.exe not found: !VSWHERE!
  goto :fatal_exit
)
set "VSW_TMP=%TEMP%\avsmeter_build_release_vswhere.txt"
"%VSWHERE%" -latest -version [17.0,19.0) -products * -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe > "%VSW_TMP%" 2>nul
set /p MSBUILD=<"%VSW_TMP%"
del "%VSW_TMP%" 2>nul
if not defined MSBUILD (
  echo ERROR: no Visual Studio 2022/2026 MSBuild found ^(BUILD_MSVC=0 skips the x86/x64 builds^)
  goto :fatal_exit
)
:skip_find_msbuild

if "%BUILD_ARM64%"=="1" if not exist "%LLVM_MINGW_ROOT%\bin\aarch64-w64-mingw32-clang++.exe" (
  if defined _OVR_ARM64 (
    echo ERROR: BUILD_ARM64 was set, but llvm-mingw was not found at: !LLVM_MINGW_ROOT!
    goto :fatal_exit
  )
  echo WARNING: llvm-mingw not found at !LLVM_MINGW_ROOT!, skipping the ARM64 build
  set "BUILD_ARM64=0"
)
if "%BUILD_ARM64%"=="1" (
  where ninja >nul 2>nul || ( echo ERROR: ninja.exe not found on PATH, needed for the ARM64 build & goto :fatal_exit )
  where cmake >nul 2>nul || ( echo ERROR: cmake.exe not found on PATH, needed for the ARM64 build & goto :fatal_exit )
)
if "%RUN_TESTS%"=="1" (
  where pwsh >nul 2>nul || ( echo ERROR: pwsh.exe ^(PowerShell 7^) not found, needed for the tests ^(RUN_TESTS=0 skips them^) & goto :fatal_exit )
)

set "DEST=%DEST_ROOT%\%VER%"
set "ARCHIVE=%DEST_ROOT%\AVSMeter-%VER%-%BUILD_DATE%.7z"

set TOTAL_BUILDS=0
if "%BUILD_MSVC%"=="1"  set /a TOTAL_BUILDS+=2
if "%BUILD_ARM64%"=="1" set /a TOTAL_BUILDS+=1
set BUILDNUM=0

echo.
echo ========================================================
echo  AVSMeter release : %VER%  (%BUILD_DATE%)
echo  Destination      : %DEST%
echo  Archive          : %ARCHIVE%
echo  MSVC=%BUILD_MSVC%  ARM64=%BUILD_ARM64%  TESTS=%RUN_TESTS%
echo  MSBuild          : !MSBUILD!
echo  llvm-mingw       : !LLVM_MINGW_ROOT!
echo ========================================================
echo.

if exist "%DEST%" (
  echo WARNING: the destination exists and will be deleted: !DEST!
  echo          Press Ctrl+C to abort, any key to continue.
  pause > nul
  rmdir /s /q "%DEST%"
)
mkdir "%DEST%" || goto :fatal_exit

rem ============================================================
rem  BUILDS
rem ============================================================

if not "%BUILD_MSVC%"=="1" goto :skip_msvc
call :vs_build Win32 AVSMeter.exe x86 || goto :fatal_exit
call :vs_build x64 AVSMeter64.exe x64 || goto :fatal_exit
:skip_msvc

if not "%BUILD_ARM64%"=="1" goto :skip_arm64
set /a BUILDNUM+=1
echo.
echo [BUILD %BUILDNUM%/%TOTAL_BUILDS%] ARM64 llvm-mingw
set "A64_BUILD=%SRC%\build\release-arm64-mingw"
if exist "%A64_BUILD%" rmdir /s /q "%A64_BUILD%"
cmake -S "%SRC%" -B "%A64_BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE="%SRC%\cmake\toolchains\llvm-mingw-aarch64.cmake" ^
  -DLLVM_MINGW_ROOT="%LLVM_MINGW_ROOT%"
if errorlevel 1 (
  echo ERROR: CMake configure failed: ARM64
  goto :fatal_exit
)
cmake --build "%A64_BUILD%" --parallel
if errorlevel 1 (
  echo ERROR: build failed: ARM64
  goto :fatal_exit
)
mkdir "%DEST%\ARM64"
call :copy_file "%A64_BUILD%\AVSMeter64.exe" "%DEST%\ARM64\AVSMeter64.exe" || goto :fatal_exit
:skip_arm64

rem ============================================================
rem  TESTS (characterization tests, x64 and x86)
rem ============================================================

if not "%RUN_TESTS%"=="1" goto :skip_tests
for %%E in (x64\AVSMeter64.exe x86\AVSMeter.exe) do (
  echo.
  echo [TEST] %%E
  pwsh -NoProfile -File "%SRC%\tests\run-characterization.ps1" -Exe "%DEST%\%%E"
  if errorlevel 1 (
    echo ERROR: tests failed: %%E
    goto :fatal_exit
  )
)
:skip_tests

rem ============================================================
rem  DOCUMENTATION
rem ============================================================
echo.
echo [DOCS] Copying documentation...
for %%F in (CHANGELOG.md LICENSE) do (
  call :copy_file "%SRC%\%%F" "%DEST%\%%F" || goto :fatal_exit
)
mkdir "%DEST%\doc"
for %%F in (AVSMeter.html history.txt) do (
  call :copy_file "%SRC%\doc\%%F" "%DEST%\doc\%%F" || goto :fatal_exit
)
for %%F in ("%SRC%\doc\*.png") do (
  call :copy_file "%%~fF" "%DEST%\doc\%%~nxF" || goto :fatal_exit
)

rem ============================================================
rem  ARCHIVE (the content of the release folder, without the folder itself)
rem ============================================================
echo.
if not exist "%SEVENZIP%" (
  echo WARNING: 7-Zip not found at !SEVENZIP!, no archive created
  goto :done
)
echo [ARCHIVE] %ARCHIVE%
if exist "%ARCHIVE%" del "%ARCHIVE%"
"%SEVENZIP%" a -mx=9 "%ARCHIVE%" "%DEST%\*" > nul
if errorlevel 1 (
  echo ERROR: 7-Zip failed
  goto :fatal_exit
)

:done
echo.
echo ========================================================
echo  RELEASE BUILD COMPLETE: %DEST%
echo ========================================================
dir /s /b "%DEST%\*.exe"
endlocal
exit /b 0

rem  "exit /b 1" inside a parenthesised block does not reliably set the exit
rem  code, so the error paths jump here.
:fatal_exit
echo.
echo RELEASE BUILD FAILED
endlocal
exit /b 1

rem ------------------------------------------------------------
rem  :vs_build <platform> <exe name> <destination subfolder>
rem ------------------------------------------------------------
:vs_build
set /a BUILDNUM+=1
echo.
echo [BUILD %BUILDNUM%/%TOTAL_BUILDS%] Release %~1
"%MSBUILD%" "%SRC%\msvc\AVSMeter.slnx" -t:Rebuild -p:Configuration=Release -p:Platform=%~1 -m -nologo -v:minimal -nodeReuse:false
if errorlevel 1 (
  echo ERROR: build failed: Release %~1
  exit /b 1
)
mkdir "%DEST%\%~3"
call :copy_file "%SRC%\msvc\bin\%~1\Release\%~2" "%DEST%\%~3\%~2" || exit /b 1
exit /b 0

rem ------------------------------------------------------------
rem  :copy_file <source> <destination file>
rem ------------------------------------------------------------
:copy_file
if not exist "%~1" (
  echo ERROR: missing: %~1
  exit /b 1
)
copy /y "%~1" "%~2" > nul || exit /b 1
for %%F in ("%~1") do echo   %%~tF  %%~nxF -^> %~2
exit /b 0
