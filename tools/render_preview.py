from pathlib import Path
import json,re
from PIL import Image,ImageDraw
root=Path('outputs/CompleteEdition/ModMenu');a=root/'assets';m=json.loads((a/'assets.json').read_text())
at=m['fontAtlas'];atlas=Image.frombytes('RGBA',(at['width'],at['height']),(a/at['file']).read_bytes())
fonts={f['name']:f for f in at['fonts']};glyphs=at['glyphs'];icons={i['name']:i for i in m['icons']}
im=Image.new('RGBA',(1040,680),(0,0,0,0));d=ImageDraw.Draw(im)
ink='#1b2742';muted='#4e607d';blue='#2c6bd3';paper='#ffffff';line='#bdcee1';gold='#f0b000'
checks=[]
def rgba(color):
 if isinstance(color,tuple):return color
 return tuple(int(color[i:i+2],16) for i in (1,3,5))+(255,)
def box(x,y,w,h,c,r=8):d.rounded_rectangle((x,y,x+w,y+h),radius=r,fill=rgba(c))
def border(x,y,w,h):d.rounded_rectangle((x,y,x+w,y+h),radius=8,outline=rgba(line),width=1)
def circle(x,y,r,c):d.ellipse((x-r,y-r,x+r,y+r),fill=rgba(c))
def width(text,font='Nunito18'):
 f=fonts[font];gs={g['codepoint']:g for g in glyphs[f['firstGlyph']:f['firstGlyph']+f['glyphCount']]}
 return sum(gs.get(ord(ch),gs[63])['advance'] for ch in text)
def text(x,y,t,font='Nunito18',color=ink,limit=1008):
 f=fonts[font];gs={g['codepoint']:g for g in glyphs[f['firstGlyph']:f['firstGlyph']+f['glyphCount']]};start=x
 checks.append({'text':t,'x':x,'y':y,'width':width(t,font),'right':x+width(t,font),'limit':limit,'fits':x+width(t,font)<=limit})
 for ch in t:
  g=gs.get(ord(ch),gs[63]);w=g['width'];h=g['height']
  if w and h:
   mask=atlas.crop((g['x'],g['y'],g['x']+w,g['y']+h)).getchannel('A');tile=Image.new('RGBA',(w,h),rgba(color));tile.putalpha(mask)
   im.alpha_composite(tile,(round(x+g['bearingX']),round(y+f['ascent']+g['bearingY'])))
  x+=g['advance']
