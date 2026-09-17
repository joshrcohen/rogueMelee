using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Security.Cryptography;
using System.Threading.Tasks;
using System.Windows.Forms;

static class LaunchMod
{
    static string Pick(string title,string filter) {
        using(OpenFileDialog d=new OpenFileDialog {Title=title,Filter=filter,CheckFileExists=true})
            return d.ShowDialog()==DialogResult.OK?d.FileName:null;
    }
    static string Q(string text) { return "\""+text+"\""; }
    public static void Prepare(string emulator,string iso) {
        if(!File.Exists(emulator))throw new IOException("Emulator executable not found.");
        using(FileStream stream=File.OpenRead(iso)) {
            byte[] header=new byte[8];
            if(stream.Read(header,0,8)!=8 || Encoding.ASCII.GetString(header,0,6)!="GRGE01")
                throw new IOException("Select the RogueMelee.iso produced by Apply Mod.exe, not the original Melee image.");
            using(BinaryReader patch=new BinaryReader(File.OpenRead(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"rogue.delta")))) {
                patch.BaseStream.Position=8;
                byte[] expected=patch.ReadBytes(32);int length=patch.ReadInt32();
                stream.Position=0x420;
                byte[] offset=new byte[4];
                if(stream.Read(offset,0,4)!=4 || length<=0 || length>32000000)throw new IOException("Invalid mod image or patch.");
                long start=((long)offset[0]<<24)|((long)offset[1]<<16)|((long)offset[2]<<8)|offset[3];
                if(start+length>stream.Length)throw new IOException("The ISO is incomplete. Recreate it from the original image.");
                stream.Position=start;byte[] dol=new byte[length];int done=0;
                while(done<length) { int n=stream.Read(dol,done,length-done);if(n==0)throw new IOException("Incomplete ISO.");done+=n; }
                using(SHA256 sha=SHA256.Create()) {
                    if(BitConverter.ToString(sha.ComputeHash(dol))!=BitConverter.ToString(expected))
                        throw new IOException("The ISO's mod code does not match this ZIP. Recreate it with the included Apply Mod.exe.");
                }
            }
        }
        string sys=Path.Combine(Path.GetDirectoryName(emulator),"Sys");
        if(!Directory.Exists(sys))throw new IOException("This folder has no Sys directory. Select the actual Slippi Dolphin.exe, not the Slippi Launcher application.");
        string settings=Path.Combine(sys,"GameSettings");
        Directory.CreateDirectory(settings);
        string source=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"GRGE01.ini");
        string dest=Path.Combine(settings,"GRGE01.ini");
        if(!File.Exists(source))throw new IOException("GRGE01.ini is missing. Extract all the files from the ZIP together.");
        // Do not replace a friend's customized configuration silently.
        if(File.Exists(dest) && File.ReadAllText(dest)!=File.ReadAllText(source))
            throw new IOException("A different GRGE01.ini already exists here:\n"+dest+"\nBack it up and replace it with the file from this ZIP, then retry.");
        if(!File.Exists(dest))File.Copy(source,dest);
    }
    [STAThread]
    static int Main(string[] args) {
        if(args.Length==3 && args[0]=="--check") {
            try { Prepare(args[1],args[2]);return 0; }catch { return 1; }
        }
        Application.EnableVisualStyles();
        try {
            string iso=Pick("Select the patched RogueMelee.iso","Rogue Melee ISO|*.iso;*.gcm");
            if(iso==null)return 0;
            string emulator=Pick("Select your actual Slippi Dolphin.exe","Slippi or Dolphin|Slippi Dolphin.exe;Dolphin.exe|Executable|*.exe");
            if(emulator==null)return 0;
            Prepare(emulator,iso);
            string log=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"Launch diagnostic.txt");
            var version=FileVersionInfo.GetVersionInfo(emulator);
            File.WriteAllText(log,"Rogue Melee launcher v2\r\nEmulator: "+emulator+"\r\nVersion: "+version.FileVersion+"\r\nISO: "+iso+"\r\nISO bytes: "+new FileInfo(iso).Length+"\r\nMod recognition file verified.\r\n");
            ProcessStartInfo start=new ProcessStartInfo(emulator,"-e "+Q(iso));
            start.UseShellExecute=false;start.WorkingDirectory=Path.GetDirectoryName(emulator);
            using(Process process=Process.Start(start)) {
                if(process.WaitForExit(10000)) {
                    File.AppendAllText(log,"Emulator exited within 10 seconds. Exit code: "+process.ExitCode+"\r\n");
                    MessageBox.Show("Slippi exited before startup could be confirmed.\n\nClose existing Slippi windows and try again. If it still closes, send the contents of Launch diagnostic.txt with the Slippi version.\n\n"+log,"Launch did not stay open",MessageBoxButtons.OK,MessageBoxIcon.Warning);
                    return 1;
                }
                File.AppendAllText(log,"Emulator process remained open for 10 seconds. This does not verify gameplay.\r\n");
            }
            return 0;
        } catch(Exception e) {
            MessageBox.Show(e.Message,"Rogue Melee launch failed",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;
        }
    }
}
