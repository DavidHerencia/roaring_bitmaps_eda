# Resumen técnico de ejecución

Datos derivados de CSV crudos; no es el informe final.

Validación: passed; consultas: 12; muestras de latencia: 360.
Construcción mediana: 435.204 ms.

| Namespace | Payload Roaring (B) | Keys (B) | Metadatos (B) | Capacidad modelada (B) | IDs32 (B) | Bitmap completo/lista (B) |
|---|---:|---:|---:|---:|---:|---:|
| term | 4613058 | 32694 | 938178 | 7947058 | 9603292 | 156477090 |
| country | 166590 | 466 | 13494 | 223898 | 2167636 | 2574082 |

Diccionario: ver dictionary.csv; los nodos STL/allocator no se miden. No sumar caracteres lógicos a objetos como si fuera RSS.

| Query | Scan mediana (ms) | Roaring mediana (ms) | Scan/Roaring |
|---|---:|---:|---:|
| term_rare | 18.521531 | 0.003099 | 5976.615 |
| term_median | 22.196464 | 0.000813 | 27301.924 |
| term_frequent | 24.437816 | 0.192829 | 126.733 |
| country_rare | 2.165715 | 0.000301 | 7195.066 |
| country_frequent | 4.828577 | 0.850712 | 5.676 |
| and_rare | 18.361628 | 0.000590 | 31121.403 |
| and_median | 22.799437 | 0.009408 | 2423.410 |
| and_frequent | 21.142518 | 0.258356 | 81.835 |
| or_rare_median | 30.897691 | 0.007581 | 4075.675 |
| or_rare_frequent | 28.971346 | 0.183908 | 157.532 |
| or_median_frequent | 32.702340 | 0.194826 | 167.854 |
| term_missing | 14.415989 | 0.000287 | 50229.927 |

Grupos: space_by_cardinality.csv, space_by_density.csv y latency_by_*.csv. Umbral y pares: synthetic_summary.csv.
No extrapolar estas mediciones a implementaciones optimizadas ni a otros equipos/cargas.
