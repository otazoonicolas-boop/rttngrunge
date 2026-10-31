#!/usr/bin/env python3
"""Genera Source/Generated/{dsp_wasm.h,param_table.h,plugin_info.h,plugin_info.cmake} a partir de un .fmx.
Uso: python3 tools/gen.py tools/Noctua_SR1_v3.fmx
Los grupos/etiquetas se eligen por "perfil" segun los nombres de parametros; si el .fmx es desconocido se usa un perfil generico.
"""
import sys, json, base64, gzip, os, re, zlib

PROFILES = [
 dict(name="Noctua SR1", code="Nct1", tail=8.0,
  # Default "grunge / punk": voz cruda y adelante. Compresion firme, saturacion marcada, pocos graves,
  # presencia alta, de-esser suave (deja el aire/sibilancia) y espacio corto (cuarto chico).
  defaults=dict(InGain=6, PreSat=6, HPF=100, LowMid=-3, Body=2, Presence=4, HarshCut=-0.5, DeEss=-12,
                CompThresh=-20, CompRatio=6, CompAttack=5, CompRelease=60, SatDrive=70, SatMix=55, Exciter=3.5,
                ReverbTime=0.6, ReverbMix=5, DelayMix=3, OutCeiling=-0.5),
  groups=[("Input", ["InGain","PreSat","HPF"]),
          ("Tone", ["LowMid","Body","Presence","HarshCut"]),
          ("Dynamics", ["DeEss","CompThresh","CompRatio","CompAttack","CompRelease"]),
          ("Saturation", ["SatDrive","SatMix","Exciter"]),
          ("Space", ["ReverbTime","ReverbMix","DelayMix"]),
          ("Output", ["OutCeiling"])],
  labels={"InGain":"In Gain","PreSat":"Pre Sat","HPF":"HPF","LowMid":"Low Mid","Body":"Body","Presence":"Presence",
          "HarshCut":"Harsh Cut","DeEss":"De-Ess","CompThresh":"Thresh","CompRatio":"Ratio","CompAttack":"Attack",
          "CompRelease":"Release","SatDrive":"Drive","SatMix":"Mix","Exciter":"Exciter","ReverbTime":"Reverb Time",
          "ReverbMix":"Reverb Mix","DelayMix":"Delay Mix","OutCeiling":"Ceiling"}),
 dict(name="SR1 Vocal Chain", code="Sr1v", tail=4.0,
  groups=[("Input", ["InGain","InDrive","PhaseInv","HpFreq","LpFreq"]),
          ("EQ", ["EqOn","EqMudFreq","EqMudGain","EqMidFreq","EqMidGain","EqPresFreq","EqPresGain","EqAirGain"]),
          ("De-Esser", ["DsOn","DsFreq","DsThresh","DsAmount","DsSplit"]),
          ("Compressor", ["CompOn","CompThresh","CompRatio","CompAttack","CompRelease","CompMakeup","CompHard"]),
          ("Saturation", ["SatOn","SatMode","SatDrive","SatTone","SatMix"]),
          ("Character", ["CharOn","CharBite","CharWarmth","CharEdge"]),
          ("Space", ["SpaceOn","Width","MicroPitch","SlapTime","SlapMix","RevType","RevMix","RevDecay","RevDamp"]),
          ("Echo", ["EchoOn","DelayTime","DelayFeedback","DelaySync","DelayMix"]),
          ("Output", ["LimiterOn","LimiterCeil","OutGain","Mix"])],
  labels={"InGain":"In Gain","InDrive":"In Drive","PhaseInv":"Phase Inv","HpFreq":"HP Freq","LpFreq":"LP Freq",
   "EqOn":"EQ On","EqMudFreq":"Mud Freq","EqMudGain":"Mud Gain","EqMidFreq":"Mid Freq","EqMidGain":"Mid Gain",
   "EqPresFreq":"Presence Freq","EqPresGain":"Presence Gain","EqAirGain":"Air Gain",
   "DsOn":"De-Ess On","DsFreq":"Freq","DsThresh":"Thresh","DsAmount":"Amount","DsSplit":"Split Band",
   "CompOn":"Comp On","CompThresh":"Thresh","CompRatio":"Ratio","CompAttack":"Attack","CompRelease":"Release",
   "CompMakeup":"Makeup","CompHard":"Hard Knee","SatOn":"Sat On","SatMode":"Mode","SatDrive":"Drive","SatTone":"Tone",
   "SatMix":"Mix","CharOn":"Char On","CharBite":"Bite","CharWarmth":"Warmth","CharEdge":"Edge","SpaceOn":"Space On",
   "Width":"Width","MicroPitch":"Micro Pitch","SlapTime":"Slap Time","SlapMix":"Slap Mix","RevType":"Reverb Type",
   "RevMix":"Reverb Mix","RevDecay":"Reverb Decay","RevDamp":"Reverb Damp","EchoOn":"Echo On","DelayTime":"Time",
   "DelayFeedback":"Feedback","DelaySync":"Sync","DelayMix":"Mix","LimiterOn":"Limiter On","LimiterCeil":"Ceiling",
   "OutGain":"Out Gain","Mix":"Dry/Wet"}),
]

