# NSUM docs packaging
# Builds the deployable docs subset from the bilingual, versioned source tree.
#
# Source layout: docs/<lang>/<version>/{user,pack-author,developer,extensions,releases}
#
# Profiles:
#   runtime     -> user only
#   with-editor -> user + pack-author (pack-author nested under _editor, promoted by NeoWorkspaceEditor on first run)
#   full        -> all four sections (docs site / local preview)
#
# By default the latest version (first entry of docs/zh/_meta.json) is flattened
# to docs/<lang>/... so the in-app help paths stay stable. Use -AllVersions with
# -Profile full to publish every version directory unchanged.
#
# Example:
#   pwsh -NoProfile -File tools/build_docs.ps1 -Profile with-editor -Out K:\tmp\nsum-docs

[CmdletBinding()]
param(
    [ValidateSet('runtime', 'with-editor', 'full')]
    [string]$Profile = 'with-editor',

    [string]$Out = (Join-Path $PSScriptRoot '..\build\docs_out'),

    [ValidateSet('all', 'zh', 'en')]
    [string]$Locale = 'all',

    [string]$Version = '',

    [switch]$AllVersions
)

$ErrorActionPreference = 'Stop'
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$docsRoot = Join-Path $repoRoot 'docs'
$outRoot = [System.IO.Path]::GetFullPath($Out)

if (-not (Test-Path $docsRoot)) { throw "docs source not found: $docsRoot" }
if (Test-Path $outRoot) { Remove-Item $outRoot -Recurse -Force }
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null

$locales = if ($Locale -eq 'all') { @('zh', 'en') } else { @($Locale) }

if (-not $Version) {
    $rootMeta = Get-Content (Join-Path $docsRoot 'zh\_meta.json') -Raw | ConvertFrom-Json
    $Version = @($rootMeta.PSObject.Properties.Name | Where-Object { $_ -ne 'index' })[0]
}
if (-not $Version) { throw 'cannot determine docs version' }

function Copy-Tree([string]$source, [string]$target) {
    if (-not (Test-Path $source)) { return }
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Copy-Item (Join-Path $source '*') $target -Recurse -Force
}

function Write-FilteredMeta([string]$source, [string]$target, [string[]]$keep) {
    if (-not (Test-Path $source)) { return }
    $meta = Get-Content $source -Raw | ConvertFrom-Json
    $filtered = [ordered]@{}
    foreach ($key in @($meta.PSObject.Properties.Name)) {
        if ($keep -contains $key) { $filtered[$key] = $meta.$key }
    }
    $json = $filtered | ConvertTo-Json
    [System.IO.File]::WriteAllText($target, $json + "`n", (New-Object System.Text.UTF8Encoding($false)))
}

# Flattening removes the <version> path segment. Links between version pages keep
# the same relative depth, but links to shared assets/_meta lose one leading "../".
function Flatten-Links([string]$dir) {
    if (-not (Test-Path $dir)) { return }
    Get-ChildItem $dir -Recurse -File -Filter '*.md' | ForEach-Object {
        $text = [System.IO.File]::ReadAllText($_.FullName)
        $text = [regex]::Replace($text, '\]\((\.\./[^)\s]*)\)', {
            param($m)
            $target = $m.Groups[1].Value
            if ($target -match '^(\.\./)+(assets|_meta)/') {
                return '](' + $target.Substring(3) + ')'
            }
            return $m.Value
        })
        [System.IO.File]::WriteAllText($_.FullName, $text,
            (New-Object System.Text.UTF8Encoding($false)))
    }
}

# shared assets
Copy-Tree (Join-Path $docsRoot 'assets') (Join-Path $outRoot 'assets')

$publishedSections = @('user')
if ($Profile -eq 'with-editor') { $publishedSections += 'pack-author' }
if ($Profile -eq 'full') {
    $publishedSections = @('user', 'pack-author', 'developer', 'extensions')
}

$editorMetaLabels = @{
    zh = [ordered]@{ index = '首页'; user = '用户指南'; 'pack-author' = '整合包作者' }
    en = [ordered]@{ index = 'Home'; user = 'User Guide'; 'pack-author' = 'Pack Authors' }
}

foreach ($lang in $locales) {
    $langRoot = Join-Path $docsRoot $lang
    if (-not (Test-Path $langRoot)) { throw "locale not found: $langRoot" }
    $dst = Join-Path $outRoot $lang
    New-Item -ItemType Directory -Force -Path $dst | Out-Null

    if ($Profile -eq 'full' -and $AllVersions) {
        Copy-Tree $langRoot $dst
        continue
    }

    $src = Join-Path $langRoot $Version
    if (-not (Test-Path $src)) { throw "docs version not found: $src" }

    Copy-Item (Join-Path $src 'index.md') $dst -Force

    if ($Profile -eq 'full') {
        foreach ($section in $publishedSections) {
            Copy-Tree (Join-Path $src $section) (Join-Path $dst $section)
        }
        if (Test-Path (Join-Path $src 'releases')) {
            Copy-Tree (Join-Path $src 'releases') (Join-Path $dst 'releases')
        }
        Copy-Item (Join-Path $src '_meta.json') (Join-Path $dst '_meta.json') -Force
    } else {
        Copy-Tree (Join-Path $src 'user') (Join-Path $dst 'user')
        Write-FilteredMeta (Join-Path $src '_meta.json') (Join-Path $dst '_meta.json') @('index', 'user')
        if ($Profile -eq 'with-editor') {
            $editorLang = Join-Path $outRoot '_editor' $lang
            Copy-Tree (Join-Path $src 'pack-author') (Join-Path $editorLang 'pack-author')
            Flatten-Links $editorLang
            $metaPath = Join-Path $outRoot ('_editor\_meta.' + $lang + '.json')
            New-Item -ItemType Directory -Force -Path (Split-Path $metaPath) | Out-Null
            [System.IO.File]::WriteAllText($metaPath,
                (($editorMetaLabels[$lang] | ConvertTo-Json) + "`n"),
                (New-Object System.Text.UTF8Encoding($false)))
        }
    }
    Flatten-Links $dst
}

if ($Profile -eq 'with-editor') {
    $readme = @"
# NSUM Editor documentation payload

This folder is promoted by NeoWorkspaceEditor.exe on first launch:

- copies _editor/<lang>/pack-author into docs/<lang>/pack-author
- replaces docs/<lang>/_meta.json with _editor/_meta.<lang>.json
- removes this _editor folder

Without the Editor installed this payload stays hidden and the in-app help only shows User Guide.
"@
    [System.IO.File]::WriteAllText((Join-Path $outRoot '_editor\README.md'), $readme,
        (New-Object System.Text.UTF8Encoding($false)))
}

$fileCount = (Get-ChildItem $outRoot -Recurse -File).Count
$size = [Math]::Round(((Get-ChildItem $outRoot -Recurse -File | Measure-Object Length -Sum).Sum / 1KB), 1)
Write-Host ("docs build OK: profile={0} locale={1} version={2} -> {3}" -f $Profile, $Locale, $Version, $outRoot) -ForegroundColor Green
Write-Host ("files={0} size={1}KB" -f $fileCount, $size)
