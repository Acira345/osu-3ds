import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

print("--- Step 1: Checking Array Counts in Localization ---")
import subprocess
res = subprocess.run([sys.executable, 'tools/check_array_count.py'], capture_output=True, text=True)
print(res.stdout)
if res.returncode != 0:
    print(res.stderr)
    sys.exit(1)

print("--- Step 2: Full Source Code Scan for Remaining Spanish Literals ---")
exclude_dirs = {'.git', 'tools', 'build'}
spanish_regex = re.compile(r'"([^"\\]*(?:[áéíóúñÁÉÍÓÚÑ¿¡]|Actualiz|Descarg|Volver|Partida|Cancion|Puntuac|Precisi|Sincroniz|Esperand|Conectand|Biblioteca|Modificad|Cargand)[^"\\]*)"')

found_issues = 0
for root, dirs, files in os.walk('source'):
    dirs[:] = [d for d in dirs if d not in exclude_dirs]
    for file in files:
        if file.endswith(('.cpp', '.hpp')) and file != 'Localization.cpp' and file != 'Localization.hpp':
            filepath = os.path.join(root, file)
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
            for line_no, line in enumerate(lines, 1):
                clean = line.strip()
                if clean.startswith('//') or clean.startswith('/*'):
                    continue
                # Ignorar si ya está dentro de LOC(...) o usa ternario esp ?
                if 'LOC(' in clean or 'isEsp()' in clean or 'esp ?' in clean or 'LANG_ES' in clean:
                    continue
                matches = spanish_regex.findall(clean)
                for m in matches:
                    # Excluir paths y strings ya localizados
                    if not m.startswith('/') and not m.startswith('sdmc:') and not m.endswith(('.png', '.wav', '.ogg', '.osz', '.osk')):
                        print(f"[{file}:{line_no}] {m}")
                        print(f"    Line: {clean}")
                        found_issues += 1

print(f"\nAudit finished. Total hardcoded Spanish issues found outside localization: {found_issues}")
