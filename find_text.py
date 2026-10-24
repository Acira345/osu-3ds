import re

with open("source/main.cpp", "r", encoding="utf-8") as f:
    lines = f.readlines()

calls = ["drawText", "drawCenteredText", "drawCenteredTextWithShadow", "drawTextWithShadow", "drawKeyBadge"]
ignore = {"osu!", "osu!3DS", "3DS", "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "SELECT", "START", "^ ", "< >", "OK", "300", "100", "50", "SS", "S"}

found = []
for idx, line in enumerate(lines):
    if line.strip().startswith("//"):
        continue
    for c in calls:
        if c in line:
            matches = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', line)
            for m in matches:
                if len(m) > 1 and not m.startswith("%") and not m.startswith("sdmc:") and not m.startswith("romfs:") and m not in ignore:
                    found.append((idx + 1, m))

for lnum, text in found:
    print(f"L{lnum}: {text}")
