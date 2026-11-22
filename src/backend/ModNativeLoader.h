// Capture the game's own DXGI factory without replacing graphics DLLs.
// Install the import hook before the game's startup; load the renderer only
// inside the normal factory call, outside DllMain's loader lock.
namespace CheatNativeLoader {
typedef HRESULT(WINAPI* FactoryFn)(UINT,REFIID,void**);
static FactoryFn original;
static HRESULT WINAPI Factory(UINT flags,REFIID iid,void** result){
 HRESULT hr=original(flags,iid,result);if(FAILED(hr)||!result||!*result)return hr;
 WCHAR path[MAX_PATH];DWORD n=GetModuleFileNameW(0,path,MAX_PATH);if(!n||n>=MAX_PATH-32)return hr;
 while(n&&path[n-1]!=L'\\')--n;path[n]=0;lstrcatW(path,L"TS3ModMenu.dll");
 HMODULE renderer=LoadLibraryW(path);if(!renderer)return hr;
 typedef BOOL(WINAPI* AttachFn)(void*,const TS3CheatMenuApi*);
 AttachFn attach=(AttachFn)GetProcAddress(renderer,"AttachCheatFactory");if(attach)attach(*result,GetCheatMenuApi(TS3MenuApiVersion));return hr;
}
static void Install(){
 BYTE* base=(BYTE*)GetModuleHandleW(0);auto dos=(IMAGE_DOS_HEADER*)base;auto nt=(IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);
 auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!directory.VirtualAddress)return;
 auto imports=(IMAGE_IMPORT_DESCRIPTOR*)(base+directory.VirtualAddress);
 for(;imports->Name;++imports){if(lstrcmpiA((char*)base+imports->Name,"dxgi.dll")||!imports->OriginalFirstThunk)continue;
  auto names=(IMAGE_THUNK_DATA64*)(base+imports->OriginalFirstThunk);auto values=(IMAGE_THUNK_DATA64*)(base+imports->FirstThunk);
  for(;names->u1.AddressOfData;++names,++values){if(IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))continue;auto name=(IMAGE_IMPORT_BY_NAME*)(base+names->u1.AddressOfData);if(lstrcmpA((char*)name->Name,"CreateDXGIFactory2"))continue;
   DWORD old;if(!VirtualProtect(&values->u1.Function,sizeof(void*),PAGE_READWRITE,&old))return;original=(FactoryFn)values->u1.Function;InterlockedExchangePointer((void**)&values->u1.Function,(void*)Factory);DWORD unused;VirtualProtect(&values->u1.Function,sizeof(void*),old,&unused);return;
  }
 }
}
}
