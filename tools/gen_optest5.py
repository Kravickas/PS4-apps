#!/usr/bin/env python3
"""OPCODE_TEST 5 rows: every V_CVT_* of GCN2 on edge-case inputs. Writes src/optest5.h (row table, labels,
op names, the 64-bit-result row range).  python3 tools/gen_optest5.py"""
import os
import struct, math
def f32b(x): return struct.unpack('<I',struct.pack('<f',x))[0]
def f64b(x): return struct.unpack('<Q',struct.pack('<d',x))[0]
import numpy as np
def lab(x, f64=False):
    """Shortest decimal that round-trips (float32 for f32 inputs); integers without a fraction."""
    if isinstance(x,str): return x
    if math.isinf(x): return "+inf" if x>0 else "-inf"
    if x == 0: return "-0" if math.copysign(1, x) < 0 else "0"
    if x == int(x) and abs(x) < 1e16: return "%d" % int(x)
    return repr(float(x)) if f64 else np.format_float_scientific(np.float32(x), unique=True, trim='-') if abs(x) < 1e-4 or abs(x) >= 1e16 else np.format_float_positional(np.float32(x), unique=True, trim='-')
M=0xFFFFFFFF
# ops: (name, kind) kind: f1 = one f32 src, d1 = one f64 src (a lo, b hi), i1 = one u32 src, f2 = two f32, i2 = two u32,
#      acc = PKACCUM (a f32, b sel, c = dst before), pk8 = three srcs; wide = 64-bit result
OPS=[("I32_F32","f1",0),("U32_F32","f1",0),("FLR_I32_F32","f1",0),("RPI_I32_F32","f1",0),("I32_F64","d1",0),("U32_F64","d1",0),
     ("F32_I32","i1",0),("F32_U32","i1",0),("F64_I32","i1",1),("F64_U32","i1",1),("F64_F32","f1",1),("F32_F64","d1",0),
     ("F16_F32","f1",0),("F32_F16","i1",0),("F32_UBYTE0","i1",0),("F32_UBYTE1","i1",0),("F32_UBYTE2","i1",0),("F32_UBYTE3","i1",0),
     ("OFF_F32_I4","i1",0),("PKRTZ_F16_F32","f2",0),("PKNORM_I16_F32","f2",0),("PKNORM_U16_F32","f2",0),("PK_U16_U32","i2",0),
     ("PK_I16_I32","i2",0),("PKACCUM_U8_F32","acc",0),("PK_U8_F32","pk8",0),
     # VOP3 input / output modifiers (ps_cvt: op 26 + i)
     ("I32_F32 (-src)","f1",0),("FLR_I32_F32 (|src|)","f1",0),("F32_I32 (clamp)","i1",0),("F32_U32 (mul:2)","i1",0),
     ("F32_U32 (div:2)","i1",0),("F16_F32 (-src)","f1",0),("F64_F32 (|src|)","f1",1),("PKRTZ_F16_F32 (-src1)","f2",0),
     ("F32_F64 (-src)","d1",0)]
OPI={n:i for i,(n,k,w) in enumerate(OPS)}
FULL_ONLY = ("U32_F64", "PKACCUM_U8_F32")  # no translator in shadPS4 (main 94e21778)
dmin32=struct.unpack('<f',struct.pack('<I',1))[0]; dmax32=struct.unpack('<f',struct.pack('<I',0x007FFFFF))[0]
F32V=[0.0,-0.0,0.5,1.5,2.5,-0.5,-1.5,-2.5,0.49999997,1.0,-1.0,dmin32,-dmin32,2147483520.0,2147483648.0,4294967040.0,4294967296.0,1e10,
      -2147483648.0,-2147483904.0,-1e10,float('inf'),float('-inf'),("NaN",0x7FC00000),("-NaN",0xFFC00000),
      ("sNaN 0x7F800001",0x7F800001),("-sNaN 0xFF800001",0xFF800001),("NaN 0x7FC12345",0x7FC12345),-dmax32]
F64V=[0.0,-0.0,0.5,1.5,2.5,-0.5,-2.5,2147483647.0,2147483647.5,2147483648.0,4294967295.0,4294967295.5,4294967296.0,-1.0,-2147483648.0,
      -2147483648.5,-2147483649.0,1e20,-1e20,("dmin64",1),float('inf'),float('-inf'),("NaN",0x7FF8000000000000),
      ("-NaN",0xFFF8000000000000),("sNaN 0x7FF0000000000001",0x7FF0000000000001),("-sNaN 0xFFF0000000000001",0xFFF0000000000001),
      ("NaN 0x7FF8A00000000000",0x7FF8A00000000000),("-dmin64",0x8000000000000001)]
