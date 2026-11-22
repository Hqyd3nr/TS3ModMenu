from pathlib import Path
import ctypes as c
p=Path(__file__).parent
dll=c.WinDLL(r'<msvc-toolchain>\Windows Kits\10\bin\10.0.22621.0\x64\d3dcompiler_47.dll')
compile=dll.D3DCompile;compile.restype=c.c_long
compile.argtypes=[c.c_void_p,c.c_size_t,c.c_char_p,c.c_void_p,c.c_void_p,c.c_char_p,c.c_char_p,c.c_uint,c.c_uint,c.POINTER(c.c_void_p),c.POINTER(c.c_void_p)]
def method(blob,index,restype,*args):
 table=c.cast(blob,c.POINTER(c.POINTER(c.c_void_p))).contents
 return c.WINFUNCTYPE(restype,c.c_void_p,*args)(table[index])
def read(blob):
 address=method(blob,3,c.c_void_p)(blob);size=method(blob,4,c.c_size_t)(blob)
 return c.string_at(address,size)
src=(p/'menu.hlsl').read_bytes();lines=['#pragma once']
for entry,target in [(b'VS',b'vs_5_0'),(b'PS',b'ps_5_0')]:
 blob=c.c_void_p();error=c.c_void_p();hr=compile(src,len(src),b'menu.hlsl',None,None,entry,target,1<<15,0,c.byref(blob),c.byref(error))
 if hr<0:raise RuntimeError(read(error).decode())
 data=read(blob);name=entry.decode();lines.append('static const unsigned char '+name+'[]={'+','.join(str(b) for b in data)+'};')
 method(blob,2,c.c_ulong)(blob)
(p/'Shaders.h').write_text('\n'.join(lines));print('Compiled UI vertex/pixel shaders offline; no GPU or game used.')
