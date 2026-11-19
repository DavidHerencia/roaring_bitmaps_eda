#!/usr/bin/env python3
"""prepare the uci online retail csv using only the python standard library."""

import argparse
import csv
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import urllib.request
import xml.etree.ElementTree as element_tree
import zipfile


source_page = "https://archive.ics.uci.edu/dataset/352/online-retail"
source_url = "https://archive.ics.uci.edu/static/public/352/online%2Bretail.zip"
expected_rows = 541909
expected_xlsx_sha256 = "43465a06f2ccf7c8b5bd2892bc7defb52f97487934fe93b16ae4c3936424676d"
spreadsheet_ns = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
project_root = Path(__file__).resolve().parents[1]


def sha256(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        block = source.read(1024 * 1024)
        while block:
            checksum.update(block)
            block = source.read(1024 * 1024)
    return checksum.hexdigest()


def download_source(raw_dir, xlsx_path):
    archive_path = raw_dir / "online_retail.zip"
    raw_dir.mkdir(parents=True, exist_ok=True)
    if not archive_path.exists():
        temporary_path = archive_path.with_suffix(".zip.tmp")
        try:
            with urllib.request.urlopen(source_url, timeout=120) as response:
                with temporary_path.open("wb") as output:
                    shutil.copyfileobj(response, output)
            temporary_path.replace(archive_path)
        finally:
            temporary_path.unlink(missing_ok=True)
    with zipfile.ZipFile(archive_path) as archive:
        member = "Online Retail.xlsx"
        xlsx_path.parent.mkdir(parents=True, exist_ok=True)
        with archive.open(member) as source, xlsx_path.open("wb") as output:
            shutil.copyfileobj(source, output)
    return archive_path


def joined_text(element):
    # une todos los trozos de texto <t> de una celda
    text = ""
    for part in element.iter(spreadsheet_ns + "t"):
        if part.text:
            text += part.text
    return text


def shared_strings(archive):
    if "xl/sharedStrings.xml" not in archive.namelist():
        return []
    result = []
    with archive.open("xl/sharedStrings.xml") as source:
        document = element_tree.parse(source)
    for item in document.getroot().findall(spreadsheet_ns + "si"):
        result.append(joined_text(item))
    return result


def cell_text(cell, strings):
    cell_type = cell.get("t")
    if cell_type == "inlineStr":
        return joined_text(cell)
    value = cell.find(spreadsheet_ns + "v")
    if value is None or value.text is None:
        return ""
    if cell_type == "s":
        return strings[int(value.text)]
    return value.text


def prepare(xlsx_path, output_path):
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = output_path.with_suffix(output_path.suffix + ".tmp")
    row_count = 0
    missing_descriptions = 0
    cancellation_rows = 0
    try:
        with zipfile.ZipFile(xlsx_path) as archive:
            strings = shared_strings(archive)
            with archive.open("xl/worksheets/sheet1.xml") as sheet:
                with temporary_path.open("w", encoding="utf-8", newline="") as output:
                    writer = csv.writer(output, lineterminator="\n")
                    writer.writerow(["row_id", "invoice_no", "description", "country"])
                    header_checked = False
                    for event, row in element_tree.iterparse(sheet, events=("end",)):
                        if row.tag != spreadsheet_ns + "row":
                            continue
                        cells = {}
                        for cell in row.findall(spreadsheet_ns + "c"):
                            # la referencia "C12" se queda solo con la columna "C"
                            column = ""
                            for character in cell.get("r", ""):
                                if character.isalpha():
                                    column += character
                            cells[column] = cell_text(cell, strings)
                        if not header_checked:
                            if (cells.get("A"), cells.get("C"), cells.get("H")) != (
                                "InvoiceNo", "Description", "Country"
                            ):
                                raise ValueError("unexpected XLSX columns: expected A/C/H InvoiceNo/Description/Country")
                            header_checked = True
                        else:
                            invoice_no = cells.get("A", "")
                            description = cells.get("C", "")
                            country = cells.get("H", "")
                            writer.writerow([row_count, invoice_no, description, country])
                            row_count += 1
                            if description == "":
                                missing_descriptions += 1
                            if invoice_no.lower().startswith("c"):
                                cancellation_rows += 1
                        row.clear()
        if row_count != expected_rows:
            raise ValueError(f"expected {expected_rows} rows, found {row_count}")
        temporary_path.replace(output_path)
    finally:
        temporary_path.unlink(missing_ok=True)
    return row_count, missing_descriptions, cancellation_rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--download", action="store_true", help="download official UCI archive only if XLSX is absent")
    parser.add_argument("--xlsx", type=Path, default=project_root / "data/raw/Online Retail.xlsx")
    parser.add_argument("--output", type=Path, default=project_root / "data/prepared/online_retail.csv")
    parser.add_argument("--manifest", type=Path, default=project_root / "data/manifest.json")
    arguments = parser.parse_args()
    archive_path = arguments.xlsx.parent / "online_retail.zip"
    if not arguments.xlsx.exists():
        if not arguments.download:
            parser.error("XLSX not found; use --download once or supply --xlsx")
        archive_path = download_source(arguments.xlsx.parent, arguments.xlsx)
    xlsx_hash = sha256(arguments.xlsx)
    if xlsx_hash != expected_xlsx_sha256:
        parser.error("XLSX SHA256 differs from the verified official UCI source; refusing to prepare")
    row_count, missing_descriptions, cancellation_rows = prepare(arguments.xlsx, arguments.output)
    manifest = {
        "dataset": "Online Retail",
        "citation": "Chen, D. (2015). Online Retail. UCI Machine Learning Repository. https://doi.org/10.24432/C5BW33",
        "license": "CC BY 4.0",
        "source_page": source_page,
        "source_url": source_url,
        "prepared_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "xlsx_file": arguments.xlsx.name,
        "xlsx_sha256": xlsx_hash,
        "xlsx_sha256_verified": True,
        "csv_file": arguments.output.name,
        "csv_sha256": sha256(arguments.output),
        "row_count": row_count,
        "row_id_min": 0,
        "row_id_max": row_count - 1,
        "missing_description_rows": missing_descriptions,
        "cancellation_rows": cancellation_rows,
        "columns": ["row_id", "invoice_no", "description", "country"],
        "filters": "none; preserve every worksheet data row in original order",
    }
    if archive_path.exists():
        manifest["archive_sha256"] = sha256(archive_path)
        manifest["archive_cached_at_utc"] = datetime.datetime.fromtimestamp(
            archive_path.stat().st_mtime, datetime.timezone.utc
        ).isoformat()
    arguments.manifest.parent.mkdir(parents=True, exist_ok=True)
    arguments.manifest.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
