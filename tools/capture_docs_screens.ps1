# Captures the documentation screenshot set from the local deploy build.
# Requires: build/deploy populated, K:\nsum_docs_demo_repo (demo workspace), installer exe present.

param(
    [string]$Out = (Join-Path $PSScriptRoot '..\docs\assets\screenshots')
)

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class DocShot {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }

    public static IntPtr Best(uint pid) {
        IntPtr best = IntPtr.Zero; int area = 0;
        EnumWindows((h, l) => {
            uint p; GetWindowThreadProcessId(h, out p);
            if (p != pid || !IsWindowVisible(h)) return true;
            RECT r; GetWindowRect(h, out r);
            int a = (r.R - r.L) * (r.B - r.T);
            if (a > area) { area = a; best = h; }
            return true;
        }, IntPtr.Zero);
        return best;
    }
}
"@

New-Item -ItemType Directory -Force -Path $Out | Out-Null
$ae = [System.Windows.Automation.AutomationElement]
$tsDesc = [System.Windows.Automation.TreeScope]::Descendants
$tsChildren = [System.Windows.Automation.TreeScope]::Children

function Save-Shot([IntPtr]$hwnd, [string]$name) {
    $r = New-Object DocShot+RECT
    [void][DocShot]::GetWindowRect($hwnd, [ref]$r)
    $w = $r.R - $r.L; $h = $r.B - $r.T
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $dc = $g.GetHdc()
    [void][DocShot]::PrintWindow($hwnd, $dc, 2)
    $g.ReleaseHdc($dc)
    $path = Join-Path $Out $name
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "saved $name (${w}x${h})"
}

function Find-Element($root, [string]$name) {
    $cond = New-Object System.Windows.Automation.PropertyCondition($ae::NameProperty, $name)
    return $root.FindFirst($tsDesc, $cond)
}

function Invoke-Element($root, [string]$name) {
    $e = Find-Element $root $name
    if (-not $e) { Write-Host "missing: $name"; return }
    try {
        $e.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    } catch {
        $rect = $e.Current.BoundingRectangle
        [void][DocShot]::SetCursorPos([int]($rect.X + $rect.Width / 2), [int]($rect.Y + $rect.Height / 2))
        Start-Sleep -Milliseconds 150
        [DocShot]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero)
        [DocShot]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
    }
    Start-Sleep -Milliseconds 700
}

function Select-Card($root, [string]$name) {
    $e = Find-Element $root $name
    if (-not $e) { Write-Host "missing card: $name"; return }
    $rect = $e.Current.BoundingRectangle
    [void][DocShot]::SetForegroundWindow($script:hwnd)
    [void][DocShot]::SetCursorPos([int]($rect.X + $rect.Width / 2), [int]($rect.Y + $rect.Height / 2))
    Start-Sleep -Milliseconds 200
    [DocShot]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero)
    [DocShot]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 700
}

function Set-Edits($root, [string[]]$values) {
    $cond = New-Object System.Windows.Automation.PropertyCondition($ae::ControlTypeProperty, ([System.Windows.Automation.ControlType]::Edit))
    $edits = $root.FindAll($tsDesc, $cond)
    for ($i = 0; $i -lt $edits.Count -and $i -lt $values.Count; $i++) {
        $edits[$i].GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).SetValue($values[$i])
    }
    Start-Sleep -Milliseconds 1500
}

