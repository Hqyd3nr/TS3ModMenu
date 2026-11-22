from pathlib import Path
from PIL import Image, ImageFont, ImageDraw
import json,math,shutil,hashlib,sys,io
ROOT=Path('outputs/CompleteEdition/UnlocksMenu/assets')
OUT=Path('outputs/CompleteEdition/ModMenu/assets')
WORK=Path('work/CompleteEdition/mod-menu')
font_specs=[('Nunito18','nunito-latin-700-normal.woff2',18),('Nunito22','nunito-latin-700-normal.woff2',22),('Nunito24','nunito-latin-700-normal.woff2',24),('Lilita30','lilita-one-latin-400-normal.woff2',30),('Lilita36','lilita-one-latin-400-normal.woff2',36)]
translations=(WORK/'localization/translations.tsv').read_text(encoding='utf-8')
codes=sorted(set(range(32,127))|{ord(c) for c in (translations+'English Español Français Deutsch Italiano') if ord(c)>=32}|{0x2013,0x2014,0x2026})
glyphs=[];fonts=[];images=[]
width=1024;x=2;y=2;rowh=0
for fi,(name,file,pixels) in enumerate(font_specs):
 f=ImageFont.truetype(str(ROOT/'fonts'/file),pixels)
 ascent,descent=f.getmetrics();first=len(glyphs)
 for code in codes:
  ch=chr(code);bbox=f.getbbox(ch,anchor='ls');l,t,r,b=bbox;w=max(0,r-l);h=max(0,b-t)
  if x+w+2>width:x=2;y+=rowh+4;rowh=0
  g=dict(font=fi,codepoint=code,x=x,y=y,width=w,height=h,bearingX=l,bearingY=t,advance=float(f.getlength(ch)))
  glyphs.append(g)
  if w and h:
   im=Image.new('RGBA',(w,h),(255,255,255,0))
   ImageDraw.Draw(im).text((-l,-t),ch,font=f,fill=(255,255,255,255),anchor='ls')
   images.append((im,(x,y)))
  x+=w+4;rowh=max(rowh,h)
 fonts.append(dict(name=name,pixels=pixels,ascent=ascent,descent=descent,lineHeight=ascent+descent,firstGlyph=first,glyphCount=len(codes),source=str((ROOT/'fonts'/file).resolve())))
height=1<<math.ceil(math.log2(y+rowh+2))
atlas=Image.new('RGBA',(width,height),(255,255,255,0))
for im,pos in images:atlas.paste(im,pos)
(OUT/'font-atlas.rgba').write_bytes(atlas.tobytes())
atlas.save(WORK/'font-atlas-preview.png')
icon_specs=[
 ('Player','charhead_buzz.png'),('Woody','charhead_woody.png'),('Jessie','charhead_jessie.png'),
 ('Fly','icon_parachute.png'),('Wings','icon_addwings.png'),('Coins','icon_gold.png'),('Coin','icon_gold_single.png'),('PixarBalls','icon_ipball.png'),
 ('Weapons','icon_lazerblaster.png'),('Tools','icon_toolbox.png'),('Speed','icon_turbo.png'),('Jump','icon_walljump.png'),
 ('Teleport','icon_checkpoint.png'),('Position','icon_compass.png'),('Unlock','icon_unlocked.png'),('Lock',str((WORK/'native-status-icons/icon_locked.png').resolve())),
 ('PlayerActions','icon_charge.png'),('Characters','icon_coop.png'),('Close','icon_cancel.png'),('Info','icon_info.png'),
 ('GameLogo','../logo.png'),('Lightning','icon_lightningrod.png'),('UfoGun','icon_abductionator.png'),('Wand','icon_wand.png'),('Camera','icon_camera.png'),('Pickaxe','icon_pickaxe.png'),('Star','icon_stargold.png'),('Bullseye','up_bullseye.png'),('Zurg','up_zurg.png'),('Dragon','up_dragon.png')]
sys.path.insert(0,str(WORK.parent))
from archives import Archive
cursor_archive=Archive(r'C:\Program Files (x86)\Steam\steamapps\common\Toy Story 3 Complete Edition\assets\ui\pointercursor\pointercursor.zip')
for ident,entry in [('GameCursor','arrow.oct'),('GamePointer','pointer.oct')]:
 data=cursor_archive.read(entry);offset=data.index(b'DDS ')
 Image.open(io.BytesIO(data[offset:])).convert('RGBA').save(WORK/(ident+'.png'))