def fl(v): return repr(float(v)) + "f"
def cstr(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
def split_camel(n): return re.sub(r'(?<=[a-z])(?=[A-Z])', ' ', n)

def pick_profile(names, fmx_name):
    for pr in PROFILES:
        if set(sum((g[1] for g in pr["groups"]), [])) == set(names): return pr
    code = (re.sub(r'[^A-Za-z0-9]', '', fmx_name) + "Plug")[:3] + "1"
    return dict(name=fmx_name or "FMX Plugin", code=code[0].upper() + code[1:], tail=6.0,
                groups=[("Parameters", list(names))], labels={n: split_camel(n) for n in names})

def main(path):
    d = json.load(open(path))
    wasm = gzip.decompress(base64.b64decode(d["wasm"]))
    P = json.loads(d["metadata"])["params"]
    pr = pick_profile([p["name"] for p in P], d.get("name", ""))
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Source", "Generated")
    os.makedirs(out, exist_ok=True)

    with open(os.path.join(out, "dsp_wasm.h"), "w") as f:
        f.write("// Autogenerado por tools/gen.py - no editar\n#pragma once\n#include <cstddef>\n")
        f.write("static const unsigned char kDspWasm[] = {\n")
        for i in range(0, len(wasm), 24):
            f.write("  " + ",".join(str(b) for b in wasm[i:i+24]) + ",\n")
        f.write("};\nstatic const size_t kDspWasmSize = %d;\n" % len(wasm))

    gof = {n: gi for gi, (g, names) in enumerate(pr["groups"]) for n in names}
    assert set(gof) == {p["name"] for p in P}
    kind = {"bool": "Bool", "enum": "Choice"}
    defs = pr.get("defaults", {})
    for p in P:
        if p["name"] in defs:
            assert p["min"] <= defs[p["name"]] <= p["max"], p["name"]
            p["default"] = defs[p["name"]]
    def unit(p):
        if p["type"] == "percent": return "%"
        if p["unit"] == "none": return "ratio" if "Ratio" in p["name"] else ""
        return p["unit"]
    with open(os.path.join(out, "param_table.h"), "w") as f:
        f.write("// Autogenerado por tools/gen.py - no editar\n#pragma once\n")
        f.write("struct ParamInfo { int wasmId; const char* id; const char* label; int group; enum Kind{Float,Bool,Choice} kind;\n")
        f.write("  const char* curve; const char* unit; float minV, maxV, defV; const char* choices; /* separados por | */ };\n")
        f.write("static const char* const kGroupNames[] = {" + ",".join(cstr(g) for g, _ in pr["groups"]) + "};\n")
        f.write("static const int kNumGroups = %d;\n" % len(pr["groups"]))
        f.write("static const ParamInfo kParams[] = {\n")
        for p in sorted(P, key=lambda p: p["id"]):
            f.write("  {%d,%s,%s,%d,ParamInfo::%s,%s,%s,%s,%s,%s,%s},\n" % (
                p["id"], cstr(p["name"]), cstr(pr["labels"][p["name"]]), gof[p["name"]], kind.get(p["type"], "Float"),
                cstr(p["curve"]), cstr(unit(p)), fl(p["min"]), fl(p["max"]), fl(p["default"]), cstr("|".join(p["choices"]))))
        f.write("};\nstatic const int kNumParams = %d;\n" % len(P))

    h = zlib.crc32(pr["name"].encode())
    with open(os.path.join(out, "plugin_info.h"), "w") as f:
        f.write("// Autogenerado por tools/gen.py - no editar\n#pragma once\n")
        f.write('#define SR1_PLUGIN_NAME %s\n#define SR1_TAIL_SECONDS %s\n' % (cstr(pr["name"]), repr(float(pr["tail"]))))
        f.write("#define SR1_UID 0x%08X, 0x%08X, 0x%08X, 0x%08X\n" % (0x53523100 | (h & 0xFF), h, zlib.crc32(pr["name"].encode()[::-1]), 0x4E4F4300))
    with open(os.path.join(out, "plugin_info.cmake"), "w") as f:
        f.write('set(SR1_PRODUCT_NAME "%s")\nset(SR1_PLUGIN_CODE %s)\n' % (pr["name"], pr["code"]))
    print("OK:", pr["name"], "-", len(P), "parametros,", len(wasm), "bytes de wasm,", len(pr["groups"]), "grupos")
main(sys.argv[1])
