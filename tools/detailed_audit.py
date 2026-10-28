import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

screens = [
    "source/frontend/screens/ModsSelectScreen.cpp",
    "source/frontend/screens/SongSelectScreen.cpp",
    "source/frontend/screens/GameplayScreen.cpp",
    "source/frontend/screens/ResultsScreen.cpp",
    "source/frontend/screens/SettingsScreen.cpp",
    "source/frontend/screens/WifiTransferScreen.cpp",
    "source/frontend/screens/MainMenuScreen.cpp",
    "source/frontend/screens/MultiBrowserScreen.cpp",
    "source/frontend/screens/MultiLobbyScreen.cpp",
    "source/frontend/screens/MultiSyncScreen.cpp",
    "source/frontend/screens/MultiResultsScreen.cpp",
    "source/game/modes/taiko/TaikoRenderer.cpp",
    "source/game/modes/catch/CatchRenderer.cpp",
    "source/game/modes/mania/ManiaRenderer.cpp",
]

ignore = {
    "", " ", "  ", "%s", "%d", "%i", "%u", "%lu", "%02d", "%02d:%02d", "%.1f", "%.2f", "%s / %s",
    "osu!", "osu!3DS", "3DS", "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "START", "SELECT",
    "OK", "300", "100", "50", "0", "SS", "S", "A", "B", "C", "D", "x", "X", "/", ":", "-",
    "FPS: %d", "romfs:/", "sdmc:/", "v", "...", "N/A", "default", "skin.ini", ".osk", ".osu",
    "C2D", "RGBA", "RGB", "%+.0f ms", "%.0f%%"
}

summary = {}

for s in screens:
    if not os.path.exists(s):
        continue
    with open(s, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()
    
    found = []
    for idx, line in enumerate(lines):
        line_s = line.strip()
        if line_s.startswith("//") or line_s.startswith("/*"):
            continue
        
        matches = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', line)
        for m in matches:
            if m in ignore or m.startswith("%") or m.startswith("/") or m.startswith("sdmc:") or m.startswith("romfs:"):
                continue
            if len(m) <= 1:
                continue
            if not re.search(r'[a-zA-ZáéíóúñÁÉÍÓÚÑ]', m):
                continue
            # Look for lines that look like UI draws or text formatting
            if any(k in line for k in ["draw", "Ui::", "UiKit::", "snprintf", "sprintf", "status", "msg", "title"]):
                # check if line contains LOC
                if f'LOC({m})' in line:
                    continue
                found.append((idx + 1, m, line_s))
    
    # deduplicate
    dedup = []
    seen = set()
    for lnum, txt, ls in found:
        if txt not in seen:
            seen.add(txt)
            dedup.append((lnum, txt))
    summary[s] = dedup

for s, items in summary.items():
    print(f"\n=== {s} ({len(items)} strings) ===")
    for lnum, txt in items:
        print(f"  [L{lnum}] \"{txt}\"")
