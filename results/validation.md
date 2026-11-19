# Evidencia de validación técnica

Fecha de ejecución: 2026-11-15 America/Bogota / 2026-11-16 UTC. Sin instalaciones nuevas, commits ni cambios a PDF/TeX. No es el informe académico final.

## Pruebas ejecutadas

| Alcance | Comando | Resultado |
|---|---|---|
| Núcleo propio (reporte roaring_core) | `./build/test_roaring_bitmap` | 5 320 160 checks; Debug1/1,14.10s; sanitizer1/1,46.98s; Release1/1,0.66s |
| Integración Debug propia inicial | `ctest --test-dir build/debug --output-on-failure` | 6/6,20.34s antes del séptimo test de evidencia |
| Integración Release propia final | `ctest --test-dir build/release --output-on-failure` | 7/7,1.31s tras docstrings, incluye6 tests Python de evidencia y5 de preparación |
| Memoria/UB integración propia | `ctest --test-dir build/sanitize --output-on-failure` | 7/7,94.21s, sin diagnósticos |
| Revalidación harness sanitizer tras agregar agrupación latencia | `ctest --test-dir build/sanitize --output-on-failure -R 'benchmark_smoke\|experiment_evidence'` | 2/2,44.78s, sin diagnósticos |
| Dataset real propio | `./build/release/test_retail_index data/prepared/online_retail.csv` | 541909 filas,1454 Description vacías,9288 cancelaciones,2310 términos,38 países;11 comparaciones IDs completas pasan |
| Compilación limpia independiente | Revisor: CMake Release en `build-review-final` + CTest | Sin warnings;7/7,1.44s |
| Oráculo independiente | Revisor: comparaciones adicionales con bitset/std::set y scan | 5898240 comparaciones bitset;65536 claves altas;80 AND reales; todo pasa |
| Preparación/procedencia independiente | Revisor: XLSX fuente vs CSV, todos registros | 541909 atributos/orden/IDs iguales; rerun sin red byte-idéntico |

## Experimentos completos y repetición

```sh
python3 scripts/run_experiments.py --binary build/release/retail_benchmark \
  --data data/prepared/online_retail.csv --output results/online_retail_2026_11_15

python3 scripts/run_experiments.py --binary build/release/retail_benchmark \
  --data data/prepared/online_retail.csv --output results/online_retail_2026_11_15_repeat
```

Ambos `status=passed`: 3 builds,12 consultas×2 métodos×15 muestras=360 latencias,3 calentamientos por consulta/método y24 escenarios-operación sintéticos×15=360 muestras. Igualdad completa de IDs fuera de cada intervalo, filas normalizadas compartidas, materialización dentro del tiempo de consulta. Entorno, flags, hashes, comandos y exclusiones constan en cada manifest.

**Ejecución primaria final:** [`online_retail_2026_11_15_final/manifest.json`](online_retail_2026_11_15_final/manifest.json), [`summary.md`](online_retail_2026_11_15_final/summary.md). Todos sus hashes de fuente coinciden con los archivos actuales; todos sus artefactos se verificaron. Tres runs conservados: inicial; repeat posterior a añadir regresión de cardinalidad0; final posterior a normalizar únicamente dos docstrings de preparación/pruebas. No cambió el algoritmo ni la metodología. La corrección0 ya estaba antes del primer run. Los dos primeros son snapshots históricos.

Ocho tablas deterministas idénticas entre los tres runs: queries,postings,containers,space,dictionary,space_by_cardinality,space_by_density,examples. `consistency.csv` conserva medianas de tres runs y cambios relativos. Final vs inicial: máximo absoluto50.554% (Roaring term_median,0.813→1.224microsegundos); final vs repeat39.249%, sin borrar outliers. Los tiempos pequeños son sensibles a reloj/cachés/carga. Builds medianos435.204ms,407.944ms y437.249ms, respectivamente. No reclamar tiempos repetibles idénticos ni inferencia estadística universal.

## Uso y límites comprobados

- CLI real AND heart/United Kingdom:37513 IDs,cinco filas mostradas,verificación completa; `cli_example.csv` y `cli_example.txt`.
- `text_character_counts.json`:32 Description contienen £,único carácter no ASCII observado;0 Country. No hay letras/dígitos no ASCII en los campos analizados. Normalización ASCII sigue siendo una política explícita,no Unicode general.
- Diccionario medido parcialmente por claves/objetos,no nodos STL/allocator. Capacidad de postings es modelo,no RSS.
- La revisión independiente auditó2348 postings,todos los contenedores/rankings/ejemplos,medianas/p95/agregados y hashes finales. **Veredicto formal independiente: APROBADO alcance técnico R01–R22, sin defectos abiertos**, comunicado por `independent_review` al orquestador. Audit final repetido de hashes y revisión documental completados.


### Última repetición posterior al estilo

```sh
python3 scripts/run_experiments.py --binary build/release/retail_benchmark \
  --data data/prepared/online_retail.csv --output results/online_retail_2026_11_15_final
```

`status=passed`,7.245827s; mismos parámetros/cobertura completos. Dos docstrings pasaron a minúsculas/sin tildes sin lógica. Python5/5 y Release7/7 revalidados. No se requirió recompilar binario ni repetir sanitizer por textos de documentación; el manifest final permite verificar todo el snapshot actual.

**Veredicto definitivo posterior al delta:** `independent_review` APROBÓ R01–R22 sin defectos abiertos, confirmó exactamente dos docstrings sin cambio lógico, binario intacto, hashes actuales de fuente/binario/artefactos, ocho tablas deterministas iguales, grupo0 con regresión y documentación final. T6 completada.
