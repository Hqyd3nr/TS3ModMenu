#include "../menu/OverlayHost.h"
#include "Shaders.h"
#include "IconMipmaps.h"
extern "C" int _fltused=0;
#pragma function(memset,memcpy)
extern "C" void* memset(void* p,int v,size_t n){volatile BYTE* d=(BYTE*)p;for(size_t i=0;i<n;++i)d[i]=(BYTE)v;return p;}
extern "C" void* memcpy(void* p,const void* s,size_t n){volatile BYTE* d=(BYTE*)p;const BYTE* a=(const BYTE*)s;for(size_t i=0;i<n;++i)d[i]=a[i];return p;}
bool InstallOptionalEffects(HMODULE,HWND,void(*)());
void DrainOptionalEffects();
namespace OverlayHost {
struct Frame{ID3D12CommandAllocator* allocator;ID3D12Resource* buffer;ID3D12Resource* vertices;void* mapped;UINT64 fence;};
struct Runtime{
 IDXGISwapChain3* swap;ID3D12CommandQueue* queue;ID3D12Device* device;HWND window;WNDPROC oldProc;
 ID3D12DescriptorHeap* srv;ID3D12DescriptorHeap* rtv;ID3D12RootSignature* root;ID3D12PipelineState* pipeline;
 ID3D12GraphicsCommandList* list;ID3D12Fence* fence;HANDLE event;UINT64 serial;Frame frames[8];unsigned count,descriptorSize,rtvSize,nextDescriptor,width,height;
 Texture white;bool ready,failed;
};
static Runtime rt;static HMODULE module;static const TS3CheatMenuApi* backend;static volatile LONG open;static bool oldTab,oldEscape;static int oldOpenKey;static bool openingRequested;static DWORD openingTick;static bool assetWarning,buildWarning;static DWORD assetWarningTick;static SRWLOCK lock=SRWLOCK_INIT;
using PresentFn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using Present1Fn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*,UINT,UINT,const DXGI_PRESENT_PARAMETERS*);
using ResizeFn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
using Resize1Fn=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*,UINT,UINT,UINT,DXGI_FORMAT,UINT,const UINT*,IUnknown* const*);
using CreateFn=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using CreateHwndFn=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
static LONG presenting;static bool optionalEffects,insideEffects;static DWORD lastEffects;
static PresentFn originalPresent;static Present1Fn originalPresent1;static ResizeFn originalResize;static Resize1Fn originalResize1;static CreateFn originalCreate;static CreateHwndFn originalCreateHwnd;
static void Log(const char* text){OutputDebugStringA(text);WCHAR path[MAX_PATH];DWORD n=GetModuleFileNameW(module,path,MAX_PATH);if(!n||n>=MAX_PATH)return;while(n&&path[n-1]!=L'\\')--n;path[n]=0;lstrcatW(path,L"Mod Menu\\renderer.log");HANDLE f=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(f!=INVALID_HANDLE_VALUE){DWORD done;WriteFile(f,text,lstrlenA(text),&done,0);WriteFile(f,"\r\n",2,&done,0);CloseHandle(f);}}
HMODULE Module(){return module;}const TS3CheatMenuApi* Backend(){return backend;}bool IsOpen(){return open;}
void Close(){assetWarning=false;openingRequested=false;InterlockedExchange(&open,0);if(backend)backend->SetOpen(0);}
void ShowAssetWarning(){Close();NativeUI::EndReveal();assetWarningTick=GetTickCount();assetWarning=true;buildWarning=false;}
void ShowBuildWarning(){ShowAssetWarning();buildWarning=true;}
bool BuildWarning(){return buildWarning;}
// Same vtable fingerprint the backend checks before installing hooks. The
// backend replaces these slots on a supported build, so a slot pointing outside
// the game image (a hook) also counts. A different build gets a passive notice.
static bool Readable(const void* p,SIZE_T bytes){
 MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(p,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
 if(!(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
 return (const BYTE*)p+bytes<=(const BYTE*)m.BaseAddress+m.RegionSize;
}
bool GameBuildMatches(){
 BYTE* base=(BYTE*)GetModuleHandleW(nullptr);if(!base)return false;
 auto dos=(IMAGE_DOS_HEADER*)base;if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return false;
 auto nt=(IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return false;
 const struct {ULONG_PTR table;int slot;ULONG_PTR expected;} checks[]={{0x7ae830,17,0x561870},{0x7ae830,20,0x5567e0},{0x7d7bd8,16,0x439020},{0x7d7bd8,17,0x439b00},{0x7608c0,16,0x50240}};
 for(const auto& c:checks){ULONG_PTR at=c.table+c.slot*sizeof(void*);
  if(at+sizeof(void*)>nt->OptionalHeader.SizeOfImage||!Readable(base+at,sizeof(void*)))return false;
  BYTE* value=*(BYTE**)(base+at);bool hooked=value<base||value>=base+nt->OptionalHeader.SizeOfImage;
  if(value!=base+c.expected&&!hooked)return false;}
 return true;
}
#ifdef UI_SELF_TEST
static bool testBuildSupported=true;
static bool SupportedBuild(){return testBuildSupported;}
#else
static bool SupportedBuild(){static int known=-1;if(known<0)known=GameBuildMatches()?1:0;return known==1;}
#endif

static bool WarningVisible(){if(assetWarning&&DWORD(GetTickCount()-assetWarningTick)>=12000)assetWarning=false;return assetWarning;}
static bool Foreground(){HWND f=GetForegroundWindow();return f&&rt.window&&GetAncestor(f,GA_ROOT)==GetAncestor(rt.window,GA_ROOT);}
template<class T>static void Release(T*& p){if(p){p->Release();p=nullptr;}}
static bool Wait(UINT64 value){if(!value||rt.fence->GetCompletedValue()>=value)return true;if(FAILED(rt.fence->SetEventOnCompletion(value,rt.event)))return false;return WaitForSingleObject(rt.event,2000)==WAIT_OBJECT_0;}
#ifdef UI_SELF_TEST
static unsigned testTextureFlushes;
#endif
static bool Flush(){
#ifdef UI_SELF_TEST
 ++testTextureFlushes;
#endif
 if(!rt.queue||!rt.fence)return true;UINT64 v=++rt.serial;return SUCCEEDED(rt.queue->Signal(rt.fence,v))&&Wait(v);}
void WaitForTextures(Runtime*){if(!Flush())rt.failed=true;}
void DestroyTexture(Runtime*,Texture& texture){Release(texture.resource);texture={};}
static D3D12_HEAP_PROPERTIES Heap(D3D12_HEAP_TYPE type){D3D12_HEAP_PROPERTIES h={};h.Type=type;h.CreationNodeMask=h.VisibleNodeMask=1;return h;}
static D3D12_RESOURCE_DESC Buffer(UINT64 bytes){D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return d;}
// Retain uploads/allocators until one shared fence completes for the asset batch.
// Same-queue ordering preserves upload-before-draw without a wait per icon.
struct TextureUpload {ID3D12Resource* upload;ID3D12CommandAllocator* allocator;ID3D12GraphicsCommandList* command;};
static TextureUpload textureUploads[256];static unsigned textureUploadCount;static bool textureBatch;
void BeginTextureBatch(Runtime*){textureBatch=true;}
bool EndTextureBatch(Runtime*){
 textureBatch=false;if(!textureUploadCount)return !rt.failed;
 if(!Flush()){rt.failed=true;return false;} // Never free resources still used by the GPU.
 for(unsigned i=0;i<textureUploadCount;++i){auto& u=textureUploads[i];Release(u.command);Release(u.allocator);Release(u.upload);}
 textureUploadCount=0;return !rt.failed;
}
Texture CreateTextureRGBA(Runtime*,uint32_t w,uint32_t h,const void* pixels,bool mipmaps){
 Texture out;if(!rt.device||!rt.srv||rt.nextDescriptor>=256||(textureBatch&&textureUploadCount>=256)||!w||!h||w>8192||h>8192||!pixels)return out;
 const unsigned levels=mipmaps?IconMipmaps::Levels(w,h):1;
 D3D12_RESOURCE_DESC desc={};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=w;desc.Height=h;desc.DepthOrArraySize=1;desc.MipLevels=(UINT16)levels;desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 auto heap=Heap(D3D12_HEAP_TYPE_DEFAULT);if(FAILED(rt.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&out.resource))))return {};
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprints[14];UINT64 total;rt.device->GetCopyableFootprints(&desc,0,levels,0,footprints,nullptr,nullptr,&total);
 ID3D12Resource* upload=nullptr;auto ud=Buffer(total);heap=Heap(D3D12_HEAP_TYPE_UPLOAD);ID3D12CommandAllocator* allocator=nullptr;ID3D12GraphicsCommandList* cmd=nullptr;
 if(FAILED(rt.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&ud,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)))){Release(out.resource);return {};}
 void* mapped=nullptr;D3D12_RANGE empty={0,0};if(FAILED(upload->Map(0,&empty,&mapped))){Release(upload);Release(out.resource);return {};}
 for(UINT y=0;y<h;++y)memcpy((BYTE*)mapped+footprints[0].Offset+y*footprints[0].Footprint.RowPitch,(const BYTE*)pixels+y*w*4,w*4);
 for(unsigned i=1;i<levels;++i){const auto& prev=footprints[i-1];const auto& next=footprints[i];
  IconMipmaps::Reduce((const BYTE*)mapped+prev.Offset,prev.Footprint.Width,prev.Footprint.Height,prev.Footprint.RowPitch,
                      (BYTE*)mapped+next.Offset,next.Footprint.Width,next.Footprint.Height,next.Footprint.RowPitch);
 }upload->Unmap(0,nullptr);
 bool ok=SUCCEEDED(rt.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)))&&SUCCEEDED(rt.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator,nullptr,IID_PPV_ARGS(&cmd)));
 if(ok){for(unsigned i=0;i<levels;++i){D3D12_TEXTURE_COPY_LOCATION dst={};dst.pResource=out.resource;dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=i;D3D12_TEXTURE_COPY_LOCATION src={};src.pResource=upload;src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=footprints[i];cmd->CopyTextureRegion(&dst,0,0,0,&src,nullptr);}
  D3D12_RESOURCE_BARRIER barrier={};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=out.resource;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;cmd->ResourceBarrier(1,&barrier);ok=SUCCEEDED(cmd->Close());
  if(ok){ID3D12CommandList* lists[]={cmd};rt.queue->ExecuteCommandLists(1,lists);ok=textureBatch||Flush();}
 }
 if(!ok){rt.failed=true;/* Retain resources if a GPU fence failed; never free in-flight memory. */return {};}
 if(textureBatch){textureUploads[textureUploadCount++]={upload,allocator,cmd};}else{Release(cmd);Release(allocator);Release(upload);}out.handle=rt.nextDescriptor++;out.width=w;out.height=h;
 D3D12_SHADER_RESOURCE_VIEW_DESC view={};view.Format=desc.Format;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=levels;
 auto handle=rt.srv->GetCPUDescriptorHandleForHeapStart();handle.ptr+=out.handle*rt.descriptorSize;rt.device->CreateShaderResourceView(out.resource,&view,handle);return out;
}
static bool Initialize(){
 if(rt.ready)return true;if(rt.failed||!rt.swap||!rt.queue)return false;
 DXGI_SWAP_CHAIN_DESC desc={};if(FAILED(rt.swap->GetDesc(&desc))||desc.BufferCount<1||desc.BufferCount>8)return false;rt.count=desc.BufferCount;rt.window=desc.OutputWindow;
 if(FAILED(rt.queue->GetDevice(IID_PPV_ARGS(&rt.device))))return false;
 rt.event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!rt.event)return false;
 if(FAILED(rt.device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&rt.fence))))return false;
 D3D12_DESCRIPTOR_HEAP_DESC hd={};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=256;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;if(FAILED(rt.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rt.srv))))return false;
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=8;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;if(FAILED(rt.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rt.rtv))))return false;
 rt.descriptorSize=rt.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);rt.rtvSize=rt.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);rt.nextDescriptor=1;
 D3D12_DESCRIPTOR_RANGE range={};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=1;
 D3D12_ROOT_PARAMETER params[2]={};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[0].Constants.Num32BitValues=9;params[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[1].DescriptorTable.NumDescriptorRanges=1;params[1].DescriptorTable.pDescriptorRanges=&range;params[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_STATIC_SAMPLER_DESC sampler={};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=3.402823466e38f;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_ROOT_SIGNATURE_DESC sig={2,params,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
 using SerializeFn=HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*,D3D_ROOT_SIGNATURE_VERSION,ID3DBlob**,ID3DBlob**);
 auto serialize=(SerializeFn)GetProcAddress(GetModuleHandleW(L"d3d12.dll"),"D3D12SerializeRootSignature");ID3DBlob* blob=nullptr;ID3DBlob* error=nullptr;
 if(!serialize||FAILED(serialize(&sig,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error))){Release(error);return false;}
 HRESULT hr=rt.device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&rt.root));Release(blob);Release(error);if(FAILED(hr))return false;
 D3D12_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
 D3D12_GRAPHICS_PIPELINE_STATE_DESC ps={};ps.pRootSignature=rt.root;ps.VS={VS,sizeof(VS)};ps.PS={PS,sizeof(PS)};ps.InputLayout={input,3};ps.SampleMask=0xffffffffU;ps.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;ps.NumRenderTargets=1;ps.RTVFormats[0]=desc.BufferDesc.Format;ps.SampleDesc.Count=1;
 ps.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;ps.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;ps.RasterizerState.DepthClipEnable=TRUE;
 ps.DepthStencilState.DepthEnable=FALSE;ps.DepthStencilState.StencilEnable=FALSE;
 auto& blend=ps.BlendState.RenderTarget[0];blend.BlendEnable=TRUE;blend.SrcBlend=D3D12_BLEND_SRC_ALPHA;blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;blend.BlendOp=D3D12_BLEND_OP_ADD;blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
 if(FAILED(rt.device->CreateGraphicsPipelineState(&ps,IID_PPV_ARGS(&rt.pipeline))))return false;
 for(unsigned i=0;i<rt.count;++i){auto& f=rt.frames[i];if(FAILED(rt.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&f.allocator))))return false;auto hp=Heap(D3D12_HEAP_TYPE_UPLOAD);auto bd=Buffer(sizeof(NativeVertex)*240000);if(FAILED(rt.device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&f.vertices))))return false;D3D12_RANGE empty={};if(FAILED(f.vertices->Map(0,&empty,&f.mapped)))return false;}
 if(FAILED(rt.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,rt.frames[0].allocator,nullptr,IID_PPV_ARGS(&rt.list))))return false;rt.list->Close();
 const DWORD pixel=0xffffffff;rt.white=CreateTextureRGBA(&rt,1,1,&pixel);if(!rt.white)return false;rt.ready=true;Log("Standalone D3D12 renderer initialized on first Tab; no ReShade API used.");return true;
}
static void ReleaseBuffers(){for(auto& f:rt.frames)Release(f.buffer);}
static void Draw(){
 if(!Initialize()){rt.failed=true;Close();Log("Renderer initialization failed; cheats/menu input released.");return;}
 unsigned i=rt.swap->GetCurrentBackBufferIndex();if(i>=rt.count){Close();return;}auto& f=rt.frames[i];if(!Wait(f.fence)){rt.failed=true;Close();return;}
 if(!f.buffer){if(FAILED(rt.swap->GetBuffer(i,IID_PPV_ARGS(&f.buffer)))){Close();return;}auto d=f.buffer->GetDesc();rt.width=(unsigned)d.Width;rt.height=d.Height;auto handle=rt.rtv->GetCPUDescriptorHandleForHeapStart();handle.ptr+=i*rt.rtvSize;rt.device->CreateRenderTargetView(f.buffer,nullptr,handle);}
 if(!NativeUI::NewFrame(rt.window,rt.width,rt.height)){Close();return;}
 if(WarningVisible())CheatMenuUI::DrawAssetWarning(&rt);else CheatMenuUI::Draw(&rt);
 if((!open&&!WarningVisible())||NativeUI::Overflowed()||rt.failed){Close();return;}
 float reveal[3]={};bool revealing=NativeUI::Reveal(reveal);unsigned oldCount=revealing?NativeUI::revealVertexCount:0;
 if(oldCount)memcpy(f.mapped,NativeUI::revealVertices,oldCount*sizeof(NativeVertex));
 memcpy((NativeVertex*)f.mapped+oldCount,NativeUI::vertices,NativeUI::vertexCount*sizeof(NativeVertex));if(FAILED(f.allocator->Reset())||FAILED(rt.list->Reset(f.allocator,rt.pipeline))){Close();return;}
 D3D12_RESOURCE_BARRIER barrier={};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=f.buffer;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;rt.list->ResourceBarrier(1,&barrier);
 auto rtv=rt.rtv->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=i*rt.rtvSize;rt.list->OMSetRenderTargets(1,&rtv,FALSE,nullptr);rt.list->SetGraphicsRootSignature(rt.root);ID3D12DescriptorHeap* heaps[]={rt.srv};rt.list->SetDescriptorHeaps(1,heaps);
 float view[]={2.f/rt.width,-2.f/rt.height,-1.f,1.f};rt.list->SetGraphicsRoot32BitConstants(0,4,view,0);D3D12_VIEWPORT vp={0,0,float(rt.width),float(rt.height),0,1};rt.list->RSSetViewports(1,&vp);rt.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 D3D12_VERTEX_BUFFER_VIEW vb={f.vertices->GetGPUVirtualAddress(),(oldCount+NativeUI::vertexCount)*UINT(sizeof(NativeVertex)),sizeof(NativeVertex)};rt.list->IASetVertexBuffers(0,1,&vb);
 for(unsigned pass=0;pass<2;++pass){
  if(pass==0&&!revealing)continue;
  auto commands=pass?NativeUI::batches:NativeUI::revealBatches;unsigned count=pass?NativeUI::batchCount:NativeUI::revealBatchCount;
  for(unsigned b=0;b<count;++b){const auto& cmd=commands[b];if(cmd.clip.right<=cmd.clip.left||cmd.clip.bottom<=cmd.clip.top)continue;
   rt.list->RSSetScissorRects(1,&cmd.clip);auto gpu=rt.srv->GetGPUDescriptorHandleForHeapStart();gpu.ptr+=(cmd.texture?cmd.texture:rt.white.handle)*rt.descriptorSize;rt.list->SetGraphicsRootDescriptorTable(1,gpu);
   UINT crisp=cmd.texture==2?1:0;rt.list->SetGraphicsRoot32BitConstant(0,crisp,4);
   rt.list->SetGraphicsRoot32BitConstants(0,3,reveal,5);UINT clipped=pass&&revealing&&cmd.first<NativeUI::contentVertexCount?1:0;rt.list->SetGraphicsRoot32BitConstant(0,clipped,8);
   rt.list->DrawInstanced(cmd.count,1,cmd.first+(pass?oldCount:0),0);
  }
 }

 barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;rt.list->ResourceBarrier(1,&barrier);if(FAILED(rt.list->Close())){Close();return;}ID3D12CommandList* lists[]={rt.list};rt.queue->ExecuteCommandLists(1,lists);f.fence=++rt.serial;if(FAILED(rt.queue->Signal(rt.fence,f.fence))){rt.failed=true;Close();}
}
static LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
 if(msg==WM_KILLFOCUS||(msg==WM_ACTIVATEAPP&&!w))Close();
 if(open){if(msg==WM_MOUSEWHEEL){NativeUI::AddWheel((short)HIWORD(w));return 0;}if(msg==WM_SETCURSOR){SetCursor(nullptr);return TRUE;}if(msg==WM_INPUT)return DefWindowProcW(hwnd,msg,w,l);if((msg>=WM_KEYFIRST&&msg<=WM_KEYLAST)||(msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST))return 0;}
 return CallWindowProcW(rt.oldProc,hwnd,msg,w,l);
}
static bool GameplayAvailable(){
 TS3CheatSnapshot snap{};
 return backend&&backend->ReadSnapshot(&snap,sizeof(snap))&&snap.gameReady&&DWORD(GetTickCount()-snap.tick)<750;
}
static void BeforePresent(IDXGISwapChain* swap,UINT flags){
 if(flags&DXGI_PRESENT_TEST)return;
 AcquireSRWLockExclusive(&lock);
 if((void*)swap!=(void*)rt.swap){ReleaseSRWLockExclusive(&lock);return;}
 // Settings > Controls chooses the key/button; while it waits for one, neither opens nor closes the menu.
 const int openKey=CheatMenuUI::OpenKey();const bool capturing=open&&CheatMenuUI::Capturing();
 bool tab=(GetAsyncKeyState(openKey)&0x8000)!=0,esc=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0;
 if(openKey!=oldOpenKey){oldOpenKey=openKey;oldTab=tab;} // a new key acts only after it is released
 // A letter/digit menu key types into the object search instead of closing the menu.
 if(open&&CheatMenuUI::Typing()&&((openKey>='A'&&openKey<='Z')||(openKey>='0'&&openKey<='9')||openKey==VK_OEM_7||openKey==VK_OEM_MINUS||openKey==VK_BACK))tab=oldTab;
 bool padOpen=NativeUI::PollController(Foreground(),open!=0,CheatMenuUI::OpenButton());if(capturing)padOpen=false;
 if(!Foreground()){if(open||openingRequested)Close();}
 else {
  if(!capturing&&((tab&&!oldTab)||padOpen)){if(open||openingRequested||assetWarning)Close();else if(!rt.failed&&backend&&!SupportedBuild())ShowBuildWarning();else if(!rt.failed&&backend){openingTick=GetTickCount();openingRequested=true;backend->SetOpen(2);}}
  if(openingRequested){
   TS3CheatSnapshot snap{};DWORD age=DWORD(GetTickCount()-openingTick);
   if(age>=750)openingRequested=false;
   else if(backend&&backend->ReadSnapshot(&snap,sizeof(snap))&&LONG(snap.tick-openingTick)>=0){
    openingRequested=false;
    if(snap.gameReady){open=true;NativeUI::EndReveal();RECT client={};GetClientRect(rt.window,&client);POINT start={(client.right-client.left)/2,(client.bottom-client.top)/2};if(ClientToScreen(rt.window,&start))SetCursorPos(start.x,start.y);NativeUI::ClearInput();backend->SetOpen(1);CheatMenuUI::Opened(&rt);}
   }
  }
  if(!capturing&&(open||openingRequested||assetWarning)&&esc&&!oldEscape)Close();
 }
 oldTab=tab;oldEscape=esc;if(open||WarningVisible()){if(open&&backend)backend->SetOpen(1);if(insideEffects)DrainOptionalEffects();Draw();}
 ReleaseSRWLockExclusive(&lock);
}
static void AfterEffects(){lastEffects=GetTickCount();insideEffects=true;BeforePresent(rt.swap,0);insideEffects=false;}
static HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* s,UINT interval,UINT flags){bool outer=InterlockedIncrement(&presenting)==1;if(outer&&(!optionalEffects||DWORD(GetTickCount()-lastEffects)>250))BeforePresent(s,flags);HRESULT hr=originalPresent(s,interval,flags);InterlockedDecrement(&presenting);return hr;}
static HRESULT STDMETHODCALLTYPE Present1(IDXGISwapChain1* s,UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* p){bool outer=InterlockedIncrement(&presenting)==1;if(outer&&(!optionalEffects||DWORD(GetTickCount()-lastEffects)>250))BeforePresent(s,flags);HRESULT hr=originalPresent1(s,interval,flags,p);InterlockedDecrement(&presenting);return hr;}
static void ResetGraphics(){
 NativeUI::EndReveal();
 if(!rt.device)return;
 CheatMenuUI::Reset(&rt);DestroyTexture(&rt,rt.white);ReleaseBuffers();
 for(auto& f:rt.frames){if(f.vertices&&f.mapped)f.vertices->Unmap(0,nullptr);Release(f.vertices);Release(f.allocator);f={};}
 Release(rt.list);Release(rt.pipeline);Release(rt.root);Release(rt.srv);Release(rt.rtv);Release(rt.fence);Release(rt.device);if(rt.event)CloseHandle(rt.event);rt.event=nullptr;rt.ready=false;rt.serial=0;
}
static HRESULT STDMETHODCALLTYPE Resize(IDXGISwapChain* s,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags){
 AcquireSRWLockExclusive(&lock);if((void*)s==(void*)rt.swap){Close();if(!Flush()){rt.failed=true;ReleaseSRWLockExclusive(&lock);return DXGI_ERROR_DEVICE_RESET;}ResetGraphics();}ReleaseSRWLockExclusive(&lock);
 HRESULT hr=originalResize(s,count,width,height,format,flags);return hr;
}
static HRESULT STDMETHODCALLTYPE Resize1(IDXGISwapChain3* s,UINT count,UINT width,UINT height,DXGI_FORMAT format,UINT flags,const UINT* masks,IUnknown* const* queues){
 AcquireSRWLockExclusive(&lock);if(s==rt.swap){Close();if(!Flush()){rt.failed=true;ReleaseSRWLockExclusive(&lock);return DXGI_ERROR_DEVICE_RESET;}ResetGraphics();}ReleaseSRWLockExclusive(&lock);
 return originalResize1(s,count,width,height,format,flags,masks,queues);
}
static bool Hook(void** table,unsigned slot,void* target,void** original){if(table[slot]==target)return true;if(*original&&*original!=table[slot])return false;DWORD old;if(!VirtualProtect(table+slot,sizeof(void*),PAGE_READWRITE,&old))return false;*original=table[slot];InterlockedExchangePointer(table+slot,target);DWORD unused;VirtualProtect(table+slot,sizeof(void*),old,&unused);return true;}
static void AttachSwap(IUnknown* device,IDXGISwapChain* swap){
 ID3D12CommandQueue* queue=nullptr;IDXGISwapChain3* chain=nullptr;if(FAILED(device->QueryInterface(IID_PPV_ARGS(&queue))))return;
 if(FAILED(swap->QueryInterface(IID_PPV_ARGS(&chain)))){Release(queue);return;}DXGI_SWAP_CHAIN_DESC desc={};chain->GetDesc(&desc);if(!desc.OutputWindow||queue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT){Release(chain);Release(queue);return;}
 if(rt.swap){
  if(rt.swap==chain||(rt.window!=desc.OutputWindow&&IsWindow(rt.window))){Release(chain);Release(queue);return;}
  AcquireSRWLockExclusive(&lock);Close();if(!Flush()){rt.failed=true;ReleaseSRWLockExclusive(&lock);Release(chain);Release(queue);return;}ResetGraphics();
  if(IsWindow(rt.window)&&(WNDPROC)GetWindowLongPtrW(rt.window,GWLP_WNDPROC)==WindowProc)SetWindowLongPtrW(rt.window,GWLP_WNDPROC,(LONG_PTR)rt.oldProc);
  Release(rt.swap);Release(rt.queue);rt.failed=false;ReleaseSRWLockExclusive(&lock);
 }

 rt.swap=chain;rt.queue=queue;rt.window=desc.OutputWindow;rt.oldProc=(WNDPROC)SetWindowLongPtrW(rt.window,GWLP_WNDPROC,(LONG_PTR)WindowProc);
 void** vt=*(void***)chain;bool ok=Hook(vt,8,(void*)Present,(void**)&originalPresent)&&Hook(vt,13,(void*)Resize,(void**)&originalResize)&&Hook(vt,22,(void*)Present1,(void**)&originalPresent1)&&Hook(vt,39,(void*)Resize1,(void**)&originalResize1);
 if(ok&&!optionalEffects){optionalEffects=InstallOptionalEffects(module,rt.window,AfterEffects);if(optionalEffects)Log("Optional existing ReShade detected: drawing after effects. ReShade is not required.");}
 if(!ok){rt.failed=true;Log("Swap-chain hooks unavailable; menu disabled.");}else Log("Game swap chain attached; Tab ready. GPU resources remain unallocated until opened.");
}
static HRESULT STDMETHODCALLTYPE Create(IDXGIFactory* f,IUnknown* d,DXGI_SWAP_CHAIN_DESC* desc,IDXGISwapChain** out){HRESULT hr=originalCreate(f,d,desc,out);if(SUCCEEDED(hr)&&out&&*out)AttachSwap(d,*out);return hr;}
static HRESULT STDMETHODCALLTYPE CreateHwnd(IDXGIFactory2* f,IUnknown* d,HWND h,const DXGI_SWAP_CHAIN_DESC1* desc,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full,IDXGIOutput* restrictOutput,IDXGISwapChain1** out){HRESULT hr=originalCreateHwnd(f,d,h,desc,full,restrictOutput,out);if(SUCCEEDED(hr)&&out&&*out)AttachSwap(d,*out);return hr;}
}
extern "C" __declspec(dllexport) BOOL WINAPI AttachCheatFactory(IUnknown* object,const TS3CheatMenuApi* api){
 using namespace OverlayHost;if(!object||!api||api->version!=TS3MenuApiVersion)return FALSE;backend=api;IDXGIFactory2* factory=nullptr;if(FAILED(object->QueryInterface(IID_PPV_ARGS(&factory))))return FALSE;void** vt=*(void***)factory;
 bool ok=Hook(vt,10,(void*)Create,(void**)&originalCreate)&&Hook(vt,15,(void*)CreateHwnd,(void**)&originalCreateHwnd);factory->Release();Log(ok?"Native menu factory hooks installed.":"Native menu factory hooks failed.");return ok;
}
BOOL WINAPI DllMain(HMODULE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(h);OverlayHost::module=h;}return TRUE;}
