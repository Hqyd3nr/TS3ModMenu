$ErrorActionPreference='Stop'
$ceTools='<workspace>\work\CompleteEdition\toolchain\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x64'
$ceOriginal='<msvc-toolchain>'
$ceInclude=Join-Path $ceOriginal 'VC\Tools\MSVC\14.34.31933\include'
$ceSdk=Join-Path $ceOriginal 'Windows Kits\10\Include\10.0.22621.0'
$env:INCLUDE="$ceInclude;$ceSdk\ucrt;$ceSdk\shared;$ceSdk\um"
$env:PATH="$ceTools;$ceOriginal\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x86;$env:PATH"
& "$ceTools\cl.exe" /nologo /O2 /Oi /Zl /GS- /GR- /std:c++17 /c work/CompleteEdition/mod-menu/standalone/InputTest.cpp /Fowork/CompleteEdition/mod-menu/standalone/InputTest.obj
if($LASTEXITCODE -ne 0){throw 'Native test compilation failed'}
& "$ceTools\link.exe" /nologo /NODEFAULTLIB /ENTRY:TestEntry /SUBSYSTEM:CONSOLE /MACHINE:X64 /OUT:work/CompleteEdition/mod-menu/standalone/InputTest.exe work/CompleteEdition/mod-menu/standalone/InputTest.obj work/CompleteEdition/toolchain/imports/kernel32.lib work/CompleteEdition/toolchain/imports/user32.lib work/CompleteEdition/lightning-rod-repair/ntdll.lib
if($LASTEXITCODE -ne 0){throw 'Native test link failed'}
& 'work/CompleteEdition/mod-menu/standalone/InputTest.exe'
if($LASTEXITCODE -ne 0){throw "Native test failed: $LASTEXITCODE"}



