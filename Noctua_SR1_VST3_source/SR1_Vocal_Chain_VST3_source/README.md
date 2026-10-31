# Noctua SR1 - VST3

Conversion del `.fmx` (modulo WebAssembly + metadatos) a un plugin VST3 nativo. El DSP original se ejecuta tal cual
dentro del plugin con el runtime `wasm3` (salida bit a bit identica a wasmtime en las pruebas).

ABI del modulo WASM: `init(sr)`, `reset()`, `process(inL, inR, outL, outR, n)` con punteros a floats en la memoria WASM;
los 52 parametros son floats en la direccion `65536 + 4*id`, en unidades naturales (dB, Hz, ms, %, indice de enum).

## Dos versiones
| Carpeta | Interfaz | Como compilar |
|---|---|---|
| `NoJUCE/` | La generica del DAW | SDK VST3 de Steinberg. Se puede cross-compilar con MinGW desde Linux. |
| raiz (`Source/`, `CMakeLists.txt`) | Perillas propias (JUCE) | Visual Studio 2022 o GitHub Actions (`.github/workflows/build.yml`). JUCE no soporta MinGW. |

### Version JUCE en Windows (Visual Studio 2022 + CMake)
    cmake -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release --target SR1VocalChain_VST3
Resultado: `build/SR1VocalChain_artefacts/Release/VST3/SR1 Vocal Chain.vst3`
(O sube la carpeta a GitHub: el workflow genera el VST3 como "artifact".)

### Version NoJUCE
    git clone --recurse-submodules https://github.com/steinbergmedia/vst3sdk
    cmake -S NoJUCE -B build -DVST3SDK_DIR=<ruta>/vst3sdk
    cmake --build build --config Release --target SR1VocalChain
(Con MinGW hay que parchear `public.sdk/source/vst/utility/alignedalloc.h`: usar `_aligned_malloc/_aligned_free` tambien con `__MINGW32__`.)

## Regenerar desde otro .fmx
    python3 tools/gen.py tools/Noctua_SR1_v3.fmx