pc_archive=Archive(r'C:\Program Files (x86)\Steam\steamapps\common\Toy Story 3 Complete Edition\assets\ui\screens\screens.zip')
data=pc_archive.read('pc_cursor.oct');offset=data.index(b'DDS ')
Image.open(io.BytesIO(data[offset:])).convert('RGBA').save(WORK/'GameCursor.png')
icon_specs += [('Shield',str((WORK/'native-status-icons/icon_fireball.png').resolve())),('GameCursor',str((WORK/'GameCursor.png').resolve())),('GamePointer',str((WORK/'GamePointer.png').resolve()))]
icon_specs += [('NpcTown','charicon_blockhead.png'),('NpcAlien','charicon_alien.png'),('NpcBandit','charicon_bendito.png'),('NpcRed','charicon_outlaw1.png'),('NpcGreen','charicon_outlaw2.png'),('NpcPurple','charicon_outlaw3.png'),('NpcCaptain',str((WORK/'npcs/captain-robot-icon.png').resolve())),('NpcRobot',str((WORK/'npcs/robot-icon.png').resolve())),('NpcGargoyle',str((WORK/'npcs/gargoyle-icon.png').resolve())),('NpcFairy',str((WORK/'npcs/fairy-icon.png').resolve()))]
icon_specs += [(name,str((WORK/'game-style-icons'/(name+'.png')).resolve())) for name in ['ObjectBarrel', 'ObjectLargeBarrel', 'ObjectCrate', 'ObjectBall', 'ObjectCarrot', 'ObjectCorn', 'ObjectPumpkin', 'ObjectTomato']]
icon_specs += [('Warning',str((WORK/'native-status-icons/icon_warning.png').resolve()))]
# Use authored character portraits where present, otherwise the exact outfit art.
icon_specs += [('NpcPioneer','cl_bh_pioneerman.png'),('NpcMiner','charicon_miner1.png'),('NpcFarmer','m_wstrn_b_farmer.png'),('NpcWestern','charicon_westernguy.png'),('NpcDanceMan','m_wstrn_b_sqdanceman2.png'),('NpcDanceWoman','f_wstrn_b_sqdancewoman1.png')]
icon_specs += [('NpcAlien'+name,'alien_b_'+file+'.png') for name,file in [('Default','default'),('Commando','commando'),('Ogre','ogre'),('StarCom1','starcomalien1'),('StarCom2','starcomalien2'),('Zombie','zombie'),('Mummy','mummy'),('Renegade','renegade'),('Deputy','deputy'),('Cowboy','cowboy'),('Blacksmith','blacksmith'),('Mechanic','mechanic'),('Tourist','tourist'),('ZurgFan','zurgy3'),('Disco','disco'),('Strawberry','strawberry')]]
icon_specs += [("Timer", "icon_timer.png")]
icon_specs += [("Pictomatic", "icon_pictomatic.png")]
prompts=Path('work/CompleteEdition/controller-prompts/inventory/textures')
icon_specs += [(name,str((prompts/(file+'.png')).resolve())) for name,file in [('PadA','button_x'),('PadB','button_circle'),('PadLeftStick','stick'),('PadRightStick','arrow_down')]]
icon_specs += [('NpcMuffin','icon_muffinhaunted.png'),('NpcPete','charicon_stinkypete.png'),('NpcWheezy','charicon_wheezy.png'),('NpcBoPeep','charicon_bopeep.png'),('NpcPricklepants','charicon_hedgehog.png')]
icon_specs += [('NpcZurg','charicon_zurg.png'),('NpcHamm','charicon_hamm.png')]
icon_specs += [('Npc'+name,'charicon_'+file+'.png') for name,file in [('Slinky','slinky'),('Rex','rex'),('Lotso','lotso'),('BigBaby','bigbaby'),('Buttercup','buttercup'),('Chunk','chunk'),('Sarge','sarge'),('Sparks','robothug'),('Stretch','octopus'),('Trixie','trixie'),('Twitch','twitch'),('DollyWitch','dollywitch')]]
icon_specs += [('VehicleEntry','icon_vehicleentry.png'),('VehicleSports','car_corvette.png'),('VehicleMini','car_miniracer.png'),('VehicleMonster','car_corvette.png'),('VehicleZurg','car_zurg.png'),('WorldWestern','icon_compass.png'),('WorldHaunted','mod_hauntedhouse.png'),('WorldEnchanted','mod_enchantedglen.png'),('WorldZurg','mod_spaceport.png')]
icons=[]
for ident,file in icon_specs:
 src=ROOT/'game'/file;im=Image.open(src).convert('RGBA')
 target={'GameLogo':48,'Info':22,'Shield':28,'GameCursor':32,'GamePointer':32,'NpcRobot':64,'NpcCaptain':64,'NpcGargoyle':64,'NpcFairy':64,'Bullseye':128,'Zurg':128,'Dragon':128}.get(ident)
 if ident.startswith('Object'):target=64
 if target:im.thumbnail((target,target),Image.Resampling.LANCZOS)
 out=OUT/(ident.lower()+'.rgba');out.write_bytes(im.tobytes())
 alpha=im.getchannel('A');mn,mx=alpha.getextrema()
 icons.append(dict(name=ident,file=out.name,width=im.width,height=im.height,bytes=out.stat().st_size,alphaMin=mn,alphaMax=mx,source=str(src.resolve()),sha256=hashlib.sha256(out.read_bytes()).hexdigest()))
