# Shrink oversized menu icons to 128x128. They are drawn at 52px or less at
# 1x and at most ~2x on high-resolution displays, with mipmaps. Integer-factor
# area averaging on premultiplied alpha avoids dark fringes.
from pathlib import Path
import json,hashlib,re
A=Path('outputs/CompleteEdition/ModMenu/assets');TARGET=128
manifest=json.loads((A/'assets.json').read_text(encoding='utf-8-sig'))
header=(A/'Assets.generated.h').read_text(encoding='utf-8')
changed=[]
for icon in manifest['icons']:
    w,h=icon['width'],icon['height']
    if w<=TARGET or w!=h or w%TARGET:continue
    f=w//TARGET;p=A/icon['file'];src=p.read_bytes();assert len(src)==w*h*4,icon['name']
    out=bytearray(TARGET*TARGET*4);n=f*f
    for y in range(TARGET):
        for x in range(TARGET):
            r=g=b=a=0
            for yy in range(y*f,y*f+f):
                row=(yy*w+x*f)*4
                for i in range(row,row+f*4,4):
                    al=src[i+3];r+=src[i]*al;g+=src[i+1]*al;b+=src[i+2]*al;a+=al
            o=(y*TARGET+x)*4
            if a:out[o]=min(255,(r+a//2)//a);out[o+1]=min(255,(g+a//2)//a);out[o+2]=min(255,(b+a//2)//a)
            out[o+3]=(a+n//2)//n
    p.write_bytes(out)
    alpha=out[3::4]
    icon.update(width=TARGET,height=TARGET,bytes=len(out),alphaMin=min(alpha),alphaMax=max(alpha),sha256=hashlib.sha256(out).hexdigest())
    line=' {"%s","%s",%d,%d},'%(icon['name'],icon['file'],w,h);assert header.count(line)==1,line
    header=header.replace(line,' {"%s","%s",%d,%d},'%(icon['name'],icon['file'],TARGET,TARGET))
    changed.append((icon['name'],w,TARGET))
(A/'assets.json').write_text(json.dumps(manifest,indent=2))
(A/'Assets.generated.h').write_text(header,encoding='utf-8')
print(changed)
