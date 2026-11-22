// Included inside NativeUI. Raw exports bypass only the game's IAT input gate.
struct PadState{DWORD packet;WORD buttons;BYTE lt,rt;SHORT lx,ly,rx,ry;};
typedef DWORD(WINAPI* ReadPad)(DWORD,PadState*);
static ReadPad readPad;static bool padLoaded,padConnected;
static WORD padPrevious,padPressed;static bool padCapture;static DWORD repeatAt,stickAt,probeAt;static int heldDirection;static DWORD padSlot=4;
static bool navMode,navAccept,navBack,manualScroll;static int navDirection,navStep;
static float navScroll;static DWORD scrollTick;
struct NavItem{unsigned id;ImVec2 a,b;RECT clip;unsigned links[4];};
static NavItem navItems[256];static unsigned navCount,navFocus,navScope,navEligible;
static unsigned savedFocus[3];
static bool navSeen,mouseKnown;static ImVec2 physicalMouse;
static float Abs(float x){return x<0?-x:x;}
static float NavMax(float a,float b){return a>b?a:b;}
bool PollController(bool foreground,bool menuOpen,WORD openButton){
 if(!padLoaded){padLoaded=true;HMODULE m=LoadLibraryW(L"xinput1_4.dll");if(!m)m=LoadLibraryW(L"xinput9_1_0.dll");if(m)readPad=(ReadPad)GetProcAddress(m,"XInputGetState");}
 DWORD now=GetTickCount();PadState p={};bool connected=false;DWORD slot=padSlot;
 if(readPad&&padSlot<4)connected=readPad(padSlot,&p)==0;
 if(readPad&&!connected&&LONG(now-probeAt)>=0){probeAt=now+500;for(DWORD i=0;i<4;++i)if(readPad(i,&p)==0){connected=true;slot=i;break;}}
 // Triggers become the unused bits 0x400/0x800 so they can be chosen as the open button.
 const WORD buttons=WORD(p.buttons|(p.lt>128?0x0400:0)|(p.rt>128?0x0800:0));
 WORD pressed=connected&&padConnected&&slot==padSlot?WORD(buttons&~padPrevious):0;
 padSlot=connected?slot:4;padPrevious=connected?buttons:0;padConnected=connected;padPressed=foreground?pressed:0;
 navDirection=navStep=0;navAccept=navBack=false;navScroll=0;
 DWORD elapsed=scrollTick?now-scrollTick:16;scrollTick=now;if(elapsed>50)elapsed=50;
 if(!foreground||!connected){heldDirection=0;navMode=false;return false;}
 // A held D-pad open button must not also move the focus once the menu appears.
 const int openDirection=openButton==1?1:openButton==2?2:openButton==4?3:openButton==8?4:0;
 if(!menuOpen){if(pressed&openButton){if(openDirection){heldDirection=openDirection;repeatAt=now+350;}navMode=true;return true;}if(!openDirection||!(buttons&openButton))heldDirection=0;return false;}
 // Settings > Controls is waiting for a button: report presses only, no navigation.
 // A D-pad still held when the change ends must not then move the focus.
 if(padCapture){heldDirection=(buttons&1)?1:(buttons&2)?2:(buttons&4)?3:(buttons&8)?4:0;repeatAt=now+350;return false;}
 // A non-D-pad open button also closes; D-pad directions stay navigation while open.
 if(!openDirection&&(pressed&openButton))return true;
 int dir=(p.buttons&1)?1:(p.buttons&2)?2:(p.buttons&4)?3:(p.buttons&8)?4:0;
 if(dir&&(dir!=heldDirection||LONG(now-repeatAt)>=0)){navDirection=dir;repeatAt=now+(dir==heldDirection?140:350);navMode=true;}heldDirection=dir;
 navAccept=(pressed&0x1000)!=0;navBack=(pressed&0x2000)!=0;if(navAccept||navBack)navMode=true;
 if(Abs(float(p.lx))<=10000)stickAt=now;
 else if(LONG(now-stickAt)>=0){navStep=p.lx>0?1:-1;stickAt=now+75;navMode=true;}
 if(Abs(float(p.ry))>9000){navScroll=(float(p.ry>0?p.ry-9000:p.ry+9000)/23768.f)*float(elapsed)*.018f;navMode=true;manualScroll=true;}
 if(navDirection||navAccept||navStep)manualScroll=false;
 return false;
}
float ControllerScroll(){return navScroll;}
void PadCapture(bool on){padCapture=on;navAccept=navBack=false;navDirection=navStep=0;}
WORD PadPressed(){return padPressed;}
void NavRedirect(const char* from,const char* to){if(navMode&&navFocus==Hash(from))navFocus=Hash(to);}
void NavLink(const char* from,int direction,const char* to){if(direction<1||direction>4)return;for(unsigned i=0;i<navCount;++i)if(navItems[i].id==Hash(from))navItems[i].links[direction-1]=Hash(to);}
// Each area (sidebar, page, settings) remembers its last control, so Left/Right between areas
// returns to it; the sidebar's is always the open page's tab (NavHome).
struct ClipMemory{RECT clip;unsigned id;};static ClipMemory clipMemory[8];static unsigned homeId;
static bool SameClip(const RECT& a,const RECT& b){return a.left==b.left&&a.top==b.top&&a.right==b.right&&a.bottom==b.bottom;}
static void Remember(const NavItem& n){
 for(auto& m:clipMemory)if(m.id&&SameClip(m.clip,n.clip)){m.id=n.id;return;}
 for(auto& m:clipMemory)if(!m.id){m={n.clip,n.id};return;}
 clipMemory[7]={n.clip,n.id};
}
static int Remembered(const RECT& clip,unsigned current){
 for(auto& m:clipMemory)if(m.id&&SameClip(m.clip,clip))for(unsigned i=0;i<navCount;++i)if(i!=current&&navItems[i].id==m.id&&SameClip(navItems[i].clip,clip))return int(i);
 return -1;
}
// Direction rules, in order:
//  1. a control in the same area that lines up (shares columns for Up/Down, rows for Left/Right);
//  2. a control in the same area within 45 degrees of the direction (never a sideways jump);
//  3. only then another area (Left/Right: its remembered control), with the same two rules.
// Within a rule the nearest edge wins, so Up always takes the next row up.
static int NextItem(unsigned current,int direction){
 auto c=navItems[current];unsigned linked=c.links[direction-1];
 if(linked)for(unsigned i=0;i<navCount;++i)if(navItems[i].id==linked)return int(i);
 const bool vertical=direction<=2;float cx=(c.a.x+c.b.x)/2,cy=(c.a.y+c.b.y)/2,best=1e30f;int chosen=-1,tier=4;
 for(unsigned i=0;i<navCount;++i){if(i==current)continue;auto n=navItems[i];
  if(direction>=3&&n.id==Hash("settings-cog"))continue;
  float dx=(n.a.x+n.b.x)/2-cx,dy=(n.a.y+n.b.y)/2-cy;
  float forward=direction==1?-dy:direction==2?dy:direction==3?-dx:dx;if(forward<=2)continue;
  float gap=direction==1?c.a.y-n.b.y:direction==2?n.a.y-c.b.y:direction==3?c.a.x-n.b.x:n.a.x-c.b.x;if(gap<0)gap=0;
  float across=vertical?NavMax(n.a.x-c.b.x,c.a.x-n.b.x):NavMax(n.a.y-c.b.y,c.a.y-n.b.y);
  bool overlap=across<0;if(!overlap&&across>gap)continue;
  int t=(SameClip(n.clip,c.clip)?0:2)+(overlap?0:1);
  // In line: nearest row, then nearest centre. Off to the side: each pixel sideways counts three times.
  float score=overlap?gap+(vertical?Abs(dx):Abs(dy))*.5f:gap+across*3;
  if(t<tier||(t==tier&&score<best)){tier=t;best=score;chosen=int(i);}
 }
 if(chosen>=0&&tier>=2&&!vertical){int back=Remembered(navItems[chosen].clip,current);if(back>=0)return back;}
 return chosen;
}
void NavHome(const char* name){homeId=Hash(name);}
void NavScope(unsigned scope){if(scope!=navScope){savedFocus[navScope]=navFocus;navScope=scope;navFocus=savedFocus[scope];navCount=0;navAccept=false;}}
bool BackPressed(){return navBack;}
bool ControllerActive(){return navMode;}
static void NavFrame(){
 if(mouseKnown&&(Abs(io.MousePos.x-physicalMouse.x)>3||Abs(io.MousePos.y-physicalMouse.y)>3||down||io.MouseWheel))if(!navDirection&&!navAccept&&!navStep&&!navScroll){navMode=false;manualScroll=false;}
 physicalMouse=io.MousePos;mouseKnown=true;
 int current=-1;for(unsigned i=0;i<navCount;++i)if(navItems[i].id==navFocus)current=int(i);
 if(navMode&&navCount){
  if(current<0){if(navFocus&&!navDirection){navCount=0;navSeen=false;navEligible=0;return;}current=0;navFocus=navItems[0].id;navAccept=false;}
  Remember(navItems[current]);
  for(unsigned i=0;i<navCount;++i)if(navItems[i].id==homeId)Remember(navItems[i]);
  if(navDirection){int chosen=NextItem(unsigned(current),navDirection);
   if(chosen>=0){current=chosen;navFocus=navItems[current].id;}
  }
  auto n=navItems[current];io.MousePos=ImVec2((n.a.x+n.b.x)/2,(n.a.y+n.b.y)/2);
  if(!manualScroll&&(n.a.y<n.clip.top||n.b.y>n.clip.bottom)){io.MouseWheel=n.a.y<n.clip.top?1.f:-1.f;io.MousePos.y=float(n.clip.top+n.clip.bottom)/2;navAccept=false;navStep=0;}
 }
 navCount=0;navSeen=false;navEligible=0;
}
void NavRegister(const char* name,ImVec2 a,ImVec2 size,bool enabled){
 navEligible=0;
 if((!enabled&&Hash(name)!=navFocus)||lstrcmpA(name,"title-drag")==0||lstrcmpA(name,"settings-scroll")==0||lstrcmpA(name,"page-scroll")==0||lstrcmpA(name,"settings-dismiss")==0||lstrcmpA(name,"language-dismiss")==0)return;
 RECT clip=clips[clipTop];ImVec2 b(a.x+size.x,a.y+size.y);
 if(a.x<clip.left||b.x>clip.right||navCount>=256)return;
 unsigned id=Hash(name);navItems[navCount++]={id,a,b,clip};if(enabled&&a.y>=clip.top&&b.y<=clip.bottom)navEligible=id;
 if(navMode&&!navFocus)navFocus=navScope==0?Hash("Player"):id;
 if(id==navFocus)navSeen=true;
}
bool NavSelected(const char* name){return navMode&&navFocus==Hash(name);}
int NavAdjust(const char* name){return NavSelected(name)&&navEligible==Hash(name)?navStep:0;}
void NavFinish(){
 if(!navMode||!navSeen)return;
 for(unsigned i=0;i<navCount;++i)if(navItems[i].id==navFocus){auto n=navItems[i];if(n.a.y<n.clip.top||n.b.y>n.clip.bottom)break;list.AddRect(ImVec2(n.a.x-2,n.a.y-2),ImVec2(n.b.x+2,n.b.y+2),IM_COL32(255,180,0,255),6,0,3);break;}
}

