param([string]$Target = "C:\dev\WinMainEx\x64\Release\WinMainEx.exe", [int]$CapMs = 1200)
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type @"
using System; using System.Runtime.InteropServices; using System.Collections.Generic; using System.Threading; using System.Text;
public class DD {
  [DllImport("d3d11.dll")] static extern int D3D11CreateDevice(IntPtr a,int dt,IntPtr sw,uint f,IntPtr fl,uint nfl,uint sdk,out IntPtr dev,out int ofl,out IntPtr ctx);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f,uint dx,uint dy,uint d,UIntPtr e);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb,IntPtr p);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassNameW(IntPtr h,StringBuilder s,int n);
  delegate bool EnumProc(IntPtr h,IntPtr p);
  static bool IsConsoleCls(string c){ return c=="ConsoleWindowClass"||c=="PseudoConsoleWindow"||c=="CASCADIA_HOSTING_WINDOW_CLASS"; }
  static HashSet<long> ConsoleWins(){ var set=new HashSet<long>(); EnumWindows((h,p)=>{ if(IsWindowVisible(h)){ var sb=new StringBuilder(64); GetClassNameW(h,sb,64); if(IsConsoleCls(sb.ToString())) set.Add(h.ToInt64()); } return true; },IntPtr.Zero); return set; }
  [StructLayout(LayoutKind.Sequential)] struct POINT{public int x,y;}
  [StructLayout(LayoutKind.Sequential)] struct FRAMEINFO{public long LastPresent;public long LastMouse;public uint Accum;public int Coalesced;public int Masked;public POINT pPos;public int pVisible;public uint TotalMeta;public uint PtrShapeBufSize;}
  [StructLayout(LayoutKind.Sequential)] struct SHAPEINFO{public uint Type;public uint Width;public uint Height;public uint Pitch;public POINT Hot;}
  [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int QI(IntPtr o,ref Guid i,out IntPtr p);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int P1(IntPtr o,out IntPtr p);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Enum(IntPtr o,uint i,out IntPtr p);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Dup(IntPtr o,IntPtr dev,out IntPtr d);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Acq(IntPtr o,uint t,out FRAMEINFO fi,out IntPtr r);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Shp(IntPtr o,uint sz,byte[] b,out uint req,out SHAPEINFO si);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate int Rel(IntPtr o);
  [UnmanagedFunctionPointer(CallingConvention.StdCall)] delegate uint Unk(IntPtr o);
  static IntPtr VT(IntPtr o,int s){ return Marshal.ReadIntPtr(Marshal.ReadIntPtr(o), s*IntPtr.Size); }
  static T D<T>(IntPtr o,int s){ return (T)(object)Marshal.GetDelegateForFunctionPointer(VT(o,s),typeof(T)); }
  static int BlueCount(byte[] b,uint w,uint h,uint pitch){ int c=0; for(int y=0;y<h;y++)for(int x=0;x<w;x++){ int o=(int)(y*pitch+x*4); if(o+3>=b.Length)continue; int B=b[o],G=b[o+1],R=b[o+2],A=b[o+3]; if(A>40 && B>140 && B>R+50 && B>G+25) c++; } return c; }
  public static string Run(int cx,int cy,int capMs){
    IntPtr dev,ctx; int fl;
    if(D3D11CreateDevice(IntPtr.Zero,1,IntPtr.Zero,0,IntPtr.Zero,0,7,out dev,out fl,out ctx)<0) return "FAIL_INIT";
    Guid gDev=new Guid("54ec77fa-1377-44e6-8c32-88fd5f44c84c"), gOut1=new Guid("00cddea8-939b-4b83-a340-a685226666cc");
    IntPtr dxgi; D<QI>(dev,0)(dev,ref gDev,out dxgi); IntPtr ad; D<P1>(dxgi,7)(dxgi,out ad);
    IntPtr outp; D<Enum>(ad,7)(ad,0,out outp); IntPtr out1; D<QI>(outp,0)(outp,ref gOut1,out out1);
    IntPtr dupl; if(D<Dup>(out1,22)(out1,dev,out dupl)<0) return "FAIL_DUP";
    var acq=D<Acq>(dupl,8); var shp=D<Shp>(dupl,11); var rel=D<Rel>(dupl,14);
    var log=new List<string>(); var hashes=new HashSet<long>(); byte[] buf=new byte[262144];
    bool busy=false; long clickMs=0; var sw=System.Diagnostics.Stopwatch.StartNew(); bool clicked=false;
    var baseCon=ConsoleWins(); bool conhost=false; long conhostMs=0;
    while(sw.ElapsedMilliseconds<capMs){
      if(!clicked && sw.ElapsedMilliseconds>120){ clickMs=sw.ElapsedMilliseconds; SetCursorPos(cx,cy); Thread.Sleep(15);
        mouse_event(2,0,0,0,UIntPtr.Zero);mouse_event(4,0,0,0,UIntPtr.Zero);Thread.Sleep(45);
        mouse_event(2,0,0,0,UIntPtr.Zero);mouse_event(4,0,0,0,UIntPtr.Zero); clicked=true; }
      if(clicked && !conhost){ foreach(var w in ConsoleWins()){ if(!baseCon.Contains(w)){ conhost=true; conhostMs=sw.ElapsedMilliseconds; break; } } }
      FRAMEINFO fi; IntPtr res; if(acq(dupl,4,out fi,out res)==0){
        if(fi.PtrShapeBufSize>0){ uint req; SHAPEINFO si; if(shp(dupl,(uint)buf.Length,buf,out req,out si)==0){
          long hsh=1469598103934665603L; for(uint i=0;i<req&&i<buf.Length;i++) hsh=(hsh^buf[i])*1099511628211L;
          if(hashes.Add(hsh)){ int bc=(si.Type==2)?BlueCount(buf,si.Width,si.Height,si.Pitch):0;
            bool isBusy=bc>40; if(isBusy && clickMs>0 && sw.ElapsedMilliseconds>=clickMs) busy=true;
            log.Add(sw.ElapsedMilliseconds+"ms type="+si.Type+" "+si.Width+"x"+si.Height+" blue="+bc+(isBusy?" <== BUSY/SPINNER":"")); } } }
        D<Unk>(res,2)(res); rel(dupl); }
    }
    string cur=busy?"FAIL: wait cursor elicited":"PASS: no wait cursor";
    string con=conhost?("FAIL: console window appeared @"+conhostMs+"ms"):"PASS: no console window";
    string verdict=(!busy && !conhost)?"OVERALL: PASS":"OVERALL: FAIL";
    return verdict+"\n  "+cur+"\n  "+con+"\n"+string.Join("\n",log.ToArray());
  }
}
"@
$shell = New-Object -ComObject Shell.Application
# fail-fast: select the file in a fresh explorer window
explorer.exe "/select,$Target"
$au=[System.Windows.Automation.AutomationElement]
$liCond = New-Object System.Windows.Automation.PropertyCondition($au::ControlTypeProperty,[System.Windows.Automation.ControlType]::ListItem)
$selCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.SelectionItemPattern]::IsSelectedProperty,$true)
$cond = New-Object System.Windows.Automation.AndCondition($liCond,$selCond)
$item=$null; $hwnd=$null; $t=[System.Diagnostics.Stopwatch]::StartNew()
while($t.ElapsedMilliseconds -lt 2000 -and -not $item){
  foreach($w in [System.Diagnostics.Process]::GetProcessesByName("explorer")){}  # noop
  $root=$au::RootElement
  $wins=$root.FindAll([System.Windows.Automation.TreeScope]::Children,(New-Object System.Windows.Automation.PropertyCondition($au::ClassNameProperty,"CabinetWClass")))
  foreach($w in $wins){ $f=$w.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$cond); if($f){ $item=$f; $hwnd=[IntPtr]$w.Current.NativeWindowHandle; break } }
  if(-not $item){ Start-Sleep -Milliseconds 40 }
}
if(-not $item){ Write-Output "HARNESS_FAIL: selected item not found"; return }
$r=$item.Current.BoundingRectangle; $cx=[int]($r.X+$r.Width/2); $cy=[int]($r.Y+$r.Height/2)
$result=[DD]::Run($cx,$cy,$CapMs)
# cleanup AFTER capture: close that explorer window + kill app
foreach($win in ($shell.Windows())){ try{ if($win.HWND -eq [int64]$hwnd){ $win.Quit() } }catch{} }
Get-Process WinMainEx,WinMainExApp,SystemUWPLauncher -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Write-Output "target: $Target  (click @ $cx,$cy)"
Write-Output $result
