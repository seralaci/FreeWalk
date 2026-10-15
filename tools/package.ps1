# Release package from the built plugin: dist\FreeWalk-<version>[-<name>]\ with the DLL, the INI, the English readme and the licenses
# (the PDB is not included); -Zip: also creates dist\<folder>.zip next to it.
# Usage: package.cmd [name] [-Zip]   (run build.cmd first)
param([string]$Name = "", [switch]$Zip)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$plugin = Get-Content -Raw -Encoding UTF8 (Join-Path $root "src\win\plugin.cpp")
if ($plugin -notmatch 'kVersion = "([^"]+)"') { throw "kVersion was not found in src\win\plugin.cpp" }
$version = $Matches[1]

$folder = "FreeWalk-$version"
if ($Name) { $folder += "-$Name" }
$target = Join-Path $root "dist\$folder"
New-Item -ItemType Directory -Force $target | Out-Null

$files = @(
  @("build\msvc-Release\FreeWalk.dll", "FreeWalk.dll"),
  @("FreeWalk.ini", "FreeWalk.ini"),
  @("docs\release\README.md", "README.md"),
  @("LICENSE", "LICENSE"),
  @("THIRD-PARTY-NOTICES.txt", "THIRD-PARTY-NOTICES.txt")
)
foreach ($f in $files) {
  $source = Join-Path $root $f[0]
  if (-not (Test-Path $source)) { throw "missing: $($f[0]) (run build.cmd first)" }
  Copy-Item -Force $source (Join-Path $target $f[1])
}
Write-Output "package: dist\$folder"
if ($Zip) {
  $archive = Join-Path $root "dist\$folder.zip"
  if (Test-Path $archive) { Remove-Item -Force $archive }
  Compress-Archive -Path (Join-Path $target "*") -DestinationPath $archive
  Write-Output "archive: dist\$folder.zip"
}
