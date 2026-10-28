import sys
import re

sys.stdout.reconfigure(encoding='utf-8')

step5_es = [
    ('STR_TAIKO_CONTROLS_HINT', 'CONTROLES TAIKO: ROJO = DON (Centro) | AZUL = KATSU (Borde)'),
    ('STR_CATCH_RADAR_DECK', 'RADAR DE FRUTAS // CATCH DECK'),
    ('STR_CATCH_TOUCH_OR_DRAG', 'TOCA O DESLIZA EL LÁPIZ DIRECTAMENTE'),
    ('STR_CATCH_LEFT_ARROW', '< IZQ'),
    ('STR_CATCH_RIGHT_ARROW', 'DER >'),
    ('STR_CATCH_HOLD_DASH', 'SOSTENER L / R / B'),
    ('STR_MANIA_BTN_Y', 'BOTÓN Y'),
    ('STR_MANIA_MINUS_SPEED', '[-] VEL'),
    ('STR_MANIA_SPEED_FMT', 'VEL: %.1fx'),
    ('STR_MANIA_PLUS_SPEED', 'VEL [+]'),
]

step5_en = [
    ('STR_TAIKO_CONTROLS_HINT', 'TAIKO CONTROLS: RED = DON (Center) | BLUE = KATSU (Rim)'),
    ('STR_CATCH_RADAR_DECK', 'FRUIT RADAR // CATCH DECK'),
    ('STR_CATCH_TOUCH_OR_DRAG', 'TAP OR DRAG STYLUS DIRECTLY'),
    ('STR_CATCH_LEFT_ARROW', '< LEFT'),
    ('STR_CATCH_RIGHT_ARROW', 'RIGHT >'),
    ('STR_CATCH_HOLD_DASH', 'HOLD L / R / B'),
    ('STR_MANIA_BTN_Y', 'BUTTON Y'),
    ('STR_MANIA_MINUS_SPEED', '[-] SPD'),
    ('STR_MANIA_SPEED_FMT', 'SPD: %.1fx'),
    ('STR_MANIA_PLUS_SPEED', 'SPD [+]'),
]

step5_ru = [
    ('STR_TAIKO_CONTROLS_HINT', 'УПРАВЛЕНИЕ TAIKO: КРАСНЫЙ = DON (Центр) | СИНИЙ = KATSU (Край)'),
    ('STR_CATCH_RADAR_DECK', 'РАДАР ФРУКТОВ // CATCH DECK'),
    ('STR_CATCH_TOUCH_OR_DRAG', 'НАЖМИТЕ ИЛИ ВЕДИТЕ СТИЛУСОМ'),
    ('STR_CATCH_LEFT_ARROW', '< ВЛЕВО'),
    ('STR_CATCH_RIGHT_ARROW', 'ВПРАВО >'),
    ('STR_CATCH_HOLD_DASH', 'УДЕРЖИВАЙТЕ L / R / B'),
    ('STR_MANIA_BTN_Y', 'КНОПКА Y'),
    ('STR_MANIA_MINUS_SPEED', '[-] СКОР'),
    ('STR_MANIA_SPEED_FMT', 'СКОР: %.1fx'),
    ('STR_MANIA_PLUS_SPEED', 'СКОР [+]'),
]

step5_pl = [
    ('STR_TAIKO_CONTROLS_HINT', 'STEROWANIE TAIKO: CZERWONY = DON (Środek) | NIEBIESKI = KATSU (Brzeg)'),
    ('STR_CATCH_RADAR_DECK', 'RADAR OWOCÓW // CATCH DECK'),
    ('STR_CATCH_TOUCH_OR_DRAG', 'DOTKNIJ LUB PRZECIĄGNIJ RYSIK'),
    ('STR_CATCH_LEFT_ARROW', '< LEWO'),
    ('STR_CATCH_RIGHT_ARROW', 'PRAWO >'),
    ('STR_CATCH_HOLD_DASH', 'PRZYTRZYMAJ L / R / B'),
    ('STR_MANIA_BTN_Y', 'PRZYCISK Y'),
    ('STR_MANIA_MINUS_SPEED', '[-] PRĘD'),
    ('STR_MANIA_SPEED_FMT', 'PRĘD: %.1fx'),
    ('STR_MANIA_PLUS_SPEED', 'PRĘD [+]'),
]

def format_lines(str_list):
    lines = []
    lines.append('        // Step 5 additions: Special Game Modes')
    for ident, val in str_list:
        escaped = val.replace('\\', '\\\\').replace('"', '\\"')
        lines.append(f'        /* {ident} */{" " * max(1, 32 - len(ident))}"{escaped}",')
    return '\n'.join(lines)

with open('source/backend/i18n/Localization.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

target = '/* STR_MULTI_BTN_FINISHED_WAIT */'
matches = [m.start() for m in re.finditer(re.escape(target), content)]
if len(matches) != 4:
    print(f"Error: expected 4 occurrences of {target}, found {len(matches)}")
    sys.exit(1)

lang_data = [step5_es, step5_en, step5_ru, step5_pl]

for i in reversed(range(4)):
    pos = matches[i]
    line_end = content.find('\n', pos)
    prev_line = content[pos:line_end].rstrip()
    if not prev_line.endswith(','):
        prev_line += ','
    formatted = format_lines(lang_data[i])
    content = content[:pos] + prev_line + '\n' + formatted + content[line_end:]

with open('source/backend/i18n/Localization.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print("Step 5 strings added successfully to all 4 languages!")
