import csv
import hashlib
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path

root_dir = Path(__file__).resolve().parents[2]
run_dir = root_dir / 'results/online_retail_2026_11_15_final'
output_dir = Path(__file__).resolve().parent

def read_csv(name):
    with (run_dir / name).open(newline='', encoding='utf-8') as source:
        return list(csv.DictReader(source))

def escape(value):
    return str(value).replace('\\', r'\textbackslash{}').replace('_', r'\_').replace('%', r'\%').replace('&', r'\&')

def integer(value):
    return f'{int(value):,}'.replace(',', r'\,')

def write_table(name, columns, header, rows):
    lines = [r'\begin{tabular}{' + columns + '}', r'\toprule', header + r' \\', r'\midrule']
    lines.extend(' & '.join(row) + r' \\' for row in rows)
    lines.extend([r'\bottomrule', r'\end{tabular}', ''])
    (output_dir / 'tablas' / name).write_text('\n'.join(lines), encoding='utf-8')

manifest = json.loads((run_dir / 'manifest.json').read_text())
assert manifest['status'] == 'passed'
checks = []
for name, expected in manifest['artifacts_sha256'].items():
    assert hashlib.sha256((run_dir / name).read_bytes()).hexdigest() == expected, name
checks.append(f"{len(manifest['artifacts_sha256'])} hashes de artefactos coinciden")
for name, expected in manifest['source_sha256'].items():
    assert hashlib.sha256((root_dir / name).read_bytes()).hexdigest() == expected, name
checks.append(f"{len(manifest['source_sha256'])} hashes de fuentes coinciden")
assert hashlib.sha256((root_dir / 'data/prepared/online_retail.csv').read_bytes()).hexdigest() == manifest['dataset']['sha256']
assert hashlib.sha256((root_dir / 'build/release/retail_benchmark').read_bytes()).hexdigest() == manifest['binary_sha256']
checks.append('hashes de CSV preparado y binario coinciden')
protected = {
    'informe/EDA_project.pdf': '78b86ae1ef6814a21008a3ce6b9140e6d814f57a13b12a8eb1c941b19be221d9',
    'informe/main.tex': '98d2ee98325d5e458c95d6ec9ab27775fad4bc7fbddf4ca0762be76e63331c0c',
    'CS3014_Estructuras_de_Datos_Avanzadas_Indicaciones_Proyecto_de_curso_entrega_1.pdf': 'c828fc04ded12659d86e516a597b450c0f33c75d867e8564fa1f30ea584d10ed',
    'informe/logo_utec.png/images.png': '12d317ae714c1408fe337e1b1fa5bd3a1b187b5ed6f28f20a80cca2e1d7e0c33',
}
for name, expected in protected.items():
    assert hashlib.sha256((root_dir / name).read_bytes()).hexdigest() == expected, name
checks.append('ambos PDF originales, main.tex y logo originales intactos')

queries = read_csv('queries.csv')
samples = read_csv('query_samples.csv')
summary = read_csv('query_summary.csv')
groups = defaultdict(list)
for row in samples:
    assert row['ids_equal'] == 'true'
    groups[row['query_id'], row['method']].append(int(row['elapsed_ns']))
for row in summary:
    values = sorted(groups[row['query_id'], row['method']])
    assert len(values) == int(row['samples']) == 15
    for field, value in [('min_ns', min(values)), ('median_ns', statistics.median(values)), ('p95_ns', values[math.ceil(.95 * len(values)) - 1]), ('max_ns', max(values))]:
        assert float(row[field]) == value, (row['query_id'], field)
assert len(queries) == 12 and len(samples) == 360 and len(summary) == 24
checks.append('12 consultas, 360 muestras y 24 resumenes: igualdad marcada, min/mediana/p95/max recalculados')
order_groups = defaultdict(list)
for row in samples:
    if row['method'] == 'roaring' and row['query_id'] in ['term_rare', 'term_missing']:
        order_groups[row['query_id'], row['order']].append(int(row['elapsed_ns']))
order_summary = {query + '_order_' + order: {'samples': len(values), 'median_ns': statistics.median(values)} for (query, order), values in order_groups.items()}
lookup = {(row['query_id'], row['method']): row for row in summary}
labels = {
    'term_rare': '100', 'term_median': 'peace', 'term_frequent': 'set',
    'country_rare': 'Saudi Arabia', 'country_frequent': 'United Kingdom',
    'and_rare': '100 AND UK', 'and_median': 'peace AND UK', 'and_frequent': 'set AND UK',
    'or_rare_median': '100 OR peace', 'or_rare_frequent': '100 OR set',
    'or_median_frequent': 'peace OR set', 'term_missing': 'ausente',
}
rows = []
for query in queries:
    query_id = query['query_id']
    scan = lookup[query_id, 'scan']
    roaring = lookup[query_id, 'roaring']
    ratio = float(scan['median_ns']) / float(roaring['median_ns'])
    rows.append([escape(labels[query_id]), integer(query['result_cardinality']), f"{int(scan['median_ns']) / 1e6:.6f}", f"{int(roaring['median_ns']) / 1e6:.6f}", f"{int(roaring['p95_ns']) / 1e6:.6f}", f'{ratio:.1f}'])
write_table('latencias.tex', 'lrrrrr', r'Consulta & IDs & Scan (ms) & Roaring (ms) & p95 R. (ms) & S/R', rows)
space = read_csv('space.csv')
rows = []
for row in space:
    logical = sum(int(row[key]) for key in ['payload_bytes', 'key_bytes', 'metadata_bytes'])
    rows.append([escape(row['namespace']), integer(row['payload_bytes']), integer(logical), integer(row['allocated_bytes']), integer(row['flat32_bytes']), integer(row['full_bitmap_bytes'])])
