import sys
import re

sys.stdout.reconfigure(encoding='utf-8')

step6_es = [
    ('STR_RESULTS_NEW_RECORD_TITLE', '¡NUEVO RÉCORD PERSONAL!'),
    ('STR_RESULTS_MAP_COMPLETED', 'CANCIÓN COMPLETADA'),
    ('STR_DIRECT_INVALID_ID', 'ID de Beatmap inválido'),
    ('STR_REPLAY_NO_DATA', 'La partida no contiene datos de repetición.'),
    ('STR_WIFI_STATUS_WAIT_TCP', 'Esperando socket TCP... (err: %d)'),
    ('STR_WIFI_STATUS_WAIT_CLIENT', 'Esperando conexión desde PC o Celular...'),
    ('STR_WIFI_STATUS_CONNECTING_SRV', 'Conectando al servidor Wi-Fi...'),
]

step6_en = [
    ('STR_RESULTS_NEW_RECORD_TITLE', 'NEW PERSONAL RECORD!'),
    ('STR_RESULTS_MAP_COMPLETED', 'BEATMAP COMPLETED'),
    ('STR_DIRECT_INVALID_ID', 'Invalid Beatmap ID'),
    ('STR_REPLAY_NO_DATA', 'The match does not contain replay data.'),
    ('STR_WIFI_STATUS_WAIT_TCP', 'Waiting for TCP socket... (err: %d)'),
    ('STR_WIFI_STATUS_WAIT_CLIENT', 'Waiting for connection from PC or Phone...'),
    ('STR_WIFI_STATUS_CONNECTING_SRV', 'Connecting to Wi-Fi server...'),
]

step6_ru = [
    ('STR_RESULTS_NEW_RECORD_TITLE', 'НОВЫЙ ЛИЧНЫЙ РЕКОРД!'),
    ('STR_RESULTS_MAP_COMPLETED', 'КАРТА ПРОЙДЕНА'),
    ('STR_DIRECT_INVALID_ID', 'Неверный ID карты'),
    ('STR_REPLAY_NO_DATA', 'Запись повтора не содержит данных.'),
    ('STR_WIFI_STATUS_WAIT_TCP', 'Ожидание TCP сокета... (err: %d)'),
    ('STR_WIFI_STATUS_WAIT_CLIENT', 'Ожидание подключения с ПК или телефона...'),
    ('STR_WIFI_STATUS_CONNECTING_SRV', 'Подключение к серверу Wi-Fi...'),
]

step6_pl = [
    ('STR_RESULTS_NEW_RECORD_TITLE', 'NOWY REKORD OSOBISTY!'),
    ('STR_RESULTS_MAP_COMPLETED', 'MAPA UKOŃCZONA'),
    ('STR_DIRECT_INVALID_ID', 'Nieprawidłowy ID mapy'),
    ('STR_REPLAY_NO_DATA', 'Mecz nie zawiera danych powtórki.'),
    ('STR_WIFI_STATUS_WAIT_TCP', 'Oczekiwanie na gniazdo TCP... (err: %d)'),
    ('STR_WIFI_STATUS_WAIT_CLIENT', 'Oczekiwanie na połączenie z PC lub telefonu...'),
    ('STR_WIFI_STATUS_CONNECTING_SRV', 'Łączenie z serwerem Wi-Fi...'),
]

def format_lines(str_list):
    lines = []
    lines.append('        // Step 6 additions: Final Audit Fixes')
    for ident, val in str_list:
        escaped = val.replace('\\', '\\\\').replace('"', '\\"')
        lines.append(f'        /* {ident} */{" " * max(1, 32 - len(ident))}"{escaped}",')
    return '\n'.join(lines)

with open('source/backend/i18n/Localization.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

target = '/* STR_MANIA_PLUS_SPEED */'
matches = [m.start() for m in re.finditer(re.escape(target), content)]
if len(matches) != 4:
    print(f"Error: expected 4 occurrences of {target}, found {len(matches)}")
    sys.exit(1)

lang_data = [step6_es, step6_en, step6_ru, step6_pl]

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

print("Step 6 strings added successfully to all 4 languages!")
