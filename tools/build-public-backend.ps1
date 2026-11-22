$ErrorActionPreference='Stop'
$ceTools='<workspace>\work\CompleteEdition\toolchain\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x64'
$ceOriginal='<msvc-toolchain>'
$ceInclude=Join-Path $ceOriginal 'VC\Tools\MSVC\14.34.31933\include'
$ceSdk=Join-Path $ceOriginal 'Windows Kits\10\Include\10.0.22621.0'
$env:INCLUDE="$ceInclude;$ceSdk\ucrt;$ceSdk\shared;$ceSdk\um"
$env:PATH="$ceTools;$ceOriginal\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x86;$env:PATH"
python work/CompleteEdition/importlibs.py
if($LASTEXITCODE -ne 0){throw 'Import library definitions failed'}
& "$ceTools\lib.exe" /nologo /machine:x64 /def:work/CompleteEdition/toolchain/imports/kernel32.def /out:work/CompleteEdition/toolchain/imports/kernel32.lib
if($LASTEXITCODE -ne 0){throw 'Kernel import library failed'}
& "$ceTools\lib.exe" /nologo /machine:x64 /def:work/CompleteEdition/toolchain/imports/user32.def /out:work/CompleteEdition/toolchain/imports/user32.lib
if($LASTEXITCODE -ne 0){throw 'User import library failed'}
& "$ceTools\lib.exe" /nologo /machine:x64 /def:work/CompleteEdition/lightning-rod-repair/ntdll.def /out:work/CompleteEdition/lightning-rod-repair/ntdll.lib
if($LASTEXITCODE -ne 0){throw 'Native exception handler import library failed'}
& "$ceTools\cl.exe" /nologo /LD /O2 /Oi /Zl /GS- /GR- /std:c++17 /DCHEAT_MENU_BRIDGE /showIncludes /Ioutputs/CompleteEdition/BugFixes work/CompleteEdition/mod-menu/PublicModMenu.cpp /Fowork/CompleteEdition/mod-menu/public-runtime.obj /link /NODEFAULTLIB /ENTRY:DllMain /MACHINE:X64 /DEF:work/dinput8.def /OUT:outputs/CompleteEdition/ModMenu/dinput8.dll /IMPLIB:work/CompleteEdition/mod-menu/public-runtime.lib work/CompleteEdition/toolchain/imports/kernel32.lib work/CompleteEdition/toolchain/imports/user32.lib work/CompleteEdition/lightning-rod-repair/ntdll.lib
if($LASTEXITCODE -ne 0){throw 'Flight mod build failed'}













