# Validación documental del primer borrador

**Resultado final: revisión documental independiente APROBADA, sin defectos abiertos; listo para publicar como primer borrador.** PDF de 11 páginas; fuente editable completa y cifras derivadas del run canónico. No es una nueva ejecución de rendimiento ni un cierre académico definitivo.

Redacción: 15 de noviembre de 2026 America/Bogota / 16 de noviembre UTC. Cierre de revisión: 16 de noviembre de 2026 America/Bogota (00:00, UTC−5). Modo real: Default con planificación explícita normal autorizada, sin Plan Mode real disponible. Se reutilizaron Python 3.14.7, pdflatex/TeX Live 2026, pdftotext y pdftoppm ya instalados. Paquetes TeX requeridos comprobados localmente; no se descargó ni instaló nada.

## Comandos y resultados

| Comprobación ejecutada | Resultado |
|---|---|
| `python3 informe/borrador/preparar_evidencia.py` | Pasa: 19 hashes de artefactos, 15 de fuentes, binario/CSV y cuatro originales protegidos; 12 consultas, 360 muestras, 24 resúmenes, 24 escenarios-operación/360 sintéticas, 2348 postings/16580 contenedores; sumas y estadísticos concordantes |
| Dos pasadas `pdflatex -interaction=nonstopmode -halt-on-error -output-directory=../build/informe_final informe_final_1.tex`, desde `informe/` | Exit 0 en ambas; 11 páginas, sin overfull/underfull, citas/referencias indefinidas ni petición de rerun. Extracto en `compilacion_resumen.txt` |
| `pdftotext -layout informe/informe_final_1.pdf build/informe_final/informe_final.txt` | Texto recuperable; comandos CLI completos con espacios y comillas ASCII; unidad correcta: 9288 filas con código de factura de cancelación, no 9288 facturas únicas |
| Inspección Python de enlaces Markdown locales | 38 enlaces de README/contexto/progreso/resumen/README documental resuelven a archivos/directorios existentes; no es un validador exhaustivo de anchors |
| Inspección Python de `input`/`includegraphics` de la fuente | Todas las tablas, figura y logo existen |
| `pdftoppm` y lectura visual de portada/tablas/resultados | Portada, tabla de espacio y agrupaciones legibles; revisor independiente informó además gráfico y tablas legibles en su render preliminar |
| Comprobación de originales | Ambos PDF fuente, `informe/main.tex` y logo conservan hashes exactos registrados en `validacion_cifras.json` |

La advertencia no fatal de babel indica que no hay patrones de separación silábica española precargados y utiliza los disponibles. No impide compilar ni provoca desbordes en el documento actual. No se instaló un paquete para eliminar una advertencia cosmética; se deja registrada. Si otro entorno cambia fuentes/paginación, revisar nuevamente el log.

## Observaciones de revisión preliminar incorporadas

- Se corrigió «9288 facturas» a **9288 filas**; la unidad indexada sigue siendo fila.
- Los comandos se muestran en bloques verbatim, no con macro de ruta que suprime espacios; se usa `upquote` existente para comillas rectas.
- Sensibilidad al orden explícita y recalculada: para Roaring `100`, primero 291 ns (n=7), segundo 3419 ns (n=8); ausente 213 ns (n=8) y 3030 ns (n=7). Son grupos descriptivos, no causalidad aislada. `validacion_cifras.json` registra estos agregados.
- El gráfico se genera desde CSV, sin transcribir ratios manualmente.
- Las exclusiones históricas del informe quedan identificadas como históricas/superadas por autorización nueva; no se borra el registro T0–T6.

## Límites y siguiente paso

No se modificaron C++, tests, scripts de benchmarks ni resultados canónicos; los hashes de medición siguen intactos. No se repitieron CTest/ASan al redactar: se cita la evidencia técnica previa como tal. La verificación documental no demuestra corrección universal, significancia de tiempos ni RSS. La recomendación de baselines indexados/robustez experimental es futura y no implementada.

Veredicto final recibido de `report_review`: **APROBADO para publicar el borrador, sin defectos abiertos**. El revisor recompiló independientemente dos pasadas/11 páginas; el texto PDF entregado y recompilado coincide; regeneró seis derivados byte-idénticos en copia temporal, comprobó 38 enlaces locales y auditó cifras/hashes. Renders finales de páginas 7/8/10 legibles. TEX SHA-256 `8cef35917f59c073668488f24c0647fe5f4500eec830cda77096ee29f2de8a4c`; PDF `bc01f1f5ba9c68d857016e37d802e4904e7ef9cecdfc8383df303d465dec4e02`.

Pendiente: aprobación de contenido/autoría por el grupo. Rúbrica posterior no recibida; no se garantizan puntajes. La publicación Git está delegada; la revisión requerida del borrador ya fue aprobada, y el orquestador coordina su gate final. Los registros originales de Git HEAD/estado sucio en los manifests describen la medición y no deben actualizarse a commits futuros.

Rollback: retirar el nuevo informe/derivados y resumen, y revertir solo permiso/enlaces/checkpoint documental; estructura, datos, pruebas y evidencia original permanecen utilizables.


## Ajuste editorial solicitado y versión de archivo

El 16 de noviembre de 2026 se retiró «Estado del documento» junto con todo su párrafo y los rótulos de borrador del informe. La versión se indica solo en los archivos `informe_final_1.tex` y `informe_final_1.pdf`. Los hashes de revisión T7 anteriores describen la edición previa a este ajuste, no el nuevo PDF. Se conservaron las limitaciones científicas.

Verificación acotada: dos pasadas pdflatex, 11 páginas, sin desbordes ni referencias indefinidas; extracción de texto sin el estado eliminado; 45 enlaces locales correctos; evidencia canónica y originales intactos. No se hizo investigación ni revisión amplia adicional. Fuente/PDF antiguos ya no existen; enlaces apuntan a los nombres versionados.
