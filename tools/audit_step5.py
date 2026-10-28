import sys
import re

sys.stdout.reconfigure(encoding='utf-8')

files = [
    'source/frontend/renderers/TaikoRenderer.cpp',
    'source/frontend/renderers/CatchRenderer.cpp',
    'source/frontend/renderers/ManiaRenderer.cpp',
    'source/frontend/screens/SettingsScreen.cpp',
]

spanish_pattern = re.compile(r'"([^"\\]*(?:[áéíóúñÁÉÍÓÚÑ¿¡]|Configur|Ajustes|Modo|Puntuac|Precisi|Velocidad|Sensib|Desactiv|Activad|Controles|Volver|Guardar)[^"\\]*)"')

for path in files:
    try:
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
        print(f"=== {path} ===")
        for i, line in enumerate(lines, 1):
            if 'LOC(' in line or '//' in line.strip()[:2]:
                continue
            matches = spanish_pattern.findall(line)
            if matches:
                print(f"  L{i}: {line.strip()}")
    except Exception as e:
        print(f"Error {path}: {e}")
