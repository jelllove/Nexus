@echo off
setlocal

set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\IntPreview\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set QT_DIR=C:\Qt\6.8.3\msvc2022_64
set SRC_DIR=%~dp0
set BUILD_DIR=%SRC_DIR%build
set DIST_DIR=%SRC_DIR%dist
set RELEASE_DIR=%BUILD_DIR%\Release

echo === Nexus Build ===

:: Phase 0: Generate .ico from .png if needed
if not exist "resources\icons\app-icon.ico" (
    echo [0/4] Generating app icon .ico from .png...
    python -c "from PIL import Image; img=Image.open(r'resources\icons\app-icon.png'); img.save(r'resources\icons\app-icon.ico', sizes=[(16,16),(32,32),(48,48),(256,256)])"
    if errorlevel 1 (
        echo WARNING: Failed to generate .ico - Python PIL may not be installed
    )
) else (
    :: Regenerate if .png is newer than .ico
    for %%P in (resources\icons\app-icon.png) do for %%I in (resources\icons\app-icon.ico) do (
        if "%%~tP" GTR "%%~tI" (
            echo [0/4] Regenerating app icon .ico ^(png is newer^)...
            python -c "from PIL import Image; img=Image.open(r'resources\icons\app-icon.png'); img.save(r'resources\icons\app-icon.ico', sizes=[(16,16),(32,32),(48,48),(256,256)])"
        ) else (
            echo [0/4] App icon .ico is up to date, skipping.
        )
    )
)

:: Configure (only if not already configured)
if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [1/4] Configuring CMake...
    %CMAKE% -S "%SRC_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_DIR%"
    if errorlevel 1 goto :error
) else (
    echo [1/4] CMake already configured, skipping.
)

:: Build
echo [2/4] Building Release...
%CMAKE% --build "%BUILD_DIR%" --config Release
if errorlevel 1 goto :error

:: Deploy Qt DLLs (only if not already deployed)
if not exist "%RELEASE_DIR%\Qt6Core.dll" (
    echo [3/4] Deploying Qt DLLs...
    "%QT_DIR%\bin\windeployqt6.exe" "%RELEASE_DIR%\Nexus.exe"
    if errorlevel 1 goto :error
) else (
    echo [3/4] Qt DLLs already deployed, skipping.
)

:: Package to dist
echo [4/4] Packaging to dist...
if exist "%DIST_DIR%" rmdir /S /Q "%DIST_DIR%"
mkdir "%DIST_DIR%"

:: Copy everything from Release
xcopy /E /Y /I /Q "%RELEASE_DIR%\*" "%DIST_DIR%\"

:: Remove unnecessary files to save space
if exist "%DIST_DIR%\opengl32sw.dll" del /Q "%DIST_DIR%\opengl32sw.dll"
if exist "%DIST_DIR%\D3Dcompiler_47.dll" del /Q "%DIST_DIR%\D3Dcompiler_47.dll"
if exist "%DIST_DIR%\translations" rmdir /S /Q "%DIST_DIR%\translations"
if exist "%DIST_DIR%\generic" rmdir /S /Q "%DIST_DIR%\generic"
if exist "%DIST_DIR%\position" rmdir /S /Q "%DIST_DIR%\position"
if exist "%DIST_DIR%\qmltooling" rmdir /S /Q "%DIST_DIR%\qmltooling"

:: MSVC runtime DLLs (so users without VS installed can run the app)
set VCRT_DIR=
for /f "delims=" %%V in ('dir /B /AD /O-N "C:\Program Files\Microsoft Visual Studio\2022\IntPreview\VC\Redist\MSVC" 2^>nul') do (
    if not defined VCRT_DIR (
        if exist "C:\Program Files\Microsoft Visual Studio\2022\IntPreview\VC\Redist\MSVC\%%V\x64\Microsoft.VC143.CRT\vcruntime140.dll" (
            set "VCRT_DIR=C:\Program Files\Microsoft Visual Studio\2022\IntPreview\VC\Redist\MSVC\%%V\x64\Microsoft.VC143.CRT"
        )
    )
)
if defined VCRT_DIR (
    echo Copying MSVC runtime from %VCRT_DIR%...
    for %%F in (vcruntime140.dll vcruntime140_1.dll msvcp140.dll msvcp140_1.dll msvcp140_2.dll concrt140.dll) do (
        if exist "%VCRT_DIR%\%%F" copy /Y "%VCRT_DIR%\%%F" "%DIST_DIR%\" >nul
    )
) else (
    echo WARNING: MSVC runtime DLLs not found. Users may need VC++ Redistributable.
)

:: Copy database if it exists
set DB_SRC=%APPDATA%\Nexus\Nexus\nexus.db
if exist "%DB_SRC%" (
    echo Copying database...
    copy /Y "%DB_SRC%" "%DIST_DIR%\nexus.db" >nul
) else (
    echo No existing database found at %DB_SRC%, skipping.
)

:: Show size
echo.
echo === Build complete! ===
echo Output: %DIST_DIR%\Nexus.exe
echo.
echo Dist contents:
dir /S "%DIST_DIR%" 2>nul | findstr /C:"File(s)"
goto :end

:error
echo.
echo === BUILD FAILED ===
exit /b 1

:end
endlocal
