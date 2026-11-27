# NSUM docs checker
# Validates the bilingual docs tree before commit / release.
# Exit code: 0 = no errors (warnings allowed), 1 = errors found.

[CmdletBinding()]
param(
    [string]$DocsRoot = (Join-Path $PSScriptRoot '..\docs')
)

$ErrorActionPreference = 'Stop'
$docsRoot = [System.IO.Path]::GetFullPath($DocsRoot)
$locales = @('zh', 'en')
$errors = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]
$planned = New-Object System.Collections.Generic.List[string]
$slotIds = New-Object System.Collections.Generic.List[string]

function Add-Error([string]$msg) { $script:errors.Add($msg) }
function Add-Warning([string]$msg) { $script:warnings.Add($msg) }

function Read-FrontMatter([string]$path) {
    $text = [System.IO.File]::ReadAllText($path)
    if (-not $text.StartsWith('---')) { return $null }
    $end = $text.IndexOf("`n---", 4)
    if ($end -lt 0) { return $null }
    $block = $text.Substring(4, $end - 4)
    $map = @{}
    foreach ($line in ($block -split "`n")) {
        if ($line -match '^\s*([A-Za-z0-9_-]+)\s*:\s*(.*)$') {
            $key = $Matches[1]
            $value = $Matches[2].Trim()
            if ($value.StartsWith('[') -and $value.EndsWith(']')) {
                $value = $value.Trim('[', ']')
            }
            $map[$key] = $value.Trim('"').Trim("'")
        }
    }
    return $map
}

function Get-Slug([string]$heading) {
    $text = $heading.Trim().ToLowerInvariant()
    $text = $text -replace '<[^>]+>', ''
    $text = $text -replace '[`*_~]', ''
    $text = $text -replace '[^\p{L}\p{N}\s\-_]', ''
    $text = $text -replace '\s+', '-'
    return $text.Trim('-')
}

function Get-Headings([string]$path) {
    $slugs = New-Object System.Collections.Generic.HashSet[string]
    foreach ($line in [System.IO.File]::ReadAllLines($path)) {
        if ($line -match '^\s{0,3}#{1,6}\s+(.*)$') {
            [void]$slugs.Add((Get-Slug $Matches[1]))
        }
    }
    return $slugs
}

if (-not (Test-Path $docsRoot)) {
    Write-Host "docs root not found: $docsRoot" -ForegroundColor Red
    exit 1
}

$placeholderFile = Join-Path $docsRoot '_meta\placeholders.md'
$registeredSlots = @{}
if (Test-Path $placeholderFile) {
    foreach ($line in [System.IO.File]::ReadAllLines($placeholderFile)) {
        if ($line -match '`([A-Z0-9_]+)`\s*\|') { $registeredSlots[$Matches[1]] = $true }
    }
}

# ---------- locale parity ----------
$localeFiles = @{}
foreach ($lang in $locales) {
    $root = Join-Path $docsRoot $lang
    if (-not (Test-Path $root)) { Add-Error "missing locale directory: docs/$lang"; continue }
    $files = @{}
    Get-ChildItem $root -Recurse -File -Filter '*.md' | ForEach-Object {
        $rel = $_.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
        $files[$rel] = $_.FullName
    }
    $localeFiles[$lang] = $files
}
if ($localeFiles.ContainsKey('zh') -and $localeFiles.ContainsKey('en')) {
    foreach ($rel in $localeFiles['zh'].Keys) {
        if (-not $localeFiles['en'].ContainsKey($rel)) { Add-Error "en missing page: $rel" }
    }
    foreach ($rel in $localeFiles['en'].Keys) {
        if (-not $localeFiles['zh'].ContainsKey($rel)) { Add-Error "zh missing page: $rel" }
    }
}

