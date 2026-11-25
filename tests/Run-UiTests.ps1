$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio C++ tools not found.' }
$msbuild = Join-Path $visualStudio 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild (Join-Path $projectRoot 'xiaoquguanlixit.sln') /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
if ($LASTEXITCODE) { throw 'Application build failed.' }
$testBuild = Join-Path ([IO.Path]::GetTempPath()) ('community-ui-build-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testBuild | Out-Null
$objects = Get-ChildItem (Join-Path $projectRoot 'xiaoquguanlixit\x64\Debug') -Filter '*.obj' |
    Where-Object { $_.Name -ne 'main.obj' } |
    ForEach-Object { '"' + $_.FullName + '"' }
$vcvars = Join-Path $visualStudio 'VC\Auxiliary\Build\vcvars64.bat'
$testSource = Join-Path $PSScriptRoot 'UiIntegrationTests.cpp'
$testExe = Join-Path $testBuild 'UiIntegrationTests.exe'
$command = 'call "' + $vcvars + '" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /MDd /I"' +
    $projectRoot + '" /I"' + (Join-Path $projectRoot 'include') + '" "' + $testSource +
    '" /Fo"' + (Join-Path $testBuild 'UiIntegrationTests.obj') + '" /Fe"' + $testExe +
    '" ' + ($objects -join ' ') +
    ' /link /SUBSYSTEM:CONSOLE /OPT:NOICF /OPT:NOREF d3d11.lib dxgi.lib d3dcompiler.lib dwmapi.lib imm32.lib shell32.lib ole32.lib user32.lib gdi32.lib'
$PSNativeCommandArgumentPassing = 'Legacy'
& $env:ComSpec /d /c $command
if ($LASTEXITCODE) { throw "Test compilation failed. Build directory: $testBuild" }
& $testExe
if ($LASTEXITCODE) { throw "UI tests failed. Build directory: $testBuild" }
Write-Output "Test executable: $testExe"