write_table('espacio.tex', 'lrrrrr', r'Claves & Payload & Usado & Capacidad & IDs32 & Bitmap/lista', rows)
rows = []
for row in read_csv('space_by_cardinality.csv'):
    if row['namespace'] != 'term':
        continue
    logical = sum(int(row[key]) for key in ['payload_bytes', 'key_bytes', 'metadata_bytes'])
    rows.append([escape(row['cardinality_group']).replace('>', '$>$'), integer(row['postings']), integer(logical), integer(row['allocated_bytes']), integer(row['flat32_bytes']), f"{logical / int(row['flat32_bytes']):.2f}"])
write_table('grupos.tex', 'lrrrrr', r'Cardinalidad & Listas & Usado (B) & Capacidad (B) & IDs32 (B) & U/IDs32', rows)
figure_groups = [row for row in read_csv('space_by_cardinality.csv') if row['namespace'] == 'term']
used_coordinates = []
capacity_coordinates = []
for label, row in zip(['a', 'b', 'c', 'd'], figure_groups):
    flat = int(row['flat32_bytes'])
    used = sum(int(row[key]) for key in ['payload_bytes', 'key_bytes', 'metadata_bytes'])
    used_coordinates.append(f'({label},{used / flat:.9f})')
    capacity_coordinates.append(f"({label},{int(row['allocated_bytes']) / flat:.9f})")
figure_lines = [
    r'\begin{tikzpicture}',
    r'\begin{axis}[ybar,bar width=11pt,width=0.88\textwidth,height=5.5cm,ymin=0,ymax=11.5,',
    r'ylabel={Bytes modelados / bytes IDs32},symbolic x coords={a,b,c,d},xtick=data,',
    r'xticklabels={1--16,17--256,257--4096,$>4096$},',
    r'legend style={at={(0.98,0.97)},anchor=north east,font=\small},grid=major,grid style={gray!15}]',
    r'\addplot[fill=black!65,draw=black!65] coordinates {' + ' '.join(used_coordinates) + '};',
    r'\addplot[fill=black!20,draw=black!50] coordinates {' + ' '.join(capacity_coordinates) + '};',
    r'\legend{Usado,Capacidad}', r'\end{axis}', r'\end{tikzpicture}', '',
]
(output_dir / 'tablas' / 'figura_espacio.tex').write_text('\n'.join(figure_lines), encoding='utf-8')

synthetic = read_csv('synthetic_summary.csv')
synthetic_samples = read_csv('synthetic.csv')
assert len(synthetic) == 24 and len(synthetic_samples) == 360
assert all(row['ids_equal'] == 'true' for row in synthetic_samples)
synthetic_groups = defaultdict(list)
for row in synthetic_samples:
    synthetic_groups[row['scenario'], row['operation']].append(int(row['elapsed_ns']))
for row in synthetic:
    values = sorted(synthetic_groups[row['scenario'], row['operation']])
    assert len(values) == 15
    assert statistics.median(values) == float(row['median_ns'])
checks.append('24 escenarios-operacion y 360 muestras sinteticas: igualdad marcada y medianas recalculadas')
selected = {'single_block_4096', 'single_block_4097', 'single_block_8192', 'dispersed_8192_blocks', 'threshold_4096_to_4097', 'array_array_overlap', 'bitmap_bitmap_to_array', 'bitmap_bitmap_empty'}
rows = []
for row in synthetic:
    if row['scenario'] not in selected or (row['operation'] == 'or' and row['scenario'] != 'array_array_overlap'):
        continue
    label = row['scenario'].replace('single_block_', 'bloque ').replace('dispersed_8192_blocks', '8192 bloques').replace('threshold_4096_to_4097', '4096 a 4097').replace('array_array_overlap', 'AA').replace('bitmap_bitmap_to_array', 'BB a array').replace('bitmap_bitmap_empty', 'BB vacio')
    rows.append([escape(label), escape(row['operation']), integer(row['cardinality']), row['array_blocks'], row['bitmap_blocks'], integer(row['payload_bytes']), f"{int(row['median_ns']) / 1000:.3f}"])
write_table('sinteticos.tex', 'llrrrrr', r'Caso & Operacion & IDs & A & B & Payload (B) & Mediana ($\mu$s)', rows)

postings = read_csv('postings.csv')
containers = read_csv('containers.csv')
assert len(postings) == 2348 and len(containers) == 16580
checks.append('2348 postings y 16580 contenedores contados directamente')
for row in space:
    posting_rows = [item for item in postings if item['namespace'] == row['namespace']]
    assert len(posting_rows) == int(row['postings'])
    for key in ['key_bytes', 'payload_bytes', 'metadata_bytes', 'allocated_bytes', 'flat32_bytes', 'full_bitmap_bytes']:
        posting_key = 'container_payload_bytes' if key == 'payload_bytes' and 'payload_bytes' not in posting_rows[0] else key
        assert sum(int(item[posting_key]) for item in posting_rows) == int(row[key]), key
checks.append('sumas de memoria por posting contrastadas con space.csv')

totals = {key: sum(int(row[key]) for row in space) for key in ['payload_bytes', 'key_bytes', 'metadata_bytes', 'allocated_bytes', 'flat32_bytes', 'full_bitmap_bytes']}
totals['used_bytes'] = totals['payload_bytes'] + totals['key_bytes'] + totals['metadata_bytes']
report = {'canonical_run': str(run_dir.relative_to(root_dir)), 'checks': checks, 'totals': totals, 'order_summary_roaring': order_summary, 'canonical_manifest_sha256': hashlib.sha256((run_dir / 'manifest.json').read_bytes()).hexdigest(), 'protected_sha256': protected}
(output_dir / 'validacion_cifras.json').write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
print('\n'.join(checks))
print(json.dumps(totals))
