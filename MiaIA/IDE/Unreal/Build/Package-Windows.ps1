[CmdletBinding()]
param(
    [ValidateSet("Development", "Shipping")]
    [string] $Configuration = "Development",

    [string] $EngineRoot = "D:\Epic Games\UE_5.8",

    [string] $OutputDirectory
)

$ErrorActionPreference = "Stop"

$projectDirectory = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectDirectory "IDE.uproject"
$runUat = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"

if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf))
{
    throw "Unreal project not found: $projectFile"
}

if (-not (Test-Path -LiteralPath $runUat -PathType Leaf))
{
    throw "RunUAT.bat not found below EngineRoot: $runUat"
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory))
{
    $miaiaDirectory = [System.IO.Path]::GetFullPath(
        (Join-Path $projectDirectory "..\.."))
    $OutputDirectory = Join-Path $miaiaDirectory (
        "Artifacts\MiaIAStudio\Windows-{0}" -f $Configuration)
}

$archiveDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $archiveDirectory -Force | Out-Null

$arguments = @(
    "BuildCookRun"
    "-project=$projectFile"
    "-target=MiaIAStudio"
    "-noP4"
    "-platform=Win64"
    "-clientconfig=$Configuration"
    "-build"
    "-cook"
    "-stage"
    "-pak"
    "-iostore"
    "-prereqs"
    "-archive"
    "-archivedirectory=$archiveDirectory"
    "-utf8output"
)

Write-Host "Packaging MiaIA Studio ($Configuration)..."
Write-Host "Engine:  $EngineRoot"
Write-Host "Project: $projectFile"
Write-Host "Output:  $archiveDirectory"

& $runUat @arguments

if ($LASTEXITCODE -ne 0)
{
    throw "MiaIA Studio packaging failed with exit code $LASTEXITCODE."
}

$executable = Get-ChildItem `
    -LiteralPath $archiveDirectory `
    -Filter "MiaIAStudio.exe" `
    -File `
    -Recurse `
    -ErrorAction SilentlyContinue |
    Sort-Object { $_.FullName.Length } |
    Select-Object -First 1

if ($null -eq $executable)
{
    throw "Packaging completed, but MiaIAStudio.exe was not found below $archiveDirectory."
}

$packageDirectory = Split-Path -Parent $executable.FullName
$repositoryDirectory = [System.IO.Path]::GetFullPath(
    (Join-Path $projectDirectory "..\..\.."))
$licenseOutputDirectory = Join-Path $packageDirectory "Licenses"
$licenseSourceDirectory = Join-Path $repositoryDirectory "LICENSES"

New-Item `
    -ItemType Directory `
    -Path $licenseOutputDirectory `
    -Force | Out-Null

$distributionDocuments = @(
    "LICENSE",
    "NOTICE",
    "LICENSING.md",
    "THIRD_PARTY_NOTICES.md",
    "TRADEMARKS.md",
    "EULA.txt"
)

foreach ($document in $distributionDocuments)
{
    $source = Join-Path $repositoryDirectory $document

    if (-not (Test-Path -LiteralPath $source -PathType Leaf))
    {
        throw "Required distribution document not found: $source"
    }

    Copy-Item `
        -LiteralPath $source `
        -Destination $licenseOutputDirectory `
        -Force
}

if (-not (Test-Path -LiteralPath $licenseSourceDirectory -PathType Container))
{
    throw "Third-party license directory not found: $licenseSourceDirectory"
}

Copy-Item `
    -LiteralPath $licenseSourceDirectory `
    -Destination $licenseOutputDirectory `
    -Recurse `
    -Force

Write-Host "MiaIA Studio package completed successfully."
Write-Host "Executable: $($executable.FullName)"
Write-Host "Licenses:   $licenseOutputDirectory"
