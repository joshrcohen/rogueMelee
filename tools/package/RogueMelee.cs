using System;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;

static class RogueMelee
{
    const string OriginalHash = "08e0bf20134dfcb260699671004527b2d6bb1a45";
    const long MetaOffset = 0x3a0;
    const string MetaMagic = "RGMETA01";
    const string LatestVersionUrl =
        "https://github.com/joshrcohen/rogueMelee/releases/latest/download/version.txt";
    const string ReleaseRoot =
        "https://github.com/joshrcohen/rogueMelee/releases/download/";

    sealed class PatchInfo
    {
        public byte[] Hash;
        public int Length;
    }

    sealed class Config
    {
        public string Iso;
        public string Emulator;
    }

    sealed class StatusForm : Form
    {
        readonly Label status;

        public StatusForm()
        {
            Text = "RogueMelee";
            Width = 430;
            Height = 120;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            status = new Label();
            status.AutoSize = false;
            status.Dock = DockStyle.Fill;
            status.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            status.Text = "Starting...";
            Controls.Add(status);
        }

        public void SetStatus(string text)
        {
            status.Text = text;
            Application.DoEvents();
        }
    }

    static string AppDir {
        get {
            string path = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                "RogueMelee");
            Directory.CreateDirectory(path);
            return path;
        }
    }

    static string ConfigPath {
        get { return Path.Combine(AppDir, "launcher.cfg"); }
    }

    static string Q(string text) { return "\"" + text + "\""; }

    static string Pick(string title, string filter)
    {
        using (OpenFileDialog d = new OpenFileDialog {
            Title = title,
            Filter = filter,
            CheckFileExists = true
        }) {
            return d.ShowDialog() == DialogResult.OK ? d.FileName : null;
        }
    }

    static string EncodePath(string value)
    {
        return Convert.ToBase64String(Encoding.UTF8.GetBytes(value ?? ""));
    }

    static string DecodePath(string value)
    {
        try { return Encoding.UTF8.GetString(Convert.FromBase64String(value)); }
        catch { return ""; }
    }

    static Config LoadConfig()
    {
        Config config = new Config();
        if (!File.Exists(ConfigPath)) return config;
        try {
            string[] lines = File.ReadAllLines(ConfigPath);
            if (lines.Length > 0) config.Iso = DecodePath(lines[0]);
            if (lines.Length > 1) config.Emulator = DecodePath(lines[1]);
        } catch { }
        return config;
    }

    static void SaveConfig(Config config)
    {
        File.WriteAllLines(ConfigPath, new string[] {
            EncodePath(config.Iso), EncodePath(config.Emulator)
        });
    }

    static bool IsRogueImage(string path)
    {
        if (String.IsNullOrEmpty(path) || !File.Exists(path)) return false;
        try {
            using (FileStream stream = File.OpenRead(path)) {
                byte[] header = new byte[6];
                return stream.Read(header, 0, 6) == 6 &&
                    Encoding.ASCII.GetString(header) == "GRGE01";
            }
        } catch { return false; }
    }

    static void EnsureConfig(Config config)
    {
        if (!IsRogueImage(config.Iso)) {
            string beside = Path.Combine(
                AppDomain.CurrentDomain.BaseDirectory, "RogueMelee.iso");
            if (IsRogueImage(beside)) config.Iso = beside;
            else config.Iso = Pick(
                "Select your RogueMelee.iso",
                "Rogue Melee ISO|*.iso;*.gcm");
            if (String.IsNullOrEmpty(config.Iso))
                throw new OperationCanceledException();
        }

        if (String.IsNullOrEmpty(config.Emulator) ||
            !File.Exists(config.Emulator)) {
            config.Emulator = Pick(
                "Select your actual Slippi Dolphin.exe",
                "Slippi or Dolphin|Slippi Dolphin.exe;Dolphin.exe|Executable|*.exe");
            if (String.IsNullOrEmpty(config.Emulator))
                throw new OperationCanceledException();
        }
        SaveConfig(config);
    }

    static WebClient NewWebClient()
    {
        ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072;
        WebClient client = new WebClient();
        client.Headers.Add("User-Agent", "RogueMelee-Launcher");
        return client;
    }

    static string DownloadLatestVersion()
    {
        using (WebClient client = NewWebClient()) {
            string version = client.DownloadString(LatestVersionUrl).Trim();
            if (version.Length == 0 || version.Length > 80)
                throw new InvalidDataException("GitHub returned an invalid RogueMelee version.");
            return version;
        }
    }

    static string SafeVersion(string version)
    {
        foreach (char c in Path.GetInvalidFileNameChars())
            version = version.Replace(c, '_');
        return version;
    }

    static string AssetUrl(string version, string name)
    {
        string tag = "build-" + version;
        return ReleaseRoot + Uri.EscapeDataString(tag) + "/" +
            Uri.EscapeDataString(name);
    }

    static string EnsureAsset(string version, string name)
    {
        string folder = Path.Combine(AppDir, "cache", SafeVersion(version));
        Directory.CreateDirectory(folder);
        string destination = Path.Combine(folder, name);
        if (File.Exists(destination) && new FileInfo(destination).Length > 0)
            return destination;

        string temp = destination + ".download-" + Guid.NewGuid().ToString("N");
        try {
            using (WebClient client = NewWebClient())
                client.DownloadFile(AssetUrl(version, name), temp);
            if (!File.Exists(temp) || new FileInfo(temp).Length == 0)
                throw new IOException("Downloaded " + name + " is empty.");
            if (File.Exists(destination)) File.Delete(destination);
            File.Move(temp, destination);
            return destination;
        } finally {
            if (File.Exists(temp)) File.Delete(temp);
        }
    }

    static string CachedOrLocalAsset(string version, string name)
    {
        if (!String.IsNullOrEmpty(version) && version != "legacy") {
            string cached = Path.Combine(
                AppDir, "cache", SafeVersion(version), name);
            if (File.Exists(cached)) return cached;
        }
        string local = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, name);
        return File.Exists(local) ? local : null;
    }

    static uint BE(byte[] b, int i)
    {
        return ((uint)b[i] << 24) | ((uint)b[i + 1] << 16) |
               ((uint)b[i + 2] << 8) | b[i + 3];
    }

    static byte[] ReadAt(Stream stream, long offset, int length)
    {
        stream.Position = offset;
        byte[] data = new byte[length];
        int done = 0;
        while (done < length) {
            int n = stream.Read(data, done, length - done);
            if (n == 0) throw new EndOfStreamException();
            done += n;
        }
        return data;
    }

    static void WriteBE(Stream stream, long offset, uint value)
    {
        stream.Position = offset;
        byte[] b = new byte[] {
            (byte)(value >> 24), (byte)(value >> 16),
            (byte)(value >> 8), (byte)value
        };
        stream.Write(b, 0, b.Length);
    }

    static string Hex(byte[] bytes)
    {
        return BitConverter.ToString(bytes).Replace("-", "").ToLowerInvariant();
    }

    static byte[] HashRange(Stream stream, long offset, int length, bool sha1)
    {
        byte[] data = ReadAt(stream, offset, length);
        using (HashAlgorithm hash =
               sha1 ? (HashAlgorithm)SHA1.Create() : SHA256.Create())
            return hash.ComputeHash(data);
    }

    static string FileSha256(string path)
    {
        using (FileStream input = File.OpenRead(path))
        using (SHA256 hash = SHA256.Create())
            return Hex(hash.ComputeHash(input));
    }

    static int DolLength(Stream stream, long offset, long maxEnd)
    {
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

    static bool TryMetadata(
        Stream stream, out long originalOffset, out int originalLength,
        out string installedVersion)
    {
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
        if (end > 16)
            installedVersion = Encoding.ASCII.GetString(data, 16, end - 16);
        if (originalLength <= 0 || originalLength > 32000000 ||
            originalOffset + originalLength > stream.Length)
            return false;
        return Hex(HashRange(stream, originalOffset, originalLength, true)) ==
            OriginalHash;
    }

    static void WriteMetadata(
        Stream stream, long originalOffset, int originalLength, string version)
    {
        byte[] data = new byte[64];
        Encoding.ASCII.GetBytes(MetaMagic).CopyTo(data, 0);
        uint off = checked((uint)originalOffset);
        uint len = checked((uint)originalLength);
        data[8] = (byte)(off >> 24);
        data[9] = (byte)(off >> 16);
        data[10] = (byte)(off >> 8);
        data[11] = (byte)off;
        data[12] = (byte)(len >> 24);
        data[13] = (byte)(len >> 16);
        data[14] = (byte)(len >> 8);
        data[15] = (byte)len;
        byte[] v = Encoding.ASCII.GetBytes(version ?? "unknown");
        Array.Copy(v, 0, data, 16, Math.Min(v.Length, data.Length - 17));
        stream.Position = MetaOffset;
        stream.Write(data, 0, data.Length);
    }

    static long FindOriginalDol(Stream stream, uint fstOffset, out int length)
    {
        long knownOffset;
        string ignored;
        if (TryMetadata(stream, out knownOffset, out length, out ignored))
            return knownOffset;

        long start = 0x2440;
        long stop = Math.Min((long)fstOffset, stream.Length);
        if (stop <= start || stop > 128L * 1024 * 1024)
            throw new InvalidDataException(
                "The RogueMelee ISO has an invalid filesystem layout.");

        byte[] first = new byte[4];
        for (long pos = (start + 31) & ~31L;
             pos + 0x100 <= stop; pos += 32) {
            stream.Position = pos;
            if (stream.Read(first, 0, 4) != 4) break;
            if (BE(first, 0) != 0x100) continue;
            int candidateLength;
            try { candidateLength = DolLength(stream, pos, stop); }
            catch { continue; }
            if (candidateLength <= 0) continue;
            try {
                if (Hex(HashRange(
                        stream, pos, candidateLength, true)) == OriginalHash) {
                    length = candidateLength;
                    return pos;
                }
            } catch { }
        }
        throw new InvalidDataException(
            "Could not find the clean US 1.02 main.dol inside this ISO. " +
            "Recreate RogueMelee.iso once with the newest Apply Mod.exe; " +
            "future updates will be automatic.");
    }

    static PatchInfo ReadPatchInfo(string patchPath)
    {
        using (BinaryReader r = new BinaryReader(File.OpenRead(patchPath))) {
            if (Encoding.ASCII.GetString(r.ReadBytes(8)) != "ROGUED01")
                throw new InvalidDataException(
                    "rogue.delta is not a RogueMelee patch.");
            byte[] hash = r.ReadBytes(32);
            if (hash.Length != 32)
                throw new InvalidDataException("rogue.delta is incomplete.");
            int length = r.ReadInt32();
            if (length <= 0 || length > 32000000)
                throw new InvalidDataException(
                    "rogue.delta has an invalid target size.");
            return new PatchInfo { Hash = hash, Length = length };
        }
    }

    static string InstalledVersion(string image)
    {
        using (FileStream iso = new FileStream(
               image, FileMode.Open, FileAccess.Read, FileShare.Read)) {
            byte[] header = ReadAt(iso, 0, 0x440);
            if (Encoding.ASCII.GetString(header, 0, 6) != "GRGE01")
                throw new InvalidDataException(
                    "Select RogueMelee.iso, not the original Melee ISO.");
            long originalOffset;
            int originalLength;
            string version;
            return TryMetadata(
                iso, out originalOffset, out originalLength, out version)
                ? version : "legacy";
        }
    }

    static void WriteTitle(Stream stream, string version)
    {
        byte[] title = new byte[0x80];
        byte[] text = Encoding.ASCII.GetBytes("Rogue Melee " + version);
        Array.Copy(text, title, Math.Min(text.Length, title.Length - 1));
        stream.Position = 0x20;
        stream.Write(title, 0, title.Length);
    }

    static bool ApplyUpdate(string image, string patchPath, string version)
    {
        PatchInfo patch = ReadPatchInfo(patchPath);
        string temp = Path.Combine(
            Path.GetTempPath(), "rogue-update-" +
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);
        try {
            using (FileStream iso = new FileStream(
                   image, FileMode.Open, FileAccess.ReadWrite, FileShare.None)) {
                byte[] header = ReadAt(iso, 0, 0x440);
                if (Encoding.ASCII.GetString(header, 0, 6) != "GRGE01")
                    throw new InvalidDataException(
                        "Select RogueMelee.iso, not the original Melee ISO.");

                uint fstOffset = BE(header, 0x424);
                int originalLength;
                long originalOffset =
                    FindOriginalDol(iso, fstOffset, out originalLength);

                uint currentOffset = BE(header, 0x420);
                int currentLength =
                    DolLength(iso, currentOffset, iso.Length);

                if (currentLength == patch.Length &&
                    currentOffset + currentLength <= iso.Length &&
                    Hex(HashRange(
                        iso, currentOffset, currentLength, false)) ==
                    Hex(patch.Hash)) {
                    byte[] originalHeader = ReadAt(iso, 0, 0x440);
                    try {
                        WriteMetadata(
                            iso, originalOffset, originalLength, version);
                        WriteTitle(iso, version);
                        iso.Flush();
                    } catch {
                        iso.Position = 0;
                        iso.Write(
                            originalHeader, 0, originalHeader.Length);
                        iso.Flush();
                        throw;
                    }
                    return false;
                }

                string clean = Path.Combine(temp, "original.dol");
                string modified = Path.Combine(temp, "latest.dol");
                iso.Position = originalOffset;
                using (FileStream output = File.Create(clean)) {
                    byte[] buffer = new byte[1024 * 1024];
                    int remaining = originalLength;
                    while (remaining > 0) {
                        int read = iso.Read(
                            buffer, 0, Math.Min(buffer.Length, remaining));
                        if (read == 0)
                            throw new EndOfStreamException(
                                "The ISO ended while reading the clean executable.");
                        output.Write(buffer, 0, read);
                        remaining -= read;
                    }
                }

                PatchCore.Patch(clean, patchPath, modified);
                byte[] latest = File.ReadAllBytes(modified);
                using (SHA256 hash = SHA256.Create()) {
                    if (latest.Length != patch.Length ||
                        Hex(hash.ComputeHash(latest)) != Hex(patch.Hash))
                        throw new InvalidDataException(
                            "The generated mod executable failed verification.");
                }

                long oldLength = iso.Length;
                byte[] oldHeader = ReadAt(iso, 0, 0x440);
                try {
                    long newOffset = (oldLength + 31) & ~31L;
                    if (newOffset > uint.MaxValue)
                        throw new InvalidDataException(
                            "The ISO is too large to update.");
                    iso.Position = newOffset;
                    iso.Write(latest, 0, latest.Length);
                    iso.Flush();

                    WriteMetadata(
                        iso, originalOffset, originalLength, version);
                    WriteTitle(iso, version);
                    WriteBE(iso, 0x420, (uint)newOffset);
                    iso.Flush();
                } catch {
                    iso.Position = 0;
                    iso.Write(oldHeader, 0, oldHeader.Length);
                    iso.SetLength(oldLength);
                    iso.Flush();
                    throw;
                }
                return true;
            }
        } finally {
            if (Directory.Exists(temp)) Directory.Delete(temp, true);
        }
    }

    static void PrepareLaunch(
        string emulator, string iso, string patchPath, string iniPath)
    {
        if (!File.Exists(emulator))
            throw new IOException("Emulator executable not found.");
        if (!File.Exists(patchPath))
            throw new IOException("rogue.delta is missing.");
        if (!File.Exists(iniPath))
            throw new IOException("GRGE01.ini is missing.");

        using (FileStream stream = File.OpenRead(iso)) {
            byte[] header = new byte[8];
            if (stream.Read(header, 0, 8) != 8 ||
                Encoding.ASCII.GetString(header, 0, 6) != "GRGE01")
                throw new IOException(
                    "Select RogueMelee.iso, not the original Melee image.");

            using (BinaryReader patch =
                   new BinaryReader(File.OpenRead(patchPath))) {
                patch.BaseStream.Position = 8;
                byte[] expected = patch.ReadBytes(32);
                int length = patch.ReadInt32();
                stream.Position = 0x420;
                byte[] offset = new byte[4];
                if (stream.Read(offset, 0, 4) != 4 ||
                    length <= 0 || length > 32000000)
                    throw new IOException("Invalid mod image or patch.");
                long start = ((long)offset[0] << 24) |
                             ((long)offset[1] << 16) |
                             ((long)offset[2] << 8) | offset[3];
                if (start + length > stream.Length)
                    throw new IOException(
                        "The ISO is incomplete. Recreate it from the original image.");
                stream.Position = start;
                byte[] dol = new byte[length];
                int done = 0;
                while (done < length) {
                    int n = stream.Read(dol, done, length - done);
                    if (n == 0) throw new IOException("Incomplete ISO.");
                    done += n;
                }
                using (SHA256 sha = SHA256.Create()) {
                    if (BitConverter.ToString(sha.ComputeHash(dol)) !=
                        BitConverter.ToString(expected))
                        throw new IOException(
                            "The ISO does not match the latest RogueMelee update.");
                }
            }
        }

        string sys = Path.Combine(Path.GetDirectoryName(emulator), "Sys");
        if (!Directory.Exists(sys))
            throw new IOException(
                "This folder has no Sys directory. Select the actual " +
                "Slippi Dolphin.exe, not Slippi Launcher.exe.");
        string settings = Path.Combine(sys, "GameSettings");
        Directory.CreateDirectory(settings);
        string dest = Path.Combine(settings, "GRGE01.ini");

        if (File.Exists(dest) &&
            File.ReadAllText(dest) != File.ReadAllText(iniPath)) {
            string backup = dest + ".before-rogue-autoupdate.bak";
            if (!File.Exists(backup)) File.Copy(dest, backup);
        }
        File.Copy(iniPath, dest, true);
    }

    static int Launch(string emulator, string iso)
    {
        string log = Path.Combine(AppDir, "Launch diagnostic.txt");
        FileVersionInfo version = FileVersionInfo.GetVersionInfo(emulator);
        File.WriteAllText(
            log,
            "RogueMelee automatic launcher\r\n" +
            "Emulator: " + emulator + "\r\n" +
            "Version: " + version.FileVersion + "\r\n" +
            "ISO: " + iso + "\r\n" +
            "ISO bytes: " + new FileInfo(iso).Length + "\r\n" +
            "Mod recognition file verified.\r\n");

        ProcessStartInfo start = new ProcessStartInfo(
            emulator, "-e " + Q(iso));
        start.UseShellExecute = false;
        start.WorkingDirectory = Path.GetDirectoryName(emulator);
        using (Process process = Process.Start(start)) {
            if (process.WaitForExit(10000)) {
                File.AppendAllText(
                    log,
                    "Emulator exited within 10 seconds. Exit code: " +
                    process.ExitCode + "\r\n");
                MessageBox.Show(
                    "Slippi exited before startup could be confirmed.\n\n" +
                    "Close existing Slippi windows and try again. If it " +
                    "still closes, send Launch diagnostic.txt from:\n" +
                    AppDir,
                    "Launch did not stay open",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return 1;
            }
            File.AppendAllText(
                log,
                "Emulator process remained open for 10 seconds. " +
                "This does not verify gameplay.\r\n");
        }
        return 0;
    }

    static bool SameFile(string a, string b)
    {
        try {
            return new FileInfo(a).Length == new FileInfo(b).Length &&
                FileSha256(a) == FileSha256(b);
        } catch { return false; }
    }

    static void ScheduleSelfUpdate(string replacement)
    {
        string current = Application.ExecutablePath;
        string batch = Path.Combine(
            Path.GetTempPath(),
            "rogue-self-update-" + Guid.NewGuid().ToString("N") + ".cmd");
        StringBuilder script = new StringBuilder();
        script.AppendLine("@echo off");
        script.AppendLine("setlocal");
        script.AppendLine("ping 127.0.0.1 -n 3 >nul");
        script.AppendLine(
            "copy /Y " + Q(replacement) + " " + Q(current) +
            " >nul 2>nul");
        script.AppendLine("if errorlevel 1 exit /b 1");
        script.AppendLine("start \"\" " + Q(current) + " --post-update");
        script.AppendLine("del \"%~f0\"");
        File.WriteAllText(batch, script.ToString(), Encoding.ASCII);

        ProcessStartInfo start = new ProcessStartInfo(
            "cmd.exe", "/c " + Q(batch));
        start.UseShellExecute = false;
        start.CreateNoWindow = true;
        Process.Start(start);
    }

    static bool HasArg(string[] args, string value)
    {
        foreach (string arg in args)
            if (String.Equals(
                    arg, value, StringComparison.OrdinalIgnoreCase))
                return true;
        return false;
    }

    [STAThread]
    static int Main(string[] args)
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);

        StatusForm status = null;
        try {
            if (HasArg(args, "--reset") && File.Exists(ConfigPath))
                File.Delete(ConfigPath);

            Config config = LoadConfig();
            EnsureConfig(config);

            status = new StatusForm();
            status.Show();
            status.SetStatus("Checking for RogueMelee updates...");

            string installed = InstalledVersion(config.Iso);
            string latest = null;
            Exception checkError = null;
            try { latest = DownloadLatestVersion(); }
            catch (Exception e) { checkError = e; }

            string delta;
            string ini;

            if (!String.IsNullOrEmpty(latest)) {
                status.SetStatus(
                    "Installed: " + installed + "    Latest: " + latest);

                delta = EnsureAsset(latest, "rogue.delta");
                ini = EnsureAsset(latest, "GRGE01.ini");

                if (!String.Equals(
                        installed, latest,
                        StringComparison.OrdinalIgnoreCase)) {
                    status.SetStatus(
                        "Updating RogueMelee " + installed + " -> " +
                        latest + "...");
                    ApplyUpdate(config.Iso, delta, latest);

                    try {
                        string newestLauncher =
                            EnsureAsset(latest, "RogueMelee.exe");
                        if (!SameFile(
                                Application.ExecutablePath,
                                newestLauncher)) {
                            status.SetStatus(
                                "Updating RogueMelee launcher...");
                            status.Close();
                            status = null;
                            ScheduleSelfUpdate(newestLauncher);
                            return 0;
                        }
                    } catch {
                        // Launcher self-update is best-effort. The game update
                        // remains valid even if the EXE asset is unavailable.
                    }
                }
            } else {
                delta = CachedOrLocalAsset(installed, "rogue.delta");
                ini = CachedOrLocalAsset(installed, "GRGE01.ini");
                if (delta == null || ini == null) {
                    throw new IOException(
                        "Could not check GitHub for updates and no cached " +
                        "files exist for build " + installed + ".\n\n" +
                        (checkError == null ? "" : checkError.Message));
                }
                status.SetStatus(
                    "Offline: launching installed build " + installed + ".");
            }

            status.SetStatus("Verifying RogueMelee and preparing Slippi...");
            PrepareLaunch(config.Emulator, config.Iso, delta, ini);
            status.Close();
            status = null;
            return Launch(config.Emulator, config.Iso);
        }
        catch (OperationCanceledException) {
            if (status != null) status.Close();
            return 0;
        }
        catch (Exception e) {
            if (status != null) status.Close();
            MessageBox.Show(
                e.Message,
                "RogueMelee launcher",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
            return 1;
        }
    }
}
