#pragma once
// Straight-alpha artwork needs alpha-weighted colour when reduced, otherwise
// transparent border pixels darken the white outlines. Font atlases opt out.
namespace IconMipmaps {
inline unsigned Levels(unsigned w,unsigned h){unsigned n=1;while(w>1||h>1){w=w>1?w/2:1;h=h>1?h/2:1;++n;}return n;}
inline void Reduce(const unsigned char* src,unsigned w,unsigned h,unsigned pitch,
                   unsigned char* dst,unsigned nw,unsigned nh,unsigned newPitch){
 for(unsigned y=0;y<nh;++y)for(unsigned x=0;x<nw;++x){
  unsigned alpha=0,colour[3]={},count=0;
  for(unsigned sy=y*h/nh;sy<(y+1)*h/nh;++sy)for(unsigned sx=x*w/nw;sx<(x+1)*w/nw;++sx){
   const auto* p=src+sy*pitch+sx*4;alpha+=p[3];++count;
   for(unsigned c=0;c<3;++c)colour[c]+=p[c]*p[3];
  }
  auto* p=dst+y*newPitch+x*4;
  for(unsigned c=0;c<3;++c)p[c]=alpha?(unsigned char)((colour[c]+alpha/2)/alpha):0;
  p[3]=(unsigned char)((alpha+count/2)/count);
 }
}
}
