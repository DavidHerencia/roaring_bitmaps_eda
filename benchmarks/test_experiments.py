#!/usr/bin/env python3
"""exercise cli, benchmark and evidence validation on the offline fixture."""

import csv
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


sys.dont_write_bytecode = True
project_root = Path(__file__).resolve().parents[1]
binary = Path(sys.argv.pop(1)).resolve()
cli = binary.parent / "retail_cli"
fixture = project_root / "tests/fixtures/retail.csv"
spec = importlib.util.spec_from_file_location("summarize_results", project_root / "scripts/summarize_results.py")
summarizer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(summarizer)


class experiment_tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.output = Path(cls.temporary.name)
        subprocess.run([str(binary), "--data", str(fixture), "--output", str(cls.output),
                        "--repetitions", "2", "--warmup", "1", "--build-repetitions", "2"],
                       check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_evidence_and_synthetic_transitions(self):
        summary = summarizer.summarize(self.output)
        self.assertEqual(summary["validation"], "passed")
        self.assertEqual(summary["queries"], 12)
        self.assertEqual(summary["query_samples"], 48)
        self.assertEqual(summary["synthetic_samples"], 48)
        self.assertEqual(summarizer.cardinality_group(0), "0")
        self.assertTrue((self.output / "query_summary.csv").is_file())
        self.assertTrue((self.output / "space_by_density.csv").is_file())
        with (self.output / "synthetic.csv").open(newline="") as source:
            rows = list(csv.DictReader(source))
        cases = {}
        for row in rows:
            cases[(row["scenario"], row["operation"])] = row
        self.assertEqual(cases[("single_block_4096", "build")]["array_blocks"], "1")
        self.assertEqual(cases[("single_block_4097", "build")]["bitmap_blocks"], "1")
        self.assertEqual(cases[("array_array_overlap", "or")]["bitmap_blocks"], "1")
        self.assertEqual(cases[("bitmap_bitmap_to_array", "and")]["array_blocks"], "1")
        self.assertEqual(cases[("bitmap_bitmap_empty", "and")]["blocks"], "0")

    def test_duplicate_sample_is_rejected(self):
        path = self.output / "query_samples.csv"
        original = path.read_text()
        try:
            path.write_text(original + original.splitlines()[1] + "\n")
            with self.assertRaises(ValueError):
                summarizer.summarize(self.output)
        finally:
            path.write_text(original)

    def test_space_corruption_is_rejected(self):
        path = self.output / "postings.csv"
        original = path.read_text()
        try:
            rows = list(csv.DictReader(original.splitlines()))
            rows[0]["full_bitmap_bytes"] = "99999"
            with path.open("w", newline="") as output:
                writer = csv.DictWriter(output, fieldnames=rows[0].keys())
                writer.writeheader()
                writer.writerows(rows)
            with self.assertRaises(ValueError):
                summarizer.summarize(self.output)
        finally:
            path.write_text(original)

    def test_cli_verifies_and_limits_rows(self):
        completed = subprocess.run([str(cli), "--data", str(fixture), "--type", "and",
                                    "--first", "WHITE", "--second", "UNITED KINGDOM",
                                    "--limit", "1", "--verify"], capture_output=True, text=True, check=True)
        self.assertEqual(len(list(csv.DictReader(completed.stdout.splitlines()))), 1)
        self.assertIn("matches=2; showing=1; verified_scan=true", completed.stderr)

    def test_cli_rejects_bad_queries(self):
        for options in (["--type", "and", "--first", "red"],
                        ["--type", "term", "--first", "red", "--second", "white"],
                        ["--type", "term", "--first", "two words"],
                        ["--type", "term", "--first", "red", "--limit", "-1"]):
            with self.subTest(options=options):
                result = subprocess.run([str(cli), "--data", str(fixture), *options], capture_output=True)
                self.assertNotEqual(result.returncode, 0)

    def test_runner_hashes_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "run"
            command = [sys.executable, str(project_root / "scripts/run_experiments.py"),
                       "--binary", str(binary), "--data", str(fixture), "--output", str(output),
                       "--repetitions", "1", "--warmup", "0", "--build-repetitions", "1", "--allow-debug"]
            subprocess.run(command, check=True, capture_output=True, text=True)
            manifest = json.loads((output / "manifest.json").read_text())
            self.assertEqual(manifest["status"], "passed")
            self.assertEqual(manifest["validation"]["query_samples"], 24)
            self.assertTrue((output / "summary.json").is_file())
            repeated = subprocess.run(command, capture_output=True)
            self.assertNotEqual(repeated.returncode, 0)


if __name__ == "__main__":
    unittest.main()
