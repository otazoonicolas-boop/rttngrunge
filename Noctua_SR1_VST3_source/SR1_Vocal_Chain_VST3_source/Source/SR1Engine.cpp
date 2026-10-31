#include "SR1Engine.h"
#include <cstring>

extern "C" {
#include "wasm3.h"
}

static constexpr uint32_t kParamBase = 65536;

m3ApiRawFunction (host_abort)
{
    m3ApiGetArg (int32_t, a); m3ApiGetArg (int32_t, b); m3ApiGetArg (int32_t, c); m3ApiGetArg (int32_t, d);
    (void) a; (void) b; (void) c; (void) d;
    m3ApiSuccess();
}

m3ApiRawFunction (host_expose_buffer)   // (namePtr, nameLen, bufPtr, nFloats) -> 0  ("telemetry", ignorado)
{
    m3ApiReturnType (int32_t);
    m3ApiGetArg (int32_t, a); m3ApiGetArg (int32_t, b); m3ApiGetArg (int32_t, c); m3ApiGetArg (int32_t, d);
    (void) a; (void) b; (void) c; (void) d;
    m3ApiReturn (0);
}

void SR1Engine::release()
{
    ready = false;
    if (rt)  { m3_FreeRuntime (reinterpret_cast<IM3Runtime> (rt)); rt = nullptr; mod = nullptr; }
    if (env) { m3_FreeEnvironment (reinterpret_cast<IM3Environment> (env)); env = nullptr; }
    fReset = fProcess = nullptr; mem = nullptr; memSize = 0;
}

bool SR1Engine::load (const unsigned char* wasm, size_t size, float sampleRate, int numParams)
{
    release();
    nParams = numParams;
    wasmBytes = wasm; wasmSize = size; sr = sampleRate;

    auto e = m3_NewEnvironment();
    if (! e) return false;
    env = reinterpret_cast<M3Environment*> (e);

    auto r = m3_NewRuntime (e, 128 * 1024, nullptr);
    if (! r) { release(); return false; }
    rt = reinterpret_cast<M3Runtime*> (r);

    IM3Module m = nullptr;
    if (m3_ParseModule (e, &m, wasm, (uint32_t) size)) { release(); return false; }
    if (m3_LoadModule (r, m)) { m3_FreeModule (m); release(); return false; }
    mod = reinterpret_cast<M3Module*> (m);

    m3_LinkRawFunction (m, "env", "abort", "v(iiii)", host_abort);
    m3_LinkRawFunction (m, "env", "hostExposeBuffer", "i(iiii)", host_expose_buffer);

    IM3Function fInit = nullptr, fr = nullptr, fp = nullptr;
    if (m3_FindFunction (&fInit, r, "init") || m3_FindFunction (&fp, r, "process"))
    { release(); return false; }
    if (m3_FindFunction (&fr, r, "reset")) fr = nullptr;   // "reset" es opcional
    fReset = reinterpret_cast<M3Function*> (fr);
    fProcess = reinterpret_cast<M3Function*> (fp);

    if (m3_CallV (fInit, sampleRate)) { release(); return false; }

    mem = m3_GetMemory (m, &memSize, 0);
    if (! mem || memSize < 65536 * 4) { release(); return false; }
    ioBase = (uint32_t) (memSize - 65536);   // el DSP usa < 1.5 MB; los ultimos 64 KB quedan libres

    ready = true;
    if (fReset) reset();
    return true;
}

void SR1Engine::setParam (int id, float value) noexcept
{
    if (! ready || id < 0 || id >= nParams) return;
    std::memcpy (mem + kParamBase + 4u * (uint32_t) id, &value, 4);
}

void SR1Engine::reset() noexcept
{
    if (! ready) return;
    if (fReset) { m3_CallV (reinterpret_cast<IM3Function> (fReset)); return; }
    load (wasmBytes, wasmSize, sr, nParams);   // sin reset(): reinstanciar el modulo (estado limpio)
}

void SR1Engine::process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
{
    if (! ready)
    {
        if (inL != outL) std::memcpy (outL, inL, sizeof (float) * (size_t) n);
        if (inR != outR) std::memcpy (outR, inR, sizeof (float) * (size_t) n);
        return;
    }

    mem = m3_GetMemory (reinterpret_cast<IM3Module> (mod), &memSize, 0);
    const uint32_t bytes = (uint32_t) n * 4u;
    const uint32_t A = ioBase, B = A + kMaxBlock * 4u, C = B + kMaxBlock * 4u, D = C + kMaxBlock * 4u;

    std::memcpy (mem + A, inL, bytes);
    std::memcpy (mem + B, inR, bytes);

    if (m3_CallV (reinterpret_cast<IM3Function> (fProcess), (int32_t) A, (int32_t) B, (int32_t) C, (int32_t) D, (int32_t) n))
    {
        std::memcpy (outL, inL, bytes);   // fallo del runtime: pasar el audio sin tocar
        std::memcpy (outR, inR, bytes);
        return;
    }

    mem = m3_GetMemory (reinterpret_cast<IM3Module> (mod), &memSize, 0);
    std::memcpy (outL, mem + C, bytes);
    std::memcpy (outR, mem + D, bytes);
}