def fv(v):   # (label, f32 bits)
    return (v[0],v[1]) if isinstance(v,tuple) else (lab(v),f32b(v))
def dv(v):
    return (v[0],v[1]) if isinstance(v,tuple) else (lab(v,True),f64b(v))
rows=[]
def add(op,label,a=0,b=0,c=0):
    if op in FULL_ONLY: return  # no shadPS4 translator, not tested
    rows.append({"op":OPI[op],"name":op,"label":"%s %s" % (op,label),"a":a&M,"b":b&M,"c":c&M,"wide":OPS[OPI[op]][2]})
for op in ("I32_F32","U32_F32","FLR_I32_F32","RPI_I32_F32"):
    for v in F32V: l,b_=fv(v); add(op,l,b_)
for op in ("I32_F64","U32_F64"):
    for v in F64V: l,q=dv(v); add(op,l,q&M,q>>32)
for op in ("F32_I32","F32_U32"):
    for x,l in ((0,"0"),(1,"1"),(M,"0xFFFFFFFF"),(16777216,"16777216"),(16777217,"16777217"),(16777219,"16777219"),(0x7FFFFFFF,"0x7FFFFFFF"),
                (0x80000000,"0x80000000"),(0x7FFFFFBF,"0x7FFFFFBF"),(0xFFFFFF80,"0xFFFFFF80"),(0xFEFFFFFF,"0xFEFFFFFF"),
                (0xFEFFFFFD,"0xFEFFFFFD"),(0x80000080,"0x80000080"),(0x80000180,"0x80000180"),(0xFFFFFF7F,"0xFFFFFF7F")): add(op,l,x)
for op in ("F64_I32","F64_U32"):
    for x,l in ((0,"0"),(1,"1"),(M,"0xFFFFFFFF"),(0x7FFFFFFF,"0x7FFFFFFF"),(0x80000000,"0x80000000")): add(op,l,x)
for v in [1.0,-0.0,0.1,dmin32,dmax32,3.4028234663852886e38,float('inf'),float('-inf'),("NaN",0x7FC00000),("NaN 0x7FC12345",0x7FC12345),("sNaN 0x7F800001",0x7F800001),
          ("-NaN",0xFFC00000),("-sNaN 0xFF800001",0xFF800001),("-NaN 0xFFC12345",0xFFC12345),-dmax32,-dmin32]:
    l,b_=fv(v); add("F64_F32",l,b_)
for v in [1.0,0.1,("1+2^-24",f64b(1+2**-24)),("1+3*2^-24",f64b(1+3*2**-24)),("1+2^-24+2^-40",f64b(1+2**-24+2**-40)),3.4028235677973366e38,1e39,1e-40,1e-46,-0.0,
          ("dmin64",1),float('inf'),("NaN",0x7FF8000000000000),("NaN 0x7FF8000012345678",0x7FF8000012345678),
          ("-NaN",0xFFF8000000000000),("sNaN 0x7FF0000000000001",0x7FF0000000000001),("sNaN 0x7FF4000000000000",0x7FF4000000000000),
          ("NaN 0x7FF8A00000000000",0x7FF8A00000000000),("-(1+2^-24)",f64b(-(1+2**-24))),-1e-40,("2^-149",f64b(2.0**-149)),
          ("2^-150",f64b(2.0**-150)),("1.5*2^-150",f64b(1.5*2.0**-150))]:
    l,q=dv(v); add("F32_F64",l,q&M,q>>32)
for v in [1.0,-0.0,("1+2^-11",f32b(1+2**-11)),("1+3*2^-11",f32b(1+3*2**-11)),65504.0,65519.0,65520.0,1e10,6.1035156e-05,5.9604645e-08,2.9802322e-08,1e-8,
          ("NaN",0x7FC00000),("NaN 0x7FC12345",0x7FC12345),float('inf'),float('-inf'),
          ("sNaN 0x7F800001",0x7F800001),("-sNaN 0xFF800001",0xFF800001),("sNaN 0x7F801FFF",0x7F801FFF),("NaN 0x7FC00001",0x7FC00001),
          ("sNaN 0x7F802000",0x7F802000),("-NaN",0xFFC00000),("-(1+2^-11)",f32b(-(1+2**-11))),("-(1+3*2^-11)",f32b(-(1+3*2**-11))),
          -65520.0,dmin32,("1.5*2^-24",f32b(1.5*2**-24))]:
    l,b_=fv(v); add("F16_F32",l,b_)
