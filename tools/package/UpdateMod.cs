using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;

static class UpdateMod
{
    const string OriginalHash = "08e0bf20134dfcb260699671004527b2d6bb1a45";
    const long MetaOffset = 0x3a0;
    const string MetaMagic = "RGMETA01";

    sealed class PatchInfo
    {
        public byte[] Hash;
        public int Length;
    }

    static uint BE(byte[] b, int i) {
        return ((uint)b[i] << 24) | ((uint)b[i + 1] << 16) | ((uint)b[i + 2] << 8) | b[i + 3];
    }

    static byte[] ReadAt(Stream s, long offset, int length) {
        s.Position = offset;
        byte[] data = new byte[length];
        int done = 0;
        while (done < length) {
            int n = s.Read(data, done, length - done);
            if (n == 0) throw new EndOfStreamException();
            done += n;
        }
        return data;
    }

    static void WriteBE(Stream s, long offset, uint n) {
        s.Position = offset;
        byte[] b = new byte[] { (byte)(n >> 24), (byte)(n >> 16), (byte)(n >> 8), (byte)n };
        s.Write(b, 0, b.Length);
    }

    static string Hex(byte[] bytes) {
        return BitConverter.ToString(bytes).Replace("-", "").ToLowerInvariant();
    }

    static byte[] HashRange(Stream stream, long offset, int length, bool sha1) {
        byte[] data = ReadAt(stream, offset, length);
        using (HashAlgorithm hash = sha1 ? (HashAlgorithm)SHA1.Create() : SHA256.Create())
            return hash.ComputeHash(data);
    }

    static int DolLength(Stream stream, long offset, long maxEnd) {
        if (offset < 0 || offset + 0x100 > maxEnd) return -1;
        byte[] h = ReadAt(stream, offset, 0x100);
        if (BE(h, 0) != 0x100) return -1;
        long extent = 0x100;
        int sections = 0;
        for (int i = 0; i < 18; i++) {
            uint fileOffset = BE(h, i * 4);
            uint size = BE(h, 0x90 + i * 4);
            if (size == 0) continue;
            sections++;
            if (fileOffset < 0x100 || size > 32000000) return -1;
            long end = (long)fileOffset + size;
            if (end > 32000000 || offset + end > maxEnd) return -1;
            if (end > extent) extent = end;
        }
        if (sections < 2 || extent > int.MaxValue) return -1;
        return (int)extent;
    }

    static bool TryMetadata(Stream stream, out long originalOffset, out int originalLength, out string installedVersion) {
        originalOffset = 0;
        originalLength = 0;
        installedVersion = "legacy";
        if (stream.Length < MetaOffset + 64) return false;
        byte[] data = ReadAt(stream, MetaOffset, 64);
        if (Encoding.ASCII.GetString(data, 0, 8) != MetaMagic) return false;
        originalOffset = BE(data, 8);
        originalLength = (int)BE(data, 12);
        int end = 16;
        while (end < data.Length && data[end] != 0) end++;
        if (end > 16) installedVersion = Encoding.ASCII.GetString(data, 16, end - 16);
        if (originalLength <= 0 || originalLength > 32000000 || originalOffset + originalLength > stream.Length)
            return false;
        return Hex(HashRange(stream, originalOffset, originalLength, true)) == OriginalHash;
    }

    static void WriteMetadata(Stream stream, long originalOffset, int originalLength, string version) {
        byte[] data = new byte[64];
        Encoding.ASCII.GetBytes(MetaMagic).CopyTo(data, 0);
        uint off = checked((uint)originalOffset);
        uint len = checked((uint)originalLength);
        data[8] = (byte)(off >> 24); data[9] = (byte)(off >> 16); data[10] = (byte)(off >> 8); data[11] = (byte)off;
        data[12] = (byte)(len >> 24); data[13] = (byte)(len >> 16); data[14] = (byte)(len >> 8); data[15] = (byte)len;
        byte[] v = Encoding.ASCII.GetBytes(version ?? "unknown");
        Array.Copy(v, 0, data, 16, Math.Min(v.Length, data.Length - 17));
        stream.Position = MetaOffset;
        stream.Write(data, 0, data.Length);
    }

    static long FindOriginalDol(Stream stream, uint fstOffset, out int length) {
        long knownOffset;
        string ignored;
        if (TryMetadata(stream, out knownOffset, out length, out ignored)) return knownOffset;

        long start = 0x2440;
        long stop = Math.Min((long)fstOffset, stream.Length);
        if (stop <= start || stop > 128L * 1024 * 1024)
            throw new InvalidDataException("The RogueMelee ISO has an invalid filesystem layout.");

        byte[] first = new byte[4];
        for (long pos = (start + 31) & ~31L; pos + 0x100 <= stop; pos += 32) {
            stream.Position = pos;
            if (stream.Read(first, 0, 4) != 4) break;
            if (BE(first, 0) != 0x100) continue;
            int candidateLength;
            try { candidateLength = DolLength(stream, pos, stop); }
            catch { continue; }
            if (candidateLength <= 0) continue;
            try {
                if (Hex(HashRange(stream, pos, candidateLength, true)) == OriginalHash) {
                    length = candidateLength;
                    return pos;
                }
            } catch { }
        }
        throw new InvalidDataException(
            "Could not find the clean US 1.02 main.dol inside this ISO. " +
            "Recreate RogueMelee.iso once with the newest Apply Mod.exe; future updates will be one-click.");
    }

