// Optional compatibility only. No DLL is loaded and no ReShade import is linked.
// ReShade 6.8 API 20's present event runs after effects and final state barriers:
// https://github.com/crosire/reshade/blob/v6.8.0/source/runtime.cpp#L854-L857
// Flush its pending list before our independent D3D12 draw, so FXAA/colour effects
// do not blur or recolour the menu. Without ReShade, the native Present hook runs.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../vendor/reshade_api.hpp"
static HWND targetWindow;static void(*notify)();static reshade::api::effect_runtime* activeRuntime;
static void AfterEffects(reshade::api::effect_runtime* runtime){
 if((HWND)runtime->get_hwnd()!=targetWindow)return;
 activeRuntime=runtime;notify();activeRuntime=nullptr;
}
bool InstallOptionalEffects(HMODULE module,HWND window,void(*callback)()){
 WCHAR path[MAX_PATH];DWORD n=GetModuleFileNameW(module,path,MAX_PATH);if(!n||n>=MAX_PATH-16)return false;while(n&&path[n-1]!=L'\\')--n;
 // ReShade may use any supported proxy name; dxgi can instead belong to BTB.
 // Inspect already-loaded local modules only. Never load a graphics proxy here.
 const WCHAR* names[]={L"dxgi.dll",L"d3d12.dll",L"d3d11.dll",L"d3d10.dll",L"d3d9.dll",L"opengl32.dll"};
 HMODULE existing=nullptr;
 for(const auto name:names){path[n]=0;lstrcatW(path,name);HMODULE candidate=GetModuleHandleW(path);
  if(candidate&&GetProcAddress(candidate,"ReShadeRegisterAddon")&&GetProcAddress(candidate,"ReShadeRegisterEventForAddon")){existing=candidate;break;}}
 if(!existing)return false;
 auto add=(bool(*)(void*,uint32_t))GetProcAddress(existing,"ReShadeRegisterAddon");
 auto event=(void(*)(void*,uint32_t,void*))GetProcAddress(existing,"ReShadeRegisterEventForAddon");
 if(!add||!event||!add(module,20))return false;
 targetWindow=window;notify=callback;event(module,75,(void*)AfterEffects);return true;
}
void DrainOptionalEffects(){if(activeRuntime)activeRuntime->get_command_queue()->flush_immediate_command_list();}
