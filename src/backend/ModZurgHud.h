// Player-only cosmetic override. Health values, timing and boss HUD are native.
namespace ZurgHud {
typedef void (*UpdateFn)(void*);
typedef void* (*DestroyFn)(void*,unsigned);
typedef void* (*FindFn)(void*,const char*);
typedef void* (*LoadFn)(const char*);
typedef void (*TextureFn)(void*,void*,int,int,int,int);
typedef void (*VisibleFn)(void*,bool);
typedef void (*UvFn)(void*,float,float,float,float);
static UpdateFn originalUpdate;
static DestroyFn originalDestroy;
static FindFn findSprite;
static LoadFn loadTexture;
static TextureFn setTexture;
static VisibleFn setVisible;
static UvFn setUv;
static bool ready;
struct SavedSprite {void* sprite;void* texture;void* reference;int index;WORD frame;BYTE columns,rows;float uv[8];};
struct Skin {void* screen;SavedSprite sprites[7];void* portraits[3];};
static Skin skins[4];
static void Retain(void* p){if(p)InterlockedIncrement((volatile LONG*)((BYTE*)p+8));}
static void Release(void* p){
 if(!p)return;LONG count=InterlockedDecrement((volatile LONG*)((BYTE*)p+8));
 if(count&0x3fffffff)return;
 if(count<0)((void(*)(void*))A(0x255220))(p);
 else ((void(*)(void*,unsigned))(*(void***)p)[0])(p,1);
}
static Skin* Find(void* screen){for(unsigned i=0;i<4;++i)if(skins[i].screen==screen)return &skins[i];return 0;}
static void Restore(Skin& skin){
 for(unsigned i=0;i<7;++i){SavedSprite& s=skin.sprites[i];
  setTexture(s.sprite,s.texture,s.index,s.frame,s.columns,s.rows);
  // Preserve arbitrary native UVs, including player-two orientation.
  Copy((BYTE*)s.sprite+0xb4,s.uv,sizeof(s.uv));At<DWORD>(s.sprite,0x2c)|=1;
  Release(s.reference);
 }
 skin.screen=0;
}
static bool Apply(void* screen,bool zurg){
 Skin* skin=Find(screen);
 if(!zurg){if(skin)Restore(*skin);return true;}
 if(!skin){
  for(unsigned i=0;i<4;++i)if(!skins[i].screen){skin=&skins[i];break;}
  if(!skin)return false;
  const char* names[]={"IDS_HEALTH_ICON_BUZZ","IDS_HEALTH_ICON_CAP","IDS_HEALTH_BACKGROUND_MIDDLE","IDS_HEALTH_BACKGROUND_RIGHT","IDS_HEALTH_BACKGROUND_BLUE_LEFT","IDS_HEALTH_BACKGROUND_BLUE_MIDDLE","IDS_HEALTH_BACKGROUND_BLUE_RIGHT"};
  const char* files[]={"UI\\Screens\\charIcon_Zurg.dbl","UI\\Screens\\backCircle12.dbl","UI\\Screens\\backLine12_h32_slice.dbl","UI\\Screens\\backLine12_h32_slice.dbl","UI\\Screens\\Meter12_h20_back_left.dbl","UI\\Screens\\Meter12_h20_back_tile.dbl","UI\\Screens\\Meter12_h20_back_right.dbl"};
  void* sprites[7]={};void* textures[7]={};bool left=false,ok=true;
  for(unsigned i=0;i<7;++i){sprites[i]=findSprite(screen,names[i]);
   if(i==3&&!sprites[i]){sprites[i]=findSprite(screen,"IDS_HEALTH_BACKGROUND_LEFT");left=true;}
   if(!sprites[i]||!At<void*>(sprites[i],0x30)||!At<void*>(sprites[i],0x120)){ok=false;break;}
  }
  void* portraits[]={sprites[0],findSprite(screen,"IDS_HEALTH_ICON_WOODY"),findSprite(screen,"IDS_HEALTH_ICON_JESSIE")};
  if(!portraits[1]||!portraits[2])ok=false;
  // Validate every asset first so unavailable art cannot leave a partial skin.
  if(ok)for(unsigned i=0;i<7;++i){void* texture=loadTexture(files[i]);
   if(!texture||At<int>(texture,0x38)<=0||!At<void*>(texture,0x30)){ok=false;break;}
   textures[i]=texture;Retain(texture);
  }
  if(!ok){for(unsigned i=0;i<7;++i)Release(textures[i]);return false;}
  skin->screen=screen;for(unsigned i=0;i<3;++i)skin->portraits[i]=portraits[i];
  for(unsigned i=0;i<7;++i){SavedSprite& s=skin->sprites[i];s.sprite=sprites[i];s.texture=At<void*>(s.sprite,0x30);s.reference=At<void*>(s.sprite,0x120);
   Retain(s.reference);s.index=At<int>(s.sprite,0x38);s.frame=At<WORD>(s.sprite,0x3c);s.columns=At<BYTE>(s.sprite,0x3e);s.rows=At<BYTE>(s.sprite,0x3f);Copy(s.uv,(BYTE*)s.sprite+0xb4,sizeof(s.uv));
   setTexture(s.sprite,textures[i],0,0,1,1);Release(textures[i]);
   // Native 128x64 black backing: retain native player dimensions and use
   // its centre strip or 64px end cap, rather than stretching a boss bar.
   if(i==2)setUv(s.sprite,.5f,0,.53125f,1);
   else if(i==3)setUv(s.sprite,left?0.f:.5f,0,left?.5f:1.f,1);
   else setUv(s.sprite,0,0,1,1);
  }
 }
 setVisible(skin->portraits[0],true);setVisible(skin->portraits[1],false);setVisible(skin->portraits[2],false);
 return true;
}
static bool Target(void* screen){
 void* player=At<void*>(screen,0x310);if(!player)return false;
 void* actor=((void*(*)(void*))A(0x558400))(player);
 Context c;return actor&&ReadContext(actor,c)&&!c.toyBox&&ZurgTransform::IsZurg(actor);
}
static void Update(void* screen){
 // Restore before the native selector runs so it chooses the returning hero.
 bool target=false;__try{target=Target(screen);if(!target)Apply(screen,false);}__except(EXCEPTION_EXECUTE_HANDLER){}
 originalUpdate(screen);
 if(target){__try{Apply(screen,true);}__except(EXCEPTION_EXECUTE_HANDLER){}}
}
static void* Destroy(void* screen,unsigned flags){
 Skin* skin=Find(screen);if(skin)Restore(*skin);
 return originalDestroy(screen,flags);
}
static void Install(){
 __try{
  const BYTE updateSig[]={0x40,0x53,0x55,0x48,0x83,0xec,0x78};
  const BYTE lookupSig[]={0x40,0x53,0x48,0x83,0xec,0x20,0x4c,0x8b,0xc1};
  const BYTE textureSig[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18};
  if(!RegionTeleport::Prefix(0x120c60,updateSig,sizeof(updateSig))||!RegionTeleport::Prefix(0x716ef0,lookupSig,sizeof(lookupSig))||!RegionTeleport::Prefix(0x4c4480,textureSig,sizeof(textureSig)))return;
  if(*(void**)A(0x788ee0)!=(void*)A(0x120c60)||*(void**)A(0x788db0)!=(void*)A(0xd7250))return;
  findSprite=(FindFn)A(0x4b2a90);loadTexture=(LoadFn)A(0x716ef0);setTexture=(TextureFn)A(0x4c4480);setVisible=(VisibleFn)A(0x4c4f20);setUv=(UvFn)A(0x4c42e0);
  originalUpdate=(UpdateFn)A(0x120c60);originalDestroy=(DestroyFn)A(0xd7250);
  if(!ChallengeTimers::Swap(0x788db0,(void*)originalDestroy,(void*)Destroy))return;
  if(!ChallengeTimers::Swap(0x788ee0,(void*)originalUpdate,(void*)Update)){ChallengeTimers::Swap(0x788db0,(void*)Destroy,(void*)originalDestroy);return;}
  ready=true;
 }__except(EXCEPTION_EXECUTE_HANDLER){ready=false;}
}
}
