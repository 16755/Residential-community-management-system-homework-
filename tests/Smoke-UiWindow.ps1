# Opens an isolated copy of the GUI, captures the actual D3D window, then closes it.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class UiSmokeNative {
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    private delegate bool EnumWindowsCallback(IntPtr window, IntPtr arg);
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsCallback callback, IntPtr arg);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassName(IntPtr window, System.Text.StringBuilder name, int size);
    public static IntPtr FindProcessWindow(int processId) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((window, arg) => {
            uint pid; GetWindowThreadProcessId(window, out pid);
            var name = new System.Text.StringBuilder(128);
            GetClassName(window, name, name.Capacity);
            if (pid == processId && name.ToString() == "CommunityManagementImGui") { found = window; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int command);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rect rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int height, uint flags);
}
'@
[void][UiSmokeNative]::SetThreadDpiAwarenessContext([IntPtr](-4))
$projectRoot = Split-Path -Parent $PSScriptRoot
$smokeRoot = Join-Path ([IO.Path]::GetTempPath()) ('community-ui-smoke-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $smokeRoot | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'x64\Debug\xiaoquguanlixit.exe') -Destination $smokeRoot
Copy-Item -LiteralPath (Join-Path $projectRoot 'data') -Destination $smokeRoot -Recurse
$process = Start-Process -FilePath (Join-Path $smokeRoot 'xiaoquguanlixit.exe') -WorkingDirectory $env:TEMP -WindowStyle Hidden -PassThru
$window = [IntPtr]::Zero
for ($i = 0; $i -lt 50 -and $window -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 100
    $process.Refresh()
    if ($process.HasExited) { throw "GUI exited during initialization: $($process.ExitCode)" }
    $window = [UiSmokeNative]::FindProcessWindow($process.Id)
}
if ($window -eq [IntPtr]::Zero) { throw 'GUI window was not created.' }
function Save-Window($name) {
    $rect = New-Object UiSmokeNative+Rect
    [void][UiSmokeNative]::GetWindowRect($window, [ref]$rect)
    $bitmap = New-Object Drawing.Bitmap ($rect.Right-$rect.Left), ($rect.Bottom-$rect.Top)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $dc = $graphics.GetHdc()
    try { $captured = [UiSmokeNative]::PrintWindow($window, $dc, 2) } finally { $graphics.ReleaseHdc($dc) }
    $path = Join-Path $smokeRoot $name
    try { $bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png) } finally { $graphics.Dispose(); $bitmap.Dispose() }
    if (-not $captured) { throw 'Window screenshot failed.' }
    Write-Output $path
}
try {
    [void][UiSmokeNative]::ShowWindow($window, 4)
    Start-Sleep -Milliseconds 800
    Save-Window 'login.png'
    # Keyboard-only login verifies that the real Win32 backend forwards text.
    [void][UiSmokeNative]::PostMessage($window, 7, [IntPtr]::Zero, [IntPtr]::Zero)
    function Key($code) {
        [void][UiSmokeNative]::PostMessage($window, 0x100, [IntPtr]$code, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 60
        [void][UiSmokeNative]::PostMessage($window, 0x101, [IntPtr]$code, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 100
    }
    Key 9
    foreach ($character in 'admin'.ToCharArray()) { [void][UiSmokeNative]::PostMessage($window, 0x102, [IntPtr][int]$character, [IntPtr]::Zero) }
    Start-Sleep -Milliseconds 150
    Key 9
    foreach ($character in 'admin123'.ToCharArray()) { [void][UiSmokeNative]::PostMessage($window, 0x102, [IntPtr][int]$character, [IntPtr]::Zero) }
    Start-Sleep -Milliseconds 150
    Key 13
    Start-Sleep -Milliseconds 500
    Save-Window 'workspace.png'
    [void][UiSmokeNative]::ShowWindow($window, 6)
    Start-Sleep -Milliseconds 150
    [void][UiSmokeNative]::ShowWindow($window, 9)
    [void][UiSmokeNative]::SetWindowPos($window, [IntPtr]::Zero, 0, 0, 1280, 800, 0x16)
    Start-Sleep -Milliseconds 300
    Save-Window 'resized.png'
} finally {
    [void][UiSmokeNative]::PostMessage($window, 0x10, [IntPtr]::Zero, [IntPtr]::Zero)
    if (-not $process.WaitForExit(5000)) { throw 'GUI did not close normally.' }
    if ($process.ExitCode -ne 0) { throw "GUI exited with code $($process.ExitCode)" }
}
Write-Output "PASS: native window, keyboard input, minimize/restore, resize, clean shutdown. Artifacts: $smokeRoot"
