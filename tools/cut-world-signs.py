# Cut the well-tunnel area stickers off their wood boards so they match the
# free-standing car-park logos. Wood is flood-filled from the edges; the
# sticker's white outline (thickened to close small gaps) is a barrier, then a
# few passes peel the wood fringe left against the outline. Writes *_cut.png.
import sys,colorsys
from pathlib import Path
from collections import deque
from PIL import Image,ImageFilter,ImageChops
SIG=Path(sys.argv[1] if len(sys.argv)>1 else 'work/CompleteEdition/mod-menu/world-signs')
def wood(p):
    r,g,b=p[:3];h,s,v=colorsys.rgb_to_hsv(r/255,g/255,b/255)
    return v<0.70 and 0.15<s<0.85 and (h<0.13 or h>0.97)
def cut_mask(im,grow=7,peel=4):
    w,h=im.size;px=im.load()
    white=Image.new('L',(w,h));wp=white.load()
    for y in range(h):
        for x in range(w):
            r,g,b=px[x,y][:3]
            if min(r,g,b)>175 and max(r,g,b)-min(r,g,b)<60:wp[x,y]=255
    barrier=white.filter(ImageFilter.MaxFilter(grow)).load()
    bg=bytearray(w*h);q=deque()
    for x in range(w):q+=[(x,0),(x,h-1)]
    for y in range(h):q+=[(0,y),(w-1,y)]
    while q:
        x,y=q.popleft()
        if x<0 or y<0 or x>=w or y>=h or bg[y*w+x] or barrier[x,y] or not wood(px[x,y]):continue
        bg[y*w+x]=1;q+=[(x+1,y),(x-1,y),(x,y+1),(x,y-1)]
    for _ in range(peel):
        edge=[(x,y) for y in range(1,h-1) for x in range(1,w-1) if not bg[y*w+x] and wood(px[x,y]) and (bg[y*w+x-1] or bg[y*w+x+1] or bg[(y-1)*w+x] or bg[(y+1)*w+x])]
        for x,y in edge:bg[y*w+x]=1
    return Image.frombytes('L',(w,h),bytes(0 if b else 255 for b in bg)).filter(ImageFilter.MedianFilter(5)).filter(ImageFilter.GaussianBlur(0.7))
def cut(im,grow=7,peel=4):return apply(im,cut_mask(im,grow,peel))
def apply(im,m):
    out=im.copy();out.putalpha(m);return out.crop(m.point(lambda a:255 if a>40 else 0).getbbox())
# The stagecoach sticker's hitching post is the same brown as the board behind it and its
# white outline is faint on the left, so the wood fill runs into the post's left half.
# Put the post back from the original pixels: fill outward from inside the post, stopped by
# its own black outline and kept to the post's column (x 89-114, y 30-122; base widened to
# x 84 from y 104), then keep that outline and the white border pixels just outside it.
def keep_post(im,m):
    px=im.load();w,h=im.size
    dark=lambda p:max(p[:3])<48
    white=lambda p:min(p[:3])>175 and max(p[:3])-min(p[:3])<60
    inside=Image.new('L',(w,h));ip=inside.load();q=deque([(95,70)])
    while q:
        x,y=q.popleft()
        if (x<89 and not (y>=104 and x>=84)) or y<30 or x>114 or y>122 or ip[x,y] or dark(px[x,y]):continue
        ip[x,y]=255;q+=[(x+1,y),(x-1,y),(x,y+1),(x,y-1)]
    ring=inside.filter(ImageFilter.MaxFilter(5)).load()
    band=Image.new('L',(w,h));bp=band.load()
    for y in range(h):
        for x in range(w):
            if ip[x,y] or (ring[x,y] and dark(px[x,y])):bp[x,y]=255
    outer=band.filter(ImageFilter.MaxFilter(9)).load()
    for y in range(h):
        for x in range(w):
            if outer[x,y] and white(px[x,y]):bp[x,y]=255
    return ImageChops.lighter(m,band.filter(ImageFilter.MedianFilter(3)))
if __name__=='__main__':
    for p in sorted(SIG.glob('wc_*_d.png')):
        im=Image.open(p).convert('RGBA');m=cut_mask(im)
        if p.stem=='wc_stagecoach_d':m=keep_post(im,m)
        c=apply(im,m);c.save(p.with_name(p.stem+'_cut.png'));print(p.stem,c.size)
