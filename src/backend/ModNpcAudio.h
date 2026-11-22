// Native .fev resource loader: bank_name_replacement.txt maps this key to
// the bakery event bank and sample bank, then registers native event names.
namespace NpcAudio {
static const char* BakeryBank(){return "st_hauntedbakery.fev";}
static bool Loaded(void* driver,const char* name){
 if(!driver)return false;
 unsigned count=At<unsigned>(driver,0x4e0);void** banks=At<void**>(driver,0x4d8);
 if(count>4096||(!banks&&count))return false;
 for(unsigned i=0;i<count;++i){void* bank=banks[i];if(!bank)continue;
  bool terminated=false;for(unsigned j=0;j<32;++j)if(!At<char>(bank,j)){terminated=true;break;}
  if(terminated&&lstrcmpiA((const char*)bank,name)==0&&At<void*>(bank,0x20)&&At<void*>(bank,0x28))return true;
 }
 return false;
}
typedef bool(*LoadFn)(void*,const char*,bool,bool,bool);
static bool EnsureWith(void* driver,LoadFn load,const char* bank=BakeryBank()){
 if(!driver||!At<BYTE>(driver,0x601)||!load)return false;
 if(Loaded(driver,bank))return true;
 // Non-persistent: normal world audio teardown owns the lifetime.
 if(!load(driver,bank,false,true,true))return false;
 return Loaded(driver,bank);
}
static bool Ensure(const char* bank=BakeryBank()){
 const BYTE prefix[]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20};
 for(unsigned i=0;i<sizeof(prefix);++i)if(*(BYTE*)A(0x26db40+i)!=prefix[i])return false;
 void* driver=*(void**)A(0xac2ec0);
 // ac2ec0 is the SoundDriver manager, not a SoundDriverWin32 instance.
 // Its first member is an InteractiveMusic object. Checking that member as
 // a vtable rejects the native loader even when audio is fully initialized.
 if(!driver)return false;
 return EnsureWith(driver,(LoadFn)A(0x26db40),bank);
}
static const char* CharacterBank(unsigned variant){
 if(variant==8||variant==9||variant==11||variant==14||variant==16)return "st_daycare.fev";
 if(variant==13)return "sparks.fev";
 if(variant==17)return "st_bonnie_woody.fev";
 if(variant==12)return "paratroopers.fev";
 return "wt_characters.fev";
}
}
