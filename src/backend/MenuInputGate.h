// Process-local game input isolation. The companion has its own input queue;
// no global keyboard/mouse hook, BlockInput, process suspension or save writes.
namespace MenuInputGate {
struct Lease {volatile LONG sequence;DWORD magic,version,game,owner,tick,open,reserved;char session[33];};
static HANDLE mapping;static volatile Lease* lease;static char expectedSession[33];
static volatile LONG wasOpen,releaseKeys,lastGoodTick,deviceFormats;static bool hooksReady,rootReady,deviceReady;
static volatile LONG externalTick,externalOpen;static bool externalReady;
static bool ExternalOpen(){return externalOpen&&(DWORD)(GetTickCount()-(DWORD)externalTick)<750;}
static void SetExternalMenu(bool open){
 if(open){InterlockedExchange(&externalTick,(LONG)GetTickCount());InterlockedExchange(&externalOpen,1);}
 else InterlockedExchange(&externalOpen,0);
}
static bool Ready(){return hooksReady&&rootReady&&deviceReady&&(deviceFormats&3)==3;}
static bool TextEqual(const char* a,const char* b){for(int i=0;i<33;++i){if(a[i]!=b[i])return false;if(!a[i])return true;}return false;}
static bool ValidLease(const Lease& s,DWORD now,DWORD pid,const char* token){
 return !(s.sequence&1)&&s.sequence&&s.magic==0x554e4c32&&s.version==2&&s.game==pid&&s.owner&&s.open==1&&
        (DWORD)(now-s.tick)<750&&TextEqual(s.session,token);
}
static bool Open(){
 if(!lease)return false;Lease s;LONG before=lease->sequence;if(before&1)return wasOpen&&(DWORD)(GetTickCount()-(DWORD)lastGoodTick)<750;
 const volatile BYTE* in=(const volatile BYTE*)lease;BYTE* out=(BYTE*)&s;
 for(SIZE_T i=0;i<sizeof(s);++i)out[i]=in[i];MemoryBarrier();
 if(before!=lease->sequence)return wasOpen&&(DWORD)(GetTickCount()-(DWORD)lastGoodTick)<750;
 bool valid=ValidLease(s,GetTickCount(),GetCurrentProcessId(),expectedSession);
 if(valid)InterlockedExchange(&lastGoodTick,(LONG)s.tick);return valid;
}
// Visibility is independent of pointer-based input blocking.
static bool ViewerActive(){
 if(!lease)return false;Lease s;LONG before=lease->sequence;if(before&1)return false;
 const volatile BYTE* in=(const volatile BYTE*)lease;BYTE* out=(BYTE*)&s;
 for(SIZE_T i=0;i<sizeof(s);++i)out[i]=in[i];MemoryBarrier();
 if(before!=lease->sequence||s.reserved!=1)return false;
 s.open=1;return ValidLease(s,GetTickCount(),GetCurrentProcessId(),expectedSession);
}
static bool releaseButton[256];
static bool MenuKey(int key){return key==VK_LBUTTON||key==VK_RBUTTON||key==VK_MBUTTON||key==VK_XBUTTON1||key==VK_XBUTTON2||key==VK_TAB||key==VK_F10||key==VK_RETURN||key==VK_ESCAPE;}
static bool Suppressed(int key){
 if(key<1||key>255||!releaseButton[key])return false;
 if(!(GetAsyncKeyState(key)&0x8000))releaseButton[key]=false;
 return releaseButton[key];
}
static bool Blocked(){
 bool open=ExternalOpen()||Open();
 if(open){InterlockedExchange(&wasOpen,1);return true;}
 if(InterlockedExchange(&wasOpen,0)){
  // Only the buttons used to interact with the menu wait for release.
  // Movement and camera controls must resume on the very first closed frame.
  for(int key=1;key<256;++key)releaseButton[key]=MenuKey(key)&&(GetAsyncKeyState(key)&0x8000);
 }
 for(int key=1;key<256;++key)if(releaseButton[key])Suppressed(key);
 return false;
}
struct VTableHook {void** table;void* original[3];};
static VTableHook roots[8],devices[16];static volatile LONG tableBusy;
static void* pendingDevices[16];
struct TableLock{TableLock(){while(InterlockedCompareExchange(&tableBusy,1,0))Sleep(0);}~TableLock(){InterlockedExchange(&tableBusy,0);}};
static void* Original(void* self,VTableHook* entries,int count,int index){
 void** table=*(void***)self;for(int i=0;i<count;++i)if(entries[i].table==table)return entries[i].original[index];return 0;
}
static bool Patch(void** slot,void* replacement,void*& original){
 if(*slot==replacement)return true;DWORD old;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))return false;
 original=InterlockedExchangePointer((void* volatile*)slot,replacement);DWORD ignored;VirtualProtect(slot,sizeof(void*),old,&ignored);return true;
}
typedef HRESULT(WINAPI* StateFn)(void*,DWORD,void*);
typedef HRESULT(WINAPI* DataFn)(void*,DWORD,void*,DWORD*,DWORD);
typedef HRESULT(WINAPI* DeviceFn)(void*,const GUID&,void**,void*);
typedef HRESULT(WINAPI* PropertyFn)(void*,const GUID&,void*);
static void* formatDevice[16];static DWORD formatBytes[16];
static DWORD Format(void* self,DWORD bytes=0){for(int i=0;i<16;++i)if(formatDevice[i]==self||(!formatDevice[i]&&bytes)){formatDevice[i]=self;if(bytes)formatBytes[i]=bytes;return formatBytes[i];}return 0;}
static int EventKey(DWORD bytes,DWORD offset){
 if(bytes==256&&offset<256)return MapVirtualKeyA((offset&0x7f)|((offset&0x80)?0xe000:0),MAPVK_VSC_TO_VK_EX);
 const int keys[]={VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2};
 return (bytes==16||bytes==20)&&offset>=12&&offset<17?keys[offset-12]:0;
}
static void Neutral(void* self,DWORD count,void* data){
 volatile BYTE* bytes=(volatile BYTE*)data;for(DWORD i=0;i<count;++i)bytes[i]=0;
 if(count==80||count==272){
  // DIJOYSTATE/DIJOYSTATE2: honor configured axis ranges; POV -1 is neutral.
  struct Range{DWORD size,header,object,how;LONG minimum,maximum;};
  PropertyFn property=(PropertyFn)(*(void***)self)[5];
  for(DWORD axis=0;axis<8;++axis){Range r={sizeof(Range),16,axis*4,1,0,0};
   if(property&&SUCCEEDED(property(self,*(const GUID*)(ULONG_PTR)4,&r))&&r.minimum<r.maximum)
    ((LONG*)data)[axis]=(LONG)(((LONGLONG)r.minimum+r.maximum)/2);
  }
  for(int i=8;i<12;++i)((DWORD*)data)[i]=0xffffffff;
 }
}
static HRESULT WINAPI DeviceState(void* self,DWORD count,void* data){
 StateFn fn=(StateFn)Original(self,devices,16,0);if(!fn)return E_FAIL;
 HRESULT result=fn(self,count,data);
 if(SUCCEEDED(result)&&data){Format(self,count);if(count==256)InterlockedOr(&deviceFormats,1);else if(count==16||count==20)InterlockedOr(&deviceFormats,2);}
 bool blocked=Blocked();
 if(SUCCEEDED(result)&&data&&count<=4096){
  if(blocked)Neutral(self,count,data);
  else if(count==256){for(unsigned scan=0;scan<256;++scan){unsigned code=(scan&0x7f)|((scan&0x80)?0xe000:0);if(Suppressed(MapVirtualKeyA(code,MAPVK_VSC_TO_VK_EX)))((BYTE*)data)[scan]=0;}}
  else if(count==16||count==20){const int keys[]={VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2};for(unsigned i=0;i<count-12&&i<5;++i)if(Suppressed(keys[i]))((BYTE*)data)[12+i]=0;}
 }
 return result;
}
static HRESULT WINAPI DeviceData(void* self,DWORD objectSize,void* data,DWORD* count,DWORD flags){
 DataFn fn=(DataFn)Original(self,devices,16,1);if(!fn)return E_FAIL;
 HRESULT result=fn(self,objectSize,data,count,flags);
 if(Blocked()&&count){
  // The original read drains the device buffer; PEEK callers are explicitly
  // drained too, so queued menu input cannot reappear after closing the panel.
  if(flags&1){DWORD discard=0xffffffff;fn(self,objectSize,0,&discard,0);}
  *count=0;
 }
 else if(SUCCEEDED(result)&&data&&count&&objectSize>=16&&*count<4096){
  DWORD kept=0,bytes=Format(self);BYTE* events=(BYTE*)data;
  for(DWORD i=0;i<*count;++i){BYTE* event=events+i*objectSize;
   if((*(DWORD*)(event+4)&0x80)&&Suppressed(EventKey(bytes,*(DWORD*)event)))continue;
   if(kept!=i)for(DWORD b=0;b<objectSize;++b)events[kept*objectSize+b]=event[b];++kept;
  }*count=kept;
 }
 return result;
}
static void AttachDevice(void* self){
 if(!self)return;TableLock lock;void** table=*(void***)self;
 if(!lease&&!externalReady){
  for(int i=0;i<16;++i)if(pendingDevices[i]==self)return;
  for(int i=0;i<16;++i)if(!pendingDevices[i]){((ULONG(WINAPI*)(void*))table[1])(self);pendingDevices[i]=self;return;}
  return;
 }
 for(int i=0;i<16;++i)if(devices[i].table==table)return;
 for(int i=0;i<16;++i)if(!devices[i].table){
  VTableHook& e=devices[i];e.original[0]=table[9];e.original[1]=table[10];e.table=table;MemoryBarrier();
  bool a=Patch(table+9,(void*)DeviceState,e.original[0]);bool b=Patch(table+10,(void*)DeviceData,e.original[1]);
  if(a&&b)deviceReady=true;else Log("Unlocks input: DirectInput hook incomplete");return;
 }
}
static HRESULT WINAPI CreateDevice(void* self,const GUID& guid,void** out,void* outer){
 DeviceFn fn=(DeviceFn)Original(self,roots,8,0);if(!fn)return E_FAIL;HRESULT result=fn(self,guid,out,outer);
 if(SUCCEEDED(result)&&out&&*out)AttachDevice(*out);return result;
}
static void AttachDirectInput(void* self){
 if(!self)return;TableLock lock;void** table=*(void***)self;
 for(int i=0;i<8;++i)if(roots[i].table==table)return;
 for(int i=0;i<8;++i)if(!roots[i].table){VTableHook& e=roots[i];e.original[0]=table[3];e.table=table;MemoryBarrier();if(Patch(table+3,(void*)CreateDevice,e.original[0]))rootReady=true;return;}
}
struct Pad {DWORD packet;WORD buttons;BYTE leftTrigger,rightTrigger;SHORT lx,ly,rx,ry;};
static_assert(sizeof(Pad)==16,"XInput ABI");
typedef DWORD(WINAPI* PadFn)(DWORD,Pad*);static PadFn padOriginal;
static bool padWasOpen[4],padRelease[4];
static bool flightOwnsFaceButtons;
static WORD padButtons[4];static bool padLeft[4],padRight[4];
static bool PadNeutral(const Pad& p){return !p.buttons&&p.leftTrigger<30&&p.rightTrigger<30&&p.lx>-7849&&p.lx<7849&&p.ly>-7849&&p.ly<7849&&p.rx>-8689&&p.rx<8689&&p.ry>-8689&&p.ry<8689;}
static DWORD WINAPI PadState(DWORD index,Pad* state){
 DWORD result=padOriginal(index,state);if(index>=4||result||!state)return result;
 bool blocked=Blocked();if(blocked)padWasOpen[index]=true;else if(padWasOpen[index]){padWasOpen[index]=false;padButtons[index]=state->buttons;padLeft[index]=state->leftTrigger>=30;padRight[index]=state->rightTrigger>=30;}
 if(blocked){BYTE* p=(BYTE*)state;for(int i=4;i<16;++i)p[i]=0;state->packet^=0x80000000;}
 else {
  padButtons[index]&=state->buttons;padLeft[index]=padLeft[index]&&state->leftTrigger>=30;padRight[index]=padRight[index]&&state->rightTrigger>=30;
  padRelease[index]=padButtons[index]||padLeft[index]||padRight[index];
  state->buttons&=~padButtons[index];if(padLeft[index])state->leftTrigger=0;if(padRight[index])state->rightTrigger=0;
  if(padRelease[index])state->packet^=0x80000000;
 }
 // Flight reads the real pad itself. The game's copy loses A/B and the left stick: native stick handling
 // still turns the avatar toward the stick (measured: stick forward left it facing the camera).
 if(flightOwnsFaceButtons){state->buttons&=~WORD(0x3000);state->lx=state->ly=0;state->packet^=0x40000000;}
 return result;
}
typedef BOOL(WINAPI* CursorFn)(int,int);static CursorFn cursorOriginal;
typedef BOOL(WINAPI* ClipFn)(const RECT*);static ClipFn clipOriginal;
typedef SHORT(WINAPI* KeyFn)(int);static KeyFn keyOriginal;
typedef LRESULT(WINAPI* DispatchFn)(const MSG*);static DispatchFn dispatchOriginal;
static BOOL WINAPI Cursor(int x,int y){return Blocked()?TRUE:cursorOriginal(x,y);}
static BOOL WINAPI Clip(const RECT* r){return Blocked()?clipOriginal(0):clipOriginal(r);}
static SHORT WINAPI Key(int key){return Blocked()||Suppressed(key)?0:keyOriginal(key);}
static bool InputMessage(UINT m){return (m>=WM_KEYFIRST&&m<=WM_KEYLAST)||(m>=WM_MOUSEFIRST&&m<=WM_MOUSELAST)||m==WM_INPUT;}
static LRESULT WINAPI Dispatch(const MSG* msg){
 // While the menu is open its window handler receives these messages and keeps them from gameplay.
 // DirectInput remains neutralized.
 if(ExternalOpen())return dispatchOriginal(msg);
 if(msg&&Blocked()&&InputMessage(msg->message)){
  // Foreground raw input packets still need the documented default cleanup.
  return msg->message==WM_INPUT?DefWindowProcA(msg->hwnd,msg->message,msg->wParam,msg->lParam):0;
 }
 return dispatchOriginal(msg);
}
static void InstallImports(){
 BYTE* base=(BYTE*)GetModuleHandleW(0);IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)base;
 IMAGE_NT_HEADERS* pe=(IMAGE_NT_HEADERS*)(base+dos->e_lfanew);
 IMAGE_IMPORT_DESCRIPTOR* descriptor=(IMAGE_IMPORT_DESCRIPTOR*)(base+pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
 int installed=0;
 for(;descriptor->Name;++descriptor){
  const char* dll=(const char*)(base+descriptor->Name);bool user=lstrcmpiA(dll,"user32.dll")==0,xinput=lstrcmpiA(dll,"xinput1_4.dll")==0;
  if(!user&&!xinput)continue;
  if(!descriptor->OriginalFirstThunk)continue;
  IMAGE_THUNK_DATA* names=(IMAGE_THUNK_DATA*)(base+descriptor->OriginalFirstThunk);void** addresses=(void**)(base+descriptor->FirstThunk);
  for(int i=0;names[i].u1.AddressOfData;++i){
   void* replacement=0;void** original=0;
   if(IMAGE_SNAP_BY_ORDINAL(names[i].u1.Ordinal)){if(xinput&&IMAGE_ORDINAL(names[i].u1.Ordinal)==2){replacement=(void*)PadState;original=(void**)&padOriginal;}}
   else {const char* name=(const char*)((IMAGE_IMPORT_BY_NAME*)(base+names[i].u1.AddressOfData))->Name;
    if(user&&lstrcmpA(name,"SetCursorPos")==0){replacement=(void*)Cursor;original=(void**)&cursorOriginal;}
    else if(user&&lstrcmpA(name,"ClipCursor")==0){replacement=(void*)Clip;original=(void**)&clipOriginal;}
    else if(user&&lstrcmpA(name,"GetKeyState")==0){replacement=(void*)Key;original=(void**)&keyOriginal;}
    else if(user&&lstrcmpA(name,"DispatchMessageA")==0){replacement=(void*)Dispatch;original=(void**)&dispatchOriginal;}
    else if(xinput&&lstrcmpA(name,"XInputGetState")==0){replacement=(void*)PadState;original=(void**)&padOriginal;}
   }
   if(replacement&&Patch(addresses+i,replacement,*original))++installed;
  }
 }
 hooksReady=installed==5;Log(hooksReady?"Unlocks input gate ready: keyboard, mouse, controller, cursor and window input":"Unlocks input gate: one or more import hooks unavailable");
}
static void AttachPendingDevices(){
 for(int i=0;i<16;++i)if(pendingDevices[i]){
  void* device=pendingDevices[i];pendingDevices[i]=0;AttachDevice(device);
  ((ULONG(WINAPI*)(void*))(*(void***)device)[2])(device);
 }
}
static void InitializeExternal(){
 if(externalReady)return;externalReady=true;
 if(!hooksReady)InstallImports();
 AttachPendingDevices();
}
static void Initialize(const char* token){
 if(lease)return;lstrcpynA(expectedSession,token,33);WCHAR name[96];wsprintfW(name,L"Local\\TS3CEUnlocksMenu_%lu",GetCurrentProcessId());
 mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,0,PAGE_READWRITE,0,sizeof(Lease),name);
 if(!mapping)return;lease=(volatile Lease*)MapViewOfFile(mapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(Lease));
 if(!lease){CloseHandle(mapping);mapping=0;return;}
 volatile BYTE* bytes=(volatile BYTE*)lease;for(SIZE_T i=0;i<sizeof(Lease);++i)bytes[i]=0;
 if(!hooksReady)InstallImports();
 AttachPendingDevices();
}
} // namespace MenuInputGate
