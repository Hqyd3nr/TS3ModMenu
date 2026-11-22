// CPU-only WARP integration test. Never opens or attaches to the game.
#include "NativeHost.cpp"
namespace CheatMenuUI {void TestSelectPage(int);void TestTheme(int);void TestAnimatedTheme(int);bool TestDark();bool TestMotion();bool TestDrawer();bool TestScrollEdges();bool TestLocalization();void TestLanguage(int);void TestCreditsScroll();void TestLanguageDropdown();}
namespace NativeUI {void TestRevealElapsed(DWORD);void TestPointer(ImVec2,bool,bool);}
namespace NativeUI {void TestNavScroll(float);bool NavSelected(const char*);}
namespace CheatMenuUI {int TestAppliedSky();float TestPageScroll();bool TestIconTextures();bool TestWarningFont();}
namespace NativeUI {void TestNavInput(int,bool,int,bool);void TestNavFocus(const char*);bool TestNavContains(const char*);bool TestNavVisible(const char*);bool TestNavReachable();void TestNavOff();}
namespace CheatMenuUI {bool TestSettingsOpen();bool TestLanguageOpen();int TestChosenLanguage();void TestSettleSettings();}
static unsigned testVehicleAvailable=2047,testVehicleSummoned,testVehicleSpawned,testOwnedVehicles;
static float testRun=1,testJump=1;static bool testWalkEnabled,testJumpEnabled;static bool testMovementReady=true;static uint32_t testPositionMask,testReturnMask;
static float testSpeed=15;static bool testFlight,testAccept;static uint32_t testAcknowledged;static TS3CheatCommand submitted;static unsigned submitCount;
static const char* testStatus="";static bool testTeleport;static unsigned checks,snapshots;static bool testGameplay=true,testToyBox=true;static DWORD testAge=0;
static void Require(bool ok,const char* what){++checks;if(!ok){DWORD n;HANDLE out=GetStdHandle(STD_OUTPUT_HANDLE);WriteFile(out,what,lstrlenA(what),&n,0);ExitProcess(1);}}
static uint32_t testOpenValue,testOpenCalls,forwardedKeys,forwardedMouse;
static void SetOpenMock(uint32_t value){testOpenValue=value;++testOpenCalls;}
static LRESULT CALLBACK TestWindowProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_KEYDOWN)++forwardedKeys;if(m==WM_MOUSEMOVE)++forwardedMouse;return DefWindowProcW(h,m,w,l);}
static bool Snapshot(TS3CheatSnapshot* s,uint32_t bytes){if(bytes!=sizeof(*s))return false;++snapshots;*s={};s->version=TS3MenuApiVersion;s->contextId=1;s->gameReady=s->toyBox=s->currencyReady=s->objectsReady=s->movementReady=s->npcsReady=1;s->timersReady=s->zurgReady=s->vehiclesReady=s->skyReady=1;s->vehicleAvailableMask=testVehicleAvailable;s->vehicleSummonedMask=testVehicleSummoned;s->vehicleSpawnedMask=testVehicleSpawned;s->spawnedVehicles=testOwnedVehicles;s->movementReady=testMovementReady;s->superSpeed=testWalkEnabled;s->superJump=testJumpEnabled;s->runMultiplier=testRun;s->jumpMultiplier=testJump;s->positionMask=testPositionMask;s->returnMask=testReturnMask;s->lastCommandId=testAcknowledged;s->result=TS3CheatOk;s->teleportPending=testTeleport;lstrcpynA(s->status,testStatus,128);s->gameReady=testGameplay;s->toyBox=testToyBox;s->tick=GetTickCount()-testAge;s->coins=883;s->pixar=71;s->flightSpeed=testSpeed;s->flightEnabled=testFlight;return true;}
static bool SubmitMock(const TS3CheatCommand* c,uint32_t){submitted=*c;++submitCount;return testAccept;}
static void Capture(const wchar_t* path){
 using namespace OverlayHost;auto& f=rt.frames[rt.swap->GetCurrentBackBufferIndex()];Require(Flush(),"render fence failed");
 auto desc=f.buffer->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;UINT rows;UINT64 rowBytes,total;rt.device->GetCopyableFootprints(&desc,0,1,0,&fp,&rows,&rowBytes,&total);
 ID3D12Resource* readback=nullptr;auto heap=Heap(D3D12_HEAP_TYPE_READBACK);auto bd=Buffer(total);Require(SUCCEEDED(rt.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback))),"readback allocation failed");
 Require(SUCCEEDED(f.allocator->Reset())&&SUCCEEDED(rt.list->Reset(f.allocator,nullptr)),"readback reset failed");D3D12_RESOURCE_BARRIER barrier={};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=f.buffer;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;rt.list->ResourceBarrier(1,&barrier);
 D3D12_TEXTURE_COPY_LOCATION src={},dst={};src.pResource=f.buffer;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.pResource=readback;dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=fp;rt.list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;rt.list->ResourceBarrier(1,&barrier);rt.list->Close();ID3D12CommandList* lists[]={rt.list};rt.queue->ExecuteCommandLists(1,lists);Require(Flush(),"readback fence failed");
 void* data=nullptr;Require(SUCCEEDED(readback->Map(0,nullptr,&data)),"readback map failed");HANDLE file=CreateFileW(path,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);Require(file!=INVALID_HANDLE_VALUE,"capture file failed");DWORD done;for(unsigned y=0;y<desc.Height;++y)WriteFile(file,(BYTE*)data+fp.Offset+y*fp.Footprint.RowPitch,DWORD(desc.Width*4),&done,0);CloseHandle(file);readback->Unmap(0,nullptr);readback->Release();
}
extern "C" void TestEntry(){
 CheatMenuUI::TestLanguage(0); // first: settings storage off, so no test run reads, migrates or writes the player's real settings
 {const unsigned char edge[]={255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0};unsigned char reduced[4]={};
  IconMipmaps::Reduce(edge,2,2,8,reduced,1,1,4);
  Require(reduced[0]==255&&reduced[1]==255&&reduced[2]==255&&reduced[3]==64,"icon outline darkened during reduction");
  const unsigned char odd[]={255,0,0,255,0,255,0,255,0,0,255,255};
  IconMipmaps::Reduce(odd,3,1,12,reduced,1,1,4);
  Require(reduced[0]==85&&reduced[1]==85&&reduced[2]==85&&reduced[3]==255,"odd-sized icon lost edge texels");
  Require(IconMipmaps::Levels(512,512)==10&&IconMipmaps::Levels(38,48)==6&&IconMipmaps::Levels(1,1)==1,"invalid icon mip chain");
 }
 using namespace OverlayHost;Require(CheatMenuUI::TestMotion(),"control animation did not interpolate");module=GetModuleHandleW(0);WCHAR system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);lstrcatW(system,L"\\dxgi.dll");HMODULE dxgi=LoadLibraryW(system);GetSystemDirectoryW(system,MAX_PATH);lstrcatW(system,L"\\d3d12.dll");HMODULE d3d=LoadLibraryW(system);Require(dxgi&&d3d,"system graphics libraries unavailable");
 using Factory=HRESULT(WINAPI*)(UINT,REFIID,void**);using Device=HRESULT(WINAPI*)(IUnknown*,D3D_FEATURE_LEVEL,REFIID,void**);
 auto create=(Factory)GetProcAddress(dxgi,"CreateDXGIFactory2");auto createDevice=(Device)GetProcAddress(d3d,"D3D12CreateDevice");IDXGIFactory4* factory=nullptr;Require(SUCCEEDED(create(0,IID_PPV_ARGS(&factory))),"factory failed");IDXGIAdapter1* adapter=nullptr;Require(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))),"CPU WARP adapter unavailable");DXGI_ADAPTER_DESC1 ad={};adapter->GetDesc1(&ad);Require((ad.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)!=0,"Refusing a hardware GPU test");
 ID3D12Device* device=nullptr;Require(SUCCEEDED(createDevice(adapter,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"WARP device failed");ID3D12CommandQueue* queue=nullptr;D3D12_COMMAND_QUEUE_DESC q={};Require(SUCCEEDED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))),"queue failed");
 WNDCLASSW wc={};wc.lpfnWndProc=TestWindowProc;wc.hInstance=module;wc.lpszClassName=L"TS3StandaloneSoftwareTest";RegisterClassW(&wc);HWND window=CreateWindowExW(0,wc.lpszClassName,L"CPU menu test (hidden)",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,module,nullptr);Require(window!=nullptr,"hidden test window failed");
 static const TS3CheatMenuApi api={TS3MenuApiVersion,sizeof(TS3CheatMenuApi),SetOpenMock,Snapshot,SubmitMock};Require(AttachCheatFactory(factory,&api)!=FALSE,"factory hook failed");Require(GameplayAvailable(),"gameplay opening blocked");testGameplay=false;Require(!GameplayAvailable(),"title screen opening allowed");testGameplay=true;testAge=1000;Require(!GameplayAvailable(),"stale gameplay opening allowed");testAge=0;
 DXGI_SWAP_CHAIN_DESC1 sd={};sd.Width=1280;sd.Height=720;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;IDXGISwapChain1* swap=nullptr;Require(SUCCEEDED(factory->CreateSwapChainForHwnd(queue,window,&sd,nullptr,nullptr,&swap)),"software swap chain failed");
 Require(rt.swap&&!rt.ready&&!rt.device,"renderer allocated before first open");
 if(GetFileAttributesW(L"expect-missing-assets")!=INVALID_FILE_ATTRIBUTES){
  open=true;testOpenValue=1;Draw();
  Require(rt.ready&&!rt.failed&&!open&&WarningVisible(),"missing assets did not close menu and show notice");
  Require(testOpenValue==0,"missing assets retained backend input capture");
  Require(CheatMenuUI::TestWarningFont()&&NativeUI::vertexCount>1000&&!NativeUI::Overflowed(),"missing asset warning has no text or visible geometry");
  for(int language=0;language<5;++language){CheatMenuUI::TestLanguage(language);Draw();
   Require(!open&&WarningVisible()&&NativeUI::vertexCount>100,"localized warning disappeared or reopened menu");
   wchar_t path[64];wsprintfW(path,L"missing-assets-%d.rgba",language);Capture(path);
  }
  unsigned keys=forwardedKeys,mouse=forwardedMouse;
  WindowProc(window,WM_KEYDOWN,'W',0);WindowProc(window,WM_MOUSEMOVE,0,0);
  Require(forwardedKeys==keys+1&&forwardedMouse==mouse+1,"warning swallowed game keyboard or mouse input");
  unsigned calls=testOpenCalls;BeforePresent(rt.swap,0);Require(testOpenCalls==calls&&testOpenValue==0,"passive warning reacquired backend input");
  assetWarningTick-=12001;UINT64 serial=rt.serial;BeforePresent(rt.swap,0);
  Require(!WarningVisible()&&!open&&rt.serial==serial,"warning did not expire without capturing input");
  ShowAssetWarning();Close();Require(!WarningVisible()&&!open&&testOpenValue==0,"notice dismissal retained input");
  Require(Flush(),"warning final fence failed");ResetGraphics();
  const char* message="Missing-assets notice: visible in five languages; input released; keyboard/mouse forwarded; timeout and dismissal passed.\r\n";
  DWORD done;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),message,lstrlenA(message),&done,0);ExitProcess(0);
 }
 open=true;Draw();Require(rt.ready&&!rt.failed&&open,"first menu render failed");Require(NativeUI::vertexCount>100&&NativeUI::batchCount>1&&!NativeUI::Overflowed(),"draw geometry invalid");Capture(L"software-1280x720.rgba");Require(CheatMenuUI::TestIconTextures(),"icon mip chains or sharp font atlas missing");
 CheatMenuUI::TestSelectPage(8);NativeUI::TestNavInput(0,false,0,false);Draw();Draw();
 // Apply is disabled only after the matching sky request completes successfully.
 testAccept=true;NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("Sid's Haunted House");for(int i=0;i<20;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();
 testAcknowledged=submitted.id;testStatus="Sky theme requested.";NativeUI::TestNavInput(0,false,0,false);Draw();Draw();
 {char debug[200];DWORD done;int len=wsprintfA(debug,"Sky debug: applied=%d submitted=%u action=%u amount=%u ack=%u haunted=%u western=%u\r\n",CheatMenuUI::TestAppliedSky(),submitted.id,submitted.action,submitted.amount,testAcknowledged,NativeUI::TestNavContains("Sid's Haunted House"),NativeUI::TestNavContains("Western"));WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),debug,len,&done,0);}
 Require(!NativeUI::TestNavContains("Sid's Haunted House")&&NativeUI::TestNavContains("Western"),"applied sky remains clickable or blocks other themes");
 Capture(L"world-applied.rgba");testAccept=false;testStatus="";
 CheatMenuUI::TestSelectPage(7);Draw();NativeUI::TestNavScroll(-10);Draw();Require(CheatMenuUI::TestPageScroll()==0,"Vehicles still scrolls");NativeUI::TestNavScroll(0);
ExitProcess(0);
}