    static PatchInfo ReadPatchInfo(string patchPath) {
        using (BinaryReader r = new BinaryReader(File.OpenRead(patchPath))) {
            if (Encoding.ASCII.GetString(r.ReadBytes(8)) != "ROGUED01")
                throw new InvalidDataException("rogue.delta is not a Rogue Melee patch.");
            byte[] hash = r.ReadBytes(32);
            if (hash.Length != 32) throw new InvalidDataException("rogue.delta is incomplete.");
            int length = r.ReadInt32();
            if (length <= 0 || length > 32000000) throw new InvalidDataException("rogue.delta has an invalid target size.");
            return new PatchInfo { Hash = hash, Length = length };
        }
    }

    static string Version() {
        string path = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "version.txt");
        if (!File.Exists(path)) return "latest";
        string text = File.ReadAllText(path).Trim();
        return text.Length == 0 ? "latest" : text;
    }

    static void WriteTitle(Stream stream, string version) {
        byte[] title = new byte[0x80];
        byte[] text = Encoding.ASCII.GetBytes("Rogue Melee " + version);
        Array.Copy(text, title, Math.Min(text.Length, title.Length - 1));
        stream.Position = 0x20;
        stream.Write(title, 0, title.Length);
    }

    static void Update(string image) {
        string patchPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "rogue.delta");
        if (!File.Exists(patchPath))
            throw new FileNotFoundException("rogue.delta is missing. Keep Update RogueMelee.exe and rogue.delta together.");
        PatchInfo patch = ReadPatchInfo(patchPath);
        string version = Version();

        string temp = Path.Combine(Path.GetTempPath(), "rogue-update-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);
        try {
            using (FileStream iso = new FileStream(image, FileMode.Open, FileAccess.ReadWrite, FileShare.None)) {
                byte[] header = ReadAt(iso, 0, 0x440);
                string discId = Encoding.ASCII.GetString(header, 0, 6);
                if (discId != "GRGE01")
                    throw new InvalidDataException("Select a RogueMelee.iso created by Apply Mod.exe, not the original Melee ISO.");

                uint fstOffset = BE(header, 0x424);
                int originalLength;
                long originalOffset = FindOriginalDol(iso, fstOffset, out originalLength);

                uint currentOffset = BE(header, 0x420);
                int currentLength = DolLength(iso, currentOffset, iso.Length);
                string installedVersion = "legacy";
                long metaOffset;
                int metaLength;
                TryMetadata(iso, out metaOffset, out metaLength, out installedVersion);

                if (currentLength == patch.Length &&
                    currentOffset + currentLength <= iso.Length &&
                    Hex(HashRange(iso, currentOffset, currentLength, false)) == Hex(patch.Hash)) {
                    byte[] originalHeader = ReadAt(iso, 0, 0x440);
                    try {
                        WriteMetadata(iso, originalOffset, originalLength, version);
                        WriteTitle(iso, version);
                        iso.Flush();
                    } catch {
                        iso.Position = 0;
                        iso.Write(originalHeader, 0, originalHeader.Length);
                        iso.Flush();
                        throw;
                    }
                    MessageBox.Show("This ISO is already up to date (" + version + ").", "Rogue Melee Updater");
                    return;
                }

                string clean = Path.Combine(temp, "original.dol");
                string modified = Path.Combine(temp, "latest.dol");
                iso.Position = originalOffset;
                using (FileStream output = File.Create(clean)) {
                    byte[] buffer = new byte[1024 * 1024];
                    int remaining = originalLength;
                    while (remaining > 0) {
                        int read = iso.Read(buffer, 0, Math.Min(buffer.Length, remaining));
                        if (read == 0) throw new EndOfStreamException("The ISO ended while reading the clean executable.");
                        output.Write(buffer, 0, read);
                        remaining -= read;
                    }
                }

                PatchCore.Patch(clean, patchPath, modified);
                byte[] latest = File.ReadAllBytes(modified);
                if (latest.Length != patch.Length ||
                    Hex(new SHA256Managed().ComputeHash(latest)) != Hex(patch.Hash))
                    throw new InvalidDataException("The newly generated mod executable failed verification.");

                long oldLength = iso.Length;
                byte[] oldHeader = ReadAt(iso, 0, 0x440);
                try {
                    long newOffset = (oldLength + 31) & ~31L;
                    if (newOffset > uint.MaxValue) throw new InvalidDataException("The ISO is too large to update.");
                    iso.Position = newOffset;
                    iso.Write(latest, 0, latest.Length);
                    iso.Flush();

                    WriteMetadata(iso, originalOffset, originalLength, version);
                    WriteTitle(iso, version);
                    WriteBE(iso, 0x420, (uint)newOffset); // Commit the update last.
                    iso.Flush();
                } catch {
                    iso.Position = 0;
                    iso.Write(oldHeader, 0, oldHeader.Length);
                    iso.SetLength(oldLength);
                    iso.Flush();
                    throw;
                }

                MessageBox.Show(
                    "Updated RogueMelee.iso successfully.\n\n" +
                    "From: " + installedVersion + "\nTo: " + version + "\n\n" +
                    "Your save/settings were not touched. You can launch the same ISO normally.",
                    "Rogue Melee Updater");
            }
        } finally {
            if (Directory.Exists(temp)) Directory.Delete(temp, true);
        }
    }

    [STAThread]
    static int Main(string[] args) {
        Application.EnableVisualStyles();
        try {
            string image = null;
            if (args.Length == 1) image = args[0];
            if (image == null) {
                using (OpenFileDialog picker = new OpenFileDialog {
                    Title = "Select your existing RogueMelee.iso",
                    Filter = "Rogue Melee ISO|*.iso;*.gcm",
                    CheckFileExists = true
                }) {
                    if (picker.ShowDialog() != DialogResult.OK) return 0;
                    image = picker.FileName;
                }
            }
            Update(image);
            return 0;
        } catch (Exception e) {
            MessageBox.Show(e.Message, "Rogue Melee update failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}
