param(
    [string]$BuildDirectory = "build-msvc",
    [string]$OutputDirectory = "dist",
    [string]$CMakePath = ""
)
$ErrorActionPreference = "Stop"

function Find-CMake {
    if ($CMakePath) {
        if (-not (Test-Path -LiteralPath $CMakePath -PathType Leaf)) {
            throw "CMake executable not found at '$CMakePath'. Pass -CMakePath with the full path to cmake.exe."
        }
        return (Resolve-Path -LiteralPath $CMakePath).Path
    }

    $command = Get-Command cmake -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Source }

    foreach ($root in @($env:ProgramFiles, ${env:ProgramFiles(x86)}, $env:LOCALAPPDATA)) {
        if (-not $root) { continue }
        foreach ($relativePath in @('CMake\bin\cmake.exe', 'Programs\CMake\bin\cmake.exe')) {
            $candidate = Join-Path $root $relativePath
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        }
    }

    $vswhere = Get-Command vswhere -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    $vswherePath = if ($vswhere) { $vswhere.Source }
    elseif (${env:ProgramFiles(x86)}) {
        Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    }
    if ($vswherePath -and (Test-Path -LiteralPath $vswherePath -PathType Leaf)) {
        $candidates = & $vswherePath -products '*' -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
        if ($LASTEXITCODE -eq 0) {
            foreach ($candidate in $candidates) {
                if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
            }
        }
    }

    throw @"
CMake was not found on PATH or in the searched installation locations.
If CMake is already installed, rerun with -CMakePath 'C:\path\to\cmake.exe',
or add its bin directory to PATH and reopen PowerShell. CMake 3.22+ is required.
If it is missing, enable C++ CMake tools for Windows in Visual Studio Installer.
"@
}

$CMake = Find-CMake
$CTest = Join-Path (Split-Path -Parent $CMake) "ctest.exe"
if (-not (Test-Path -LiteralPath $CTest -PathType Leaf)) {
    throw "CTest was not found beside '$CMake'. Repair or reinstall CMake, then rerun this script."
}
Write-Host "Using CMake: $CMake"
$SourceDirectory = Split-Path -Parent $PSScriptRoot
Push-Location $SourceDirectory
try {
    & $CMake -S . -B $BuildDirectory -A x64
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
    & $CMake --build $BuildDirectory --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw "Release build failed" }
    & $CTest --test-dir $BuildDirectory -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Core tests failed" }
    $PackageDirectory = Join-Path $OutputDirectory "TinyRadial"
    & $CMake --install $BuildDirectory --config Release --prefix $PackageDirectory
    if ($LASTEXITCODE -ne 0) { throw "Packaging failed" }
    $Executable = Get-Item (Join-Path $PackageDirectory "TinyRadial.exe")
    if ($Executable.Length -gt 5000000) { throw "EXE exceeds the 5 MB review threshold; inspect size before distributing" }
    if ($Executable.Length -gt 3000000) { Write-Warning "EXE misses the preferred 3 MB target" }
    $ExecutableHash = (Get-FileHash $Executable.FullName -Algorithm SHA256).Hash
    @(
        "TinyRadial 0.1.0 - Windows host testing build"
        "Built: $([DateTime]::UtcNow.ToString('o'))"
        "Toolchain: MSVC x64 Release, static CRT"
        "Build host: $([Environment]::OSVersion.VersionString)"
        "Executable bytes: $($Executable.Length)"
        "SHA256: $ExecutableHash"
        "Core tests: passed via CTest"
        "Desktop/manual acceptance and runtime measurements: pending"
    ) | Set-Content -Path (Join-Path $PackageDirectory "BUILD_INFO.txt") -Encoding UTF8
    Write-Host "SHA256: $ExecutableHash"
    Compress-Archive -Path $PackageDirectory -DestinationPath (Join-Path $OutputDirectory "TinyRadial-windows-x64.zip") -Force
    Write-Host "Portable test package: $PackageDirectory ($($Executable.Length) executable bytes). Windows manual checks remain required."
} finally { Pop-Location }
