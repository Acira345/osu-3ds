import sys
import re
sys.stdout.reconfigure(encoding='utf-8')

with open('source/backend/i18n/Localization.hpp', 'r', encoding='utf-8') as f:
    text = f.read()

enum_block = re.search(r'enum StringId \{(.*?)\s*STR_COUNT', text, re.DOTALL).group(1)
enum_block = re.sub(r'//.*', '', enum_block)
enum_block = re.sub(r'/\*.*?\*/', '', enum_block, flags=re.DOTALL)
enums = [x.strip() for x in enum_block.split(',') if x.strip()]

print(f"Total StringIds in enum: {len(enums)}")

with open('source/backend/i18n/Localization.cpp', 'r', encoding='utf-8') as f:
    cpp_text = f.read()

# Let's extract the STRINGS array
# Look for outer array
m = re.search(r'const char\* const STRINGS\[LANG_COUNT\]\[STR_COUNT\]\s*=\s*\{\s*(.*)\n\};', cpp_text, re.DOTALL)
if not m:
    print("Could not find STRINGS array!")
    sys.exit(1)

body = m.group(1)

# Split into languages: there are 4 languages enclosed in { ... }
# Let's find top-level blocks { ... }
lang_blocks = []
depth = 0
cur = []
for char in body:
    if char == '{':
        if depth == 0:
            cur = []
        else:
            cur.append(char)
        depth += 1
    elif char == '}':
        depth -= 1
        if depth == 0:
            lang_blocks.append("".join(cur))
            cur = []
        else:
            cur.append(char)
    else:
        if depth > 0:
            cur.append(char)

print(f"Parsed {len(lang_blocks)} language blocks")

langs = ['ES', 'EN', 'RU', 'PL', 'DE']
for idx, lblock in enumerate(lang_blocks):
    # extract string literals in lblock
    # handle escaped quotes
    tokens = re.findall(r'"((?:[^"\\]|\\.)*)"', lblock)
    print(f"Lang {langs[idx]}: {len(tokens)} string elements")
    if len(tokens) != len(enums):
        print(f"  --> WARNING: enum count is {len(enums)} but strings count is {len(tokens)}!")

