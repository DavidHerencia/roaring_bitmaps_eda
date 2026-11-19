# Resumen técnico de ejecución

Datos derivados de CSV crudos; no es el informe final.

Validación: passed; consultas: 12; muestras de latencia: 360.
Construcción mediana: 407.944 ms.

| Namespace | Payload Roaring (B) | Keys (B) | Metadatos (B) | Capacidad modelada (B) | IDs32 (B) | Bitmap completo/lista (B) |
|---|---:|---:|---:|---:|---:|---:|
| term | 4613058 | 32694 | 938178 | 7947058 | 9603292 | 156477090 |
| country | 166590 | 466 | 13494 | 223898 | 2167636 | 2574082 |

Diccionario: ver dictionary.csv; los nodos STL/allocator no se miden. No sumar caracteres lógicos a objetos como si fuera RSS.

| Query | Scan mediana (ms) | Roaring mediana (ms) | Scan/Roaring |
|---|---:|---:|---:|
| term_rare | 17.676433 | 0.002991 | 5909.874 |
| term_median | 22.270296 | 0.000879 | 25335.945 |
| term_frequent | 22.887046 | 0.179506 | 127.500 |
| country_rare | 2.552353 | 0.000324 | 7877.633 |
| country_frequent | 4.510929 | 0.914299 | 4.934 |
| and_rare | 18.656484 | 0.000527 | 35401.298 |
| and_median | 22.904905 | 0.009182 | 2494.544 |
| and_frequent | 21.704692 | 0.246385 | 88.093 |
| or_rare_median | 29.725635 | 0.007776 | 3822.741 |
| or_rare_frequent | 27.522080 | 0.171972 | 160.038 |
| or_median_frequent | 31.039534 | 0.182935 | 169.675 |
| term_missing | 15.048644 | 0.000251 | 59954.757 |

Grupos: space_by_cardinality.csv, space_by_density.csv y latency_by_*.csv. Umbral y pares: synthetic_summary.csv.
No extrapolar estas mediciones a implementaciones optimizadas ni a otros equipos/cargas.