# ---------- per-page checks ----------
foreach ($lang in $locales) {
    if (-not $localeFiles.ContainsKey($lang)) { continue }
    $root = Join-Path $docsRoot $lang
    foreach ($rel in $localeFiles[$lang].Keys) {
        $path = $localeFiles[$lang][$rel]
        $display = "docs/$lang/$rel"
        $text = [System.IO.File]::ReadAllText($path)

        $fm = Read-FrontMatter $path
        if ($null -eq $fm) {
            Add-Error "$display : missing front-matter"
        } else {
            foreach ($required in @('title')) {
                if (-not $fm.ContainsKey($required) -or [string]::IsNullOrWhiteSpace($fm[$required])) {
                    Add-Error "$display : front-matter missing '$required'"
                }
            }
            if (-not $fm.ContainsKey('status')) {
                Add-Warning "$display : front-matter missing 'status' (defaults to complete)"
            } elseif ($fm['status'] -eq 'planned') {
                $planned.Add($display)
            } elseif ($fm['status'] -ne 'complete') {
                Add-Error "$display : invalid status '$($fm['status'])'"
            }
            if ($fm.ContainsKey('lang') -and $fm['lang'] -ne $lang) {
                Add-Error "$display : front-matter lang '$($fm['lang'])' != '$lang'"
            }
        }

        # absolute local paths (flag only sensitive/internal roots; generic drive examples are allowed)
        if ($text -match '(?im)^.*(?:/Users/|C:/Users/|C:\\Users|file://).*$') {
            $bad = $Matches[0].Trim()
            if ($bad -notmatch 'https?://') { Add-Error "$display : absolute local path in page: $bad" }
        }

        # markdown links / images (ignore fenced code blocks)
        $scanText = [regex]::Replace($text, '(?ms)^```.*?^```', '')
        foreach ($m in [regex]::Matches($scanText, '!?\[[^\]]*\]\(([^)\s]+)(?:\s+"[^"]*")?\)')) {
            $target = $m.Groups[1].Value.Trim()
            if ($target -match '^(https?:|mailto:|tel:|#)') { continue }
            if ($target -match '^<') { $target = $target.Trim('<', '>') }
            $fragment = ''
            $hash = $target.IndexOf('#')
            if ($hash -ge 0) { $fragment = $target.Substring($hash + 1); $target = $target.Substring(0, $hash) }
            if ([string]::IsNullOrWhiteSpace($target)) { continue }
            $targetPath = [System.IO.Path]::GetFullPath((Join-Path (Split-Path $path) $target))
            if (-not (Test-Path $targetPath)) {
                Add-Error "$display : broken link/image -> $target"
                continue
            }
            if ($fragment -and $targetPath.EndsWith('.md')) {
                $heads = Get-Headings $targetPath
                if (-not $heads.Contains($fragment)) {
                    Add-Warning "$display : anchor #$fragment not found in $target"
                }
            }
        }

        # cross-section relative links are not allowed from packaged sections
        if (($rel.StartsWith('user/') -or $rel.StartsWith('pack-author/')) -and
            ($text -match '\]\((\.\./(developer|extensions)/)')) {
            Add-Error "$display : packaged section links to developer/extensions relatively; use an online URL"
        }

        # slot markers
        foreach ($m in [regex]::Matches($text, '<!--\s*slot:([A-Z0-9_]+)\s*-->')) {
            $id = $m.Groups[1].Value
            $slotIds.Add($id)
            if (-not $registeredSlots.ContainsKey($id)) {
                Add-Error "$display : unregistered placeholder slot '$id' (add it to docs/_meta/placeholders.md)"
            }
        }

        # mermaid fences balanced
        $fences = ([regex]::Matches($text, '```')).Count
        if ($fences % 2 -ne 0) { Add-Error "$display : unbalanced code fences" }
    }
}

# ---------- _meta.json checks ----------
foreach ($lang in $locales) {
    $root = Join-Path $docsRoot $lang
    if (-not (Test-Path $root)) { continue }
    Get-ChildItem $root -Recurse -File -Filter '_meta.json' | ForEach-Object {
        $dir = $_.Directory.FullName
        $rel = $dir.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
        try {
            $meta = [System.IO.File]::ReadAllText($_.FullName) | ConvertFrom-Json
        } catch {
            Add-Error "docs/$lang/$rel/_meta.json : invalid JSON"
            return
        }
        $keys = @($meta.PSObject.Properties.Name)
        if ($keys.Count -eq 0) { Add-Warning "docs/$lang/$rel/_meta.json : empty" }
        foreach ($key in $keys) {
            $targetMd = Join-Path $dir ($key + '.md')
            $targetDir = Join-Path $dir $key
            if (-not (Test-Path $targetMd) -and -not (Test-Path $targetDir)) {
                Add-Error "docs/$lang/$rel/_meta.json : entry '$key' has no page/dir"
            }
        }
        Get-ChildItem $dir -File -Filter '*.md' | ForEach-Object {
            $name = $_.BaseName
            if ($name -eq 'index') { return }
            if (-not $keys.Contains($name)) {
                Add-Warning "docs/$lang/$rel/$name.md : not listed in _meta.json"
            }
        }
        Get-ChildItem $dir -Directory | ForEach-Object {
            if (-not $keys.Contains($_.Name)) {
                Add-Warning "docs/$lang/$rel/$($_.Name)/ : not listed in _meta.json"
            }
        }
    }
}

# ---------- report ----------
$slotCounts = $slotIds | Group-Object | Sort-Object Name
Write-Host ''
Write-Host "==== docs_check ====" -ForegroundColor Cyan
Write-Host ("pages: " + (($localeFiles.Values | ForEach-Object { $_.Count }) -join ' / ') + " (zh / en)")
Write-Host ("planned pages: {0}" -f $planned.Count) -ForegroundColor Yellow
Write-Host ("placeholder slots: {0}" -f $slotIds.Count) -ForegroundColor Yellow
foreach ($g in $slotCounts) { Write-Host ("  slot {0}: {1}" -f $g.Name, $g.Count) }

if ($warnings.Count -gt 0) {
    Write-Host ''
    Write-Host ("warnings ({0}):" -f $warnings.Count) -ForegroundColor Yellow
    $warnings | Select-Object -First 80 | ForEach-Object { Write-Host "  WARN  $_" }
}
if ($errors.Count -gt 0) {
    Write-Host ''
    Write-Host ("errors ({0}):" -f $errors.Count) -ForegroundColor Red
    $errors | Select-Object -First 200 | ForEach-Object { Write-Host "  ERROR $_" }
    Write-Host ''
    Write-Host "docs_check FAILED" -ForegroundColor Red
    exit 1
}
Write-Host ''
Write-Host "docs_check OK" -ForegroundColor Green
exit 0
