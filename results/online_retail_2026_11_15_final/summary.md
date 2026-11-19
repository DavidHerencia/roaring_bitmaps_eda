# Resumen técnico de ejecución

Datos derivados de CSV crudos; no es el informe final.

Validación: passed; consultas: 12; muestras de latencia: 360.
Construcción mediana: 437.249 ms.

| Namespace | Payload Roaring (B) | Keys (B) | Metadatos (B) | Capacidad modelada (B) | IDs32 (B) | Bitmap completo/lista (B) |
|---|---:|---:|---:|---:|---:|---:|
| term | 4613058 | 32694 | 938178 | 7947058 | 9603292 | 156477090 |
| country | 166590 | 466 | 13494 | 223898 | 2167636 | 2574082 |

Diccionario: ver dictionary.csv; los nodos STL/allocator no se miden. No sumar caracteres lógicos a objetos como si fuera RSS.

| Query | Scan mediana (ms) | Roaring mediana (ms) | Scan/Roaring |
|---|---:|---:|---:|
| term_rare | 16.526909 | 0.002854 | 5790.788 |
| term_median | 20.615261 | 0.001224 | 16842.533 |
| term_frequent | 21.744897 | 0.197382 | 110.167 |
| country_rare | 2.937224 | 0.000271 | 10838.465 |
| country_frequent | 4.389661 | 0.872068 | 5.034 |
| and_rare | 17.769275 | 0.000668 | 26600.711 |
| and_median | 20.744259 | 0.009054 | 2291.171 |
| and_frequent | 18.422302 | 0.237067 | 77.709 |
| or_rare_median | 27.176090 | 0.007296 | 3724.793 |
| or_rare_frequent | 26.983176 | 0.179648 | 150.200 |
| or_median_frequent | 29.088881 | 0.184386 | 157.761 |
| term_missing | 11.937236 | 0.000248 | 48134.016 |

Grupos: space_by_cardinality.csv, space_by_density.csv y latency_by_*.csv. Umbral y pares: synthetic_summary.csv.
No extrapolar estas mediciones a implementaciones optimizadas ni a otros equipos/cargas.
