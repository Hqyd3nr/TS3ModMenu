// CPU-only WARP integration test. Never opens or attaches to the game.
#include "NativeHost.cpp"
namespace CheatMenuUI {void TestPointAt(float,float);void TestSelectPage(int);void TestTheme(int);void TestAnimatedTheme(int);bool TestDark();bool TestMotion();bool TestDrawer();bool TestScrollEdges();bool TestLocalization();void TestLanguage(int);void TestCreditsScroll();void TestLanguageDropdown();}
namespace NativeUI {void TestRevealElapsed(DWORD);void TestPointer(ImVec2,bool,bool);}
namespace NativeUI {void TestNavScroll(float);bool NavSelected(const char*);}
namespace CheatMenuUI {bool TestSearchEditing();bool TestSearchClicks();bool TestSearchFocused();void TestObjectFilter(const char*,int);float TestScale();const char* TestFooter();bool TestMotionCapacity();int TestAppliedSky();float TestPageScroll();bool TestIconTextures();bool TestWarningFont();}
namespace NativeUI {void TestNavInput(int,bool,int,bool);void TestNavFocus(const char*);bool TestNavContains(const char*);bool TestNavVisible(const char*);bool TestNavReachable();void TestNavOff();}
namespace CheatMenuUI {const char* TestKeyConflict(int);}
namespace CheatMenuUI {bool TestSettingsOpen();bool TestLanguageOpen();int TestChosenLanguage();void TestSettleSettings();}
static int testActiveSky=-1;static bool testPlaces=true;static bool testBuzzReady=true,testBuzzOn=false;static bool testCharacterReady=true;static int testCharacter=0;
static unsigned testVehicleAvailable=4095,testVehicleSummoned,testVehicleSpawned,testOwnedVehicles;
static float testRun=1,testJump=1;static bool testWalkEnabled,testJumpEnabled;static bool testMovementReady=true;static uint32_t testPositionMask,testReturnMask;
static float testSpeed=15;static bool testFlight,testAccept;static uint32_t testAcknowledged;static TS3CheatCommand submitted;static unsigned submitCount;
static const char* testStatus="";static bool testTeleport;static unsigned checks,snapshots;static bool testGameplay=true,testToyBox=true;static DWORD testAge=0;
static void Require(bool ok,const char* what){++checks;if(!ok){DWORD n;HANDLE out=GetStdHandle(STD_OUTPUT_HANDLE);WriteFile(out,what,lstrlenA(what),&n,0);ExitProcess(1);}}
static uint32_t testOpenValue,testOpenCalls,forwardedKeys,forwardedMouse;
static void SetOpenMock(uint32_t value){testOpenValue=value;++testOpenCalls;}
static LRESULT CALLBACK TestWindowProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_KEYDOWN)++forwardedKeys;if(m==WM_MOUSEMOVE)++forwardedMouse;return DefWindowProcW(h,m,w,l);}
static bool Snapshot(TS3CheatSnapshot* s,uint32_t bytes){if(bytes!=sizeof(*s))return false;++snapshots;*s={};s->version=TS3MenuApiVersion;s->activeSky=testActiveSky;s->placesReady=testPlaces;s->buzzLaserReady=testBuzzReady;s->buzzLaserEnabled=testBuzzOn;s->characterReady=testCharacterReady;s->character=testCharacter;s->contextId=1;s->gameReady=s->toyBox=s->currencyReady=s->objectsReady=s->movementReady=s->npcsReady=1;s->timersReady=s->zurgReady=s->vehiclesReady=s->skyReady=1;s->vehicleAvailableMask=testVehicleAvailable;s->vehicleSummonedMask=testVehicleSummoned;s->vehicleSpawnedMask=testVehicleSpawned;s->spawnedVehicles=testOwnedVehicles;s->movementReady=testMovementReady;s->superSpeed=testWalkEnabled;s->superJump=testJumpEnabled;s->runMultiplier=testRun;s->jumpMultiplier=testJump;s->positionMask=testPositionMask;s->returnMask=testReturnMask;s->lastCommandId=testAcknowledged;s->result=TS3CheatOk;s->teleportPending=testTeleport;lstrcpynA(s->status,testStatus,128);s->gameReady=testGameplay;s->toyBox=testToyBox;s->tick=GetTickCount()-testAge;s->coins=883;s->pixar=71;s->flightSpeed=testSpeed;s->flightEnabled=testFlight;return true;}
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
 using namespace OverlayHost;Require(CheatMenuUI::TestMotion(),"control animation did not interpolate");Require(CheatMenuUI::TestMotionCapacity(),"control animations stop after many buttons");module=GetModuleHandleW(0);WCHAR system[MAX_PATH];GetSystemDirectoryW(system,MAX_PATH);lstrcatW(system,L"\\dxgi.dll");HMODULE dxgi=LoadLibraryW(system);GetSystemDirectoryW(system,MAX_PATH);lstrcatW(system,L"\\d3d12.dll");HMODULE d3d=LoadLibraryW(system);Require(dxgi&&d3d,"system graphics libraries unavailable");
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
  // A different game build gets the same passive notice with build wording.
  Require(!GameBuildMatches(),"test host matched the game build fingerprint");
  for(int language=0;language<5;++language){CheatMenuUI::TestLanguage(language);ShowBuildWarning();Draw();
   Require(!open&&WarningVisible()&&BuildWarning()&&NativeUI::vertexCount>100,"unsupported-build notice missing");
   wchar_t path[64];wsprintfW(path,L"unsupported-build-%d.rgba",language);Capture(path);Close();
  }
  ShowAssetWarning();Require(!BuildWarning(),"missing-files notice kept build wording");Close();CheatMenuUI::TestLanguage(0);
  Require(Flush(),"warning final fence failed");ResetGraphics();
  const char* message="Missing-assets notice: visible in five languages; input released; keyboard/mouse forwarded; timeout and dismissal passed.\r\n";
  DWORD done;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),message,lstrlenA(message),&done,0);ExitProcess(0);
 }
 // Never read or write the player's real saved settings: start from default Light/English.
 CheatMenuUI::TestLanguage(0);CheatMenuUI::TestTheme(0);
 open=true;unsigned initialFlushes=testTextureFlushes;Draw();Require(rt.ready&&!rt.failed&&open,"first menu render failed");
 Require(testTextureFlushes-initialFlushes==2,"first open did not batch texture uploads into two fences");Require(textureUploadCount==0&&!textureBatch,"completed uploads retained staging resources");
 unsigned cachedFlushes=testTextureFlushes;Close();open=true;CheatMenuUI::Opened(&rt);Draw();Require(testTextureFlushes==cachedFlushes,"reopening reuploaded cached textures");Require(NativeUI::vertexCount>100&&NativeUI::batchCount>1&&!NativeUI::Overflowed(),"draw geometry invalid");Capture(L"software-1280x720.rgba");Require(CheatMenuUI::TestIconTextures(),"icon mip chains or sharp font atlas missing");
 for(int page=1;page<=8;++page){CheatMenuUI::TestSelectPage(page);Draw();Require(open&&!rt.failed&&!NativeUI::Overflowed(),"menu page failed");wchar_t path[64];wsprintfW(path,L"software-page-%d.rgba",page);Capture(path);}
 testToyBox=false;CheatMenuUI::TestSelectPage(0);Draw();Capture(L"story-locked-categories.rgba");testToyBox=true;
 Require(CheatMenuUI::TestDrawer(),"settings drawer animation failed");
 Require(CheatMenuUI::TestScrollEdges(),"scroll overshoot at top/bottom");
 CheatMenuUI::TestAnimatedTheme(1);Draw();float circle[3];Require(NativeUI::Reveal(circle)&&NativeUI::revealVertexCount>0,"theme reveal failed to retain old geometry");
 NativeUI::TestRevealElapsed(240);Draw();Require(NativeUI::Reveal(circle)&&circle[2]>0,"theme reveal midpoint failed");Capture(L"theme-reveal-midpoint.rgba");
 NativeUI::TestRevealElapsed(500);Draw();Require(!NativeUI::Reveal(circle),"theme reveal did not finish");
 CheatMenuUI::TestAnimatedTheme(0);Draw();Require(NativeUI::Reveal(circle),"reverse theme reveal failed");NativeUI::TestRevealElapsed(500);Draw();Require(!NativeUI::Reveal(circle),"reverse reveal did not finish");
 CheatMenuUI::TestTheme(1);
 for(int page=0;page<=8;++page){CheatMenuUI::TestSelectPage(page);Draw();Require(CheatMenuUI::TestDark()&&open&&!rt.failed&&!NativeUI::Overflowed(),"dark menu page failed");wchar_t path[64];wsprintfW(path,L"dark-page-%d.rgba",page);Capture(path);}
 CheatMenuUI::TestTheme(0);Draw();Require(!CheatMenuUI::TestDark(),"light theme restoration failed");
 Require(CheatMenuUI::TestLocalization(),"translation, glyph or language dropdown failure");
 for(int language=0;language<5;++language){CheatMenuUI::TestLanguage(language);
  for(int page=0;page<9;++page){CheatMenuUI::TestSelectPage(page);Draw();Require(open&&!rt.failed&&!NativeUI::Overflowed(),"localized page failed");
   wchar_t path[80];wsprintfW(path,L"language-%d-page-%d.rgba",language,page);Capture(path);}
  CheatMenuUI::TestCreditsScroll();Draw();wchar_t path[80];wsprintfW(path,L"language-%d-credits.rgba",language);Capture(path);
 }
 CheatMenuUI::TestLanguage(1);CheatMenuUI::TestSelectPage(6);CheatMenuUI::TestLanguageDropdown();Draw();Capture(L"language-dropdown.rgba");
 CheatMenuUI::TestLanguage(0);
 CheatMenuUI::TestSelectPage(0);
 Require(SUCCEEDED(rt.swap->ResizeBuffers(2,2560,1440,sd.Format,0)),"resize failed");Require(!rt.ready&&!open,"resize failed to release menu resources/input");open=true;Draw();Require(rt.ready&&!rt.failed&&open&&rt.width==2560&&rt.height==1440,"render after resize failed");Capture(L"software-2560x1440.rgba");

 // Exercise the real menu registry and actions using synthetic controller input.
 CheatMenuUI::TestSelectPage(4);NativeUI::ClearInput();NativeUI::TestNavInput(0,false,0,false);Draw();Draw();
 NativeUI::TestNavFocus("Pict-O-Matic");for(int i=0;i<30;++i)Draw();Require(NativeUI::TestNavVisible("Pict-O-Matic"),"controller cannot scroll to Pict-O-Matic");unsigned pictoBefore=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();
 Require(submitCount==pictoBefore+1&&submitted.action==TS3CheatEquipWeapon&&submitted.amount==8,"Pict-O-Matic did not request native tool 8");
 // Buzz's laser: Player page panel, reachable by controller, sends its own action; off when not Buzz.
 testAcknowledged=submitted.id;NativeUI::TestNavInput(0,false,0,false);CheatMenuUI::TestSelectPage(0);Draw();Draw();Require(NativeUI::TestNavContains("buzz-laser"),"Buzz laser switch missing from Player");NativeUI::TestNavFocus("buzz-laser");for(int i=0;i<30;++i)Draw();
 unsigned laserBefore=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();
 Require(submitCount==laserBefore+1&&submitted.action==TS3CheatBuzzLaser,"Buzz laser switch did not request its action");
 testAcknowledged=submitted.id;NativeUI::TestNavInput(0,false,0,false);testBuzzOn=true;Draw();Draw();Capture(L"player-buzz-laser.rgba");
 testBuzzReady=false;testBuzzOn=false;{const wchar_t* shots[]={L"laser-en.rgba",L"laser-es.rgba",L"laser-fr.rgba",L"laser-de.rgba",L"laser-it.rgba"};const wchar_t* off[]={L"laser-off-en.rgba",L"laser-off-es.rgba",L"laser-off-fr.rgba",L"laser-off-de.rgba",L"laser-off-it.rgba"};NativeUI::TestNavFocus("zurg-transform");for(int i=0;i<40;++i)Draw();for(int lang=0;lang<5;++lang){CheatMenuUI::TestLanguage(lang);testBuzzReady=true;Draw();Draw();Capture(shots[lang]);testBuzzReady=false;Draw();Draw();Capture(off[lang]);}testBuzzReady=false;CheatMenuUI::TestLanguage(0);Draw();Draw();}laserBefore=submitCount;NativeUI::TestNavFocus("buzz-laser");NativeUI::TestNavInput(0,true,0,false);Draw();
 Require(submitCount==laserBefore,"Buzz laser switch accepted input while unavailable");NativeUI::TestNavInput(0,false,0,false);Draw();testBuzzReady=true;
 NativeUI::TestNavFocus("zurg-transform");for(int i=0;i<40;++i)Draw();Require(NativeUI::TestNavVisible("zurg-transform"),"controller cannot scroll Player down to Play as Zurg");Capture(L"player-bottom.rgba");NativeUI::TestNavFocus("buzz-laser");for(int i=0;i<40;++i)Draw();
 // Player > Character: three portrait tiles; the current one sends nothing, others send their index; locked outside Toy Box.
 {Require(NativeUI::TestNavContains("character-woody")&&NativeUI::TestNavContains("character-buzz")&&NativeUI::TestNavContains("character-jessie"),"Character tiles missing from Player");
  NativeUI::TestNavFocus("character-jessie");for(int i=0;i<40;++i)Draw();Require(NativeUI::TestNavVisible("character-jessie"),"controller cannot scroll Player down to Character");
  unsigned before=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();
  Require(submitCount==before+1&&submitted.action==TS3CheatSwitchCharacter&&submitted.amount==2,"Jessie tile did not request a switch to Jessie");
  testTeleport=true;testAcknowledged=submitted.id;NativeUI::TestNavInput(0,false,0,false);Draw();Draw();Require(IsOpen(),"character switch closed the menu");testTeleport=false;Draw();
  NativeUI::TestNavFocus("character-woody");for(int i=0;i<10;++i)Draw();before=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();
  Require(submitCount==before,"current character tile sent a switch");NativeUI::TestNavInput(0,false,0,false);Draw();
  const wchar_t* shots[]={L"character-en.rgba",L"character-es.rgba",L"character-fr.rgba",L"character-de.rgba",L"character-it.rgba"};const wchar_t* off[]={L"character-off-en.rgba",L"character-off-es.rgba",L"character-off-fr.rgba",L"character-off-de.rgba",L"character-off-it.rgba"};
  testCharacter=1;for(int lang=0;lang<5;++lang){CheatMenuUI::TestLanguage(lang);testCharacterReady=true;for(int i=0;i<12;++i)Draw();Capture(shots[lang]);testCharacterReady=false;testCharacter=-1;for(int i=0;i<12;++i)Draw();Capture(off[lang]);testCharacter=1;}
  CheatMenuUI::TestLanguage(0);testCharacterReady=false;testCharacter=-1;Draw();Draw();NativeUI::TestNavFocus("character-buzz");Draw();before=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();
  Require(submitCount==before,"Character tile accepted input outside Toy Box");NativeUI::TestNavInput(0,false,0,false);Draw();
  // Locked laser text: Toy Box asks to switch to Buzz; Story keeps "Play as Buzz in Toy Box".
  testBuzzReady=false;NativeUI::TestNavFocus("buzz-laser");for(int i=0;i<40;++i)Draw();Capture(L"laser-locked-toybox.rgba");testToyBox=false;Draw();Draw();Capture(L"laser-locked-story.rgba");testToyBox=true;testBuzzReady=true;Require(IsOpen(),"menu closed during locked laser captures");
  testCharacterReady=true;testCharacter=0;NativeUI::TestNavFocus("buzz-laser");for(int i=0;i<40;++i)Draw();}
 // D-pad directions on Player follow the layout: Up takes the next row up in the same column,
 // never a sidebar tab or the other column; Left/Right between page and sidebar come back to where they left.
 {bool flight=testFlight;testFlight=false;testBuzzReady=true;
  auto move=[](const char* from,int dir,const char* to,const char* why){NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus(from);for(int i=0;i<40;++i)Draw();NativeUI::TestNavInput(dir,false,0,false);Draw();Require(NativeUI::NavSelected(to),why);NativeUI::TestNavInput(0,false,0,false);Draw();};
  move("character-buzz",1,"flight","Buzz tile Up did not select Flight");
  move("character-woody",1,"flight","Woody tile Up did not select Flight");
  move("character-jessie",1,"flight","Jessie tile Up did not select Flight");
  move("character-jessie",4,"zurg-transform","Jessie tile Right did not select Transform");
  move("zurg-transform",1,"buzz-laser","Transform Up did not select Buzz's laser");
  move("zurg-transform",3,"character-jessie","Transform Left did not select Jessie");
  move("zurg-transform",2,"zurg-transform","Transform Down moved off the last row");
  move("flight",1,"flight","Flight Up left the page");
  move("buzz-laser",2,"zurg-transform","Buzz's laser Down did not select Transform");
  move("character-buzz",3,"Player","Buzz tile Left did not select the open page's tab");
  NativeUI::TestNavInput(4,false,0,false);Draw();Require(NativeUI::NavSelected("character-buzz"),"Right from the tab did not return to Buzz");NativeUI::TestNavInput(0,false,0,false);Draw();
  testBuzzReady=false;Draw();Draw();move("zurg-transform",1,"freeze-timers","Transform Up skipped to a far control when the laser is locked");
  // Teleport: the last places row goes down into the slot in its own column, and back up.
  CheatMenuUI::TestSelectPage(2);Draw();Draw();
  move("place-10",2,"remember-0","Skate Park Down did not select Slot 1");
  move("place-9",2,"remember-1","Stunt Park Down did not select Slot 2");
  move("remember-1",1,"place-9","Slot 2 Up did not select Stunt Park");
  move("remember-0",1,"place-10","Slot 1 Up did not select Skate Park");
  CheatMenuUI::TestSelectPage(0);
  testBuzzReady=true;testFlight=flight;Draw();Draw();}
 NativeUI::TestNavInput(0,false,0,false);Draw();CheatMenuUI::TestSelectPage(5);Draw();Draw();
 NativeUI::TestNavScroll(-10);Draw();Require(CheatMenuUI::TestPageScroll()==116,"right stick did not reach expanded NPC bottom");Draw();Require(CheatMenuUI::TestPageScroll()==116,"expanded NPC scroll bounced");NativeUI::TestNavScroll(0);Draw();Capture(L"expanded-npcs-bottom.rgba");
 Require(!NativeUI::TestNavContains("page-scroll"),"scrollbar became a D-pad selection");
 NativeUI::TestNavFocus("npc-next-6");NativeUI::TestNavInput(0,true,0,false);Draw();NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavInput(1,false,0,false);Draw();Require(NativeUI::NavSelected("Muffin"),"muffin variant up did not select Spawn");
 unsigned muffinBefore=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitCount==muffinBefore+1&&submitted.action==TS3CheatSpawnNpc&&submitted.amount==0x106,"giant muffin variant did not reach backend");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("npc-prev-7");NativeUI::TestNavInput(0,true,0,false);Draw();NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavInput(1,false,0,false);Draw();Require(NativeUI::NavSelected("Characters"),"character variant up did not select Spawn");
 unsigned characterBefore=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitCount==characterBefore+1&&submitted.action==TS3CheatSpawnNpc&&submitted.amount==0x407,"previous Zurg character selection did not reach backend");NativeUI::TestNavInput(0,false,0,false);Draw();
 CheatMenuUI::TestSelectPage(0);NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavScroll(-10);Draw();Require(CheatMenuUI::TestPageScroll()==256,"right stick did not scroll Player to bottom");Draw();Require(CheatMenuUI::TestPageScroll()==256,"Player scroll bounced past bottom");NativeUI::TestNavScroll(0);Draw();Require(CheatMenuUI::TestPageScroll()==256,"Player scroll snapped back after stick release");NativeUI::TestNavScroll(10);Draw();Require(CheatMenuUI::TestPageScroll()==0,"right stick did not scroll Player to top");NativeUI::TestNavScroll(0);
 CheatMenuUI::TestSelectPage(5);NativeUI::ClearInput();NativeUI::TestNavInput(0,false,0,false);Draw();Draw();NativeUI::TestNavFocus("npc-next-0");NativeUI::TestNavInput(3,false,0,false);Draw();Require(NativeUI::NavSelected("npc-prev-0"),"NPC left selected diagonal Spawn");NativeUI::TestNavInput(1,false,0,false);Draw();Require(NativeUI::NavSelected("Townsperson"),"NPC up did not select same-card Spawn");NativeUI::TestNavInput(2,false,0,false);Draw();Require(NativeUI::NavSelected("npc-next-0"),"NPC down did not return to variants");NativeUI::TestNavInput(0,false,0,false);
 for(int p=0;p<9;++p){CheatMenuUI::TestSelectPage(p);NativeUI::ClearInput();NativeUI::TestNavInput(0,false,0,false);Draw();Draw();Require(NativeUI::TestNavReachable(),"controller cannot reach every control");}
 // New controls route distinct slot, vehicle, sky and multiplier commands.
 testPositionMask=testReturnMask=63;CheatMenuUI::TestSelectPage(2);NativeUI::TestNavInput(0,false,0,false);Draw();Draw();Require(NativeUI::TestNavReachable(),"filled teleport slots are not reachable");
 NativeUI::TestNavFocus("return-0");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatReturnPosition&&submitted.amount==0,"wrong teleport slot returned");
 NativeUI::TestNavInput(0,false,0,false);NativeUI::TestNavFocus("remember-5");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatRememberPosition&&submitted.amount==5,"wrong teleport slot saved");
 NativeUI::TestNavInput(0,false,0,false);NativeUI::TestNavFocus("forget-5");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatClearPosition&&submitted.amount==5,"wrong teleport slot cleared");testPositionMask=testReturnMask=0;
 CheatMenuUI::TestSelectPage(7);NativeUI::TestNavInput(0,false,0,false);Draw();Draw();NativeUI::TestNavFocus("Zurg's car");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatSpawnVehicle&&submitted.amount==3,"Zurg car spawn route failed");
 NativeUI::TestNavInput(0,false,0,false);NativeUI::TestNavFocus("Bullseye");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatSpawnVehicle&&submitted.amount==4,"Bullseye spawn route failed");
 NativeUI::TestNavInput(0,false,0,false);NativeUI::TestNavFocus("Dragon");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatSpawnVehicle&&submitted.amount==5,"Dragon spawn route failed");
 {const char* carNames[]={"Sports car","Teeny Racer","Zurg's car"};for(unsigned i=0;i<3;++i){NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus(carNames[i]);for(int j=0;j<12;++j)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatSpawnVehicle&&submitted.amount==(i==2?3:i),"native car summon route failed");}
  testToyBox=false;NativeUI::TestNavInput(0,false,0,false);Draw();Draw();unsigned before=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitCount==before,"Story activated the remembered car selection");NativeUI::TestNavInput(0,false,0,false);NativeUI::TestNavFocus("Vehicles");Draw();Draw();for(const char* name:carNames)Require(!NativeUI::TestNavContains(name),"Story can select Toy Box native car");Require(NativeUI::TestNavContains("Dragon"),"Story mount was disabled with native cars");testToyBox=true;Draw();}
 // Variant selectors preserve IDs and intuitive up/down access to the action.
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("vehicle-next-0");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();
 NativeUI::TestNavInput(1,false,0,false);Draw();Require(NativeUI::NavSelected("Sports car"),"sports variant Up did not reach Summon");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatSpawnVehicle&&submitted.amount==2,"Monster is not a Sports car variant");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("vehicle-next-2");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();
 NativeUI::TestNavInput(1,false,0,false);Draw();Require(NativeUI::NavSelected("Bullseye"),"Bullseye variant Up did not reach Spawn");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatSpawnVehicle&&submitted.amount==10,"Bizarro Bullseye route failed");
 for(unsigned id=6;id<=9;++id){
  NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("vehicle-next-1");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();
  NativeUI::TestNavInput(1,false,0,false);Draw();Require(NativeUI::NavSelected("Teeny Racer"),"Mini variant Up did not reach Summon");
  NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatSpawnVehicle&&submitted.amount==id,"Mini finish route failed");
 }
 // Previously spawned vehicles remain actionable when capacity is available.
 testVehicleAvailable=4095;testVehicleSummoned=1u<<9;testVehicleSpawned=1u<<10;
 CheatMenuUI::TestSelectPage(7);NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("Vehicles");Draw();Draw();
 Require(NativeUI::TestNavContains("Teeny Racer")&&NativeUI::TestNavContains("Sports car")&&NativeUI::TestNavContains("Bullseye"),"live vehicle imposed one-per-variant limit");
 Require(!NativeUI::TestNavContains("Monster car"),"Monster car remains a separate card");
 Capture(L"vehicles-repeatable.rgba");
 CheatMenuUI::TestSelectPage(0);Draw();CheatMenuUI::TestSelectPage(7);Draw();Draw();Require(NativeUI::TestNavContains("Bullseye"),"reopening disabled another Bullseye spawn");
 NativeUI::TestNavFocus("Bullseye");unsigned repeatBefore=submitCount;for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitCount==repeatBefore+1&&submitted.amount==10,"existing Bizarro prevented a second request");
 NativeUI::TestNavInput(0,false,0,false);Draw();testVehicleAvailable=(1u<<4)|(1u<<5)|(1u<<10);NativeUI::TestNavFocus("Vehicles");Draw();Draw();
 Require(!NativeUI::TestNavContains("Teeny Racer")&&!NativeUI::TestNavContains("Sports car")&&!NativeUI::TestNavContains("Zurg's car")&&NativeUI::TestNavContains("Bullseye"),"native car pool exhaustion incorrectly blocked independent mounts");
 testOwnedVehicles=5;Draw();Draw();Require(NativeUI::TestNavContains("clear-vehicles"),"owned vehicles cannot be cleared");
 NativeUI::TestNavFocus("clear-vehicles");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatClearVehicles,"clear vehicles command mismatch");
 NativeUI::TestNavInput(0,false,0,false);Draw();Capture(L"vehicles-slots-full.rgba");
 Require(NativeUI::TestNavReachable(),"clear or vehicle controls are unreachable with full pool");
 testOwnedVehicles=0;NativeUI::TestNavFocus("Vehicles");Draw();Draw();Require(!NativeUI::TestNavContains("clear-vehicles"),"empty vehicle clear is enabled");
 testVehicleAvailable=4095;testVehicleSummoned=testVehicleSpawned=0;Draw();Draw();
 Require(NativeUI::TestNavReachable(),"vehicle variant controls are unreachable");
 NativeUI::TestNavFocus("vehicle-next-3");NativeUI::TestNavInput(0,true,0,false);Draw();NativeUI::TestNavInput(0,false,0,false);Draw();
 NativeUI::TestNavFocus("Zurg's car");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatSpawnVehicle&&submitted.amount==11,"Zurg monster selector routed to the wrong native size");NativeUI::TestNavInput(0,false,0,false);Draw();

 CheatMenuUI::TestSelectPage(8);NativeUI::TestNavInput(0,false,0,false);Draw();Draw();NativeUI::TestNavFocus("Sid's Haunted House");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatWorldSky&&submitted.amount==1,"Haunted sky route failed");
 // Apply is disabled only after the matching sky request completes successfully.
 testAccept=true;NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("Sid's Haunted House");for(int i=0;i<20;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();
 testActiveSky=3;NativeUI::TestNavInput(0,false,0,false);Draw();Require(CheatMenuUI::TestAppliedSky()==3,"existing native sky was not reflected");
 testAcknowledged=submitted.id;testStatus="Sky theme requested.";NativeUI::TestNavInput(0,false,0,false);Draw();Draw();
 Require(lstrcmpA(CheatMenuUI::TestFooter(),"Sky theme requested.")==0,"sky reported applied before native readback");
 testAcknowledged=submitted.id;testStatus="Sky theme requested.";testActiveSky=1;NativeUI::TestNavInput(0,false,0,false);Draw();Draw();
 Require(lstrcmpA(CheatMenuUI::TestFooter(),"Sky theme applied.")==0,"sky readback did not confirm the applied theme");
 unsigned skyBefore=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitCount==skyBefore,"applied sky accepted a duplicate click");NativeUI::TestNavInput(0,false,0,false);NativeUI::TestNavFocus("World");Draw();Draw();
 Require(!NativeUI::TestNavContains("Sid's Haunted House")&&NativeUI::TestNavContains("Western"),"applied sky remains clickable or blocks other themes");
 Capture(L"world-applied.rgba");testAccept=false;testStatus="";
 // Teleport places: hub-only, routed to native travel with the chosen index.
 CheatMenuUI::TestSelectPage(2);testPlaces=false;NativeUI::TestNavInput(0,false,0,false);Draw();Draw();
 Require(!NativeUI::TestNavContains("place-0")&&NativeUI::TestNavContains("remember-0"),"places enabled outside the Toy Box hub");
 testPlaces=true;NativeUI::TestNavFocus("place-0");for(int i=0;i<30;++i)Draw();Require(NativeUI::TestNavVisible("place-0"),"controller cannot scroll back to places");Capture(L"teleport-places.rgba");NativeUI::TestNavFocus("place-4");for(int i=0;i<12;++i)Draw();
 NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatTeleportPlace&&submitted.amount==4,"place did not request native travel");
 testAcknowledged=submitted.id;NativeUI::TestNavInput(0,false,0,false);Draw();
 NativeUI::TestNavFocus("forget-5");for(int i=0;i<30;++i)Draw();Require(NativeUI::TestNavVisible("forget-5"),"controller cannot scroll to the last saved point");
 // Objects: full game carriable catalogue, search, categories and controller reach.
 CheatMenuUI::TestSelectPage(3);CheatMenuUI::TestObjectFilter("",0);NativeUI::TestNavInput(0,false,0,false);Draw();Draw();Capture(L"objects-all.rgba");
 Require(NativeUI::TestNavContains("Medium barrel")&&NativeUI::TestNavContains("Tomato")&&NativeUI::TestNavContains("Space parcel B")&&!NativeUI::Overflowed(),"object catalogue incomplete or over budget");
 NativeUI::TestNavFocus("Tomato");for(int i=0;i<40;++i){Draw();Require(IsOpen()&&!NativeUI::Overflowed()&&NativeUI::vertexCount<100000,"objects page exceeded the draw budget while scrolling");}Require(NativeUI::TestNavVisible("Tomato"),"controller cannot reach the last object");
 // Controller: search box hidden, typed text ignored, TNT reachable by focus.
 CheatMenuUI::TestObjectFilter("tnt",0);Draw();Draw();Require(NativeUI::TestNavContains("Medium barrel")&&!NativeUI::TestNavContains("object-search"),"hidden search still filters for controller");
 CheatMenuUI::TestObjectFilter("",0);NativeUI::TestNavFocus("TNT barrel");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();
 Require(submitted.action==TS3CheatSpawnObject&&submitted.amount==8,"TNT barrel requested the wrong object");testAcknowledged=submitted.id;NativeUI::TestNavInput(0,false,0,false);Draw();
 CheatMenuUI::TestObjectFilter("",5);Draw();Draw();Require(NativeUI::TestNavContains("Carrot")&&!NativeUI::TestNavContains("Ball"),"Food category did not filter");
 // Mouse: search box shown and filtering; click focus, Ctrl+A and Backspace.
 NativeUI::TestNavOff();CheatMenuUI::TestObjectFilter("tnt",0);Draw();Draw();Require(NativeUI::TestNavContains("TNT barrel")&&!NativeUI::TestNavContains("Medium barrel"),"object search did not filter");Capture(L"objects-search.rgba");
 // Search X hover (light and dark), then move away.
 CheatMenuUI::TestPointAt(987,205);Draw();Draw();Capture(L"objects-search-clear-hover.rgba");
 CheatMenuUI::TestTheme(1);Draw();Draw();Capture(L"objects-search-clear-hover-dark.rgba");CheatMenuUI::TestTheme(0);
 CheatMenuUI::TestPointAt(-1,-1);Draw();
 Require(CheatMenuUI::TestSearchEditing(),"search editing (Ctrl+A, Backspace, typing) failed");
 CheatMenuUI::TestObjectFilter("",0);Draw();Require(CheatMenuUI::TestSearchClicks(),"search box click focus failed");
 CheatMenuUI::TestObjectFilter("",5);Draw();Draw();Require(NativeUI::TestNavContains("Carrot")&&!NativeUI::TestNavContains("Ball"),"Food category did not filter with mouse");Capture(L"objects-food.rgba");
 CheatMenuUI::TestLanguage(1);CheatMenuUI::TestObjectFilter("bateria",0);Draw();Draw();Require(NativeUI::TestNavContains("Space battery")&&!NativeUI::TestNavContains("Carrot"),"accent-insensitive search failed");
 CheatMenuUI::TestObjectFilter("zzz",0);Draw();Draw();Require(!NativeUI::TestNavContains("Carrot"),"empty search result still lists objects");
 CheatMenuUI::TestLanguage(0);CheatMenuUI::TestObjectFilter("",0);
 CheatMenuUI::TestSelectPage(7);Draw();NativeUI::TestNavScroll(-10);Draw();Require(CheatMenuUI::TestPageScroll()==0,"Vehicles still scrolls");NativeUI::TestNavScroll(0);
 // Direct sliders start at normal movement; no hidden toggle activation.
 testRun=testJump=1;testWalkEnabled=testJumpEnabled=false;CheatMenuUI::TestSelectPage(0);
 NativeUI::TestNavInput(0,false,0,false);unsigned openingBefore=submitCount;Draw();Draw();
 Require(submitCount==openingBefore,"opening Player changed normal movement");
 Require(!NativeUI::TestNavContains("super-speed")&&!NativeUI::TestNavContains("super-jump"),"removed movement toggle remains selectable");
 Require(NativeUI::TestNavContains("run-slider")&&NativeUI::TestNavContains("jump-slider"),"direct movement sliders require an old toggle");
 Require(!NativeUI::TestNavContains("run-reset")&&!NativeUI::TestNavContains("jump-reset"),"default movement reset remains selectable");
 NativeUI::TestNavFocus("run-slider");NativeUI::TestNavInput(0,false,1,false);Draw();
 Require(submitCount==openingBefore+1&&submitted.action==TS3CheatRunMultiplier&&submitted.value>1.09f&&submitted.value<1.11f,"Walk speed cannot adjust directly from normal");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("jump-slider");NativeUI::TestNavInput(0,false,-1,false);Draw();
 Require(submitCount==openingBefore+2&&submitted.action==TS3CheatJumpMultiplier&&submitted.value>.89f&&submitted.value<.91f,"Jump height cannot adjust directly below normal");
 NativeUI::TestNavInput(0,false,0,false);testMovementReady=false;Draw();Draw();
 Require(!NativeUI::TestNavContains("run-slider")&&!NativeUI::TestNavContains("jump-slider"),"unavailable movement sliders are selectable");
 unsigned disabledBefore=submitCount;NativeUI::TestNavFocus("run-slider");NativeUI::TestNavInput(0,false,1,false);Draw();
 NativeUI::TestNavFocus("jump-slider");NativeUI::TestNavInput(0,false,1,false);Draw();
 Require(submitCount==disabledBefore,"unavailable movement submitted a change");
 testMovementReady=true;
 testRun=2;testJump=1.8f;CheatMenuUI::TestSelectPage(0);NativeUI::TestNavInput(0,false,0,false);Draw();Draw();NativeUI::TestNavFocus("run-slider");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,false,1,false);Draw();Require(submitted.action==TS3CheatRunMultiplier&&submitted.value>2.09f&&submitted.value<2.11f,"running multiplier stick adjustment failed");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("run-slider");
 NativeUI::TestNavInput(2,false,0,false);Draw();Require(NativeUI::NavSelected("run-reset"),"Walk speed down did not reach Reset");
 NativeUI::TestNavInput(1,false,0,false);Draw();Require(NativeUI::NavSelected("run-slider"),"Walk Reset up did not return to slider");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("run-reset");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatRunMultiplier&&submitted.value==1,"running multiplier reset failed");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("jump-slider");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,false,1,false);Draw();Require(submitted.action==TS3CheatJumpMultiplier&&submitted.value>1.89f&&submitted.value<1.91f,"jump multiplier stick adjustment failed");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("jump-reset");for(int i=0;i<12;++i)Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatJumpMultiplier&&submitted.value==1,"jump multiplier reset failed");testRun=testJump=1;
 NativeUI::TestNavInput(0,false,0,false);Draw();Draw();
 Require(!NativeUI::TestNavContains("run-reset")&&!NativeUI::TestNavContains("jump-reset"),"Reset did not hide when restored to defaults");
 Require(NativeUI::NavSelected("jump-slider"),"hidden Reset lost controller focus");
 Capture(L"movement-default-enabled.rgba");
 testRun=2;testJump=1.8f;Draw();Draw();Capture(L"movement-modified-enabled.rgba");testTeleport=true;NativeUI::TestNavFocus("Player");Draw();Draw();Require(!NativeUI::TestNavContains("run-slider")&&!NativeUI::TestNavContains("jump-slider"),"transform transition allowed slider commands");Capture(L"movement-transforming.rgba");testTeleport=false;
 testMovementReady=false;Draw();Draw();Capture(L"movement-modified-disabled.rgba");testMovementReady=true;
 testRun=testJump=1;
 NativeUI::TestNavInput(0,false,0,false);
 CheatMenuUI::TestSelectPage(0);testToyBox=false;Draw();Draw();Require(!NativeUI::TestNavContains("Currency")&&!NativeUI::TestNavContains("Weapons"),"controller selects locked Story category");testToyBox=true;Draw();
 NativeUI::TestNavFocus("settings-cog");NativeUI::TestNavInput(0,true,0,false);Draw();Require(CheatMenuUI::TestSettingsOpen(),"A did not open Settings");CheatMenuUI::TestSettleSettings();NativeUI::TestNavInput(0,false,0,false);Draw();
 Require(!NativeUI::TestNavContains("flight"),"Settings allows background navigation");
 {auto used=[](int vk,const char* why){const char* got=CheatMenuUI::TestKeyConflict(vk);return why?got&&lstrcmpA(got,why)==0:!got;};
  const char* no="This key can't be used.";
  Require(used('W',no)&&used('D',no)&&used(VK_SPACE,no)&&used('E',no)&&used('Q',no)&&used(VK_LSHIFT,no)&&used(VK_RCONTROL,no)&&used(VK_ESCAPE,no)&&used(VK_LWIN,no)&&used(VK_RWIN,no),"movement/flight/menu/Windows key accepted as menu key");
  Require(used('K',nullptr)&&used('F',nullptr)&&used('5',nullptr)&&used(VK_RETURN,nullptr)&&used(VK_UP,nullptr)&&used(VK_BACK,nullptr),"ordinary key refused");
  Require(used(VK_F12,no)&&used(VK_RMENU,no)&&used(VK_HOME,nullptr),"system key accepted, or ReShade's Home still refused");
  Require(used(VK_TAB,nullptr)&&used(VK_F1,nullptr)&&used(VK_INSERT,nullptr)&&used(VK_END,nullptr)&&used(VK_OEM_4,nullptr),"free key refused");}
 NativeUI::TestNavFocus("open-binding");for(int i=0;i<30;++i)Draw();Require(NativeUI::TestNavVisible("open-binding")&&!NativeUI::TestNavContains("reset-position"),"menu key control missing from Settings");
 NativeUI::TestNavInput(0,true,0,false);Draw();Require(CheatMenuUI::Capturing(),"A did not start choosing a menu button");NativeUI::TestNavInput(0,false,0,false);Draw();Draw();Require(CheatMenuUI::Capturing(),"menu button change ended without a press");Capture(L"settings-controls-capture.rgba");
 NativeUI::TestNavInput(0,false,0,false);CheatMenuUI::Closed(&rt);Require(!CheatMenuUI::Capturing(),"closing did not cancel the menu button change");Draw();Capture(L"settings-controls.rgba");NativeUI::TestNavOff();Draw();Draw();Capture(L"settings-controls-keyboard.rgba");
 NativeUI::TestNavFocus("language-select");for(int i=0;i<30;++i)Draw();Require(NativeUI::TestNavVisible("language-select"),"controller failed to scroll Settings up");
 NativeUI::TestNavInput(0,true,0,false);Draw();Require(CheatMenuUI::TestLanguageOpen(),"A did not open Language");NativeUI::TestNavInput(0,false,0,false);Draw();Require(NativeUI::TestNavReachable(),"controller cannot reach language options");
 NativeUI::TestNavFocus("language-1");NativeUI::TestNavInput(0,true,0,false);Draw();Require(!CheatMenuUI::TestLanguageOpen()&&CheatMenuUI::TestChosenLanguage()==1,"A did not select Spanish");CheatMenuUI::TestLanguage(0);
 NativeUI::TestNavInput(0,false,0,true);Draw();Require(!CheatMenuUI::TestSettingsOpen()&&IsOpen(),"B did not close only Settings");CheatMenuUI::TestSettleSettings();
 NativeUI::TestNavInput(0,false,0,false);testFlight=false;Draw();Require(!NativeUI::TestNavContains("speed-slider"),"disabled speed is focusable");
 testFlight=true;Draw();Draw();NativeUI::TestNavFocus("speed-slider");unsigned before=submitCount;NativeUI::TestNavInput(0,false,1,false);Draw();Require(submitCount==before+1&&submitted.action==TS3CheatFlightSpeed,"stick did not send flight speed");Capture(L"controller-focus.rgba");
 testSpeed=45;NativeUI::TestNavInput(0,false,0,false);Draw();Draw();NativeUI::TestNavFocus("flight");NativeUI::TestNavInput(2,false,0,false);Draw();Require(NativeUI::NavSelected("speed-slider"),"Flight down did not select Speed");
 NativeUI::TestNavInput(2,false,0,false);Draw();Require(NativeUI::NavSelected("reset-speed"),"Speed down did not select Reset");NativeUI::TestNavInput(1,false,0,false);Draw();Require(NativeUI::NavSelected("speed-slider"),"Reset up skipped Speed");NativeUI::TestNavInput(1,false,0,false);Draw();Require(NativeUI::NavSelected("flight"),"Speed up did not select Flight");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("reset-speed");NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitted.action==TS3CheatFlightSpeed&&submitted.value==15&&NativeUI::NavSelected("speed-slider"),"Reset did not return focus to Speed");testSpeed=15;NativeUI::TestNavInput(0,false,0,false);Draw();Draw();Require(!NativeUI::TestNavContains("reset-speed")&&NativeUI::NavSelected("speed-slider"),"disappearing Reset lost controller focus");
 NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("freeze-timers");for(int i=0;i<12;++i)Draw();unsigned timerBefore=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitCount==timerBefore+1&&submitted.action==TS3CheatFreezeTimers,"controller did not toggle freeze timers");
 // Delay an accepted toggle acknowledgement: unrelated controls retain their
 // enabled appearance/navigation, but repeat activations cannot enqueue work.
 testAccept=true;NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavFocus("god-mode");NativeUI::TestNavInput(0,true,0,false);Draw();unsigned acceptedCount=submitCount;
 for(int i=0;i<8;++i){NativeUI::TestNavInput(0,false,0,false);Draw();Require(NativeUI::TestNavContains("zurg-transform")&&NativeUI::TestNavContains("god-mode")&&NativeUI::TestNavContains("freeze-timers"),"toggle acknowledgement flashes controls disabled");}
 NativeUI::TestNavFocus("god-mode");NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitCount==acceptedCount,"duplicate command submitted while awaiting acknowledgement");
 testAcknowledged=submitted.id;NativeUI::TestNavInput(0,false,0,false);Draw();NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitCount==acceptedCount+1,"toggle did not resume after acknowledgement");testAcknowledged=submitted.id;NativeUI::TestNavInput(0,false,0,false);Draw();
 CheatMenuUI::TestSelectPage(1);Draw();NativeUI::TestNavFocus("add-coins");NativeUI::TestNavInput(0,true,0,false);Draw();NativeUI::TestNavInput(0,false,0,false);Draw();Require(submitted.action==TS3CheatAddCoins&&!NativeUI::TestNavContains("add-pixar"),"queued currency did not block Pixar Balls");unsigned currencyCount=submitCount;NativeUI::TestNavInput(0,true,0,false);Draw();Require(submitCount==currencyCount,"queued currency accepted a duplicate");NativeUI::TestNavInput(0,false,0,false);testAcknowledged=submitted.id;Draw();Require(NativeUI::TestNavContains("add-coins")&&NativeUI::TestNavContains("add-pixar"),"currency buttons did not recover");
 NativeUI::TestNavInput(0,false,0,true);Draw();Require(!IsOpen(),"B did not close menu");NativeUI::TestNavOff();
 open=true;testOpenValue=1;testGameplay=false;Draw();Require(!IsOpen()&&testOpenValue==0,"No game did not close menu and release input");
 unsigned resumeKeys=forwardedKeys,resumeMouse=forwardedMouse;WindowProc(window,WM_KEYDOWN,'W',0);WindowProc(window,WM_MOUSEMOVE,0,0);Require(forwardedKeys==resumeKeys+1&&forwardedMouse==resumeMouse+1,"automatic close retained keyboard or mouse capture");
 testGameplay=true;open=true;Draw();Require(IsOpen(),"valid gameplay failed after automatic close");
 // Large displays keep the compact 1x menu rather than growing with the screen.
 CheatMenuUI::TestSelectPage(0);open=true;Draw();Draw();Require(CheatMenuUI::TestScale()==1.f,"menu grew past 1x at 1440p");
 // Unsupported build: the notice loads its own font when the menu never opened.
 Close();CheatMenuUI::Reset(&rt);ShowBuildWarning();Draw();
 Require(!IsOpen()&&WarningVisible()&&BuildWarning()&&CheatMenuUI::TestWarningFont()&&NativeUI::vertexCount>1000,"unsupported-build notice has no text");
 Capture(L"unsupported-build.rgba");
 Close();Require(!IsOpen(),"close failed");unsigned oldSnapshots=snapshots;UINT64 oldSerial=rt.serial;BeforePresent(rt.swap,0);Require(snapshots==oldSnapshots&&rt.serial==oldSerial,"hidden menu did work");Require(Flush(),"final fence failed");ResetGraphics();Require(!rt.device&&!rt.ready,"cleanup failed");
 char text[160];int n=wsprintfA(text,"CPU-only WARP renderer: %u checks passed; no game launch or ReShade dependency.\r\n",checks);DWORD done;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),text,n,&done,0);ExitProcess(0);
}
