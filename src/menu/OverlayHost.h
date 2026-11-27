#pragma once
// What the menu UI (ModMenuUI.cpp) and the Direct3D 12 host (NativeHost.cpp) need from each other.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "NativeUI.h"
#include "ModMenuApi.h"
namespace OverlayHost {struct Runtime;}
namespace CheatMenuUI {void Draw(OverlayHost::Runtime*);void DrawAssetWarning(OverlayHost::Runtime*);void Opened(OverlayHost::Runtime*);void Closed(OverlayHost::Runtime*);void Reset(OverlayHost::Runtime*);int OpenKey();WORD OpenButton();bool Capturing();bool Typing();void OpenControlName(bool pad,char* out,int size);void Preload(OverlayHost::Runtime*);}
namespace OverlayHost {
 struct Texture {ID3D12Resource* resource=nullptr;uint64_t handle=0;uint32_t width=0,height=0;explicit operator bool()const{return handle!=0;}ImTextureRef ImGuiRef()const{return handle;}};
 bool IsOpen();void Close(const char* reason=nullptr);bool GameBuildMatches();void ShowAssetWarning();void ShowBuildWarning();bool BuildWarning();const TS3CheatMenuApi* Backend();HMODULE Module();
 Texture CreateTextureRGBA(Runtime*,uint32_t,uint32_t,const void*,bool mipmaps=false);void WaitForTextures(Runtime*);void BeginTextureBatch(Runtime*);bool EndTextureBatch(Runtime*);void DestroyTexture(Runtime*,Texture&);
}
