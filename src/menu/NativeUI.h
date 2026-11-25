#pragma once
#include <windows.h>
#include <stdint.h>
// Small immediate-mode input/drawing adapter for the existing hand-drawn menu.
// No third-party UI runtime is used.
using ImU32=uint32_t;using ImTextureID=uint64_t;using ImTextureRef=uint64_t;
struct ImVec2{float x,y;ImVec2(float a=0,float b=0):x(a),y(b){}};
#define IM_COL32(r,g,b,a) ((ImU32)(r)|((ImU32)(g)<<8)|((ImU32)(b)<<16)|((ImU32)(a)<<24))
enum{ImGuiMouseCursor_None=-1,ImGuiMouseCursor_Arrow,ImGuiMouseCursor_Hand,ImGuiMouseCursor_ResizeAll};
enum{ImGuiCond_Always=0,ImGuiStyleVar_WindowPadding,ImGuiStyleVar_WindowBorderSize};
enum{ImGuiWindowFlags_NoCollapse=1,ImGuiWindowFlags_NoTitleBar=2,ImGuiWindowFlags_NoResize=4,ImGuiWindowFlags_NoMove=8,ImGuiWindowFlags_NoSavedSettings=16,ImGuiWindowFlags_NoBackground=32,ImGuiWindowFlags_NoScrollbar=64,ImGuiWindowFlags_NoScrollWithMouse=128,ImGuiWindowFlags_NoNav=256};
struct NativeVertex{float x,y,u,v;ImU32 color;};
struct NativeBatch{uint64_t texture;RECT clip;unsigned first,count;};
struct ImDrawList{
 void AddTriangleFilled(ImVec2,ImVec2,ImVec2,ImU32);
 void AddImage(ImTextureRef,ImVec2,ImVec2,ImVec2=ImVec2(0,0),ImVec2=ImVec2(1,1),ImU32=0xffffffff);
 void AddRectFilled(ImVec2,ImVec2,ImU32,float=0,int=0);
 void AddRect(ImVec2,ImVec2,ImU32,float=0,int=0,float=1);
 void AddLine(ImVec2,ImVec2,ImU32,float=1);
 void AddCircleFilled(ImVec2,float,ImU32,int=0);
 void AddEllipseFilled(ImVec2,ImVec2,ImU32,float=0,int=0);
 void AddRectFilledMultiColor(ImVec2,ImVec2,ImU32,ImU32,ImU32,ImU32);
 void PushClipRect(ImVec2,ImVec2,bool);void PopClipRect();
};
namespace NativeUI{
 struct IO{ImVec2 DisplaySize,MousePos;bool MouseDrawCursor;float MouseWheel;};
 extern NativeVertex* vertices;extern NativeBatch* batches;extern unsigned vertexCount,batchCount;
 void AddWheel(short delta);
 // openButton: XInput button bit that opens the menu (0x400/0x800 = LT/RT pressed past half).
 bool PollController(bool foreground,bool menuOpen,WORD openButton=0x0001);void PadCapture(bool);WORD PadPressed();void NavScope(unsigned);bool BackPressed();bool ControllerActive();
 void NavRegister(const char*,ImVec2,ImVec2,bool);bool NavSelected(const char*);int NavAdjust(const char*);void NavFinish();
 void NavLink(const char*,int,const char*);float ControllerScroll();
 void NavRedirect(const char*,const char*);void NavHome(const char*);
#ifdef UI_SELF_TEST
 void TestHover(ImVec2);
#endif
 bool NewFrame(HWND,unsigned,unsigned);void ClearInput();void Shutdown();bool Overflowed();
 extern NativeVertex* revealVertices;extern NativeBatch* revealBatches;
 extern unsigned revealVertexCount,revealBatchCount;
 extern unsigned contentVertexCount;void MarkContentEnd();
 void BeginReveal(ImVec2,ImVec2,ImVec2);void EndReveal();bool Reveal(float*);
 IO& GetIO();ImDrawList* GetWindowDrawList();ImDrawList* GetForegroundDrawList();
 void SetCursorScreenPos(ImVec2);bool InvisibleButton(const char*,ImVec2);
 bool IsItemHovered();bool IsItemActive();bool IsItemActivated();bool IsMouseDown(int);bool IsMouseClicked(int);
 void SetMouseCursor(int);int GetMouseCursor();
 inline void SetNextWindowPos(ImVec2,int=0){}inline void SetNextWindowSize(ImVec2,int=0){}
 inline void PushStyleVar(int,ImVec2){}inline void PushStyleVar(int,float){}inline void PopStyleVar(int){}
 inline bool Begin(const char*,void*,int){return true;}inline void End(){}
 void TextUnformatted(const char*);
}
namespace ImGui=NativeUI;
