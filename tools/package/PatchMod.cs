using System;
using System.IO;
using System.Text;
using System.Windows.Forms;

static class PatchMod
{
    static uint BE(byte[] b, int i) { return ((uint)b[i]<<24)|((uint)b[i+1]<<16)|((uint)b[i+2]<<8)|b[i+3]; }
    static byte[] ReadAt(Stream s, long offset, int length) {
        s.Position=offset;
        byte[] data=new byte[length]; int done=0;
        while(done<length) { int n=s.Read(data,done,length-done); if(n==0) throw new InvalidDataException("Incomplete game image."); done+=n; }
        return data;
    }
    static void WriteBE(Stream s, long offset, uint n) {
        s.Position=offset; s.Write(new byte[]{(byte)(n>>24),(byte)(n>>16),(byte)(n>>8),(byte)n},0,4);
    }
    static void MakeISO(string image, string output) {
        if(File.Exists(output)) throw new IOException("Output already exists. Choose another name to preserve your previous copy.");
        string folder=Path.GetDirectoryName(Path.GetFullPath(output));
        string temp=Path.Combine(folder,"rogue-patch-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);
        try {
            using(FileStream input=File.OpenRead(image)) {
                byte[] header=ReadAt(input,0,0x440);
                if(BE(header,0x1c)!=0xc2339f3d || Encoding.ASCII.GetString(header,0,6)!="GALE01" || header[7]!=2)
                    throw new InvalidDataException("Choose an unmodified US 1.02 Melee ISO/GCM. For RVZ, use your emulator's Convert File option to make an ISO first.");
                uint dolOffset=BE(header,0x420);
                byte[] dolHeader=ReadAt(input,dolOffset,0x100);
                long length=0x100;
                for(int i=0;i<18;i++) {
                    uint size=BE(dolHeader,0x90+i*4);
                    if(size!=0) length=Math.Max(length,(long)BE(dolHeader,i*4)+size);
                }
                if(length>32000000 || dolOffset+length>input.Length) throw new InvalidDataException("Invalid executable bounds.");
                string original=Path.Combine(temp,"original.dol"), modified=Path.Combine(temp,"modified.dol");
                File.WriteAllBytes(original,ReadAt(input,dolOffset,(int)length));
                PatchCore.Patch(original,Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"rogue.delta"),modified);
                string staged=Path.Combine(temp,"RogueMelee.iso");
                using(FileStream dest=File.Create(staged)) {
                    Console.WriteLine("Creating RogueMelee.iso. Your original image is unchanged...");
                    input.Position=0;input.CopyTo(dest);
                    long newOffset=(dest.Length+31)&~31L;
                    if(newOffset>uint.MaxValue) throw new InvalidDataException("Image is too large.");
                    dest.Position=newOffset;
                    byte[] dol=File.ReadAllBytes(modified);dest.Write(dol,0,dol.Length);
                    WriteBE(dest,0x420,(uint)newOffset);
                    // A separate disc identity prevents GALE01's absolute-address
                    // Slippi/Gecko patches from targeting this relocated DOL.
                    dest.Position=0;byte[] id=Encoding.ASCII.GetBytes("GRGE01");dest.Write(id,0,id.Length);
                    dest.Position=7;dest.WriteByte(0);
                    byte[] title=new byte[0x3e0];
                    Encoding.ASCII.GetBytes("Rogue Melee Playtest").CopyTo(title,0);
                    dest.Position=0x20;dest.Write(title,0,title.Length);
                }
                File.Move(staged,output);
            }
        } finally {
            // Only this invocation's randomly named work directory is removed.
            if(Directory.Exists(temp)) Directory.Delete(temp,true);
        }
    }
    [STAThread]
    static int Main(string[] args) {
        bool headless=args.Length==2;
        try {
            Application.EnableVisualStyles();
            string image,output;
            if(headless) { image=args[0];output=args[1]; }
            else {
                using(OpenFileDialog picker=new OpenFileDialog { Title="Select US 1.02 Melee ISO", Filter="Melee ISO or GCM|*.iso;*.gcm",CheckFileExists=true }) {
                    if(picker.ShowDialog()!=DialogResult.OK)return 0; image=picker.FileName;
                }
                using(SaveFileDialog picker=new SaveFileDialog { Title="Save Rogue Melee ISO",Filter="GameCube ISO|*.iso",FileName="RogueMelee.iso",OverwritePrompt=true }) {
                    if(picker.ShowDialog()!=DialogResult.OK)return 0; output=picker.FileName;
                }
            }
            MakeISO(image,output);
            Console.WriteLine("Ready: "+output);
            if(!headless)MessageBox.Show("Ready! Open this ISO in Slippi Dolphin or Dolphin:\n\n"+output+"\n\nFor Slippi, first copy the included GRGE01.ini into its Sys/GameSettings folder. Then use File > Open for offline play.","Rogue Melee");
            return 0;
        } catch(Exception e) {
            Console.Error.WriteLine(e.Message);
            if(!headless)MessageBox.Show(e.Message,"Patch failed",MessageBoxButtons.OK,MessageBoxIcon.Error);
            return 1;
        }
    }
}
