[CmdletBinding()]
param(
    [string] $PackageVersion = "1.0.2.0",

    [string] $SourceDirectory,

    [string] $OutputDirectory
)

$ErrorActionPreference = "Stop"

$projectDirectory = Split-Path -Parent $PSScriptRoot
$miaiaDirectory = [System.IO.Path]::GetFullPath(
    (Join-Path $projectDirectory "..\.."))
$storeSourceDirectory = Join-Path $PSScriptRoot "Store"
$manifestTemplate = Join-Path $storeSourceDirectory "AppxManifest.template.xml"
$assetSourceDirectory = Join-Path $storeSourceDirectory "Assets"

$versionParts = $PackageVersion.Split(".")

if ($versionParts.Count -ne 4)
{
    throw "PackageVersion must contain four numeric parts, for example 1.0.0.0."
}

$numericVersionParts = @()

foreach ($versionPart in $versionParts)
{
    [uint16] $numericVersionPart = 0

    if (-not [uint16]::TryParse($versionPart, [ref] $numericVersionPart))
    {
        throw "Every PackageVersion part must be an integer from 0 through 65535."
    }

    $numericVersionParts += $numericVersionPart
}

if ($numericVersionParts[0] -eq 0)
{
    throw "The first PackageVersion part cannot be zero for a Microsoft Store package."
}

if ($numericVersionParts[3] -ne 0)
{
    throw "The fourth PackageVersion part is reserved by Microsoft Store and must be zero."
}

if ([string]::IsNullOrWhiteSpace($SourceDirectory))
{
    $SourceDirectory = Join-Path $miaiaDirectory (
        "Artifacts\MiaIAStudio\Windows-Shipping")
}

$sourcePath = [System.IO.Path]::GetFullPath($SourceDirectory)

if (-not (Test-Path -LiteralPath $sourcePath -PathType Container))
{
    throw "Shipping archive not found: $sourcePath"
}

if ($sourcePath -match "(?i)development")
{
    throw "A Development archive cannot be submitted to Microsoft Store. Build a Shipping archive first."
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory))
{
    $OutputDirectory = Join-Path $miaiaDirectory (
        "Artifacts\MiaIAStudio\Store\{0}" -f $PackageVersion)
}

