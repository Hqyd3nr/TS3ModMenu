# Assets.generated.h is the source of truth for shipped icons (assets.json lagged
# behind later feature scripts). This script:
#  1) shrinks any square icon larger than 128px to 128px (premultiplied area average),
#  2) adds the Teleport place icons from the game's own art,
#  3) rewrites assets.json so every shipped icon is listed with its real size/hash.
# Run with a Python that has Pillow, from the workspace root.
import json,re,hashlib
from pathlib import Path
from PIL import Image
A=Path('outputs/CompleteEdition/ModMenu/assets');GAME=Path('outputs/CompleteEdition/UnlocksMenu/assets/game');T=128
header=(A/'Assets.generated.h').read_text(encoding='utf-8')
manifest=json.loads((A/'assets.json').read_text(encoding='utf-8-sig'));byname={i['name']:i for i in manifest['icons']}
line=re.compile(r'^ \{"(\w+)","([\w.\-]+)",(\d+),(\d+)\},$',re.M)
def store(name,file,im,source):
    im=im.convert('RGBA');data=im.tobytes();(A/file).write_bytes(data);a=im.getchannel('A').getextrema()
    e=byname.get(name) or {'name':name};e.update(file=file,width=im.width,height=im.height,bytes=len(data),source=source,sha256=hashlib.sha256(data).hexdigest(),alphaMin=a[0],alphaMax=a[1])
    if name not in byname:manifest['icons'].append(e);byname[name]=e
def shrink(im,size):
    # Fit the longer side to size, keeping the aspect ratio (wide signs stay wide).
    if im.width<=size and im.height<=size:return im
    w,h=(size,max(1,round(im.height*size/im.width))) if im.width>=im.height else (max(1,round(im.width*size/im.height)),size)
    exact=im.width%w==0 and im.height%h==0 and im.width//w==im.height//h
    return im.convert('RGBa').resize((w,h),Image.Resampling.BOX if exact else Image.Resampling.LANCZOS).convert('RGBA')
changed=[]
for name,file,w,h in line.findall(header):
    w,h=int(w),int(h);raw=(A/file).read_bytes();assert len(raw)==w*h*4,name
    im=Image.frombytes('RGBA',(w,h),raw)
    if w==h and w>T:im=shrink(im,T);changed.append((name,w))
    store(name,file,im,byname.get(name,{}).get('source','existing '+file))
    header=header.replace(' {"%s","%s",%d,%d},'%(name,file,w,h),' {"%s","%s",%d,%d},'%(name,file,im.width,im.height))
# Teleport place icons: the game's own area signs from the well tunnels and the
# car-park logos (extract-world-signs.py). Existing entries are regenerated.
SIGNS=Path('work/CompleteEdition/mod-menu/world-signs')
# Well signs are cut off their wood boards (cut-world-signs.py) to match the park logos.
new=[('PlaceTown','placetown.rgba',SIGNS/'wc_town_hall_d_cut.png'),('PlaceFarm','placefarm.rgba',SIGNS/'wc_uppercorral_d_cut.png'),
     ('PlaceSuburbs','placesuburbs.rgba',SIGNS/'wc_suburbs_d_cut.png'),('PlaceBandit','placebandit.rgba',SIGNS/'wc_bendito_d_cut.png'),
     ('PlaceQuarry','placequarry.rgba',SIGNS/'wc_quarry_d_cut.png'),('PlaceStagecoach','placestagecoach.rgba',SIGNS/'wc_stagecoach_d_cut.png'),
     ('PlaceHaunted','placehaunted.rgba',SIGNS/'wc_haunted_d_cut.png'),('PlaceEnchanted','placeenchanted.rgba',SIGNS/'wc_enchanted_d_cut.png'),
     ('PlaceSpaceport','placespaceport.rgba',SIGNS/'wc_spaceport_d_cut.png'),
     ('PlaceStuntPark','placestuntpark.rgba',SIGNS/'stunt_motocross_sign_alpha.png'),('PlaceSkatePark','placeskatepark.rgba',SIGNS/'stunt_skatepark_sign.png'),
     # Player > Abilities: the game's own Buzz wrist-laser HUD icon.
     ('BuzzLaser','buzzlaser.rgba',GAME/'icon_buzzlaser.png'),
     # World tab: the game's ringed-planet icon (icon_ammocowtapult), user's pick 2026-11-11.
     ('WorldCategory','worldcategory.rgba',GAME/'icon_ammocowtapult.png')]
# Settings > Controls: the game's own Xbox prompt textures (ui/fonts/ctrlbtns.oct, names are PS-style).
# The game has no D-pad Down-only art; it is arrow_up mirrored top-to-bottom.
PADS=Path('work/CompleteEdition/controller-prompts/inventory/textures');DERIVED=Path('work/CompleteEdition/mod-menu/controller-icons');DERIVED.mkdir(exist_ok=True)
Image.open(PADS/'arrow_up.png').convert('RGBA').transpose(Image.Transpose.FLIP_TOP_BOTTOM).save(DERIVED/'dpad_down_from_arrow_up.png')
new+=[('PadX','padx.rgba',PADS/'button_square.png'),('PadY','pady.rgba',PADS/'button_triangle.png'),
      ('PadLB','padlb.rgba',PADS/'button_l1.png'),('PadRB','padrb.rgba',PADS/'button_r1.png'),
      ('PadLT','padlt.rgba',PADS/'button_l2.png'),('PadRT','padrt.rgba',PADS/'button_r2.png'),
      ('PadUp','padup.rgba',PADS/'arrow_up.png'),('PadDown','paddown.rgba',DERIVED/'dpad_down_from_arrow_up.png'),
      ('PadLeft','padleft.rgba',PADS/'arrow_left.png'),('PadRight','padright.rgba',PADS/'arrow_right.png'),
      ('PadDpad','paddpad.rgba',PADS/'dpad.png')]
# Objects page: made-up prop icons removed until real game artwork exists for each prop.
removed=['PadView','PadMenu','ObjectBarrel','ObjectLargeBarrel','ObjectCrate','ObjectBall','ObjectCarrot','ObjectCorn','ObjectPumpkin','ObjectTomato']
for name,file,src in new:
    im=shrink(Image.open(src).convert('RGBA'),T)
    m=re.search(r'^ \{"%s","([\w.\-]+)",(\d+),(\d+)\},$'%name,header,re.M)
    if m:header=header.replace(m.group(0),' {"%s","%s",%d,%d},'%(name,file,im.width,im.height))
    else:
        header=header.replace('};\ninline constexpr int kIconCount',' {"%s","%s",%d,%d},\n};\ninline constexpr int kIconCount'%(name,file,im.width,im.height))
        header=re.sub(r'(enum IconIndex \{[^}]*?)( \};)',lambda x:x.group(1)+', '+name+x.group(2),header,count=1)
    store(name,file,im,str(src));changed.append((name,src.name))
for name in removed:
    m=re.search(r'^ \{"%s","([\w.\-]+)",(\d+),(\d+)\},\n'%name,header,re.M)
    if not m:continue
    header=header.replace(m.group(0),'');header=header.replace(', '+name+' }',' }').replace(', '+name+',',',')
    (A/m.group(1)).unlink(missing_ok=True);byname.pop(name,None);changed.append((name,'removed'))
order=[m[0] for m in line.findall(header)];manifest['icons']=[byname[n] for n in order]
(A/'Assets.generated.h').write_text(header,encoding='utf-8');(A/'assets.json').write_text(json.dumps(manifest,indent=2))
print(changed);print('icons',len(order))
