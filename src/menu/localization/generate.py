from pathlib import Path
import json
root=Path(__file__).resolve().parent
rows=[line.split('|') for line in (root/'translations.tsv').read_text(encoding='utf-8').splitlines() if line]
assert all(len(row)==5 and all(row) for row in rows)
assert len({row[0] for row in rows})==len(rows)
quote=lambda s:'u8'+json.dumps(s,ensure_ascii=True)
text='''#pragma once
// Menu translations. Supported languages verified against Steam app 4049180.
namespace Localization {
inline int language=0;
inline constexpr int count=5;
inline const WCHAR* codes[]={L"en",L"es",L"fr",L"de",L"it"};
inline const char* names[]={u8"English",u8"Espa\\u00f1ol",u8"Fran\\u00e7ais",u8"Deutsch",u8"Italiano"};
inline const char* entries[][5]={
'''
text+='\n'.join(' {'+','.join(map(quote,row))+'},' for row in rows)
text+='''
};
inline const char* Get(const char* source){
 if(language<=0||language>=count||!source)return source;
 for(const auto& row:entries)if(lstrcmpA(source,row[0])==0)return row[language];
 return source;
}
inline unsigned Next(const unsigned char*& p){
 unsigned c=*p++;if(c<128)return c;
 unsigned remaining=c>=0xf0?3:c>=0xe0?2:c>=0xc0?1:0;
 if(!remaining)return '?';c&=(1u<<(6-remaining))-1;
 for(unsigned i=0;i<remaining;++i){if((*p&0xc0)!=0x80)return '?';c=(c<<6)|(*p++&63);}
 return c;
}
}
'''
(root.parent/'Localization.generated.h').write_text(text,encoding='utf-8')
print(f'{len(rows)} strings x 5 languages generated.')
