param(
    [string]$QtRoot = $env:QT_ROOT,
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\build'),
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'This helper requires Windows. Use the CMake/CTest commands in README.md on other platforms.'
}

if (-not $QtRoot) {
    $QtRoot = 'C:\Qt\6.8.3\msvc2022_64'
}
if (-not (Test-Path -LiteralPath (Join-Path $QtRoot 'lib\cmake\Qt6Test\Qt6TestConfig.cmake'))) {
    throw "Qt with the Test component was not found at '$QtRoot'. Set QT_ROOT or pass -QtRoot."
}

$cmakeCommand = Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($cmakeCommand) {
    $cmake = $cmakeCommand.Source
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw 'CMake is not on PATH and vswhere is unavailable. Install Visual Studio 2022 C++ and CMake tools.'
    }
    $candidates = @(& $vswhere -products '*' -version '[17.0,18.0)' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
                  Microsoft.VisualStudio.Component.VC.CMake.Project `
        -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')
    if ($LASTEXITCODE -ne 0 -or $candidates.Count -eq 0) {
        throw 'Visual Studio 2022 C++ and CMake tools were not found.'
    }
    $cmake = $candidates[0]
}
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctest)) {
    throw "CTest was not found beside '$cmake'. Install the complete CMake toolset."
}
if (-not (Get-Command npm -ErrorAction SilentlyContinue)) {
    throw 'Node.js and npm are required for editor validation. Install Node.js 22 or newer.'
}

Write-Host "Configuring with $cmake and Qt at $QtRoot"
& $cmake -S $repoRoot -B $BuildDirectory -G 'Visual Studio 17 2022' -A x64 `
    "-DCMAKE_PREFIX_PATH=$QtRoot" -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed (exit $LASTEXITCODE)." }

& $cmake --build $BuildDirectory --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }

& $ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure `
    --no-tests=error --output-junit test-results.xml
if ($LASTEXITCODE -ne 0) { throw "Tests failed (exit $LASTEXITCODE)." }

& npm --prefix (Join-Path $repoRoot 'editor-bundle') run check
if ($LASTEXITCODE -ne 0) { throw "Editor syntax validation failed (exit $LASTEXITCODE)." }
