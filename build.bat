@echo off
setlocal

set CMAKE="C:\Program Files\Microsoft Visual Studio\2022\IntPreview\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set QT_DIR=C:\Qt\6.8.3\msvc2022_64
set SRC_DIR=%~dp0
set BUILD_DIR=%SRC_DIR%build
set DIST_DIR=%SRC_DIR%dist
set RELEASE_DIR=%BUILD_DIR%\Release

echo === Nexus Build ===

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

:: Package to dist (only necessary files)
echo [4/4] Packaging to dist...
if exist "%DIST_DIR%" rmdir /S /Q "%DIST_DIR%"
mkdir "%DIST_DIR%"

:: Core exe
copy /Y "%RELEASE_DIR%\Nexus.exe" "%DIST_DIR%\"

:: Required Qt DLLs
for %%F in (Qt6Core Qt6Gui Qt6Widgets Qt6Sql Qt6Network Qt6Svg Qt6WebChannel Qt6WebEngineCore Qt6WebEngineWidgets) do (
    copy /Y "%RELEASE_DIR%\%%F.dll" "%DIST_DIR%\" >nul
)

:: WebEngine process
copy /Y "%RELEASE_DIR%\QtWebEngineProcess.exe" "%DIST_DIR%\"

:: MSVC runtime DLLs (so users without VS installed can run the app)
set VCRT_DIR=
for /f "delims=" %%V in ('dir /B /AD /O-N "C:\Program Files\Microsoft Visual Studio\2022\IntPreview\VC\Redist\MSVC" 2^>nul') do (
    if not defined VCRT_DIR set "VCRT_DIR=C:\Program Files\Microsoft Visual Studio\2022\IntPreview\VC\Redist\MSVC\%%V\x64\Microsoft.VC143.CRT"
)
if defined VCRT_DIR (
    echo Copying MSVC runtime from %VCRT_DIR%...
    for %%F in (vcruntime140.dll vcruntime140_1.dll msvcp140.dll msvcp140_1.dll msvcp140_2.dll concrt140.dll) do (
        if exist "%VCRT_DIR%\%%F" copy /Y "%VCRT_DIR%\%%F" "%DIST_DIR%\" >nul
    )
) else (
    echo WARNING: MSVC runtime DLLs not found. Users may need VC++ Redistributable.
)

:: Required plugin directories
for %%D in (platforms sqldrivers styles iconengines imageformats tls networkinformation) do (
    if exist "%RELEASE_DIR%\%%D" (
        xcopy /E /Y /I /Q "%RELEASE_DIR%\%%D" "%DIST_DIR%\%%D\" >nul
    )
)

:: WebEngine resources (needed for QWebEngineView)
if exist "%RELEASE_DIR%\resources" (
    xcopy /E /Y /I /Q "%RELEASE_DIR%\resources" "%DIST_DIR%\resources\" >nul
)

:: Copy database if it exists
set DB_SRC=%APPDATA%\Nexus\Nexus\nexus.db
if exist "%DB_SRC%" (
    echo Copying database...
    copy /Y "%DB_SRC%" "%DIST_DIR%\nexus.db" >nul
) else (
    echo No existing database found at %DB_SRC%, skipping.
)

:: Show size comparison
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