#ifdef UI_SELF_TEST
void TestNavInput(int direction,bool accept,int step,bool back){navMode=true;navDirection=direction;navAccept=accept;navStep=step;navBack=back;navScroll=0;if(direction||accept||step)manualScroll=false;}
void TestNavScroll(float value){navMode=true;navScroll=value;manualScroll=true;}
void TestNavFocus(const char* name){navMode=true;navFocus=Hash(name);}
bool TestNavContains(const char* name){for(unsigned i=0;i<navCount;++i)if(navItems[i].id==Hash(name))return true;return false;}
bool TestNavVisible(const char* name){for(unsigned i=0;i<navCount;++i)if(navItems[i].id==Hash(name)){auto n=navItems[i];return n.a.y>=n.clip.top&&n.b.y<=n.clip.bottom;}return false;}
bool TestNavReachable(){
 if(!navCount)return false;bool seen[256]={};seen[0]=true;
 for(unsigned pass=0;pass<navCount;++pass)for(unsigned c=0;c<navCount;++c)if(seen[c])for(int d=1;d<=4;++d){
  int chosen=NextItem(c,d);
  if(chosen>=0)seen[chosen]=true;
 }
 for(unsigned i=0;i<navCount;++i)if(!seen[i])return false;return true;
}
void TestNavOff(){navMode=false;navFocus=navCount=0;navDirection=navStep=0;navAccept=navBack=false;}
#endif
