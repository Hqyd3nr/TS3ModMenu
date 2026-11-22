from pathlib import Path
import json,hashlib,pefile
out=Path('outputs/CompleteEdition/ModMenu');game=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Toy Story 3 Complete Edition')
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name in ['dinput8.dll','TS3CheatMenu.addon64']:
 p=pefile.PE(str(out/name));assert p.FILE_HEADER.Machine==0x8664
 exports=[s.name.decode() for s in p.DIRECTORY_ENTRY_EXPORT.symbols if s.name]
 print(name,hex(p.OPTIONAL_HEADER.SizeOfImage),exports)
 if name=='dinput8.dll':assert 'GetCheatMenuApi' in exports and 'DirectInput8Create' in exports
 else:assert 'NAME' in exports and 'DESCRIPTION' in exports
files={str(p.relative_to(out)):h(p) for p in out.rglob('*') if p.is_file() and p.name not in ['build-report.json','installation.json'] and p.suffix.lower() in ['.rgba','.dll','.addon64','.txt','.h','.json']}
base={'status':'built and checked; not installed or game-tested','files':files,'before_dll':h(game/'dinput8.dll'),'before_exe':h(game/'Toy Story 3 Complete Edition.exe'),'backend_checks':34,'input_checks':48,'game_launched':False,'gpu_test_run':False,'preserved_settings':{p.name:h(p) for p in game.iterdir() if p.is_file() and p.name not in ['build-report.json','installation.json'] and p.suffix.lower()=='.ini'}}
(out/'build-report.json').write_text(json.dumps(base,indent=2))