# ---------------- main wizard ----------------
$exe = 'K:\NeoServerUpdateModpack\build\deploy\NeoServerUpdateModpack.exe'
$p = Start-Process $exe -PassThru
Start-Sleep -Seconds 7
$script:hwnd = [DocShot]::Best([uint32]$p.Id)
$root = $ae::FromHandle($script:hwnd)
Invoke-Element $root '打开本地仓库目录'
Set-Edits $root @('K:\nsum_docs_demo_repo')
Save-Shot $script:hwnd 'screen-main-repo.png'
Invoke-Element $root '下一步'
Start-Sleep -Seconds 3
Save-Shot $script:hwnd 'screen-main-branch.png'
Select-Card $root 'master (默认)'
Save-Shot $script:hwnd 'screen-main-branch-selected.png'
Invoke-Element $root '下一步'
Start-Sleep -Seconds 3
Save-Shot $script:hwnd 'screen-main-modpack.png'
Select-Card $root 'base'
Invoke-Element $root '下一步'
Start-Sleep -Seconds 3
Save-Shot $script:hwnd 'screen-main-export-type.png'
Select-Card $root 'nsum (.nsumpack)'
Invoke-Element $root '下一步'
Start-Sleep -Seconds 3
Set-Edits $root @('K:\nsum_docs_build\wizard_out')
Save-Shot $script:hwnd 'screen-main-export-dir.png'
Invoke-Element $root '下一步'
Start-Sleep -Seconds 3
Set-Edits $root @('NSUM Docs Demo', '1.0.0', 'NSUM', '文档截图示例', '')
Save-Shot $script:hwnd 'screen-main-extra.png'
Invoke-Element $root '下一步'
Start-Sleep -Seconds 3
Save-Shot $script:hwnd 'screen-main-checklist.png'
Invoke-Element $root '开始构建'
Start-Sleep -Milliseconds 1500
Save-Shot $script:hwnd 'screen-main-build1.png'
Start-Sleep -Milliseconds 1500
Save-Shot $script:hwnd 'screen-main-build2.png'
Start-Sleep -Milliseconds 1500
Save-Shot $script:hwnd 'screen-main-build3.png'
Start-Sleep -Seconds 6
Save-Shot $script:hwnd 'screen-main-done.png'
try { $p.Kill() } catch {}

# ---------------- editor ----------------
$p = Start-Process 'K:\NeoServerUpdateModpack\build\deploy\NeoWorkspaceEditor.exe' -ArgumentList 'K:\nsum_docs_demo_repo' -PassThru
Start-Sleep -Seconds 10
$hwnd2 = [DocShot]::Best([uint32]$p.Id)
$root2 = $ae::FromHandle($hwnd2)
Save-Shot $hwnd2 'screen-editor-settings.png'
Invoke-Element $root2 '分支管理'
Start-Sleep -Seconds 2
Save-Shot $hwnd2 'screen-editor-branches.png'
Invoke-Element $root2 '整合包内容'
Start-Sleep -Seconds 2
$rect = New-Object DocShot+RECT
[void][DocShot]::GetWindowRect($hwnd2, [ref]$rect)
[void][DocShot]::SetCursorPos($rect.L + 500, $rect.B - 120)
[DocShot]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero)
[DocShot]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 600
Save-Shot $hwnd2 'screen-editor-content.png'
try { $p.Kill() } catch {}

# ---------------- installer ----------------
$p = Start-Process 'K:\NeoServerUpdateModpack\build_nsum_installer\NSUM_Installer_1.0.0.exe' -PassThru
Start-Sleep -Seconds 6
$hwnd3 = [DocShot]::Best([uint32]$p.Id)
$root3 = $ae::FromHandle($hwnd3)
Invoke-Element $root3 'OK'
Start-Sleep -Seconds 3
$hwnd3 = [DocShot]::Best([uint32]$p.Id)
$root3 = $ae::FromHandle($hwnd3)
Save-Shot $hwnd3 'screen-installer-welcome.png'
Invoke-Element $root3 '下一步'
Start-Sleep -Seconds 2
$hwnd3 = [DocShot]::Best([uint32]$p.Id)
Save-Shot $hwnd3 'screen-installer-step2.png'
Invoke-Element $root3 '我接受许可协议'
Invoke-Element $root3 '下一步'
Start-Sleep -Seconds 2
$hwnd3 = [DocShot]::Best([uint32]$p.Id)
Save-Shot $hwnd3 'screen-installer-step3.png'
try { $p.Kill() } catch {}

Write-Host 'done'
