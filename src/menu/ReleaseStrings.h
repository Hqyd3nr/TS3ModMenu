#pragma once
#include "ReleaseSeed.generated.h"
// Obfuscation, not confidentiality: keys necessarily ship with the program.
// Only immediate-consumption calls may use the temporary returned by the macro.
namespace ReleaseStrings {
constexpr unsigned Mix(unsigned x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
template<unsigned N,unsigned Site> struct Encoded {
 unsigned char bytes[N];
 static constexpr unsigned Key=Mix(TS3_STRING_SEED^Site^(N*0x9e3779b9u));
 constexpr Encoded(const char (&s)[N]):bytes{} {
  for(unsigned i=0;i<N;++i)bytes[i]=(unsigned char)s[i]^(unsigned char)Mix(Key+i*0x9e3779b9u);
 }
};
template<unsigned N,unsigned Site> struct Decoded {
 char bytes[N];
 __declspec(noinline) explicit Decoded(const Encoded<N,Site>& source){
  const volatile unsigned char* p=source.bytes;
  volatile unsigned key=Encoded<N,Site>::Key;
  for(unsigned i=0;i<N;++i)bytes[i]=(char)(p[i]^(unsigned char)Mix(key+i*0x9e3779b9u));
 }
 __declspec(noinline) ~Decoded(){volatile char* p=bytes;for(unsigned i=0;i<N;++i)p[i]=0;}
 const char* get() const{return bytes;}
};
}
#define TS3_PRIVATE_TEXT_IMPL(s,site) ([](){static constexpr ReleaseStrings::Encoded<sizeof(s),site> data(s);return ReleaseStrings::Decoded<sizeof(s),site>(data);}().get())
#define TS3_PRIVATE_TEXT(s) TS3_PRIVATE_TEXT_IMPL(s,__COUNTER__+1)
