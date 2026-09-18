import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class CrashReportingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.launcher = (
            ROOT / "tools/package/RogueMelee.cs"
        ).read_text(encoding="utf-8")
        cls.readme = (ROOT / "README.md").read_text(encoding="utf-8")

    def test_launcher_creates_persistent_session_report(self):
        self.assertIn('"Crash Reports"', self.launcher)
        self.assertIn('"Last session report.txt"', self.launcher)
        self.assertIn(
            '"RogueMelee automatic session/crash report"',
            self.launcher,
        )
        self.assertIn("SessionHeader(emulator, iso)", self.launcher)

    def test_launcher_monitors_emulator_until_exit(self):
        self.assertIn("process.WaitForExit();", self.launcher)
        self.assertIn("process.ExitCode", self.launcher)
        self.assertIn("RedirectStandardOutput = true", self.launcher)
        self.assertIn("RedirectStandardError = true", self.launcher)

    def test_report_collects_host_diagnostics(self):
        self.assertIn("AppendDolphinLogs", self.launcher)
        self.assertIn("AppendWerReports", self.launcher)
        self.assertIn("Windows Error Reporting", self.launcher)
        self.assertIn("Emulator stdout/stderr", self.launcher)

    def test_abnormal_exit_gets_timestamped_copy(self):
        self.assertIn('"RogueMelee-crash-"', self.launcher)
        self.assertIn("SaveCrashCopy(reportPath)", self.launcher)
        self.assertIn("probableCrash", self.launcher)

    def test_report_folder_can_be_opened_directly(self):
        self.assertIn('"--crash-reports"', self.launcher)
        self.assertIn("Automatic crash/session reports", self.readme)


if __name__ == "__main__":
    unittest.main()
