
param([string]$Config="Release", [string]$BuildDir="build", [string]$Target="")
$ErrorActionPreference = "Stop"
$repo = Join-Path (Split-Path -Parent $PSScriptRoot) "."
$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
$cmd = "\"$vcvars\" >nul && cmake -S \"$repo\" -B \"$repo\$BuildDir\" -G Ninja -DCMAKE_BUILD_TYPE=$Config && cmake --build \"$repo\$BuildDir\""
if ($Target -ne "") { $cmd = "$cmd --target $Target" }
cmd /c $cmd
exit $LASTEXITCODE
