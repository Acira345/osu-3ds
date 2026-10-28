import json
import subprocess
import sys

def main():
    # 1. Checkout clean Localization.hpp and Localization.cpp
    subprocess.run(['git', 'checkout', 'HEAD', '--', 'source/backend/i18n/Localization.cpp', 'source/backend/i18n/Localization.hpp'], check=True)

    # 2. Apply Step 1-3 edits
    with open('tools/step1_2_3_edits.json', 'r', encoding='utf-8') as f:
        edits = json.load(f)

    for e in edits:
        path = e['path']
        target = e['target']
        repl = e['replacement']
        with open(path, 'r', encoding='utf-8') as f:
            c = f.read()
        if target not in c:
            print(f"ERROR: Target not found for step {e['step']} in {path}")
            sys.exit(1)
        c = c.replace(target, repl, 1)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(c)
        print(f"Applied step {e['step']} to {path}")

    # Now let's update Localization.hpp with Steps 4, 5, 6 enums
    path_transcript = r'C:\Users\Diyel\.gemini\antigravity\brain\55e64ff4-6890-4f61-a422-f1e27127c03f\.system_generated\logs\transcript_full.jsonl'
    hpp_edits = []
    with open(path_transcript, 'r', encoding='utf-8') as f:
        for line in f:
            data = json.loads(line)
            idx = data.get('step_index', 0)
            if idx in [421, 513, 553]:
                for tc in data.get('tool_calls', []):
                    args = tc.get('args', {})
                    hpp_edits.append({
                        'step': idx,
                        'path': args.get('Path'),
                        'target': args.get('Target'),
                        'replacement': args.get('Replacement')
                    })
    
    bad_ids = {'STR_MULTI_ROOM_FULL', 'STR_MULTI_ALL_READY', 'STR_MULTI_WAITING_PLAYERS', 'STR_MULTI_WAITING_HOST'}

    for e in hpp_edits:
        path = 'source/backend/i18n/Localization.hpp'
        target = e['target']
        repl = e['replacement']
        # Remove bad_ids from replacement
        repl_lines = []
        for rl in repl.splitlines(keepends=True):
            if any(bid in rl for bid in bad_ids):
                continue
            repl_lines.append(rl)
        repl = ''.join(repl_lines)

        with open(path, 'r', encoding='utf-8') as f:
            c = f.read()
        if target not in c:
            print(f"ERROR: HPP Target not found for step {e['step']}")
            sys.exit(1)
        c = c.replace(target, repl, 1)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(c)
        print(f"Applied HPP step {e['step']}")

    # Filter bad_ids from add_multiplayer_strings.py
    with open('tools/add_multiplayer_strings.py', 'r', encoding='utf-8') as f:
        ams = f.read()
    ams_lines = []
    for l in ams.splitlines():
        if any(f"'{bid}'" in l or f'"{bid}"' in l for bid in bad_ids):
            continue
        ams_lines.append(l)
    with open('tools/add_multiplayer_strings.py', 'w', encoding='utf-8') as f:
        f.write('\n'.join(ams_lines) + '\n')

    # 3. Run add_multiplayer_strings.py
    print("Running add_multiplayer_strings.py...")
    res = subprocess.run([sys.executable, 'tools/add_multiplayer_strings.py'], capture_output=True, text=True)
    print(res.stdout, res.stderr)
    if res.returncode != 0:
        sys.exit(1)

    # 4. Run add_step5_strings.py
    print("Running add_step5_strings.py...")
    res = subprocess.run([sys.executable, 'tools/add_step5_strings.py'], capture_output=True, text=True)
    print(res.stdout, res.stderr)
    if res.returncode != 0:
        sys.exit(1)

    # 5. Run add_step6_strings.py
    print("Running add_step6_strings.py...")
    res = subprocess.run([sys.executable, 'tools/add_step6_strings.py'], capture_output=True, text=True)
    print(res.stdout, res.stderr)
    if res.returncode != 0:
        sys.exit(1)

    # 6. Run check_array_count.py
    print("Running check_array_count.py...")
    res = subprocess.run([sys.executable, 'tools/check_array_count.py'], capture_output=True, text=True)
    print(res.stdout)
    if res.returncode != 0:
        sys.exit(1)

if __name__ == '__main__':
    main()