for file in ['OFL-LilitaOne.txt','OFL-Nunito.txt']:shutil.copy2(ROOT/'fonts'/file,OUT/file)
palette={'background':'#c9e3f6','sidebar':'#eaf5fd','header':'#f7fbff','panel':'#ffffff','control':'#f2f7fc','border':'#bdcee1','controlBorder':'#a9bfd9','text':'#1b2742','muted':'#4e607d','accentBlue':'#2c6bd3','selectedText':'#1d55b8','selectedFill':'#e0edff','buttonGold':'#f0b000','buttonGoldBorder':'#d69b00','buttonGoldText':'#2a1d00','success':'#23724b','successFill':'#d9f3e7','disabledText':'#71849c'}
manifest={'source':str(ROOT.resolve()),'themeSource':'outputs/CompleteEdition/UnlocksMenu/guide.css','palette':palette,'fontAtlas':{'file':'font-atlas.rgba','width':width,'height':height,'format':'RGBA8_UNORM','bytes':width*height*4,'textOrigin':'top-left; baseline = y + font.ascent; glyph top-left = baseline + bearingY, x + bearingX','fonts':fonts,'glyphs':glyphs},'icons':icons,'notes':['All conversion is CPU-only. Original game artwork is packed into RGBA8; logo, info, shield and cursor icons are CPU downsampled with Lanczos for their displayed size.','WOFF2 is rasterized directly by bundled Pillow FreeType; no TTF needed.','Load static textures once; do not reread them every frame.','Use white font pixels tinted by vertex color. Use icon pixels with white tint.','Game artwork retains original Disney/Pixar rights. Included font licenses accompany the atlas.']}
(OUT/'assets.json').write_text(json.dumps(manifest,indent=2))
(WORK/'assets-manifest.json').write_text(json.dumps({k:v for k,v in manifest.items() if k!='fontAtlas'}|{'fontAtlas':{k:v for k,v in manifest['fontAtlas'].items() if k!='glyphs'}},indent=2))
lines=['#pragma once','#include <cstdint>','namespace CheatMenuAssets {','struct Font { const char* name; int pixels, ascent, descent, lineHeight, firstGlyph, glyphCount; };','struct Glyph { std::uint32_t codepoint; int x, y, width, height, bearingX, bearingY; float advance; };','struct Icon { const char* name; const char* file; int width, height; };',f'inline constexpr int kFontAtlasWidth={width}, kFontAtlasHeight={height};','inline constexpr const char* kFontAtlasFile="font-atlas.rgba";','enum FontIndex { Nunito18, Nunito22, Nunito24, Lilita30, Lilita36 };','inline constexpr Font kFonts[]={']
for f in fonts:lines.append(' {"%s",%d,%d,%d,%d,%d,%d},'%(f['name'],f['pixels'],f['ascent'],f['descent'],f['lineHeight'],f['firstGlyph'],f['glyphCount']))
lines+=['};','inline constexpr Glyph kGlyphs[]={']
for g in glyphs:lines.append(' {%d,%d,%d,%d,%d,%d,%d,%.6ff},'%(g['codepoint'],g['x'],g['y'],g['width'],g['height'],g['bearingX'],g['bearingY'],g['advance']))
lines+=['};','enum IconIndex { '+', '.join(i['name'] for i in icons)+' };','inline constexpr Icon kIcons[]={']
for i in icons:lines.append(' {"%s","%s",%d,%d},'%(i['name'],i['file'],i['width'],i['height']))
lines+=['};','inline constexpr int kIconCount=sizeof(kIcons)/sizeof(kIcons[0]);','inline const Glyph* FindGlyph(FontIndex font, std::uint32_t codepoint) { const Font& f=kFonts[font]; for(int i=0;i<f.glyphCount;++i)if(kGlyphs[f.firstGlyph+i].codepoint==codepoint)return &kGlyphs[f.firstGlyph+i]; return &kGlyphs[f.firstGlyph+31]; }','}']
(OUT/'Assets.generated.h').write_text('\n'.join(lines)+'\n')
(OUT/'README.txt').write_text('Prepared art and fonts for the in-game Mod Menu. Font and icon pixels use tightly packed RGBA8 bytes. See Assets.generated.h or assets.json for sizes and glyph metrics. Font baseline is top y + ascent; each glyph uses bearingX/bearingY from the baseline. Use linear filtering and clamp edges. A transparent gutter surrounds every glyph. The original Nunito and Lilita One font licenses are included. Native game artwork comes from the game's own assets and cursor archive; small icons are CPU filtered to their display size. This does not install or launch the game.\n')
print(json.dumps({'fontAtlas':[width,height],'fontCount':len(fonts),'glyphCount':len(glyphs),'iconCount':len(icons),'totalBytes':sum(p.stat().st_size for p in OUT.iterdir() if p.is_file())},indent=2))
