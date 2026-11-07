using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

namespace Aurelia.Launcher
{
    internal static class Program
    {
        [STAThread]
        private static int Main(string[] args)
        {
            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            string qemuDir = Path.Combine(baseDir, "qemu");
            string qemu = Path.Combine(qemuDir, "qemu-system-x86_64w.exe");
            string iso = Path.Combine(baseDir, "os", "aurelia.iso");
            string share = Path.Combine(qemuDir, "share");

            if (!File.Exists(qemu) || !File.Exists(iso))
            {
                MessageBox.Show(
                    "AURELIA OS files are missing from this installation.\r\n\r\n" +
                    "Run the AURELIA OS Setup program again to repair it.",
                    "AURELIA OS",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return 1;
            }

            bool headless = false;
            bool testBoot = false;
            foreach (string arg in args)
            {
                if (String.Equals(arg, "--headless", StringComparison.OrdinalIgnoreCase))
                    headless = true;
                if (String.Equals(arg, "--test-boot", StringComparison.OrdinalIgnoreCase))
                {
                    headless = true;
                    testBoot = true;
                }
            }

            string bootLog = Path.Combine(baseDir, "boot.log");
            string qemuArgs =
                "-L " + Quote(share) +
                " -m 1024" +
                " -cdrom " + Quote(iso) +
                " -boot d" +
                " -rtc base=localtime" +
                " -name \"AURELIA OS - Legacy Alpha\"";

            if (headless)
            {
                qemuArgs += " -display none -serial file:" + Quote(bootLog) + " -no-reboot";
            }
            else
            {
                qemuArgs += " -display gtk";
            }

            try
            {
                ProcessStartInfo start = new ProcessStartInfo(qemu, qemuArgs);
                start.UseShellExecute = false;
                start.WorkingDirectory = qemuDir;
                start.CreateNoWindow = headless;

                if (headless)
                {
                    start.RedirectStandardOutput = true;
                    start.RedirectStandardError = true;
                }

                Process process = Process.Start(start);
                if (process == null)
                    throw new InvalidOperationException("QEMU did not start.");

                if (testBoot)
                {
                    if (!process.WaitForExit(20000))
                    {
                        try { process.Kill(); } catch { }
                    }
                    process.Dispose();
                    return 0;
                }

                if (headless)
                {
                    process.WaitForExit();
                    process.Dispose();
                }
                return 0;
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    "AURELIA OS could not start its virtual machine.\r\n\r\n" + ex.Message,
                    "AURELIA OS",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return 2;
            }
        }

        private static string Quote(string value)
        {
            return "\"" + value.Replace("\"", "\\\"") + "\"";
        }
    }
}