def icon(name,x,y,size,opacity=255):
 spec=icons[name];img=Image.frombytes('RGBA',(spec['width'],spec['height']),(a/spec['file']).read_bytes());
 if opacity!=255:img.putalpha(img.getchannel('A').point(lambda v:round(v*opacity/255)))
 if name=='GameLogo':
  img.thumbnail((size,size),Image.Resampling.LANCZOS);im.alpha_composite(img,(x+(size-img.width)//2,y+(size-img.height)//2))
 else:
  img=img.resize((size,size),Image.Resampling.LANCZOS);im.alpha_composite(img,(x,y))
def button(label,x,y,w,h):
 box(x,y,w,h,gold if label in ['Add coins','Add Pixar Balls'] else '#f2f7fc',10);d.rounded_rectangle((x,y,x+w,y+h),radius=10,outline=rgba(line),width=1);text(x+(w-width(label))/2,y+(h-26)/2,label,limit=x+w)

def rule(x,y,w):d.line((x,y,x+w,y),fill=rgba(line),width=1)
def panel(x,y,w,h,title):box(x,y,w,h,paper);border(x,y,w,h);text(x+20,y+16,title,color=muted)
def toggle(x,y,on=False):box(x,y,48,26,blue if on else '#cdd9e7',13);circle(x+(35 if on else 13),y+13,10,paper)
def heading(title,description):text(240,96,title,'Nunito24');text(240,134,description,color=muted)
def chrome(page=0,story=False):
 global im,d
 im=Image.new('RGBA',(1040,680),(0,0,0,0));d=ImageDraw.Draw(im)
 box(0,0,1040,680,'#f2f7fc',12);box(0,0,208,680,'#e4effa',12);box(196,0,12,680,'#e4effa',0);border(0,0,1040,680);rule(208,68,832);d.line((208,0,208,680),fill=rgba(line))
 box(208,69,831,560,'#c9e3f6',0)
 for row in range(3):
  for col in range(3):
   x,y=208+340*col,69+270*row
   # Clip the CSS-equivalent cloud tile to the content background.
   tile=Image.new('RGBA',(340,270),(0,0,0,0));td=ImageDraw.Draw(tile)
   for cx,cy,r in [(55,64,22),(85,48,32),(118,65,24)]:td.ellipse((cx-r,cy-r,cx+r,cy+r),fill='#eaf5fd')
   td.ellipse((22,62,150,98),fill='#eaf5fd')
   w,h=min(340,1039-x),min(270,629-y)
   if w>0 and h>0:im.alpha_composite(tile.crop((0,0,w,h)),(x,y))
 icon('GameLogo',18,10,48);text(78,23,'Cheats','Nunito22',limit=196)
 tabs=[('Player','PlayerActions'),('Currency','Coins'),('Teleport','Teleport'),('Objects','Tools'),('Weapons','Weapons')]
 text(240,23,'Mod menu',color=muted);text(357,23,'/',color=muted);text(378,23,tabs[page][0])
 box(880,20,128,30,'#e0ebf7',6);mode='Story Mode' if story else 'Toy Box';text(944-width(mode)/2,22,mode,color=muted)
 text(20,90,'CATEGORIES',color=muted,limit=196)
 for i,(label,art) in enumerate(tabs):
  y=126+66*i;available=not(story and i in [1,4])
  if not available:box(12,y,184,52,'#e0e7ef',6)
  elif i==page:box(12,y,184,52,'#cfe1f7',6);box(12,y+12,3,28,blue,1)
  icon(art,24,y+12,28,255 if available else 90);text(64,y+(13 if available else 2),label,color=('#8592a2' if not available else blue if i==page else ink),limit=196)
  if not available:text(64,y+27,'Toy Box only',color='#8592a2',limit=196)
 rule(240,630,768);text(240,646,'Ready. Changes apply to your current character.',color=muted)
def player(story=False):
 chrome(0,story);heading('Player','Movement, protection and flight controls.');panel(634,184,374,314,'Movement & protection')
 for i,(label,desc,art) in enumerate([('Super speed','Move faster on foot','Speed'),('Super jump','Jump higher','Jump'),('God mode','Protection from normal damage','Shield')]):
  y=238+80*i;icon(art,654,y,28);text(694,y,label,limit=934);text(654,y+33,desc,color=muted,limit=988);toggle(940,y+1)
  if i<2:rule(654,y+65,334)
 panel(240,184,374,432,'Flight');icon('Fly',260,238,28);text(300,238,'Flight / noclip',limit=540);toggle(546,239)
 text(260,274,'Fly freely through scenery',color=muted);rule(260,310,334);text(260,318,'Speed',color=muted);text(594-width('15'),318,'15',color=muted)
 box(260,355,334,4,'#d9e4f0',2);box(260,355,334*14/199,4,'#adbaca',2);circle(260+334*14/199,357,7,'#adbaca')
 text(260,389,'Flight is off',color=muted);rule(260,430,334)
 text(260,442,'Keyboard',color=blue,limit=594);text(260,466,'WASD: move   /   Shift: boost',color=muted,limit=594);text(260,490,'Space / E: up   Ctrl / Q: down',color=muted,limit=594)
 text(260,519,'Controller',color=blue,limit=594)
 for y,label,key in [(542,'Move / look','Left / right stick'),(566,'Up / down','RT / LT'),(590,'Boost','Click left stick')]:text(260,y,label,color=muted,limit=390);text(398,y,key,color=muted,limit=594)
player();im.save(root/'design-preview.png')
player(True);im.save(root/'story-preview.png')
chrome(4);heading('Weapons & tools','Add and equip an item from the Toy Box weapon cycle.')
for i,(label,art) in enumerate([('Lightning Rod','Lightning'),('UFO Gun','UfoGun'),('Fairy Wand','Wand'),('Blaster','Weapons'),('Pickaxe','Pickaxe'),('Toolbox','Tools'),('Camera','Camera')]):
 x,y=240+394*(i%2),184+100*(i//2);box(x,y,374,84,paper);border(x,y,374,84);icon(art,x+14,y+22,40);text(x+66,y+15,label,limit=x+234);text(x+66,y+45,'Not added',color=muted,limit=x+234);button('Add & equip',x+238,y+26,122,32)
text(240,592,'Added weapons stay unlocked when you save.',color=muted);im.save(root/'weapons-preview.png')
chrome(3);heading('Objects','Spawn a carryable prop in front of your character.')
for i,label in enumerate(['Medium barrel','Large barrel','Wooden crate','Ball','Carrot','Corn','Pumpkin','Tomato']):
 x,y=240+394*(i%2),184+82*(i//2);box(x,y,374,66,paper);border(x,y,374,66);text(x+18,y+21,label,limit=x+252);button('Spawn',x+262,y+17,94,32)
button('Clear spawned objects',240,530,256,40);text(520,538,'Only objects spawned by this menu',color=muted);im.save(root/'objects-preview.png')
chrome(1);heading('Currency','Add coins or Pixar Balls to your Toy Box.')
for i,label in enumerate(['+100','+1,000','+10,000','+100,000']):button(label,240+196*i,184,180,38)
for i,label in enumerate(['Coins','Pixar Balls']):
 x=240+394*i;panel(x,246,374,180,label);icon('PixarBalls' if i else 'Coins',x+294,266,52);text(x+20,310,'999999','Nunito24');button('Add Pixar Balls' if i else 'Add coins',x+20,360,334,42)
icon('Info',242,458,22);text(278,456,'Collect your currency, then save normally to keep it.',color=muted);im.save(root/'currency-preview.png')
chrome(2);heading('Teleport',"Remember a location and return across this world's regions.")
panel(240,184,768,328,'Return point');icon('Position',260,242,44);text(322,241,'Remember this spot','Nunito22');text(322,278,'Keep a position to return to while exploring.',color=muted)
box(260,328,728,48,'#edf4fc',6);text(278,340,'No return point saved',color=muted);button('Remember position',260,424,354,42);button('Teleport back',634,424,354,42);text(240,540,"Return within this level. Separate maps aren't supported.",color=muted);im.save(root/'teleport-preview.png')
source=Path('work/CompleteEdition/mod-menu/ModMenuUI.cpp').read_text()
expression=re.search(r'\bscale=(Min\(1\.f.*?);',source).group(1)
expression=re.sub(r'(\d+)\.f',r'\1.0',expression);expression=re.sub(r'(\d+(?:\.\d+)?)f\b',r'\1',expression)
expression=expression.replace('Min(','min(').replace('viewport.x','width_px').replace('viewport.y','height_px')
viewport_checks=[]
for width_px,height_px in [(1280,720),(1920,1080),(2560,1440),(3440,1440),(1024,768),(800,600)]:
 factor=eval(expression,{'__builtins__':{}},{'min':min,'width_px':width_px,'height_px':height_px})
 w,h=1040*factor,680*factor;x,y=(width_px-w)/2,(height_px-h)/2
 fits=factor>=.25 and x>=24 and y>=24 and h<=height_px*.90+.001;assert fits
 if (width_px,height_px)==(2560,1440):assert (w,h)==(1040,680)
 viewport_checks.append({'viewport':[width_px,height_px],'menu_size':[w,h],'fits':fits})
assert all(c['fits'] for c in checks),[c for c in checks if not c['fits']]
assert 'Icon(Woody' not in source and 'TAB  Close' not in source
assert 'Ready()&&state.flightEnabled' in source and 'int(slider+0.5f)!=15&&Button' in source
assert 'SelectNewGun' in Path('outputs/CompleteEdition/BugFixes/ModMovementWeapons.h').read_text()
(root/'design-preview-layout.json').write_text(json.dumps({'preview':'Offline CPU preview, not gameplay','viewport_checks':viewport_checks,'checks':checks,'all_text_fits':True},indent=2))
(root/'design-preview-README.txt').write_text('Offline CPU-rendered design previews of the light desktop layout, not game screenshots. All five pages and disabled Story Mode categories are shown. Source scale checked at six viewport sizes. Gameplay and overlay need user testing.\n')
print('Six viewport checks and',len(checks),'text boundaries passed; five pages plus Story preview rendered.')
