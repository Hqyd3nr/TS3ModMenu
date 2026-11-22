# Extract Toy Box world sign textures (embedded DDS in westerntown.oct) as PNG
# sources for Teleport place icons. Run from the workspace root with a Python
# that has Pillow. Read-only on the game files.
import sys,io,re
from pathlib import Path
sys.path.insert(0,'work/CompleteEdition')
from archives import Archive
from tup_records import grouped
from PIL import Image
OUT=Path('work/CompleteEdition/mod-menu/world-signs');OUT.mkdir(exist_ok=True)
WANT=['wc_town_hall_d','wc_slinky_barn_d','wc_suburbs_d','wc_bendito_d','wc_haunted_d','wc_enchanted_d','wc_spaceport_d',
      'wc_uppercorral_d','wc_quarry_d','wc_stagecoach_d','stunt_skatepark_sign','stunt_motocross_sign_alpha']
d=Archive(r'C:\Program Files (x86)\Steam\steamapps\common\Toy Story 3 Complete Edition\assets\worlds\westerntown.zip').read('westerntown.oct')
tex=list(grouped(d,'Texture'));idx={t['fields'].get('Name'):i for i,t in enumerate(tex)}
for n in WANT:
    i=idx[n];end=tex[i+1]['offset'] if i+1<len(tex) else len(d);seg=d[tex[i]['offset']:end];k=seg.find(b'DDS ')
    im=Image.open(io.BytesIO(seg[k:])).convert('RGBA');im.save(OUT/(n+'.png'));print(n,im.size)
