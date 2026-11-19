#!/usr/bin/env python3
"""resume los csv crudos del benchmark en tablas por consulta, por grupo y sinteticas."""

import argparse
import csv
import json
import statistics
from pathlib import Path


def read_csv(directory, name):
    with (directory / name).open(encoding="utf-8", newline="") as source:
        return list(csv.DictReader(source))


def write_csv(path, rows, columns):
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=columns, lineterminator="\n")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def time_summary(values):
    return {
        "samples": len(values),
        "min_ns": min(values),
        "median_ns": statistics.median(values),
        "max_ns": max(values),
    }


def cardinality_group(count):
    if count == 0:
        return "0"
    if count <= 16:
        return "1-16"
    if count <= 256:
        return "17-256"
    if count <= 4096:
        return "257-4096"
    return ">4096"


def density_group(density):
    if density <= 0.01:
        return "<=1%"
    if density <= 0.0625:
        return ">1%-6.25%"
    if density <= 0.25:
        return ">6.25%-25%"
    return ">25%"


def summarize_queries(directory, queries, samples):
    # agrupa los tiempos por (consulta, metodo) y rechaza muestras repetidas
    times = {}
    seen = set()
    for sample in samples:
        if sample["ids_equal"] != "true":
            raise ValueError("an ID comparison did not pass")
        identity = (sample["query_id"], sample["method"], sample["repetition"])
        if identity in seen:
            raise ValueError("duplicate timing sample")
        seen.add(identity)
        key = (sample["query_id"], sample["method"])
        if key not in times:
            times[key] = []
        times[key].append(int(sample["elapsed_ns"]))

    rows = []
    for query in queries:
        for method in ("scan", "roaring"):
            values = times[(query["query_id"], method)]
            row = {"query_id": query["query_id"], "method": method}
            row.update(time_summary(values))
            rows.append(row)
    write_csv(directory / "query_summary.csv", rows,
              ["query_id", "method", "samples", "min_ns", "median_ns", "max_ns"])
    return rows


def summarize_space(directory, postings, containers, universe):
    space_columns = ["key_bytes", "payload_bytes", "metadata_bytes", "allocated_bytes",
                     "flat32_bytes", "full_bitmap_bytes"]

    # suma el espacio de los postings segun su cardinalidad
    by_cardinality = {}
    for posting in postings:
        if int(posting["full_bitmap_bytes"]) != (universe + 7) // 8:
            raise ValueError("full bitmap universe mismatch")
        key = (posting["namespace"], cardinality_group(int(posting["cardinality"])))
        if key not in by_cardinality:
            by_cardinality[key] = {"postings": 0}
            for name in space_columns:
                by_cardinality[key][name] = 0
        by_cardinality[key]["postings"] += 1
        for name in space_columns:
            by_cardinality[key][name] += int(posting[name])

    # suma los contenedores segun su densidad dentro del bloque
    by_density = {}
    for block in containers:
        key = (block["namespace"], density_group(float(block["density"])))
        if key not in by_density:
            by_density[key] = {"blocks": 0, "cardinality": 0, "payload_bytes": 0}
        by_density[key]["blocks"] += 1
        by_density[key]["cardinality"] += int(block["cardinality"])
        by_density[key]["payload_bytes"] += int(block["payload_bytes"])

    cardinality_rows = []
    for key in sorted(by_cardinality):
        row = {"namespace": key[0], "cardinality_group": key[1]}
        row.update(by_cardinality[key])
        cardinality_rows.append(row)
    density_rows = []
    for key in sorted(by_density):
        row = {"namespace": key[0], "density_group": key[1]}
        row.update(by_density[key])
        density_rows.append(row)

    write_csv(directory / "space_by_cardinality.csv", cardinality_rows,
              ["namespace", "cardinality_group", "postings"] + space_columns)
    write_csv(directory / "space_by_density.csv", density_rows,
              ["namespace", "density_group", "blocks", "cardinality", "payload_bytes"])
    return cardinality_rows, density_rows


