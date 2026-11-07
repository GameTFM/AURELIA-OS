using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Windows.Forms;

namespace Aurelia.Setup
{
    internal static class Program
    {
        [STAThread]
        private static int Main(string[] args)
        {
            bool silent = HasFlag(args, "--silent") || HasFlag(args, "/silent");
            string destination = GetValue(args, "--dir=") ?? GetValue(args, "/dir=");
            if (String.IsNullOrEmpty(destination))
            {
                destination = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "AURELIA OS");
            }

            bool noLaunch = HasFlag(args, "--no-launch");
            bool noShortcuts = HasFlag(args, "--no-shortcuts");

            if (silent)
            {
                try
                {
                    Installer.Install(destination, !noShortcuts, null);
                    if (!noLaunch)
                        Installer.Launch(destination);
                    return 0;
                }
                catch (Exception ex)
                {
                    try
                    {
                        Directory.CreateDirectory(destination);
                        File.WriteAllText(Path.Combine(destination, "install-error.log"), ex.ToString());
                    }
                    catch { }
                    return 1;
                }
            }

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new SetupForm(destination, !noShortcuts, !noLaunch));
            return 0;
        }

        private static bool HasFlag(string[] args, string flag)
        {
            foreach (string arg in args)
            {
                if (String.Equals(arg, flag, StringComparison.OrdinalIgnoreCase))
                    return true;
            }
            return false;
        }

        private static string GetValue(string[] args, string prefix)
        {
            foreach (string arg in args)
            {
                if (arg.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                    return arg.Substring(prefix.Length).Trim('"');
            }
            return null;
        }
    }

    internal static class Installer
    {
        private const string PayloadName = "AureliaSetup.payload.zip";

        public static void Install(string destination, bool createShortcuts, Action<int> report)
        {
            string full = Path.GetFullPath(destination);
            string root = Path.GetPathRoot(full);
            if (String.Equals(
                    full.TrimEnd(Path.DirectorySeparatorChar),
                    root.TrimEnd(Path.DirectorySeparatorChar),
                    StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidOperationException("Refusing to install into a drive root.");
            }

            if (Directory.Exists(full))
                Directory.Delete(full, true);
            Directory.CreateDirectory(full);

            string log = Path.Combine(full, "install.log");
            try
            {
                using (Stream payload = Assembly.GetExecutingAssembly().GetManifestResourceStream(PayloadName))
                {
                    if (payload == null)
                        throw new InvalidOperationException("The installer payload is missing.");

                    using (ZipArchive archive = new ZipArchive(payload, ZipArchiveMode.Read))
                    {
                        long total = 0;
                        foreach (ZipArchiveEntry entry in archive.Entries)
                            total += entry.Length;

                        long done = 0;
                        string prefix = full.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;

                        foreach (ZipArchiveEntry entry in archive.Entries)
                        {
                            if (String.IsNullOrEmpty(entry.Name) && entry.FullName.EndsWith("/"))
                                continue;

                            string relative = entry.FullName.Replace('/', Path.DirectorySeparatorChar);
                            string target = Path.GetFullPath(Path.Combine(full, relative));
                            if (!target.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                                throw new InvalidOperationException("The installer payload contains an invalid path.");

                            string parent = Path.GetDirectoryName(target);
                            if (!String.IsNullOrEmpty(parent))
                                Directory.CreateDirectory(parent);

                            using (Stream input = entry.Open())
                            using (FileStream output = new FileStream(target, FileMode.Create, FileAccess.Write, FileShare.None))
                            {
                                byte[] buffer = new byte[131072];
                                int read;
                                while ((read = input.Read(buffer, 0, buffer.Length)) > 0)
                                {
                                    output.Write(buffer, 0, read);
                                    done += read;
                                    if (report != null && total > 0)
                                        report((int)Math.Min(99, (done * 100) / total));
                                }
                            }
                        }
                    }
                }

                if (createShortcuts)
                    CreateShortcuts(full);

                File.AppendAllText(log, "Installed " + DateTime.Now.ToString("u") + Environment.NewLine);
                if (report != null)
                    report(100);
            }
            catch (Exception ex)
            {
                try { File.AppendAllText(log, ex.ToString()); } catch { }
                throw;
            }
        }

        public static void Launch(string destination)
        {
            string launcher = Path.Combine(destination, "AureliaLauncher.exe");
            if (!File.Exists(launcher))
                throw new FileNotFoundException("AURELIA OS launcher not found.", launcher);

            ProcessStartInfo start = new ProcessStartInfo(launcher);
            start.WorkingDirectory = destination;
            start.UseShellExecute = true;
            Process.Start(start);
        }

        private static void CreateShortcuts(string destination)
        {
            string launcher = Path.Combine(destination, "AureliaLauncher.exe");
            string desktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
            string programs = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.Programs),
                "AURELIA OS");

            Directory.CreateDirectory(programs);
            CreateShortcut(Path.Combine(desktop, "AURELIA OS.lnk"), launcher, destination);
            CreateShortcut(Path.Combine(programs, "AURELIA OS.lnk"), launcher, destination);
        }

        private static void CreateShortcut(string linkPath, string target, string workingDirectory)
        {
            Type shellType = Type.GetTypeFromProgID("WScript.Shell");
            if (shellType == null)
                throw new InvalidOperationException("Windows Script Host is unavailable.");

            dynamic shell = Activator.CreateInstance(shellType);
            dynamic shortcut = shell.CreateShortcut(linkPath);
            shortcut.TargetPath = target;
            shortcut.WorkingDirectory = workingDirectory;
            shortcut.IconLocation = target + ",0";
            shortcut.Description = "AURELIA OS - Legacy Alpha";
            shortcut.Save();
        }
    }

    internal sealed class SetupForm : Form
    {
        private static readonly Color Midnight = Color.FromArgb(9, 10, 14);
        private static readonly Color Panel = Color.FromArgb(21, 22, 28);
        private static readonly Color Gold = Color.FromArgb(214, 184, 120);
        private static readonly Color Ivory = Color.FromArgb(244, 240, 231);
        private static readonly Color WarmGray = Color.FromArgb(179, 174, 186);

        private readonly string destination;
        private readonly bool createShortcuts;
        private readonly bool launchAfter;
        private readonly BackgroundWorker worker;
        private readonly Button installButton;
        private readonly Button cancelButton;
        private readonly ProgressBar progress;
        private readonly Label status;
        private readonly CheckBox launchCheck;
        private bool busy;

        public SetupForm(string destination, bool createShortcuts, bool launchAfter)
        {
            this.destination = destination;
            this.createShortcuts = createShortcuts;
            this.launchAfter = launchAfter;

            worker = new BackgroundWorker();
            worker.WorkerReportsProgress = true;
            worker.DoWork += WorkerDoWork;
            worker.ProgressChanged += WorkerProgressChanged;
            worker.RunWorkerCompleted += WorkerCompleted;

            SetStyle(
                ControlStyles.AllPaintingInWmPaint |
                ControlStyles.UserPaint |
                ControlStyles.OptimizedDoubleBuffer,
                true);
            DoubleBuffered = true;

            Text = "AURELIA OS Setup";
            ClientSize = new Size(860, 560);
            BackColor = Midnight;
            ForeColor = Ivory;
            Font = new Font("Segoe UI", 9.5f, FontStyle.Regular, GraphicsUnit.Point);
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            AutoScaleMode = AutoScaleMode.Dpi;
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }

            installButton = MakeButton("Install AURELIA", 606, 476, 198, 42, true);
            installButton.Click += InstallClicked;
            cancelButton = MakeButton("Cancel", 486, 476, 110, 42, false);
            cancelButton.Click += delegate { if (!busy) Close(); };

            progress = new ProgressBar();
            progress.Location = new Point(44, 382);
            progress.Size = new Size(760, 8);
            progress.Style = ProgressBarStyle.Continuous;
            progress.Visible = false;

            status = new Label();
            status.Text = "Ready to install.";
            status.Location = new Point(44, 396);
            status.Size = new Size(500, 22);
            status.ForeColor = WarmGray;
            status.BackColor = Color.Transparent;

            launchCheck = new CheckBox();
            launchCheck.Text = "Launch AURELIA after installation";
            launchCheck.Location = new Point(44, 438);
            launchCheck.Size = new Size(320, 24);
            launchCheck.Checked = launchAfter;
            launchCheck.ForeColor = Ivory;
            launchCheck.BackColor = Color.Transparent;
            launchCheck.FlatStyle = FlatStyle.Standard;

            Controls.Add(installButton);
            Controls.Add(cancelButton);
            Controls.Add(progress);
            Controls.Add(status);
            Controls.Add(launchCheck);

            FormClosing += delegate(object sender, FormClosingEventArgs e)
            {
                if (busy)
                {
                    e.Cancel = true;
                    MessageBox.Show(
                        "AURELIA OS is still installing. Wait for it to finish.",
                        "AURELIA OS",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Information);
                }
            };

            AcceptButton = installButton;
            CancelButton = cancelButton;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

            using (LinearGradientBrush brush = new LinearGradientBrush(
                ClientRectangle,
                Midnight,
                Color.FromArgb(25, 23, 29),
                35f))
            {
                e.Graphics.FillRectangle(brush, ClientRectangle);
            }

            using (Pen gold = new Pen(Gold, 1f))
            {
                e.Graphics.DrawEllipse(gold, 44, 32, 44, 44);
                e.Graphics.DrawLine(gold, 44, 92, 816, 92);
                e.Graphics.DrawLine(gold, 44, 292, 816, 292);
            }

            using (Font markFont = new Font("Georgia", 20f, FontStyle.Bold, GraphicsUnit.Point))
            using (StringFormat centered = new StringFormat())
            {
                centered.Alignment = StringAlignment.Center;
                centered.LineAlignment = StringAlignment.Center;
                e.Graphics.DrawString("A", markFont, new SolidBrush(Gold), new RectangleF(44, 32, 44, 44), centered);
            }

            using (Font brand = new Font("Georgia", 32f, FontStyle.Regular, GraphicsUnit.Point))
            {
                e.Graphics.DrawString("AURELIA", brand, new SolidBrush(Ivory), 104, 26);
            }

            using (Font subtitle = new Font("Segoe UI", 8.5f, FontStyle.Bold, GraphicsUnit.Point))
            {
                e.Graphics.DrawString("OS   /   SOLSTICE   /   LEGACY ALPHA", subtitle, new SolidBrush(Gold), 107, 72);
            }

            using (Font headline = new Font("Segoe UI Light", 22f, FontStyle.Regular, GraphicsUnit.Point))
            {
                e.Graphics.DrawString("A real desktop, in one click.", headline, new SolidBrush(Ivory), 44, 132);
            }

            using (Font body = new Font("Segoe UI", 10.5f, FontStyle.Regular, GraphicsUnit.Point))
            using (Font warning = new Font("Segoe UI", 9.5f, FontStyle.Bold, GraphicsUnit.Point))
            using (Font small = new Font("Segoe UI", 8.5f, FontStyle.Regular, GraphicsUnit.Point))
            {
                e.Graphics.DrawString(
                    "This installs the AURELIA legacy kernel in a dedicated QEMU virtual machine.\r\n" +
                    "It does not modify Windows or install to your physical disk.",
                    body,
                    new SolidBrush(WarmGray),
                    new RectangleF(44, 178, 760, 48));

                e.Graphics.DrawString(
                    "Legacy Alpha - not the Ubuntu/KDE release. No bare-metal installation yet.",
                    warning,
                    new SolidBrush(Gold),
                    44, 246);

                e.Graphics.DrawString("INSTALL LOCATION", small, new SolidBrush(Gold), 44, 308);
                e.Graphics.DrawString(destination, small, new SolidBrush(Ivory), new RectangleF(44, 330, 760, 22));
                e.Graphics.DrawString(
                    "QEMU virtual machine   /   no administrator rights   /   unsigned development build",
                    small,
                    new SolidBrush(WarmGray),
                    44, 524);
            }
        }

        private Button MakeButton(string text, int x, int y, int width, int height, bool primary)
        {
            Button button = new Button();
            button.Text = text;
            button.Location = new Point(x, y);
            button.Size = new Size(width, height);
            button.FlatStyle = FlatStyle.Flat;
            button.Font = new Font("Segoe UI", 9.5f, FontStyle.Bold, GraphicsUnit.Point);
            button.Cursor = Cursors.Hand;
            button.UseVisualStyleBackColor = false;

            if (primary)
            {
                button.BackColor = Gold;
                button.ForeColor = Midnight;
                button.FlatAppearance.BorderSize = 0;
            }
            else
            {
                button.BackColor = Panel;
                button.ForeColor = Ivory;
                button.FlatAppearance.BorderColor = Color.FromArgb(70, 68, 78);
                button.FlatAppearance.BorderSize = 1;
            }
            return button;
        }

        private void InstallClicked(object sender, EventArgs e)
        {
            if (busy)
                return;

            if (Directory.Exists(destination))
            {
                DialogResult answer = MessageBox.Show(
                    "AURELIA OS is already installed.\r\n\r\nReplace the existing installation?",
                    "AURELIA OS",
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Question);
                if (answer != DialogResult.Yes)
                    return;
            }

            busy = true;
            installButton.Enabled = false;
            cancelButton.Enabled = false;
            launchCheck.Enabled = false;
            progress.Visible = true;
            progress.Value = 0;
            status.Text = "Installing AURELIA OS...";
            worker.RunWorkerAsync();
        }

        private void WorkerDoWork(object sender, DoWorkEventArgs e)
        {
            Installer.Install(destination, createShortcuts, delegate(int percent)
            {
                worker.ReportProgress(percent);
            });
        }

        private void WorkerProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progress.Value = Math.Max(0, Math.Min(100, e.ProgressPercentage));
            status.Text = "Installing AURELIA OS... " + e.ProgressPercentage + "%";
        }

        private void WorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            busy = false;
            installButton.Enabled = true;
            cancelButton.Enabled = true;
            launchCheck.Enabled = true;

            if (e.Error != null)
            {
                status.Text = "Installation failed.";
                MessageBox.Show(
                    "AURELIA OS could not be installed.\r\n\r\n" + e.Error.Message,
                    "AURELIA OS",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }

            progress.Value = 100;
            status.Text = "Installed.";
            if (launchCheck.Checked)
            {
                try
                {
                    Installer.Launch(destination);
                    Close();
                }
                catch (Exception ex)
                {
                    MessageBox.Show(
                        "AURELIA OS is installed, but could not be started.\r\n\r\n" + ex.Message,
                        "AURELIA OS",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Warning);
                }
            }
            else
            {
                MessageBox.Show(
                    "AURELIA OS is installed.\r\n\r\nOpen the AURELIA OS shortcut on your desktop to start it.",
                    "AURELIA OS",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
                Close();
            }
        }
    }
}
