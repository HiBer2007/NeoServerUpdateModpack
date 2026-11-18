# NSUM 双版本号管理（外部发布版本 / 内部构建版本）
#
# 规则（用户裁定 2026-11-17）：
#   - 外部版本 = 公开语义化版本 x.x.x，只在用户明确说「发布一版」时递增；
#   - 内部版本 = 内部构建版本 x.x.x，平时只递增它；
#   - 完整版本 = 外部.内部 = x.x.x.x.x.x（6 段）。
#
# 用法：
#   pwsh tools/bump_version.ps1 -Part external-patch     # 发布：1.0.1 -> 1.0.2
#   pwsh tools/bump_version.ps1 -Part external-minor     # 发布：1.1.0
#   pwsh tools/bump_version.ps1 -Part external-major     # 发布：2.0.0
#   pwsh tools/bump_version.ps1 -Part internal-patch     # 日常迭代：0.1.0 -> 0.1.1
#   pwsh tools/bump_version.ps1 -Part internal-minor     # 内部功能批次：0.2.0
#   pwsh tools/bump_version.ps1 -Part internal-major     # 内部大改：1.0.0
#   pwsh tools/bump_version.ps1 -Set external=1.2.3      # 直接指定
#   pwsh tools/bump_version.ps1 -Set internal=0.5.2
#   pwsh tools/bump_version.ps1 -Show                    # 只显示当前版本

param(
    [ValidateSet('external-major', 'external-minor', 'external-patch',
                 'internal-major', 'internal-minor', 'internal-patch')]
    [string]$Part,
    [string]$Set,
    [switch]$Show
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$file = Join-Path $root 'version.json'

function Read-Version {
    $json = Get-Content -LiteralPath $file -Raw | ConvertFrom-Json
    return @{ external = [string]$json.external; internal = [string]$json.internal }
}

function Parse-Part([string]$text, [string]$name) {
    if ($text -notmatch '^\d+\.\d+\.\d+$') {
        throw "$name 版本号必须是 x.x.x（当前：'$text'）"
    }
    return @([int]($text -split '\.')[0], [int]($text -split '\.')[1], [int]($text -split '\.')[2])
}

$version = Read-Version

if ($Show -or (-not $Part -and -not $Set)) {
    "[{0}]（内部版本号：{1}）完整版本：{0}.{1}" -f $version.external, $version.internal
    exit 0
}

if ($Set) {
    if ($Set -notmatch '^(external|internal)=(.+)$') {
        throw "-Set 语法：external=1.2.3 或 internal=0.5.2"
    }
    $which = $Matches[1]
    $value = $Matches[2]
    Parse-Part $value $which | Out-Null
    $version[$which] = $value
} elseif ($Part) {
    $which = ($Part -split '-')[0]
    $whichPart = ($Part -split '-')[1]
    $current = Parse-Part $version[$which] $which
    switch ($whichPart) {
        'major' { $current[0]++; $current[1] = 0; $current[2] = 0 }
        'minor' { $current[1]++; $current[2] = 0 }
        'patch' { $current[2]++ }
    }
    $version[$which] = "{0}.{1}.{2}" -f $current[0], $current[1], $current[2]
}

$json = [ordered]@{ external = $version.external; internal = $version.internal }
$json | ConvertTo-Json | Set-Content -LiteralPath $file -Encoding utf8

"[{0}]（内部版本号：{1}）完整版本：{0}.{1}" -f $version.external, $version.internal
