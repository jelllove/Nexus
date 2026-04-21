@echo off
setlocal

:: Extract version from CMakeLists.txt using PowerShell
for /f "usebackq delims=" %%V in (`powershell -NoProfile -Command "(Select-String -Path CMakeLists.txt -Pattern 'project\(Nexus VERSION (\S+)').Matches[0].Groups[1].Value"`) do set "VERSION=%%V"
if not defined VERSION (
    echo ERROR: Could not extract version from CMakeLists.txt
    exit /b 1
)
echo === Nexus Release v%VERSION% ===

:: Step 1: Build
echo.
echo [1/4] Building...
call build.bat
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

:: Step 2: Create installer
echo.
echo [2/4] Creating installer...
set ISCC="C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist %ISCC% (
    echo ERROR: InnoSetup not found at %ISCC%
    exit /b 1
)
%ISCC% installer\nexus-setup.iss
if errorlevel 1 (
    echo Installer creation failed!
    exit /b 1
)

:: Step 3: Create zip
echo.
echo [3/4] Creating zip archive...
set ZIP_NAME=Nexus-v%VERSION%-windows-x64.zip
if exist "%ZIP_NAME%" del /Q "%ZIP_NAME%"
powershell -NoProfile -Command "Compress-Archive -Path 'dist\*' -DestinationPath '%ZIP_NAME%' -Force"
if errorlevel 1 (
    echo Zip creation failed!
    exit /b 1
)

:: Step 4: Publish to GitHub Release
echo.
echo [4/4] Publishing GitHub Release...
set INSTALLER=installer\Output\Nexus-Setup-v%VERSION%-x64.exe
if not exist "%INSTALLER%" (
    echo ERROR: Installer not found at %INSTALLER%
    exit /b 1
)

gh release create "v%VERSION%" --title "Nexus v%VERSION%" --generate-notes "%INSTALLER%" "%ZIP_NAME%"
if errorlevel 1 (
    echo GitHub release failed! Files are still available locally:
    echo   Installer: %INSTALLER%
    echo   Zip: %ZIP_NAME%
    exit /b 1
)

echo.
echo === Release v%VERSION% published successfully! ===
echo   Installer: %INSTALLER%
echo   Zip: %ZIP_NAME%

endlocal
