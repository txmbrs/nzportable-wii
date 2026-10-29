from pathlib import Path
import zipfile, subprocess, shutil
root=Path('.')
log=Path('build-wii.log').read_text()
if '[100%] Built target boot' not in log: raise SystemExit('Build is not complete; package not created')
dist=Path('dist')
shutil.copyfile('WII_PORT.md',dist/'README-WII.md')
shutil.copyfile('LICENSE',dist/'LICENSE-engine.txt')
files=subprocess.check_output(['git','ls-files'],text=True).splitlines()
files += ['CMakeLists.txt','build_wii.sh','WII_PORT.md','platform/wii/meta.xml','source/platform/wii/in_wii.c','source/platform/wii/boot_wii.c','source/platform/wii/boot_wii.h','tools/pad_dol.py','make_package.py','package_startup_update.py']
files += ['tools/build_smooth_font.py','tools/fonts/LiberationSans-Regular.ttf','tools/fonts/FONT-LICENSE.txt']
files += ['tools/build_hitmarker.py','dist/apps/nzportable/nzp/gfx/hud/hit_marker_smooth.tga']
files += ['tools/inspect_bsp_costs.py']
files += ['source/platform/wii/chainloader_wii.c','source/platform/wii/chainloader_wii.h','source/platform/wii/chainstub_bytes.h','tools/chainstub.c','tools/chainstub.S','tools/build_chainstub.sh']
files += ['dist/apps/nzportable/nzp/gfx/'+name for name in ('charset_smooth.tga','kerning_smooth.txt','FONT-LICENSE.txt')]
with zipfile.ZipFile(dist/'NZPortable-Wii-source.zip','w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n in sorted(set(files)):
  if Path(n).is_file(): z.write(n,n)
with zipfile.ZipFile('NZPortable-Wii.zip','w',zipfile.ZIP_DEFLATED,compresslevel=1) as z:
 for p in dist.rglob('*'):
  if p.is_file(): z.write(p,p.relative_to(dist))
print('Package:',Path('NZPortable-Wii.zip').stat().st_size,'bytes')
print('DOL:',(dist/'apps/nzportable/boot.dol').stat().st_size,'bytes')
