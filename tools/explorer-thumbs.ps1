# Asks Windows' shell for each file's Explorer thumbnail, the way Explorer does, thumbnail only (no icon fallback).
# One line per file: "<name>|ok|<ms>" or "<name>|none|<ms>|<hresult>". Called by tools/explorer-thumbs.sh.
param([Parameter(Mandatory)][string]$Folder)
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ShellThumb {
    [StructLayout(LayoutKind.Sequential)] struct SIZE { public int cx, cy; }
    [ComImport, Guid("bcc18b79-ba16-442f-80c4-8a59c30c463b"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IShellItemImageFactory { [PreserveSig] int GetImage(SIZE size, int flags, out IntPtr bitmap); }
    [DllImport("shell32.dll", CharSet = CharSet.Unicode, PreserveSig = false)]
    static extern void SHCreateItemFromParsingName(string path, IntPtr bc, [MarshalAs(UnmanagedType.LPStruct)] Guid riid,
        [MarshalAs(UnmanagedType.Interface)] out IShellItemImageFactory item);
    [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr o);
    // SIIGBF_THUMBNAILONLY (0x8): fails instead of handing back the file type's icon.
    public static int Get(string path) {
        IShellItemImageFactory f;
        SHCreateItemFromParsingName(path, IntPtr.Zero, typeof(IShellItemImageFactory).GUID, out f);
        IntPtr bmp;
        int hr = f.GetImage(new SIZE { cx = 256, cy = 256 }, 0x8, out bmp);
        if (hr == 0) DeleteObject(bmp);
        Marshal.ReleaseComObject(f);
        return hr;
    }
}
'@
Get-ChildItem -File $Folder | Sort-Object Name | ForEach-Object {
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $hr = [ShellThumb]::Get($_.FullName)
    $ms = $watch.ElapsedMilliseconds
    if ($hr -eq 0) { "$($_.Name)|ok|$ms" } else { "$($_.Name)|none|$ms|0x{0:X8}" -f $hr }
}
