"""offline tests for the uci-specific xlsx preparation script."""

import csv
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile


project_root = Path(__file__).resolve().parents[1]
script_path = project_root / "scripts/prepare_data.py"
specification = importlib.util.spec_from_file_location("prepare_data", script_path)
prepare_data = importlib.util.module_from_spec(specification)
specification.loader.exec_module(prepare_data)


class prepare_data_tests(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary_directory.name)
        self.xlsx_path = self.directory / "fixture.xlsx"
        self.csv_path = self.directory / "fixture.csv"
        self.previous_expected_rows = prepare_data.expected_rows
        prepare_data.expected_rows = 2
        self.make_fixture()

    def tearDown(self):
        prepare_data.expected_rows = self.previous_expected_rows
        self.temporary_directory.cleanup()

    def make_fixture(self, description_header="Description"):
        namespace = prepare_data.spreadsheet_ns[1:-1]
        sheet = f'''<worksheet xmlns="{namespace}"><sheetData>
<row r="1"><c r="A1" t="inlineStr"><is><t>InvoiceNo</t></is></c>
<c r="C1" t="inlineStr"><is><t>{description_header}</t></is></c>
<c r="H1" t="inlineStr"><is><t>Country</t></is></c></row>
<row r="2"><c r="A2" t="inlineStr"><is><t>C42</t></is></c>
<c r="C2" t="s"><v>0</v></c><c r="H2" t="inlineStr"><is><t>UK</t></is></c></row>
<row r="3"><c r="A3"><v>42</v></c>
<c r="H3" t="inlineStr"><is><t>FR</t></is></c></row>
</sheetData></worksheet>'''
        shared = f'''<sst xmlns="{namespace}"><si>
<r><t>quote "a",</t></r><r><t>\nnext</t></r></si></sst>'''
        with zipfile.ZipFile(self.xlsx_path, "w") as archive:
            archive.writestr("xl/worksheets/sheet1.xml", sheet)
            archive.writestr("xl/sharedStrings.xml", shared)

    def test_shared_inline_sparse_and_multiline(self):
        counts = prepare_data.prepare(self.xlsx_path, self.csv_path)
        self.assertEqual(counts, (2, 1, 1))
        with self.csv_path.open(encoding="utf-8", newline="") as source:
            rows = list(csv.reader(source))
        self.assertEqual(rows, [
            ["row_id", "invoice_no", "description", "country"],
            ["0", "C42", 'quote "a",\nnext', "UK"],
            ["1", "42", "", "FR"],
        ])
        previous_bytes = self.csv_path.read_bytes()
        prepare_data.prepare(self.xlsx_path, self.csv_path)
        self.assertEqual(self.csv_path.read_bytes(), previous_bytes)

    def test_count_failure_preserves_previous_csv(self):
        self.csv_path.write_text("previous valid output", encoding="utf-8")
        prepare_data.expected_rows = 3
        with self.assertRaisesRegex(ValueError, "expected 3 rows, found 2"):
            prepare_data.prepare(self.xlsx_path, self.csv_path)
        self.assertEqual(self.csv_path.read_text(), "previous valid output")
        self.assertFalse(self.csv_path.with_suffix(".csv.tmp").exists())

    def test_header_failure_preserves_previous_csv(self):
        self.make_fixture(description_header="Wrong")
        self.csv_path.write_text("previous valid output", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "unexpected XLSX columns"):
            prepare_data.prepare(self.xlsx_path, self.csv_path)
        self.assertEqual(self.csv_path.read_text(), "previous valid output")
        self.assertFalse(self.csv_path.with_suffix(".csv.tmp").exists())

    def test_missing_source_requires_explicit_download(self):
        result = subprocess.run([
            sys.executable, str(script_path), "--xlsx", str(self.directory / "absent.xlsx"),
            "--output", str(self.csv_path), "--manifest", str(self.directory / "manifest.json"),
        ], capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 2)
        self.assertIn("XLSX not found", result.stderr)
        self.assertFalse(self.csv_path.exists())

    def test_unverified_hash_preserves_previous_csv(self):
        self.csv_path.write_text("previous valid output", encoding="utf-8")
        result = subprocess.run([
            sys.executable, str(script_path), "--xlsx", str(self.xlsx_path),
            "--output", str(self.csv_path), "--manifest", str(self.directory / "manifest.json"),
        ], capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 2)
        self.assertIn("XLSX SHA256 differs", result.stderr)
        self.assertEqual(self.csv_path.read_text(), "previous valid output")
        self.assertFalse((self.directory / "manifest.json").exists())


if __name__ == "__main__":
    unittest.main()
