# Roaring Bitmaps para un índice de ventas

Implementación educativa C++17 de Roaring de 32 bits: contenedores array/bitmap (umbral 4096), inserción, pertenencia, AND/OR e índice de términos/países sobre **Online Retail (UCI)**. Incluye pruebas, scan independiente y evaluación reproducible. No usa una biblioteca Roaring terminada; no implementa run containers, SIMD, concurrencia ni rank/select.

## 1. Preparar los datos

Se necesitan CMake ≥3.16, compilador C++17, Make y Python 3 (stdlib; no pandas/openpyxl). Sin descarga durante el build.

```sh
# primera vez, solo si falta el XLSX local
python3 scripts/prepare_data.py --download

# siguientes veces, sin red: reutilizar el XLSX
python3 scripts/prepare_data.py
```

Se preservan **541909 filas**, incluidas cancelaciones/descripciones vacías. `row_id` = posición original desde cero; `InvoiceNo` no es el ID. CSV generado: `data/prepared/online_retail.csv`; procedencia y hashes: `data/manifest.json`.

## 2. Construir y probar

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release -j2
ctest --test-dir build/release --output-on-failure

# contraste adicional con filas reales (no necesario para tests offline)
./build/release/test_retail_index data/prepared/online_retail.csv
```

CTest incluye núcleo, índice, preparación XLSX offline y CLI/harness con fixture. Los tests no descargan datos. Para detección de errores de memoria/UB con GCC o Clang:

```sh
cmake -S . -B build/sanitize -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build/sanitize -j2
ctest --test-dir build/sanitize --output-on-failure
```

## 3. Consultar y recuperar filas

```sh
./build/release/retail_cli --data data/prepared/online_retail.csv \
  --type term --first HEART --limit 5 --verify

./build/release/retail_cli --data data/prepared/online_retail.csv \
  --type and --first heart --second 'United Kingdom' --limit 5 --verify

./build/release/retail_cli --data data/prepared/online_retail.csv \
  --type or --first heart --second white --limit 5 --verify
```

Tipos: `term`, `country`, `and` (término/país), `or` (término/término). `--second` se exige solo en and/or. La salida CSV contiene filas originales; stderr indica cantidad total y filas mostradas. `--verify` compara **todos los IDs** con scan normalizado. `--limit` limita solo impresión, nunca resultado/validación.

Normalización: términos únicos lowercase ASCII `[a-z0-9]+`; cada filtro de término debe producir exactamente un token. Países lowercase ASCII y espacios recortados/colapsados, en diccionario independiente. Clave ausente → conjunto vacío. Política ASCII explícita, no normalizador Unicode general.

## 4. Repetir experimentos

```sh
python3 scripts/run_experiments.py --binary build/release/retail_benchmark \
  --data data/prepared/online_retail.csv --output results/mi_ejecucion

python3 scripts/summarize_results.py results/mi_ejecucion
```

El directorio debe ser nuevo/vacío. Defaults: 3 builds, 15 repeticiones por consulta/método, semilla 20261115. Separa lectura/normalización/construcción/latencia, compara IDs completos y materializa ambas rutas. Conserva muestras CSV, entorno, SHA-256, espacio vs IDs32/bitmap completo, grupos por cardinalidad/densidad, sintéticos de umbral y filas de ejemplo.

Evidencia completa verificada: [ejecución final](results/online_retail_2026_11_15_final/summary.md), [manifest](results/online_retail_2026_11_15_final/manifest.json) y [validación](results/validation.md).

## Mapa del proyecto

| Ruta | Contenido |
|---|---|
| `include/`, `src/` | Roaring e índice/normalización/oráculo scan |
| `apps/retail_cli.cpp` | Consulta mínima con recuperación de filas |
| `benchmarks/` | Harness real/sintético y pruebas de evidencia |
| `scripts/` | Preparación y ejecución/resumen con Python stdlib |
| `tests/` | Pruebas de estructura/datos y fixture offline |
| `results/` | Ejecuciones verificadas; crudos y manifiestos reproducibles |

## Fuentes y atribución

Algoritmos estudiados: Chambi et al., *Better bitmap performance with Roaring bitmaps* (2016), [doi:10.1002/spe.2325](https://doi.org/10.1002/spe.2325); contexto de optimizaciones: Lemire et al. (2018), [doi:10.1002/spe.2560](https://doi.org/10.1002/spe.2560). Datos: Chen, D. (2015), *Online Retail*, UCI, [doi:10.24432/C5BW33](https://doi.org/10.24432/C5BW33), CC BY 4.0.


Link version final: 
https://github.com/DavidHerencia/roaring_bitmaps_eda.git
