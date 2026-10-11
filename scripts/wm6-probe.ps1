param([string]$OutDir = "$PSScriptRoot\..\evidence\wm6-probe",[switch]$UsbInfoOnly)
$ErrorActionPreference='Stop'
if ([Environment]::Is64BitProcess) { throw 'Use SysWOW64 Windows PowerShell.' }
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$source=Get-Content -LiteralPath 'C:\Users\Joe\Documents\C#\Positron\scripts\device_gate.ps1' -Raw
$match=[regex]::Match($source,"(?s)\`$rapiSource = @'\r?\n(.*?)\r?\n'@")
if (!$match.Success) { throw 'RAPI source missing' }
Add-Type -TypeDefinition $match.Groups[1].Value
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
public static class WmRead {
 [DllImport("rapi.dll",CharSet=CharSet.Unicode)] static extern int CeRegOpenKeyEx(uint root,string sub,uint options,uint access,out IntPtr key);
 [DllImport("rapi.dll")] static extern int CeRegCloseKey(IntPtr key);
 [DllImport("rapi.dll",CharSet=CharSet.Unicode)] static extern int CeRegEnumKeyEx(IntPtr key,uint index,StringBuilder name,ref uint length,IntPtr reserved,IntPtr cls,IntPtr clslen,IntPtr time);
 [DllImport("rapi.dll",CharSet=CharSet.Unicode)] static extern int CeRegEnumValue(IntPtr key,uint index,StringBuilder name,ref uint length,IntPtr reserved,out uint type,byte[] data,ref uint size);
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] public struct Version { public uint size,major,minor,build,platform; [MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)] public string csd; }
 [DllImport("rapi.dll")] static extern bool CeGetVersionEx(ref Version v);
 public static Version GetVersion() { Version v=new Version(); v.size=(uint)Marshal.SizeOf(v); if(!CeGetVersionEx(ref v)) throw new Exception("CeGetVersionEx failed"); return v; }
 public static List<string> Reg(string path,int depth) { var output=new List<string>(); Walk(path,depth,output);return output; }
 static void Walk(string path,int depth,List<string> output) {
  if(path.Equals("Comm\\Security",StringComparison.OrdinalIgnoreCase)||path.StartsWith("Comm\\Security\\",StringComparison.OrdinalIgnoreCase))return;
  IntPtr key;int result=CeRegOpenKeyEx(0x80000002,path,0,0,out key); output.Add("[HKLM\\"+path+"] open="+result);if(result!=0)return;
  try {
   for(uint i=0;i<256;i++) { var n=new StringBuilder(512);uint len=512,type,size=65536;byte[] data=new byte[size];result=CeRegEnumValue(key,i,n,ref len,IntPtr.Zero,out type,data,ref size);if(result==259)break;if(result!=0){output.Add("enumValue error="+result);break;}
    string value= type==4 && size>=4 ? "0x"+BitConverter.ToUInt32(data,0).ToString("x8") : (type==1||type==2||type==7 ? Encoding.Unicode.GetString(data,0,(int)size).TrimEnd('\0').Replace("\0"," | ") : BitConverter.ToString(data,0,(int)Math.Min(size,128)));
    output.Add(n.ToString()+" ("+type+")="+value);
   }
   if(depth>0) for(uint i=0;i<256;i++){var n=new StringBuilder(512);uint len=512;result=CeRegEnumKeyEx(key,i,n,ref len,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero);if(result==259)break;if(result!=0){output.Add("enumKey error="+result);break;}Walk(path+"\\"+n,depth-1,output);}
  }finally{CeRegCloseKey(key);}
 }
}
'@
try {
 [PositronDeviceRapi]::Connect()
 if($UsbInfoOnly) {
  [PositronDeviceRapi]::TryCopyFileFromDevice('\USBDeviceInfo.txt',"$OutDir\USBDeviceInfo.txt")
  if(Test-Path "$OutDir\USBDeviceInfo.txt") { Get-Item "$OutDir\USBDeviceInfo.txt" | Select-Object Name,Length }
  return
 }
 [WmRead]::GetVersion() | ConvertTo-Json | Set-Content -LiteralPath "$OutDir\version.json" -Encoding UTF8
 $find=[PositronDeviceRapi].GetMethod('FindChildren',[Reflection.BindingFlags]'NonPublic,Static')
 $files=$find.Invoke($null,@('\Windows'))
 $rows=@($files | ForEach-Object { [pscustomobject]@{Name=$_.cFileName;Size=$_.nFileSizeLow;Attributes=('0x{0:x8}' -f $_.dwFileAttributes)} })
 $rows | ConvertTo-Json | Set-Content -LiteralPath "$OutDir\windows-files.json" -Encoding UTF8
 $dirs=New-Object 'System.Collections.Generic.List[string]'
 foreach($dir in @('\','\Program Files','\Storage Card','\Windows\AppMgr')) {
  try {
   $items=$find.Invoke($null,@($dir))
   foreach($item in $items) { $dirs.Add($dir+'\'+$item.cFileName) }
   foreach($item in $items) {
    if(($item.dwFileAttributes -band 16) -and $item.cFileName -match '(?i)zte|driver|gosuncn') {
     $sub=$dir.TrimEnd('\')+'\'+$item.cFileName
     foreach($child in $find.Invoke($null,@($sub))) { $dirs.Add($sub+'\'+$child.cFileName) }
    }
   }
  } catch { $dirs.Add($dir+' ERROR '+$_.Exception.Message) }
 }
 $dirs | Set-Content -LiteralPath "$OutDir\directories.txt" -Encoding UTF8
 $rows | Where-Object Name -Match '(?i)zte|rndis|usb|ndis|ppp|tapi|asyncmac|unimodem|tcpstk|winsock|ceconfig|notify' | Format-Table -AutoSize
 foreach($path in @('\Windows\ceconfig.h','\USBLog.txt','\USBDeviceInfo.txt')) {
  try { $ok=[PositronDeviceRapi]::TryCopyFileFromDevice($path,(Join-Path $OutDir ($path.Split('\')[-1]))); Write-Output "copy $path=$ok" } catch { Write-Output "copy $path error=$($_.Exception.Message)" }
 }
 $reg=New-Object 'System.Collections.Generic.List[string]'
 foreach($entry in @(@('Drivers\USB',5),@('Drivers\BuiltIn\OHCI',1),@('Drivers\Active',1),@('Comm',2),@('Software\ZTE',3),@('System\State',0))) {
  $reg.AddRange([WmRead]::Reg($entry[0],[int]$entry[1]))
 }
 $reg | Set-Content -LiteralPath "$OutDir\registry.txt" -Encoding UTF8
 Write-Output "EVIDENCE=$OutDir"
 Get-Content -LiteralPath "$OutDir\version.json"
} finally { [PositronDeviceRapi]::Disconnect() }
