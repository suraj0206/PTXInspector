import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "main.cpp"
GOOD = ROOT / "samples" / "sample.ptx"
BAD = ROOT / "samples" / "bad_branch.ptx"


class PTXInspectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp_dir = tempfile.TemporaryDirectory()
        cls.exe = pathlib.Path(cls.temp_dir.name) / "ptx_inspector"
        subprocess.run(
            ["g++", "-std=c++17", "-Wall", "-Wextra", "-pedantic",
             str(SOURCE), "-o", str(cls.exe)],
            check=True,
            capture_output=True,
            text=True,
        )

    @classmethod
    def tearDownClass(cls):
        cls.temp_dir.cleanup()

    def test_counts_instructions_and_ignores_comments(self):
        result = subprocess.run(
            [str(self.exe), str(GOOD)], capture_output=True, text=True
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Total instructions   : 7", result.stdout)
        self.assertIn("Memory loads         : 2", result.stdout)
        self.assertIn("Memory stores        : 1", result.stdout)
        self.assertIn("Branch validation    : PASS", result.stdout)

    def test_missing_branch_target_is_reported(self):
        result = subprocess.run(
            [str(self.exe), str(BAD)], capture_output=True, text=True
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("branch target 'MISSING_LABEL' does not exist", result.stdout)
        self.assertIn("Branch validation    : FAIL", result.stdout)

    def test_missing_file_returns_usage_error(self):
        result = subprocess.run(
            [str(self.exe), str(ROOT / "samples" / "does_not_exist.ptx")],
            capture_output=True, text=True
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("Error: cannot open file", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
