"""Stage the compiled online module and official data without launching it.

This produces a development module, not an integrated Cooperation release.
Private per-device TLS seeds are provisioned at installation, never archived.
"""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

root=Path(__file__).resolve().parent
repo=root.parent
stage=repo/'build/wii-online/module/online'
client=repo/'build/wii-online/client.dol'
if not client.is_file():
    raise SystemExit('Compile the online client first.')
(stage/'nzp').mkdir(parents=True,exist_ok=True)
files={
    'client.dol':client,
    'default.fmf':root/'default.fmf',
    'cacert.pem':root/'data/cacert.pem',
    'nzp/game.pk3':root/'data/game.pk3',
    'nzp/progs.pk3':root/'data/progs.pk3',
    'nzp/wii-online.cfg':root/'wii-online.cfg',
    'LICENSE-FTE.txt':root/'fteqw-master/LICENSE',
    'LICENSE-mbedTLS.txt':root/'dependencies/mbedtls-3.6.7/LICENSE',
}
receipt={'status':'development-not-runtime-validated','runtime_tested':False,
         'integrated_menu':False,'fte_revision':'f68a2b547d2dc4bf6886b922baa1bff487cc5038',
         'tls_library':'mbedtls-3.6.7','files':{}}
for name,source in files.items():
    target=stage/name
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source,target)
    receipt['files'][name]=hashlib.sha256(target.read_bytes()).hexdigest()
(stage/'development-receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
destination=repo/'NZPortable-Wii-online-module-development.zip'
source_archive=repo/'NZPortable-Wii-online-source.zip'
with zipfile.ZipFile(source_archive,'w',zipfile.ZIP_DEFLATED) as source_zip:
    engine=root/'fteqw-master'
    for source in engine.rglob('*'):
        if source.is_file() and not set(source.relative_to(engine).parts)&{'release','debug','profile','.git'}:
            source_zip.write(source,Path('fte-online')/source.relative_to(root))
    for name in ('build_wii_online.sh','sdl2-wii-config.sh','build_tls.sh','wii-toolchain.cmake',
                 'mbedtls_wii_config.h','provision_tls_seed.py','prepare_online_module.py',
                 'default.fmf','wii-online.cfg','INTEGRATION.md'):
        source_zip.write(root/name,'fte-online/'+name)
    source_zip.write(root/'dependencies/mbedtls-3.6.7.tar.bz2','fte-online/dependencies/mbedtls-3.6.7.tar.bz2')
    source_zip.write(repo/'tools/pad_dol.py','tools/pad_dol.py')
with zipfile.ZipFile(destination.with_suffix('.zip.tmp'),'w',zipfile.ZIP_DEFLATED) as archive:
    for name in files:
        archive.write(stage/name,'apps/nzportable/online/'+name)
    archive.write(stage/'development-receipt.json','online-development-receipt.json')
    archive.write(root/'INTEGRATION.md','ONLINE-DEVELOPMENT.md')
    archive.write(root/'provision_tls_seed.py','provision_tls_seed.py')
    archive.write(source_archive,'NZPortable-Wii-online-source.zip')
destination.with_suffix('.zip.tmp').replace(destination)
print('Development online module staged; menu handoff and hardware validation are pending.')
print(destination)
