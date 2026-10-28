import sys
import re
sys.stdout.reconfigure(encoding='utf-8')

with open('source/backend/i18n/Localization.hpp', 'r', encoding='utf-8') as f:
    text = f.read()

enum_block = re.search(r'enum StringId \{(.*?)\s*STR_COUNT', text, re.DOTALL).group(1)
enum_block = re.sub(r'//.*', '', enum_block)
enum_block = re.sub(r'/\*.*?\*/', '', enum_block, flags=re.DOTALL)
enums = [x.strip() for x in enum_block.split(',') if x.strip()]

with open('source/backend/i18n/Localization.cpp', 'r', encoding='utf-8') as f:
    cpp_text = f.read()

m = re.search(r'const char\* const STRINGS\[LANG_COUNT\]\[STR_COUNT\]\s*=\s*\{\s*(.*)\n\};', cpp_text, re.DOTALL)
body = m.group(1)

lang_blocks = []
depth = 0
cur = []
for char in body:
    if char == '{':
        if depth == 0: cur = []
        else: cur.append(char)
        depth += 1
    elif char == '}':
        depth -= 1
        if depth == 0:
            lang_blocks.append("".join(cur))
            cur = []
        else: cur.append(char)
    else:
        if depth > 0: cur.append(char)

es_tokens = re.findall(r'"((?:[^"\\]|\\.)*)"', lang_blocks[0])
en_tokens = re.findall(r'"((?:[^"\\]|\\.)*)"', lang_blocks[1])

print(f"Comparing {len(enums)} items between ES and EN:\n")

# Detect Spanish-like words in EN or exact identicals
spanish_indicators = [
    ' de ', ' en ', ' el ', ' la ', ' los ', ' las ', ' para ', ' por ', ' con ', ' sin ',
    'cancion', 'pantalla', 'ajust', 'boton', 'puls', 'toca', 'juego', 'tamano', 'musica',
    'modo', 'volver', 'guardar', 'listo', 'reiniciar', 'borrar', 'limpiar', 'continuar',
    'reintentar', 'dificultad', 'estrellas', 'objetos', 'resumen', 'activad', 'desactivad',
    'elegir', 'mover', 'configuracion', 'salir', 'teclas', 'sonidos', 'idioma', 'fondos',
    'velocidad', 'partida', 'esperando', 'jugador', 'sala', 'cola', 'unirse', 'crear'
]

suspicious_en = []
for i in range(len(enums)):
    e = enums[i]
    es = es_tokens[i]
    en = en_tokens[i]
    
    # Check if identical and longer than 4 chars
    is_same = (es == en) and len(es) > 4 and not es.startswith('%')
    # Check if EN contains typical Spanish words
    has_spanish = any(w in en.lower() for w in spanish_indicators)
    
    if is_same or has_spanish:
        suspicious_en.append((i, e, es, en, "SAME" if is_same else "SPANISH_WORD"))

print(f"Total suspicious or unchanged entries in EN: {len(suspicious_en)}")
for idx, enum_name, es_val, en_val, reason in suspicious_en:
    print(f"[{idx:03d}] {enum_name} ({reason}):")
    print(f"     ES: \"{es_val}\"")
    print(f"     EN: \"{en_val}\"")

