from pathlib import Path
import re
root=Path('outputs/CompleteEdition/ModMenu/assets')
header=(root/'Assets.generated.h').read_text()
w,h=map(int,re.search(r'kFontAtlasWidth=(\d+), kFontAtlasHeight=(\d+)',header).groups())
raw=(root/'font-atlas.rgba').read_bytes();assert len(raw)==w*h*4
alpha=raw[3::4];runs=[];i=0
while i<len(alpha):
 j=i+1
 while j<len(alpha) and j-i<65535 and alpha[j]==alpha[i]:j+=1
 count=j-i;runs.extend([count&255,count>>8,alpha[i]]);i=j
decoded=bytearray()
for i in range(0,len(runs),3):decoded.extend(bytes([runs[i+2]])*(runs[i]|runs[i+1]<<8))
assert decoded==alpha
out=Path('work/CompleteEdition/mod-menu/FallbackFont.generated.h')
out.write_text('// Embedded copy of the licensed menu font alpha atlas for missing-asset notices.\n#pragma once\nnamespace FallbackFont {\ninline constexpr unsigned width=%d,height=%d;\ninline constexpr unsigned char runs[]={\n'%(w,h)+''.join(','.join(str(v) for v in runs[i:i+48])+',\n' for i in range(0,len(runs),48))+'};\n}\n')
print('Embedded fallback font:',len(runs),'bytes; round trip verified.')
