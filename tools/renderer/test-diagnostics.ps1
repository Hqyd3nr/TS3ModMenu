$ErrorActionPreference='Stop'
$ceTools='<workspace>\work\CompleteEdition\toolchain\VC\Tools\MSVC\14.34.31933\bin\Hostx64\x64'
$ceOriginal='<msvc-toolchain>'
$ceMsvc=Join-Path $ceOriginal 'VC\Tools\MSVC\14.34.31933'
$ceSdk=Join-Path $ceOriginal 'Windows Kits\10'
$env:INCLUDE="$ceMsvc\include;$ceSdk\Include\10.0.22621.0\ucrt;$ceSdk\Include\10.0.22621.0\shared;$ceSdk\Include\10.0.22621.0\um"
$env:LIB="$ceMsvc\lib\x64;$ceSdk\Lib\10.0.22621.0\ucrt\x64;$ceSdk\Lib\10.0.22621.0\um\x64"
$env:PATH="$ceTools;$env:PATH"
$out=Join-Path $env:TEMP 'ts3-diagnostics-test'
if(Test-Path $out){Remove-Item -Recurse -Force $out}
New-Item -ItemType Directory -Force "$out\Mod Menu\assets" | Out-Null
Copy-Item outputs/CompleteEdition/ModMenu/assets/font-atlas.rgba "$out\Mod Menu\assets\"
& "$ceTools\cl.exe" /nologo /c /O2 /Oi /Zl /GS- /GR- /std:c++17 /Zc:threadSafeInit- /W3 /DUNICODE /D_UNICODE work/CompleteEdition/mod-menu/standalone/DiagnosticsTest.cpp /Fo$out\DiagnosticsTest.obj
if($LASTEXITCODE -ne 0){throw 'Diagnostics test compile failed'}
& "$ceTools\link.exe" /nologo /NODEFAULTLIB /ENTRY:TestEntry /SUBSYSTEM:CONSOLE /MACHINE:X64 /OUT:$out\DiagnosticsTest.exe $out\DiagnosticsTest.obj work/CompleteEdition/toolchain/imports/kernel32.lib work/CompleteEdition/toolchain/imports/user32.lib
if($LASTEXITCODE -ne 0){throw 'Diagnostics test link failed'}
Push-Location $out
try{
 & .\DiagnosticsTest.exe; if($LASTEXITCODE -ne 0){throw "Diagnostics test failed: $LASTEXITCODE"}
 & .\DiagnosticsTest.exe crash; Write-Output "crash run exit code: $LASTEXITCODE"
 & .\DiagnosticsTest.exe verify; if($LASTEXITCODE -ne 0){throw "Crash report check failed"}
} finally {Pop-Location}
Write-Output $out
