import os
import re
import sys

# Ensure UTF-8 output
sys.stdout.reconfigure(encoding='utf-8')

files_to_check = []
for root, dirs, files in os.walk('source'):
    for file in files:
        if file.endswith('.cpp') or file.endswith('.hpp'):
            if 'Localization.' not in file and 'third_party' not in root:
                files_to_check.append(os.path.join(root, file))

print(f"Checking {len(files_to_check)} files for hardcoded text/ui strings...")

ignore_exact = {
    "", " ", "  ", "%s", "%d", "%i", "%u", "%lu", "%02d", "%02d:%02d", "%.1f", "%.2f", "%s / %s",
    "osu!", "osu!3DS", "3DS", "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "START", "SELECT",
    "OK", "300", "100", "50", "0", "SS", "S", "A", "B", "C", "D", "x", "X", "/", ":", "-",
    "FPS: %d", "romfs:/", "sdmc:/", "v", "...", "N/A", "default", "skin.ini", ".osk", ".osu"
}

found_by_file = {}

draw_keywords = [
    "drawText", "drawCentered", "drawKeyBadge", "drawButton", "drawTab", 
    "drawHeader", "drawFooter", "drawCard", "drawModal", "snprintf", "sprintf"
]

for fpath in files_to_check:
    with open(fpath, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()
    
    file_matches = []
    for idx, line in enumerate(lines):
        line_s = line.strip()
        if line_s.startswith("//") or line_s.startswith("/*"):
            continue
        
        # Check if line contains any draw keyword
        if any(k in line for k in draw_keywords):
            matches = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', line)
            for m in matches:
                if m in ignore_exact or m.startswith("sdmc:") or m.startswith("romfs:") or m.startswith("%") or m.startswith("/") or m.startswith("\\"):
                    continue
                if len(m) <= 1:
                    continue
                # If the string is already a LOC or just formatting
                if not re.search(r'[a-zA-ZáéíóúñÁÉÍÓÚÑ]', m):
                    continue
                # Check if it looks like English/Spanish sentence or label
                file_matches.append((idx + 1, m, line_s))

    if file_matches:
        # Deduplicate per line
        dedup = []
        seen = set()
        for lnum, txt, ls in file_matches:
            key = (lnum, txt)
            if key not in seen:
                seen.add(key)
                dedup.append((lnum, txt, ls))
        found_by_file[fpath] = dedup

for fpath, matches in sorted(found_by_file.items()):
    print(f"\n--- {fpath} ({len(matches)} potential UI strings) ---")
    for lnum, text, lcode in matches[:20]:
        print(f"  L{lnum}: \"{text}\"")
    if len(matches) > 20:
        print(f"  ... and {len(matches) - 20} more.")
