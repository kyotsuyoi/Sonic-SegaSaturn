# /!\ THIS SCRIPT REQUIRES VIRTUAL CLONE DRIVE INSTALLED /!\
$projectRoot = Join-Path $PSScriptRoot ".."
$cuePath = Join-Path $projectRoot "game_build\game.cue"
$ssfDirectory = Join-Path $projectRoot "..\..\Emulators\SSF\"
$vcdMountExe = "C:\Program Files (x86)\Elaborate Bytes\VirtualCloneDrive\vcdmount.exe"

if (-not (Test-Path $vcdMountExe)) {
	Write-Host "Virtual CloneDrive not found at: $vcdMountExe"
	exit 1
}

if (-not (Test-Path $cuePath)) {
	Write-Host "Please compile first ! Missing $cuePath"
	exit 1
}

Write-Host "Mounting image..."
& $vcdMountExe $cuePath

Write-Host "Running SSF..."
Push-Location $ssfDirectory
Start-Process -FilePath "SSF.exe" -Wait
Pop-Location

Write-Host "Unmounting image..."
& $vcdMountExe /u
