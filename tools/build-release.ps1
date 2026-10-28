param([string]$BuildDirectory = "build-msvc", [string]$OutputDirectory = "dist")
$ErrorActionPreference = "Stop"
$SourceDirectory = Split-Path -Parent $PSScriptRoot
Push-Location $SourceDirectory
try {
    cmake -S . -B $BuildDirectory -A x64
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
    cmake --build $BuildDirectory --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw "Release build failed" }
    ctest --test-dir $BuildDirectory -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Core tests failed" }
    $PackageDirectory = Join-Path $OutputDirectory "TinyRadial"
    cmake --install $BuildDirectory --config Release --prefix $PackageDirectory
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
