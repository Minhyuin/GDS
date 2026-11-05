$ErrorActionPreference = 'Stop'

$testDir = $PSScriptRoot
$firmwareDir = (Resolve-Path (Join-Path $testDir '..\..\..')).Path
$source = Join-Path $firmwareDir 'user\utils\OperatorNew\OperatorNew.cpp'
$outDir = Join-Path $env:TEMP ("gds_runtime_support_{0}" -f $PID)
$exe = Join-Path $outDir 'test_runtime_support.exe'
$tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
$outFull = [IO.Path]::GetFullPath($outDir)

if ($env:CXX) {
    $cxxCommand = Get-Command $env:CXX -CommandType Application -ErrorAction Stop
} else {
    $cxxCommand = Get-Command g++ -CommandType Application -ErrorAction SilentlyContinue
    if (-not $cxxCommand) {
        $cxxCommand = Get-Command clang++ -CommandType Application -ErrorAction Stop
    }
}
$cxx = $cxxCommand.Source

$cxxDir = Split-Path -Parent $cxx
$msysRoot = Split-Path -Parent $cxxDir
$runtimeDirs = @(
    $cxxDir,
    (Join-Path $cxxDir 'cpp'),
    (Join-Path $msysRoot 'usr\bin')
) | Where-Object { Test-Path -LiteralPath $_ }
$env:PATH = ($runtimeDirs -join ';') + ';' + $env:PATH
$cxxFlags = @($env:CXXFLAGS -split '\s+' | Where-Object { $_ })
Write-Host "CXX: $cxx"
Write-Host "CXXFLAGS: $($env:CXXFLAGS)"
& $cxx --version | Select-Object -First 1

if (-not $outFull.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to use non-temporary output path: $outFull"
}

New-Item -ItemType Directory -Path $outDir -Force | Out-Null
try {
    & $cxx @cxxFlags -std=c++17 -Wall -Wextra -Werror -static -fno-exceptions -fno-rtti -I $testDir `
        -include (Join-Path $testDir 'aligned_alloc_host_shim.h') `
        (Join-Path $testDir 'test_runtime_support.cpp') $source -o $exe
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    & $exe
    exit $LASTEXITCODE
}
finally {
    if (Test-Path -LiteralPath $outFull) {
        Remove-Item -LiteralPath $outFull -Recurse -Force
    }
}
