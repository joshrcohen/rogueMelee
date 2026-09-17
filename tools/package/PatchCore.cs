using System;
using System.IO;
using System.IO.Compression;
using System.Diagnostics;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Drawing;

static class PatchCore
{
    const string OriginalHash = "08e0bf20134dfcb260699671004527b2d6bb1a45";
    static string Hash(byte[] bytes, bool sha1) {
        using (HashAlgorithm h = sha1 ? (HashAlgorithm)SHA1.Create() : SHA256.Create())
            return BitConverter.ToString(h.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
    }
    static string Quote(string s) { return "\"" + s + "\""; }
    static byte[] Exact(BinaryReader r, int n) {
        if (n < 0 || n > 32000000) throw new InvalidDataException("Invalid patch length.");
        byte[] data = r.ReadBytes(n);
        if (data.Length != n) throw new InvalidDataException("Incomplete patch.");
        return data;
    }
    public static void Patch(string original, string patch, string output) {
        byte[] source = File.ReadAllBytes(original);
        if (Hash(source, true) != OriginalHash)
            throw new InvalidDataException("This is not an unmodified US 1.02 Melee image. Please select the supported revision.");
        using (BinaryReader r = new BinaryReader(File.OpenRead(patch))) {
            if (Encoding.ASCII.GetString(Exact(r, 8)) != "ROGUED01") throw new InvalidDataException("Invalid patch.");
            string expected = BitConverter.ToString(Exact(r, 32)).Replace("-", "").ToLowerInvariant();
            int length = r.ReadInt32();
            if (length <= 0 || length > 32000000) throw new InvalidDataException("Invalid executable size.");
            using (MemoryStream result = new MemoryStream(length)) {
                while (true) {
                    byte op = r.ReadByte();
                    if (op == 255) break;
                    int offset = op == 1 ? r.ReadInt32() : 0;
                    int count = r.ReadInt32();
                    if (count <= 0 || count > length - result.Length) throw new InvalidDataException("Invalid patch operation.");
                    if (op == 0) { byte[] literal = Exact(r, count); result.Write(literal, 0, count); }
                    else if (op == 1 && offset >= 0 && offset <= source.Length - count) result.Write(source, offset, count);
                    else throw new InvalidDataException("Invalid patch operation.");
                }
                byte[] bytes = result.ToArray();
                if (r.BaseStream.Position != r.BaseStream.Length || bytes.Length != length || Hash(bytes, false) != expected)
                    throw new InvalidDataException("The mod patch failed its integrity check. Download the installer again.");
                File.WriteAllBytes(output, bytes);
            }
        }
    }
}