$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$sourcePathWithSeparator = $sourcePath.TrimEnd("\") + "\"
$outputPathWithSeparator = $outputPath.TrimEnd("\") + "\"

if ($outputPathWithSeparator.StartsWith(
        $sourcePathWithSeparator,
        [System.StringComparison]::OrdinalIgnoreCase))
{
    throw "OutputDirectory cannot be inside SourceDirectory."
}

if (Test-Path -LiteralPath $outputPath)
{
    $existingOutput = Get-ChildItem `
        -LiteralPath $outputPath `
        -Force `
        -ErrorAction Stop |
        Select-Object -First 1

    if ($null -ne $existingOutput)
    {
        throw "OutputDirectory must be empty: $outputPath"
    }
}

$applicationExecutableDirectory = Join-Path $sourcePath "IDE\Binaries\Win64"

if (-not (Test-Path -LiteralPath $applicationExecutableDirectory -PathType Container))
{
    throw "Shipping executable directory not found: $applicationExecutableDirectory"
}

$applicationExecutableCandidates = Get-ChildItem `
    -LiteralPath $applicationExecutableDirectory `
    -Filter "MiaIAStudio*.exe" `
    -File `
    -ErrorAction Stop
$applicationExecutable = $applicationExecutableCandidates |
    Where-Object { $_.Name -match "(?i)shipping\.exe$" } |
    Select-Object -First 1

if ($null -eq $applicationExecutable)
{
    $applicationExecutable = $applicationExecutableCandidates |
        Where-Object { $_.Name -ieq "MiaIAStudio.exe" } |
        Select-Object -First 1
}

if ($null -eq $applicationExecutable)
{
    throw "MiaIA Studio Shipping executable not found below $applicationExecutableDirectory."
}

$applicationExecutableRelativePath = (
    "IDE\Binaries\Win64\{0}" -f $applicationExecutable.Name)

$requiredSourceFiles = @(
    "Licenses\LICENSE",
    "Licenses\THIRD_PARTY_NOTICES.md",
    "NOTICES.txt"
)

foreach ($requiredSourceFile in $requiredSourceFiles)
{
    $requiredSourcePath = Join-Path $sourcePath $requiredSourceFile

    if (-not (Test-Path -LiteralPath $requiredSourcePath -PathType Leaf))
    {
        throw "Required Shipping file not found: $requiredSourcePath"
    }
}

if (-not (Test-Path -LiteralPath $manifestTemplate -PathType Leaf))
{
    throw "MSIX manifest template not found: $manifestTemplate"
}

if (-not (Test-Path -LiteralPath $assetSourceDirectory -PathType Container))
{
    throw "MSIX asset directory not found: $assetSourceDirectory"
}

$makeAppxCommand = Get-Command "makeappx.exe" -ErrorAction SilentlyContinue
$makeAppx = $null

if ($null -ne $makeAppxCommand)
{
    $makeAppx = $makeAppxCommand.Source
}
else
{
    $windowsKitsBin = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"

    if (Test-Path -LiteralPath $windowsKitsBin -PathType Container)
    {
        $makeAppx = Get-ChildItem `
            -Path (Join-Path $windowsKitsBin "*\x64\makeappx.exe") `
            -File `
            -ErrorAction SilentlyContinue |
            Sort-Object { [version] $_.Directory.Parent.Name } -Descending |
            Select-Object -First 1 -ExpandProperty FullName
    }
}

if ([string]::IsNullOrWhiteSpace($makeAppx))
{
    throw "MakeAppx.exe was not found. Install the Windows SDK before creating the Store package."
}

$layoutDirectory = Join-Path $outputPath "Layout"
$assetOutputDirectory = Join-Path $layoutDirectory "Assets"
$manifestOutput = Join-Path $layoutDirectory "AppxManifest.xml"
$msixOutput = Join-Path $outputPath (
    "MiaIAStudio_{0}_x64.msix" -f $PackageVersion)

New-Item -ItemType Directory -Path $layoutDirectory -Force | Out-Null

$excludedFiles = @(
    "MiaIAStudio.exe"
)

$excludedPrefixes = @(
    "Engine\Extras\Redist\",
    "IDE\Saved\"
)

$shippingFiles = Get-ChildItem `
    -LiteralPath $sourcePath `
    -File `
    -Recurse `
    -ErrorAction Stop

foreach ($shippingFile in $shippingFiles)
{
    $relativePath = $shippingFile.FullName.Substring($sourcePathWithSeparator.Length)
    $extension = $shippingFile.Extension.ToLowerInvariant()
    $excludeFile = $excludedFiles -contains $relativePath

    if ($relativePath -match "(?i)^Manifest_.*\.txt$")
    {
        $excludeFile = $true
    }

    if ($extension -in @(".pdb", ".debug", ".map", ".log"))
    {
        $excludeFile = $true
    }

    foreach ($excludedPrefix in $excludedPrefixes)
    {
        if ($relativePath.StartsWith(
                $excludedPrefix,
                [System.StringComparison]::OrdinalIgnoreCase))
        {
            $excludeFile = $true
            break
        }
    }

    if ($excludeFile)
    {
        continue
    }

    $destinationFile = Join-Path $layoutDirectory $relativePath
    $destinationDirectory = Split-Path -Parent $destinationFile
    New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    Copy-Item `
        -LiteralPath $shippingFile.FullName `
        -Destination $destinationFile `
        -Force
}

Copy-Item `
    -LiteralPath $assetSourceDirectory `
    -Destination $assetOutputDirectory `
    -Recurse `
    -Force

$manifestContent = [System.IO.File]::ReadAllText($manifestTemplate)

if (-not $manifestContent.Contains("{{PACKAGE_VERSION}}") -or
    -not $manifestContent.Contains("{{APPLICATION_EXECUTABLE}}"))
{
    throw "The MSIX manifest template does not contain every required replacement token."
}

$manifestContent = $manifestContent.Replace(
    "{{PACKAGE_VERSION}}",
    $PackageVersion)
$manifestContent = $manifestContent.Replace(
    "{{APPLICATION_EXECUTABLE}}",
    $applicationExecutableRelativePath)
$utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText(
    $manifestOutput,
    $manifestContent,
    $utf8WithoutBom)

Write-Host "Creating unsigned Microsoft Store package..."
Write-Host "Source:   $sourcePath"
Write-Host "Layout:   $layoutDirectory"
Write-Host "Manifest: $manifestOutput"
Write-Host "Executable: $applicationExecutableRelativePath"
Write-Host "Package:  $msixOutput"
Write-Host "MakeAppx: $makeAppx"

& $makeAppx pack /d $layoutDirectory /p $msixOutput /o

if ($LASTEXITCODE -ne 0)
{
    throw "MakeAppx failed with exit code $LASTEXITCODE."
}

$packageFile = Get-Item -LiteralPath $msixOutput
$packageHash = Get-FileHash -LiteralPath $msixOutput -Algorithm SHA256

Write-Host "Microsoft Store package completed successfully."
Write-Host "Upload file: $($packageFile.FullName)"
Write-Host ("Size:        {0:N2} MB" -f ($packageFile.Length / 1MB))
Write-Host "SHA-256:     $($packageHash.Hash)"
Write-Host "Signature:   intentionally omitted; Microsoft Store signs the certified package."
