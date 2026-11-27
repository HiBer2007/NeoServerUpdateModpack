# Captures top-level GUI windows of a process to PNG (PrintWindow with screen fallback).
param(
    [string]$Out = (Join-Path $PSScriptRoot '..\docs\assets\screenshots')
)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class WinShot {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    public delegate bool ChildProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, ChildProc cb, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }

    public static bool ClickButton(IntPtr parent, string text) {
        bool done = false;
        EnumChildWindows(parent, (h, l) => {
            var sb = new System.Text.StringBuilder(256); GetWindowText(h, sb, 256);
            if (sb.ToString().Trim() == text) { SendMessage(h, 0x00F5, IntPtr.Zero, IntPtr.Zero); done = true; return false; }
            return true;
        }, IntPtr.Zero);
        return done;
    }

    public static IntPtr FindBest(uint pid) {
        IntPtr best = IntPtr.Zero; int bestArea = 0;
        EnumWindows((h, l) => {
            uint p; GetWindowThreadProcessId(h, out p);
            if (p != pid || !IsWindowVisible(h)) return true;
            RECT r; GetWindowRect(h, out r);
            int area = (r.R - r.L) * (r.B - r.T);
            if (area > bestArea) { bestArea = area; best = h; }
            return true;
        }, IntPtr.Zero);
        return best;
    }
}
"@

New-Item -ItemType Directory -Force -Path $Out | Out-Null

function Shot($exe, $argList, $file, $wait = 8, $preWait = 0, $preClick = "") {
    $start = @{ FilePath = $exe; PassThru = $true }
    if ($argList) { $start['ArgumentList'] = $argList }
    try { $p = Start-Process @start } catch { Write-Host "start failed: $file - $($_.Exception.Message)"; return }
    Start-Sleep -Seconds $wait
    if ($p.HasExited) { Write-Host "exited early: $file"; return }
    $h = [WinShot]::FindBest([uint32]$p.Id)
    if ($h -eq [IntPtr]::Zero) { Write-Host "no visible window: $file"; $p.Kill(); return }
    [void][WinShot]::SetForegroundWindow($h)
    Start-Sleep -Milliseconds 700
    $rect = New-Object WinShot+RECT
    [void][WinShot]::GetWindowRect($h, [ref]$rect)
    $w = $rect.R - $rect.L; $hh = $rect.B - $rect.T
    if ($w -lt 100 -or $hh -lt 100) { Write-Host "window too small: $file"; $p.Kill(); return }
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    $ok = [WinShot]::PrintWindow($h, $hdc, 2)
    $g.ReleaseHdc($hdc)
    if (-not $ok) {
        $g.CopyFromScreen($rect.L, $rect.T, 0, 0, (New-Object System.Drawing.Size $w, $hh))
    }
    $path = Join-Path $Out $file
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Host "saved $file ($($w)x$($hh))"
    try { $p.CloseMainWindow() | Out-Null; Start-Sleep -Milliseconds 800; if (-not $p.HasExited) { $p.Kill() } } catch {}
    Start-Sleep -Milliseconds 500
}

$deploy = 'K:\NeoServerUpdateModpack\build\deploy'
Shot (Join-Path $deploy 'NeoServerUpdateModpack.exe') @() 'screen-main-wizard.png' 8
Shot (Join-Path $deploy 'NeoWorkspaceEditor.exe') @('K:\nsum_e2e\repo') 'screen-editor.png' 26
Shot 'K:\NeoServerUpdateModpack\build_nsum_installer\NSUM_Installer_1.0.0.exe' @() 'screen-installer.png' 8 4 'OK'
