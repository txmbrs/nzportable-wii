"""Build the r18 delta package. Compile/static inspection only; never runs a game."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import zipfile

repo=Path(__file__).resolve().parent.parent
online=repo/'fte-online'
stage=repo/'build/cooperation-r18/package'
app=stage/'apps/nzportable'
app.mkdir(parents=True,exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
manifest={'revision':'cooperation-r18','runtime_tested':False,'integrated_menu':True,
          'fte_revision':'f68a2b547d2dc4bf6886b922baa1bff487cc5038',
          'shared':[],'files':[]}
if '[100%] Built target boot' not in (repo/'build-wii.log').read_text(errors='replace'):
    raise SystemExit('Solo build is incomplete.')
if 'client.dol:' not in (repo/'build-wii-online.log').read_text(errors='replace'):
    raise SystemExit('FTE build is incomplete.')
delta=app/'online/compat.pk3'
delta.parent.mkdir(parents=True,exist_ok=True)
changed=0
with zipfile.ZipFile(online/'data/game.pk3') as original, zipfile.ZipFile(delta,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as target:
    for entry in original.infolist():
        if entry.is_dir(): continue
        data=original.read(entry)
        current=repo/'dist/apps/nzportable/nzp'/entry.filename
        if current.is_file() and current.read_bytes()==data:
            manifest['shared'].append({'path':entry.filename,'sha256':sha(data)})
        else:
            target.writestr(entry.filename,data)
            changed+=1

copies={
    'boot.dol':repo/'dist/apps/nzportable/boot.dol',
    'meta.xml':repo/'platform/wii/meta.xml',
    'online/client.dol':repo/'build/wii-online/client.dol',
    'online/default.fmf':online/'default.fmf',
    'online/wii-online.cfg':online/'wii-online.cfg',
    'online/progs.pk3':online/'data/progs.pk3',
    'online/cacert.pem':online/'data/cacert.pem',
    'online/LICENSE-FTE.txt':online/'fteqw-master/LICENSE',
    'online/LICENSE-mbedTLS.txt':online/'dependencies/mbedtls-3.6.7/LICENSE',
}
for name,source in copies.items():
    destination=app/name
    destination.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,destination)
for name in [*copies,'online/compat.pk3']:
    manifest['files'].append({'path':name,'sha256':sha((app/name).read_bytes())})
(stage/'cooperation-r18.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
for name in ('Install-Cooperation.ps1','Install-Cooperation.cmd'):
    shutil.copyfile(online/name,stage/name)
readme='''NZPortable Wii Cooperation r18 — aktualizacja obecnej paczki r17

1. Rozpakuj ZIP na komputerze i podlacz karte SD / USB z obecna gra.
2. Uruchom Install-Cooperation.cmd. W razie pytania podaj dysk, np. E:\\.
3. Uruchom NZPortable na Wii i wybierz COOPERATIVE.

Instalator sprawdza i wykorzystuje obecne dane nzp. Nie kopiuje ich ponownie.
Dodaje tylko klienta FTE, jego programy i 101 brakujacych/rozniacych sie plikow
w online/compat.pk3. Nie nadpisuje konfiguracji Solo ani oryginalnego progs.dat.
Zachowuje boot-before-cooperation.dol jako kopie poprzedniej aplikacji.
Generuje prywatny klucz startowy TLS tylko na Twojej karcie; nie udostepniaj tls-seed.bin.

COOPERATIVE otwiera przegladarke serwerow FTE z protokolami NZP-REBOOT-WEB
i NZP-REBOOT oraz brokerem frag-net. Wyjscie z klienta wraca do Solo; HOME
konczy aplikacje. Logi: wii-startup.txt i online/wii-online-startup.txt.

To kompilacja eksperymentalna. Kompilacja i kontrola plikow sa wykonane,
ale uruchomienie, obraz, sterowanie i polaczenie z graczami nzp.gay nie zostaly
zweryfikowane na konsoli. Nie jest to potwierdzenie dzialajacej rozgrywki online.
Zrodla wraz z kodem ladowania RAM sa w NZPortable-Wii-cooperation-r18-source.zip.
'''
(stage/'CZYTAJ.txt').write_text(readme,encoding='utf-8-sig')

# Complete corresponding sources, kept separate from the SD update payload.
source_archive=repo/'NZPortable-Wii-cooperation-r18-source.zip'
tracked=subprocess.check_output(['git','ls-files'],cwd=repo,text=True).splitlines()
extra=['CMakeLists.txt','build_wii.sh','make_package.py','WII_PORT.md','platform/wii/meta.xml']
extra += [str(p.relative_to(repo)) for p in (repo/'source/platform/wii').glob('*') if p.is_file()]
extra += [str(p.relative_to(repo)) for p in (repo/'tools').glob('*') if p.is_file()]
with zipfile.ZipFile(source_archive,'w',zipfile.ZIP_DEFLATED) as source_zip:
    for name in sorted(set(tracked+extra)):
        path=repo/name
        if path.is_file() and path.suffix.lower() not in {'.zip','.dol','.elf','.bin'}:
            source_zip.write(path,Path(name).as_posix())
    for path in (online/'fteqw-master').rglob('*'):
        if path.is_file() and not set(path.relative_to(online/'fteqw-master').parts)&{'release','debug','profile','.git'}:
            source_zip.write(path,path.relative_to(repo).as_posix())
    for path in online.iterdir():
        if path.is_file(): source_zip.write(path,path.relative_to(repo).as_posix())
    source_zip.write(online/'dependencies/mbedtls-3.6.7.tar.bz2','fte-online/dependencies/mbedtls-3.6.7.tar.bz2')

destination=repo/'NZPortable-Wii-cooperation-r18-update.zip'
with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED,compresslevel=1) as archive:
    for file in manifest['files']:
        path=app/file['path']
        archive.write(path,path.relative_to(stage).as_posix())
    for name in ('cooperation-r18.json','CZYTAJ.txt','Install-Cooperation.ps1','Install-Cooperation.cmd'):
        archive.write(stage/name,name)
receipt={'package':destination.name,'bytes':destination.stat().st_size,
         'sha256':sha(destination.read_bytes()),'reused_files':len(manifest['shared']),
         'delta_files':changed,'runtime_tested':False,
         'boot_sha256':sha((app/'boot.dol').read_bytes()),
         'client_sha256':sha((app/'online/client.dol').read_bytes())}
(repo/'cooperation-r18-release.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
print(json.dumps(receipt,indent=2))