def summarize_synthetic(directory, synthetics):
    groups = {}
    for row in synthetics:
        if row["ids_equal"] != "true":
            raise ValueError("a synthetic ID comparison did not pass")
        key = (row["scenario"], row["operation"])
        if key not in groups:
            groups[key] = []
        groups[key].append(row)

    rows = []
    for key in sorted(groups):
        group = groups[key]
        first = group[0]
        times = []
        for row in group:
            times.append(int(row["elapsed_ns"]))
        summary = {
            "scenario": key[0],
            "operation": key[1],
            "cardinality": int(first["cardinality"]),
            "array_blocks": int(first["array_blocks"]),
            "bitmap_blocks": int(first["bitmap_blocks"]),
            "payload_bytes": int(first["payload_bytes"]),
        }
        summary.update(time_summary(times))
        rows.append(summary)
    write_csv(directory / "synthetic_summary.csv", rows,
              ["scenario", "operation", "cardinality", "array_blocks", "bitmap_blocks",
               "payload_bytes", "samples", "min_ns", "median_ns", "max_ns"])
    return rows


def write_markdown(directory, queries, query_rows, totals, build):
    lines = ["# Resumen de la ejecucion", "",
             f"Construccion mediana: {build['median_ns'] / 1e6:.3f} ms.", "",
             "| Namespace | Payload (B) | Claves (B) | Metadatos (B) | Capacidad (B) | IDs32 (B) |",
             "|---|---:|---:|---:|---:|---:|"]
    for row in totals:
        lines.append(f"| {row['namespace']} | {row['payload_bytes']} | {row['key_bytes']} | "
                     f"{row['metadata_bytes']} | {row['allocated_bytes']} | {row['flat32_bytes']} |")

    medians = {}
    for row in query_rows:
        medians[(row["query_id"], row["method"])] = row["median_ns"]
    lines += ["", "| Consulta | Scan (ms) | Roaring (ms) | Veces mas rapido |", "|---|---:|---:|---:|"]
    for query in queries:
        scan = medians[(query["query_id"], "scan")]
        roaring = medians[(query["query_id"], "roaring")]
        lines.append(f"| {query['query_id']} | {scan / 1e6:.6f} | {roaring / 1e6:.6f} | {scan / roaring:.1f} |")
    (directory / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def summarize(directory):
    queries = read_csv(directory, "queries.csv")
    samples = read_csv(directory, "query_samples.csv")
    postings = read_csv(directory, "postings.csv")
    containers = read_csv(directory, "containers.csv")
    totals = read_csv(directory, "space.csv")
    synthetics = read_csv(directory, "synthetic.csv")
    builds = read_csv(directory, "build_samples.csv")
    if not queries or not samples or not postings or not synthetics or not builds:
        raise ValueError("incomplete evidence: a required table is empty")

    universe = int(builds[0]["rows"])
    build_times = []
    for row in builds:
        build_times.append(int(row["elapsed_ns"]))
    build = time_summary(build_times)

    query_rows = summarize_queries(directory, queries, samples)
    cardinality_rows, density_rows = summarize_space(directory, postings, containers, universe)
    synthetic_rows = summarize_synthetic(directory, synthetics)
    write_markdown(directory, queries, query_rows, totals, build)

    summary = {
        "validation": "passed",
        "queries": len(queries),
        "query_samples": len(samples),
        "synthetic_samples": len(synthetics),
        "build": build,
        "query_timing": query_rows,
        "space": totals,
        "cardinality_groups": cardinality_rows,
        "density_groups": density_rows,
        "synthetic": synthetic_rows,
    }
    (directory / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    arguments = parser.parse_args()
    summary = summarize(arguments.directory)
    print(f"validated: {summary['queries']} queries, {summary['query_samples']} query samples, "
          f"{summary['synthetic_samples']} synthetic samples")


if __name__ == "__main__":
    main()
