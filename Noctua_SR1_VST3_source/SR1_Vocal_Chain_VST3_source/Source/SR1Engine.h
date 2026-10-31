#pragma once
#include <cstdint>
#include <cstddef>

struct M3Environment; struct M3Runtime; struct M3Module; struct M3Function;

// Envuelve el modulo WebAssembly del .fmx (ejecutado con wasm3).
//   process(inL, inR, outL, outR, n)  -> punteros a floats dentro de la memoria WASM
//   parametros: floats en memoria WASM en 65536 + 4*id (unidades naturales: dB, Hz, ms, %...)
class SR1Engine
{
public:
    static constexpr int kMaxBlock = 2048;   // 4 buffers * 2048 * 4 B = 32 KB (cabe en los ultimos 64 KB)

    SR1Engine() = default;
    ~SR1Engine() { release(); }

    bool load (const unsigned char* wasm, size_t size, float sampleRate, int numParams);
    void release();
    bool isReady() const noexcept { return ready; }

    void setParam (int id, float value) noexcept;                     // llamar antes de process()
    void reset() noexcept;
    // numSamples <= kMaxBlock. outL/outR pueden ser igual a inL/inR.
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

private:
    bool ready = false;
    int nParams = 0;
    M3Environment* env = nullptr;
    M3Runtime* rt = nullptr;
    M3Module* mod = nullptr;
    M3Function* fReset = nullptr;
    M3Function* fProcess = nullptr;
    uint8_t* mem = nullptr;
    size_t memSize = 0;
    uint32_t ioBase = 0;
    const unsigned char* wasmBytes = nullptr;   // para recargar si el modulo no exporta reset()
    size_t wasmSize = 0;
    float sr = 48000.0f;
};
