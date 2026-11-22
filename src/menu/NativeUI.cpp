#include "NativeUI.h"
#include <xmmintrin.h>
namespace NativeUI{
NativeVertex* vertices;NativeBatch* batches;unsigned vertexCount,batchCount;
NativeVertex* revealVertices;NativeBatch* revealBatches;unsigned revealVertexCount,revealBatchCount;
static unsigned previousVertices,previousBatches;static DWORD revealStart;static ImVec2 revealOrigin;static float revealRadius;
unsigned contentVertexCount;void MarkContentEnd(){contentVertexCount=vertexCount;}
static volatile LONG wheelDelta;void AddWheel(short delta){InterlockedExchangeAdd(&wheelDelta,delta);}
static IO io;static ImDrawList list;static ImVec2 cursor;static bool down,previous,clicked,released,hover,activated,overflow;
static unsigned active,lastId;static int mouseCursor;static RECT clips[8];static int clipTop;
static float Root(float x){return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(x)));}
void EndReveal(){revealVertexCount=revealBatchCount=0;previousVertices=previousBatches=0;}
void BeginReveal(ImVec2 origin,ImVec2 a,ImVec2 b){
 if(!previousVertices||!previousBatches)return;
 if(!revealVertices)revealVertices=(NativeVertex*)VirtualAlloc(0,sizeof(NativeVertex)*120000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(!revealBatches)revealBatches=(NativeBatch*)VirtualAlloc(0,sizeof(NativeBatch)*4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(!revealVertices||!revealBatches)return;
 // Keep the old menu geometry, but only the live cursor.
 revealVertexCount=previousVertices;revealBatchCount=0;
 for(unsigned i=0;i<revealVertexCount;++i)revealVertices[i]=vertices[i];
 for(unsigned i=0;i<previousBatches;++i){auto cmd=batches[i];if(cmd.first>=revealVertexCount)break;if(cmd.first+cmd.count>revealVertexCount)cmd.count=revealVertexCount-cmd.first;revealBatches[revealBatchCount++]=cmd;}
 revealOrigin=origin;float dx=origin.x-a.x;if(b.x-origin.x>dx)dx=b.x-origin.x;float dy=origin.y-a.y;if(b.y-origin.y>dy)dy=b.y-origin.y;
 revealRadius=Root(dx*dx+dy*dy)+4;revealStart=GetTickCount();
}
bool Reveal(float* out){
 if(!revealVertexCount)return false;float t=float(DWORD(GetTickCount()-revealStart))/480.f;
 if(t>=1){revealVertexCount=revealBatchCount=0;return false;}
 // Solve cubic-bezier(.4,0,.2,1), matching the Companion theme reveal.
 float low=0,high=1,u=t;for(int i=0;i<12;++i){u=(low+high)*.5f;float x=3*(1-u)*(1-u)*u*.4f+3*(1-u)*u*u*.2f+u*u*u;if(x<t)low=u;else high=u;}
 float ease=3*(1-u)*u*u+u*u*u;out[0]=revealOrigin.x;out[1]=revealOrigin.y;out[2]=ease*revealRadius;return true;
}
#ifdef UI_SELF_TEST
void TestRevealElapsed(DWORD elapsed){revealStart=GetTickCount()-elapsed;}
#endif
static unsigned Hash(const char* text){unsigned h=2166136261;while(*text){h^=(unsigned char)*text++;h*=16777619;}return h?h:1;}
#include "ControllerNav.h"
#ifdef UI_SELF_TEST
static ImVec2 testHover(-1000,-1000); // a resting pointer for hover screenshots
#endif
bool NewFrame(HWND window,unsigned width,unsigned height){
 if(!vertices)vertices=(NativeVertex*)VirtualAlloc(0,sizeof(NativeVertex)*120000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(!batches)batches=(NativeBatch*)VirtualAlloc(0,sizeof(NativeBatch)*4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(!vertices||!batches)return false;
 io.MouseWheel=float(InterlockedExchange(&wheelDelta,0))/120.f;
 previousVertices=contentVertexCount;previousBatches=batchCount;contentVertexCount=0;vertexCount=batchCount=0;overflow=false;io.DisplaySize=ImVec2(float(width),float(height));
 POINT p;GetCursorPos(&p);ScreenToClient(window,&p);RECT client={};GetClientRect(window,&client);io.MousePos=ImVec2(client.right?float(p.x)*width/client.right:float(p.x),client.bottom?float(p.y)*height/client.bottom:float(p.y));
 #ifdef UI_SELF_TEST
 io.MousePos=testHover;down=false;
#else
 down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
#endif
 NavFrame();
 clicked=down&&!previous;released=!down&&previous;previous=down;
 mouseCursor=ImGuiMouseCursor_Arrow;clipTop=0;clips[0]={0,0,LONG(width),LONG(height)};return true;
}
void ClearInput(){mouseKnown=false;navEligible=0;navCount=0;navAccept=navBack=false;navStep=navDirection=0;navScroll=0;manualScroll=false;InterlockedExchange(&wheelDelta,0);io.MouseWheel=0;active=0;previous=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;}
void Shutdown(){if(vertices)VirtualFree(vertices,0,MEM_RELEASE);if(batches)VirtualFree(batches,0,MEM_RELEASE);if(revealVertices)VirtualFree(revealVertices,0,MEM_RELEASE);if(revealBatches)VirtualFree(revealBatches,0,MEM_RELEASE);vertices=0;batches=0;revealVertices=0;revealBatches=0;EndReveal();}
bool Overflowed(){return overflow;}IO& GetIO(){return io;}ImDrawList* GetWindowDrawList(){return &list;}ImDrawList* GetForegroundDrawList(){return &list;}
void SetCursorScreenPos(ImVec2 p){cursor=p;}
bool InvisibleButton(const char* name,ImVec2 size){
 unsigned id=Hash(name);lastId=id;hover=io.MousePos.x>=cursor.x&&io.MousePos.y>=cursor.y&&io.MousePos.x<cursor.x+size.x&&io.MousePos.y<cursor.y+size.y;
 const RECT& clip=clips[clipTop];hover=hover&&io.MousePos.x>=clip.left&&io.MousePos.x<clip.right&&io.MousePos.y>=clip.top&&io.MousePos.y<clip.bottom;
 activated=hover&&clicked;if(activated)active=id;
 bool result=released&&active==id&&hover;if(released&&active==id)active=0;if(navMode&&navFocus==id&&navEligible==id&&navAccept){navAccept=false;result=true;}return result;
}
bool IsItemHovered(){return hover;}bool IsItemActive(){return active!=0&&down&&active==lastId;} // replaced below with last id
bool IsItemActivated(){return activated;}bool IsMouseDown(int){return down;}bool IsMouseClicked(int){return clicked;}
void SetMouseCursor(int c){mouseCursor=c;}int GetMouseCursor(){return mouseCursor;}
void TextUnformatted(const char* text){OutputDebugStringA(text);}
static void Tri(uint64_t texture,NativeVertex a,NativeVertex b,NativeVertex c){
 if(vertexCount+3>120000||batchCount>=4096){overflow=true;return;}
 RECT clip=clips[clipTop];NativeBatch* last=batchCount?&batches[batchCount-1]:0;
 if(!last||last->texture!=texture||last->clip.left!=clip.left||last->clip.top!=clip.top||last->clip.right!=clip.right||last->clip.bottom!=clip.bottom){last=&batches[batchCount++];*last={texture,clip,vertexCount,0};}
 vertices[vertexCount++]=a;vertices[vertexCount++]=b;vertices[vertexCount++]=c;last->count+=3;
}
static NativeVertex V(ImVec2 p,ImU32 color,float u=0,float v=0){return {p.x,p.y,u,v,color};}
// Fixed unit circle, generated offline; avoids a CRT or per-frame trigonometry.
static const float unit[32][2]={
 {1,0},{.980785f,.19509f},{.92388f,.382683f},{.83147f,.55557f},{.707107f,.707107f},{.55557f,.83147f},{.382683f,.92388f},{.19509f,.980785f},
 {0,1},{-.19509f,.980785f},{-.382683f,.92388f},{-.55557f,.83147f},{-.707107f,.707107f},{-.83147f,.55557f},{-.92388f,.382683f},{-.980785f,.19509f},
 {-1,0},{-.980785f,-.19509f},{-.92388f,-.382683f},{-.83147f,-.55557f},{-.707107f,-.707107f},{-.55557f,-.83147f},{-.382683f,-.92388f},{-.19509f,-.980785f},
 {0,-1},{.19509f,-.980785f},{.382683f,-.92388f},{.55557f,-.83147f},{.707107f,-.707107f},{.83147f,-.55557f},{.92388f,-.382683f},{.980785f,-.19509f}};
static void Polygon(const ImVec2* p,unsigned count,ImU32 color){
 for(unsigned i=1;i+1<count;++i)Tri(0,V(p[0],color),V(p[i],color),V(p[i+1],color));
 // One-pixel transparent fringe smooths curves without MSAA.
 ImVec2 center;for(unsigned i=0;i<count;++i){center.x+=p[i].x;center.y+=p[i].y;}center.x/=count;center.y/=count;
 for(unsigned i=0;i<count;++i){unsigned j=(i+1)%count;ImVec2 a=p[i],b=p[j],ao=a,bo=b;float ax=a.x-center.x,ay=a.y-center.y,bx=b.x-center.x,by=b.y-center.y;
  float an=Root(ax*ax+ay*ay),bn=Root(bx*bx+by*by);if(an){ao.x+=ax/an;ao.y+=ay/an;}if(bn){bo.x+=bx/bn;bo.y+=by/bn;}
  Tri(0,V(a,color),V(b,color),V(bo,color&0xffffff));Tri(0,V(a,color),V(bo,color&0xffffff),V(ao,color&0xffffff));
 }
}
}
using namespace NativeUI;
void ImDrawList::AddTriangleFilled(ImVec2 a,ImVec2 b,ImVec2 c,ImU32 color){Tri(0,V(a,color),V(b,color),V(c,color));}
void ImDrawList::AddImage(ImTextureRef tex,ImVec2 a,ImVec2 b,ImVec2 uv0,ImVec2 uv1,ImU32 c){
 NativeVertex v0=V(a,c,uv0.x,uv0.y),v1=V(ImVec2(b.x,a.y),c,uv1.x,uv0.y),v2=V(b,c,uv1.x,uv1.y),v3=V(ImVec2(a.x,b.y),c,uv0.x,uv1.y);Tri(tex,v0,v1,v2);Tri(tex,v0,v2,v3);
}
void ImDrawList::AddRectFilled(ImVec2 a,ImVec2 b,ImU32 c,float r,int){
 if(r<=0){AddImage(0,a,b,ImVec2(),ImVec2(),c);return;}
 if(r>(b.x-a.x)/2)r=(b.x-a.x)/2;if(r>(b.y-a.y)/2)r=(b.y-a.y)/2;
 ImVec2 p[36];unsigned n=0;for(int k=0;k<4;++k){ImVec2 center(k==0||k==3?b.x-r:a.x+r,k<2?b.y-r:a.y+r);for(int j=0;j<=8;++j){int index=(k*8+j)%32;p[n++]=ImVec2(center.x+r*unit[index][0],center.y+r*unit[index][1]);}}Polygon(p,n,c);
}
void ImDrawList::AddLine(ImVec2 a,ImVec2 b,ImU32 c,float width){float dx=b.x-a.x,dy=b.y-a.y,n=Root(dx*dx+dy*dy);if(!n)return;float x=-dy/n*width/2,y=dx/n*width/2;ImVec2 p[]={ImVec2(a.x+x,a.y+y),ImVec2(b.x+x,b.y+y),ImVec2(b.x-x,b.y-y),ImVec2(a.x-x,a.y-y)};Polygon(p,4,c);}
void ImDrawList::AddRect(ImVec2 a,ImVec2 b,ImU32 c,float r,int,float w){
 if(r<=0){AddLine(a,ImVec2(b.x,a.y),c,w);AddLine(ImVec2(b.x,a.y),b,c,w);AddLine(b,ImVec2(a.x,b.y),c,w);AddLine(ImVec2(a.x,b.y),a,c,w);return;}
 ImVec2 p[36];unsigned n=0;for(int k=0;k<4;++k){ImVec2 center(k==0||k==3?b.x-r:a.x+r,k<2?b.y-r:a.y+r);for(int j=0;j<=8;++j){int index=(k*8+j)%32;p[n++]=ImVec2(center.x+r*unit[index][0],center.y+r*unit[index][1]);}}for(unsigned i=0;i<n;++i)AddLine(p[i],p[(i+1)%n],c,w);
}
void ImDrawList::AddCircleFilled(ImVec2 c,float r,ImU32 color,int){AddEllipseFilled(c,ImVec2(r,r),color);}
void ImDrawList::AddEllipseFilled(ImVec2 c,ImVec2 r,ImU32 color,float,int){ImVec2 p[32];for(int i=0;i<32;++i)p[i]=ImVec2(c.x+r.x*unit[i][0],c.y+r.y*unit[i][1]);Polygon(p,32,color);}
void ImDrawList::AddRectFilledMultiColor(ImVec2 a,ImVec2 b,ImU32 tl,ImU32 tr,ImU32 br,ImU32 bl){Tri(0,V(a,tl),V(ImVec2(b.x,a.y),tr),V(b,br));Tri(0,V(a,tl),V(b,br),V(ImVec2(a.x,b.y),bl));}
void ImDrawList::PushClipRect(ImVec2 a,ImVec2 b,bool intersect){if(clipTop>=7)return;RECT p=clips[clipTop],c={LONG(a.x),LONG(a.y),LONG(b.x),LONG(b.y)};if(intersect){if(c.left<p.left)c.left=p.left;if(c.top<p.top)c.top=p.top;if(c.right>p.right)c.right=p.right;if(c.bottom>p.bottom)c.bottom=p.bottom;}clips[++clipTop]=c;}
void ImDrawList::PopClipRect(){if(clipTop)--clipTop;}

#ifdef UI_SELF_TEST
namespace NativeUI {void TestHover(ImVec2 p){testHover=p;}void TestPointer(ImVec2 p,bool press,bool release){io.MousePos=p;clicked=press;released=release;down=press;}}
#endif
