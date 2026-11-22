param([switch]$HostOnly)
$ErrorActionPreference='Stop'
$ceTools='<workspace>\work\CompleteEdition\toolchain\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x64'
$ceOriginal='<msvc-toolchain>'
$ceMsvc=Join-Path $ceOriginal 'VC\Tools\MSVC\14.34.31933'
$ceSdk=Join-Path $ceOriginal 'Windows Kits\10'
$env:INCLUDE="$ceMsvc\include;$ceSdk\Include\10.0.22621.0\ucrt;$ceSdk\Include\10.0.22621.0\shared;$ceSdk\Include\10.0.22621.0\um"
$env:LIB="$ceMsvc\lib\x64;$ceSdk\Lib\10.0.22621.0\ucrt\x64;$ceSdk\Lib\10.0.22621.0\um\x64"
$env:PATH="$ceTools;$env:PATH"

$out='outputs/CompleteEdition/ModMenu/StandaloneTest'
New-Item -ItemType Directory -Force "$out/Mod Menu/assets" | Out-Null
Copy-Item outputs/CompleteEdition/ModMenu/assets/* "$out/Mod Menu/assets" -Force
& "$ceTools\cl.exe" /nologo /c /O2 /Oi /Zl /GS- /GR- /std:c++17 /Zc:threadSafeInit- /DUI_SELF_TEST /DUNICODE /D_UNICODE work/CompleteEdition/mod-menu/standalone/RendererTest.cpp /Fowork/CompleteEdition/mod-menu/standalone/RendererTest.obj
if($LASTEXITCODE -ne 0){throw 'Renderer test compile failed'}
& "$ceTools\cl.exe" /nologo /c /O2 /Oi /Zl /GS- /GR- /std:c++17 /Zc:threadSafeInit- /DUI_SELF_TEST /DUNICODE /D_UNICODE work/CompleteEdition/mod-menu/NativeUI.cpp /Fowork/CompleteEdition/mod-menu/standalone/NativeUITest.obj
if($LASTEXITCODE -ne 0){throw 'Input test compile failed'}
& "$ceTools\cl.exe" /nologo /c /O2 /Oi /Zl /GS- /GR- /std:c++17 /Zc:threadSafeInit- /DUI_SELF_TEST /DUNICODE /D_UNICODE work/CompleteEdition/mod-menu/ModMenuUI.cpp /Fowork/CompleteEdition/mod-menu/standalone/CheatMenuUITest.obj
if($LASTEXITCODE -ne 0){throw 'Menu test compile failed'}
& "$ceTools\link.exe" /nologo /NODEFAULTLIB /ENTRY:TestEntry /SUBSYSTEM:CONSOLE /MACHINE:X64 /OUT:$out/RendererTest.exe work/CompleteEdition/mod-menu/standalone/RendererTest.obj work/CompleteEdition/mod-menu/standalone/NativeUITest.obj work/CompleteEdition/mod-menu/standalone/CheatMenuUITest.obj work/CompleteEdition/mod-menu/standalone/OptionalEffects.obj work/CompleteEdition/toolchain/imports/kernel32.lib work/CompleteEdition/toolchain/imports/user32.lib work/CompleteEdition/mod-menu/ntdll.lib
if($LASTEXITCODE -ne 0){throw 'Renderer test link failed'}
Push-Location $out
try { & .\RendererTest.exe; if($LASTEXITCODE -ne 0){throw "Software renderer failed: $LASTEXITCODE"} } finally {Pop-Location}
& "$PSScriptRoot/test-missing-assets.ps1" -RendererExecutable "$out/RendererTest.exe" -AssetDirectory "$out/Mod Menu/assets"