for x,l in ((0x3C00,"0x3C00"),(0xABCD3C00,"0xABCD3C00"),(0x0001,"0x0001"),(0x03FF,"0x03FF"),(0x0400,"0x0400"),(0x7BFF,"0x7BFF"),(0x7C00,"0x7C00"),
            (0xFC00,"0xFC00"),(0x7E00,"0x7E00"),(0x7C01,"0x7C01"),(0x7E3F,"0x7E3F"),(0x8000,"0x8000"),(0xFE00,"0xFE00"),
            (0xFC01,"0xFC01"),(0x8001,"0x8001"),(0x83FF,"0x83FF")): add("F32_F16",l,x)
for op in ("F32_UBYTE0","F32_UBYTE1","F32_UBYTE2","F32_UBYTE3"):
    for x in (0x80FF7F01,M): add(op,"0x%08X" % x,x)
for x in list(range(16))+[0xFFFFFFF7,0x12345678]: add("OFF_F32_I4",("%d" % x) if x<16 else "0x%08X" % x,x)
def P(op,pairs):
    for p0,p1 in pairs:
        l0,b0=fv(p0); l1,b1=fv(p1); add(op,"%s, %s" % (l0,l1),b0,b1)
P("PKRTZ_F16_F32",[(1.0,2.0),(("1+2^-11",f32b(1+2**-11)),("1+3*2^-11",f32b(1+3*2**-11))),(65504.0,65519.0),(65520.0,1e10),(-65520.0,float('-inf')),
                   (5.9604645e-08,2.9802322e-08),(("NaN",0x7FC00000),("NaN 0x7FC12345",0x7FC12345)),(-0.0,0.1),
                   (("sNaN 0x7F800001",0x7F800001),("-sNaN 0xFF800001",0xFF800001)),(("sNaN 0x7F801FFF",0x7F801FFF),("NaN 0x7FC00001",0x7FC00001)),
                   (("-NaN",0xFFC00000),("sNaN 0x7F802000",0x7F802000)),(("-(1+3*2^-11)",f32b(-(1+3*2**-11))),-65519.0)])
P("PKNORM_I16_F32",[(0.0,1.0),(-1.0,0.5),(-1.5,2.0),(("NaN",0x7FC00000),float('-inf')),(("0.5/32767",f32b(0.5/32767)),("1.5/32767",f32b(1.5/32767))),
                    (("-0.5/32767",f32b(-0.5/32767)),("-2.5/32767",f32b(-2.5/32767))),(-0.0,dmin32),(1.0000001,-1.0000001),(float('inf'),("NaN 0x7FC12345",0x7FC12345)),
                    (-0.5,0.5),(("-NaN",0xFFC00000),("sNaN 0x7F800001",0x7F800001))])
P("PKNORM_U16_F32",[(0.0,1.0),(0.25,0.75),(-0.5,1.5),(("NaN",0x7FC00000),float('inf')),(("0.5/65535",f32b(0.5/65535)),("1.5/65535",f32b(1.5/65535))),
                    (("2.5/65535",f32b(2.5/65535)),("32767.5/65535",f32b(32767.5/65535))),(-0.0,dmin32),(0.99999994,1.0000001),(float('-inf'),("-NaN",0xFFC00000)),
                    (("sNaN 0x7F800001",0x7F800001),("-sNaN 0xFF800001",0xFF800001))])
for a,b_,l in ((0x1234,0xABCD,"0x1234, 0xABCD"),(0xFFFF,0x10000,"0xFFFF, 0x10000"),(70000,M,"70000, 0xFFFFFFFF"),(0x80000000,1,"0x80000000, 1"),(0,0xFFFE,"0, 0xFFFE")):
    add("PK_U16_U32",l,a,b_)
for a,b_,l in ((1,-1,"1, -1"),(32767,32768,"32767, 32768"),(-32768,-32769,"-32768, -32769"),(0x7FFFFFFF,0x80000000,"0x7FFFFFFF, 0x80000000"),(0,-1,"0, -1")):
    add("PK_I16_I32",l,a,b_)
for v,s,d in ((171.0,0,0x11223344),(171.0,1,0x11223344),(171.0,3,0x11223344),(171.0,4,0x11223344),(300.0,2,0x11223344),(-1.0,1,0x11223344),
              (("NaN",0x7FC00000),0,0x11223344),(127.5,0,0),(1.5,0,0),(2.5,0,0),(("-NaN",0xFFC00000),0,0),(254.5,0,0)):
    l,b_=fv(v); add("PKACCUM_U8_F32","%s, %d, 0x%08X" % (l,s,d),b_,s,d)
