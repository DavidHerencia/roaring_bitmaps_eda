# Reproducir el informe

La versión se identifica únicamente en el nombre de archivo: `informe_final_1.tex` y `informe_final_1.pdf`. El documento se presenta como informe final y conserva sus limitaciones científicas. Las cifras provienen de `results/online_retail_2026_11_15_final`; este directorio solo contiene derivados documentales. No se cambian mediciones para mejorar el relato.

## Regenerar cifras y PDF

Desde la raíz del repositorio, con Python 3 estándar y TeX Live/pdflatex ya disponibles en el entorno de redacción:

```sh
python3 informe/borrador/preparar_evidencia.py
mkdir -p build/informe_final_1
cd informe
pdflatex -interaction=nonstopmode -halt-on-error \
  -output-directory=../build/informe_final_1 informe_final_1.tex
pdflatex -interaction=nonstopmode -halt-on-error \
  -output-directory=../build/informe_final_1 informe_final_1.tex
cd ..
cp build/informe_final_1/informe_final_1.pdf informe/informe_final_1.pdf
```

Dos pasadas resuelven índice, citas y referencias. Auxiliares TeX e imágenes temporales quedan en `build/`, ignorado; no se versionan `.aux`, `.toc`, `.out` ni capturas. Si cambia la paginación y el log pide otra pasada, repetir pdflatex. No hay nuevas dependencias de ejecución para C++; TeX se necesita solo para recompilar el documento. No se instaló ninguna herramienta para esta tarea.

El script exige el CSV preparado y el binario Release usados en la medición para verificar sus hashes. Si no existen, seguir el [README del proyecto](../../README.md) para preparar datos/compilar. Si el binario reconstruido difiere por entorno/toolchain, su hash puede no coincidir: no modificar el manifest histórico para forzar éxito. Las tablas ya generadas permiten compilar el PDF sin dataset ni binario; para una auditoría completa se necesita la evidencia pertinente.

## Archivos

| Archivo | Función |
|---|---|
| `../informe_final_1.tex` | Fuente académica editable en español; formato y nombres tomados de `main.tex`, sin modificarlo |
| `../informe_final_1.pdf` | PDF del informe |
| `preparar_evidencia.py` | Verifica hashes, resúmenes y sumas; genera tablas y figura TeX con stdlib |
| `tablas/*.tex` | Tablas y gráfico derivados de CSV; no editar sus cifras manualmente |
| `validacion_cifras.json` | Comprobaciones, totales y sensibilidad descriptiva al orden; no nuevos tiempos |
| `validacion_documento.md` | Comandos/resultados de compilación, enlaces, revisión visual y límites |
| `compilacion_resumen.txt` | Extracto relevante de compilación final; no todo el log auxiliar |

Las cuatro tablas y la figura se insertan desde `tablas/`. Payload+claves+metadatos y capacidad son vistas distintas; no se suman. Los tiempos de sintéticos excluyen materialización, a diferencia de consultas reales.

## Límites de validación

La evidencia de CTest y sanitizadores es la registrada en `results/validation.md`, no se presenta como corrida de nuevo al redactar. La auditoría de cifras revisa CSV y hashes, pero una marca `ids_equal` en CSV no reejecuta ni reemplaza el oráculo C++ que produjo esa evidencia. Tampoco ofrece significancia estadística, memoria RSS ni cumplimiento de una rúbrica no recibida.

La bibliografía primaria fue contrastada en web: [paper original](https://arxiv.org/abs/1402.6407), [paper de biblioteca optimizada](https://arxiv.org/abs/1709.07821), [UCI](https://archive.ics.uci.edu/dataset/352/online+retail), [ClickHouse, ingenieros autores](https://clickhouse.com/blog/clickhouse-full-text-search-object-storage) y [CRoaring oficial](https://github.com/RoaringBitmap/CRoaring). No se copió código de esas fuentes ni se atribuyeron sus benchmarks al proyecto.
