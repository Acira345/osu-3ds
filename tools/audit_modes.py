import sys
import re

sys.stdout.reconfigure(encoding='utf-8')

files = [
    'source/game/modes/taiko/TaikoRenderer.cpp',
    'source/game/modes/catch/CatchRenderer.cpp',
    'source/game/modes/mania/ManiaRenderer.cpp',
    'source/game/modes/taiko/ModeTaiko.cpp',
    'source/game/modes/catch/ModeCatch.cpp',
    'source/game/modes/mania/ModeMania.cpp',
]

for path in files:
    try:
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
        print(f"=== {path} ===")
        for i, line in enumerate(lines, 1):
            if '//' in line.strip()[:2]:
                continue
            matches = re.findall(r'"([^"\\]+)"', line)
            for m in matches:
                # filtrar rutas o extensiones
                if not any(m.endswith(ext) for ext in ['.png', '.wav', '.ogg', '.osu', '.mp3']) and not m.startswith('/') and len(m) > 2:
                    print(f"  L{i}: {m}")
    except Exception as e:
        print(f"Error {path}: {e}")