for v,s,d in ((0.5,0,0),(1.5,0,0),(2.5,0,0),(127.5,0,0),(300.0,0,0),(-1.0,0,0),(("NaN",0x7FC00000),0,0),(float('inf'),0,0),(dmin32,0,0),(171.0,4,0x11223344),(171.0,M,0x11223344),
              (("-NaN",0xFFC00000),0,0),(float('-inf'),0,0),(254.5,0,0),(-0.5,0,0),(("sNaN 0x7F800001",0x7F800001),0,0)):
    l,b_=fv(v); add("PK_U8_F32","%s, %s, 0x%08X" % (l,("0x%X" % s) if s>9 else s,d),b_,s,d)
s32x=lambda x: x-(1<<32) if x>>31 else x
for v in (2.5,-2.5,0.5,("NaN",0x7FC00000),("-NaN",0xFFC00000),-0.0): l,b_=fv(v); add("I32_F32 (-src)",l,b_)
for v in (-1.5,-0.5,1.5,("-NaN",0xFFC00000),float('-inf')): l,b_=fv(v); add("FLR_I32_F32 (|src|)",l,b_)
for x in (0,1,5,M,0x80000000): add("F32_I32 (clamp)","%d" % s32x(x),x)
for x in (0,3,0x7FFFFFFF,M): add("F32_U32 (mul:2)","0x%X" % x,x)
for x in (1,3,M): add("F32_U32 (div:2)","0x%X" % x,x)
for v in (1.0,-0.0,("NaN",0x7FC00000),("-NaN",0xFFC00000),65520.0): l,b_=fv(v); add("F16_F32 (-src)",l,b_)
for v in (-1.0,("-NaN",0xFFC00000),("-sNaN 0xFF800001",0xFF800001),-dmin32): l,b_=fv(v); add("F64_F32 (|src|)",l,b_)
P("PKRTZ_F16_F32 (-src1)",[(1.0,2.0),(("NaN",0x7FC00000),("NaN",0x7FC00000)),(-0.0,0.0)])
for v in (1.0,("NaN",0x7FF8000000000000),-0.0): l,q=dv(v); add("F32_F64 (-src)",l,q&M,q>>32)
w=[i for i,r in enumerate(rows) if r["wide"]]
# main.c OPT5_LINE: labels and headings up to 64 characters
assert max(len(r["label"] if isinstance(r, dict) else r[4]) for r in rows) <= 64
def cstr(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
L = ["/* Generated by tools/gen_optest5.py - do not edit. OPCODE_TEST 5: %d rows {op, a, b, c}; op indexes" % len(rows),
     "   k_opt5_op (ps_cvt runs V_CVT_<name>); a, b = the f64 source (low, high) for *_F64 sources; c = PK_U8's S2 and",
     "   PKACCUM's destination before the write. k_opt5_wide: ops with a 64-bit result. */",
     "#pragma once", "#include <stdint.h>", "#define OPT5_ROWS %d" % len(rows),
     "#define OPT5_OPS %d" % len(OPS),
     "static const char* const k_opt5_op[OPT5_OPS] = {" + ", ".join(cstr("V_CVT_" + n) for n, k, wd in OPS) + "};",
     "#define OPT5_MODES 5", '#define OPT5_TITLE "V_CVT_* (GCN2)"',
     "/* PGM_RSRC1 FLOAT_MODE per pass: [7:6] f64/f16 denormals, [5:4] f32 denormals, [3:0] rounding */",
     "static const uint8_t k_opt5_float_mode[OPT5_MODES] = {0x00, 0xC0, 0x10, 0x20, 0x30};",
     "static const uint8_t k_opt5_wide[OPT5_OPS] = {" + ", ".join("1" if wd else "0" for n, k, wd in OPS) + "};",
     "static const uint32_t k_opt5_row[OPT5_ROWS][4] = {"]
for r in rows: L.append("    {%d, 0x%08Xu, 0x%08Xu, 0x%08Xu}, /* %s */" % (r["op"], r["a"], r["b"], r["c"], r["label"].replace("*/", "* /")))
L.append("};")
L.append("static const char* const k_opt5_label[OPT5_ROWS] = {")
for r in rows: L.append("    %s," % cstr(r["label"][len(r["name"]) + 1:]))
L.append("};")
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
open(os.path.join(root, "src", "optest5.h"), "w").write("\n".join(L) + "\n")
print("src/optest5.h: %d rows (%d ops), %d with 64-bit results" % (len(rows), len(OPS), len(w)))
