param(
    [string]$Config = "Release",
    [string]$BuildDir = "build",
    [switch]$Asan,
    [switch]$Clean,
    [string]$Target = ""
)
$ErrorActionPreference = "Stop"
$repo = "E:\The Journey\Coding\GitHub\production\Utilization-Observatory"
$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if ($Clean) { Remove-Item -Recurse -Force "$repo\$BuildDir" -ErrorAction SilentlyContinue }
$extra = "-DCMAKE_BUILD_TYPE=$Config"
$asanFlag = "OFF"
if ($Asan) { $asanFlag = "ON" }
$cmd = "`"$vcvars`" >nul && cmake -S "$repo" -B "$repo\$BuildDir" -G Ninja $extra -DUO_ENABLE_ASAN=$asanFlag && cmake --build "$repo\$BuildDir""
if ($Target -ne "") { $cmd = "$cmd --target $Target" }
cmd /c $cmd
exit $LASTEXITCODE
