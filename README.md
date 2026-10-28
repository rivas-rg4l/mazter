# MasterPack - plugins de masterizacion (VST3)

Un solo proyecto, **cinco plugins separados**. Al compilar se generan 5 archivos
`.vst3` independientes (uno por plugin) que se cargan por separado en el DAW.

| Plugin | Que hace | Parametros |
|---|---|---|
| **Master EQ** | HPF + low shelf + 2 campanas + high shelf | 12 |
| **Master Comp** | Compresor de bus (rodilla suave, sidechain HPF, mezcla paralela, auto makeup) | 9 |
| **Master Sat** | Saturacion Tape / Tube / Soft / Hard con sobremuestreo 4x | 5 |
| **Master Imager** | Ancho estereo mid/side, graves en mono, mono check, medidor de correlacion | 6 |
| **Master Limiter** | Limitador look-ahead (1.5 ms), ceiling, release, reporta latencia | 3 |

Orden tipico en la cadena: EQ -> Comp -> Sat -> Imager -> Limiter.

## Compilar (Windows, Visual Studio 2022)

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

Cada plugin queda en su propia carpeta:

```
build/MasterEQ_artefacts/Release/VST3/Master EQ.vst3
build/MasterComp_artefacts/Release/VST3/Master Comp.vst3
build/MasterSat_artefacts/Release/VST3/Master Sat.vst3
build/MasterImager_artefacts/Release/VST3/Master Imager.vst3
build/MasterLimiter_artefacts/Release/VST3/Master Limiter.vst3
```

Para instalar, copia cada `.vst3` a `C:\Program Files\Common Files\VST3`.

Compilar solo uno: `cmake --build build --config Release --target MasterEQ`

Tambien incluye un workflow de GitHub Actions (`.github/workflows/build.yml`) que
compila todo y entrega un zip por plugin.

## Estructura

```
Common/Dsp.h           DSP puro (biquads, compresor, limitador, saturacion)
Common/MasterBase.h    base de los procesadores (buses, estado, parametros, medidores)
Common/MasterEditor.h  editor automatico: una perilla por parametro + medidores
Plugins/<Nombre>/<Nombre>.cpp   un archivo por plugin (parametros + DSP)
```

## Agregar otro plugin al pack

1. Copia una carpeta de `Plugins/` (por ejemplo `MasterSat`) y renombra la carpeta y el `.cpp`.
2. Cambia el nombre de la clase, los parametros y el DSP.
3. Agrega una linea al final de `CMakeLists.txt`:
   `add_master_plugin(MiPlugin "Mi Plugin" Mipp "Fx;EQ")`
   (el codigo de 4 letras debe ser unico y llevar al menos una mayuscula).

## Notas

- Cambiar los nombres que ve el DAW: segundo argumento de `add_master_plugin`.
- Cambiar los colores: `juce::Colour` que se pasa en el constructor de cada procesador.
- Master Limiter y Master Sat informan su latencia al host (compensacion automatica).
- El EQ actualiza sus filtros por bloque, sin suavizado: pensado para ajustes de
  masterizacion, no para automatizaciones muy rapidas.
