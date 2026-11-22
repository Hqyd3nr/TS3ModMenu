param([switch]$HostOnly)
$ErrorActionPreference='Stop'
$ceTools='<workspace>\work\CompleteEdition\toolchain\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x64'
$ceOriginal='<msvc-toolchain>'
$ceMsvc=Join-Path $ceOriginal 'VC\Tools\MSVC\14.34.31933'
$ceSdk=Join-Path $ceOriginal 'Windows Kits\10'
$env:INCLUDE="$ceMsvc\include;$ceSdk\Include\10.0.22621.0\ucrt;$ceSdk\Include\10.0.22621.0\shared;$ceSdk\Include\10.0.22621.0\um"
$env:LIB="$ceMsvc\lib\x64;$ceSdk\Lib\10.0.22621.0\ucrt\x64;$ceSdk\Lib\10.0.22621.0\um\x64"
$env:PATH="$ceTools;$env:PATH"
New-Item -ItemType Directory -Force outputs/CompleteEdition/ModMenu | Out-Null
& "$ceTools\cl.exe" /nologo /c /O2 /Oi /Zl /GS- /GR- /Zc:threadSafeInit- /std:c++17 /W4 /DUNICODE /D_UNICODE work/CompleteEdition/mod-menu/OverlayHost.cpp /Fowork/CompleteEdition/mod-menu/OverlayHost.obj
if($LASTEXITCODE -ne 0){throw 'Overlay host compilation failed'}
if($HostOnly){return}
& "$ceTools\cl.exe" /nologo /c /O2 /Oi /Zl /GS- /GR- /Zc:threadSafeInit- /std:c++17 /W4 /DUNICODE /D_UNICODE work/CompleteEdition/mod-menu/ModMenuUI.cpp /Fowork/CompleteEdition/mod-menu/CheatMenuUI.obj
if($LASTEXITCODE -ne 0){throw 'Mod Menu UI compilation failed'}
& "$ceTools\cl.exe" /nologo /c /O2 /Oi /Zl /GS- /GR- /Zc:threadSafeInit- /std:c++17 /W4 /DUNICODE /D_UNICODE work/CompleteEdition/mod-menu/NativeUI.cpp /Fowork/CompleteEdition/mod-menu/NativeUI.obj
if($LASTEXITCODE -ne 0){throw 'Native UI compilation failed'}
& "$ceTools\cl.exe" /nologo /c /O2 /Oi /Zl /GS- /GR- /std:c++17 /DUNICODE /D_UNICODE work/CompleteEdition/mod-menu/standalone/OptionalEffects.cpp /Fowork/CompleteEdition/mod-menu/standalone/OptionalEffects.obj
if($LASTEXITCODE -ne 0){throw 'Optional effects compatibility compile failed'}
& "$ceTools\lib.exe" /nologo /machine:x64 /def:work/CompleteEdition/mod-menu/ntdll.def /out:work/CompleteEdition/mod-menu/ntdll.lib
if($LASTEXITCODE -ne 0){throw 'Stack probe import library failed'}
& "$ceTools\link.exe" /nologo /DLL /NODEFAULTLIB /ENTRY:DllMain /MACHINE:X64 /OUT:outputs/CompleteEdition/ModMenu/TS3ModMenu.dll /IMPLIB:work/CompleteEdition/mod-menu/TS3CheatMenu.lib work/CompleteEdition/mod-menu/OverlayHost.obj work/CompleteEdition/mod-menu/CheatMenuUI.obj work/CompleteEdition/mod-menu/NativeUI.obj work/CompleteEdition/mod-menu/standalone/OptionalEffects.obj work/CompleteEdition/toolchain/imports/kernel32.lib work/CompleteEdition/toolchain/imports/user32.lib work/CompleteEdition/mod-menu/ntdll.lib
if($LASTEXITCODE -ne 0){throw 'Mod Menu add-on link failed'}
