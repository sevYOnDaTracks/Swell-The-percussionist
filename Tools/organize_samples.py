"""Moves the files of the sample folder into one folder per tab, named like the pads of the plugin.

Usage: python3 -I Tools/organize_samples.py <sample folder> [--undo]

Reads docs/classement.csv (written by pack_samples.py). Pads go to <Tab>/<pad name>.wav, loops to Loops/,
duplicates (named in the report, or exact copies of a kept file) to Doublons/, every other file to Autres/.
Nothing is deleted: each move is written to _journal.csv in the folder, and --undo puts every file back.
"""
import sys, os, csv, shutil

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from analyze_samples import read_wav  # noqa: E402

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FOLDERS = {'Djembe': 'Djembé', 'Metal': 'Métal'}  # tab id -> folder name, when they differ


def unique(path):
    base, ext = os.path.splitext(path)
    n = 2
    while os.path.exists(path):
        path = f'{base} ({n}){ext}'
        n += 1
    return path


def move(root, source, dest, journal):
    src, dst = os.path.join(root, source), unique(os.path.join(root, dest))
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.move(src, dst)
    journal.append((source, os.path.relpath(dst, root)))


def undo(root):
    path = os.path.join(root, '_journal.csv')
    rows = list(csv.reader(open(path, encoding='utf-8'), delimiter=';'))[1:]
    for source, dest in reversed(rows):
        os.makedirs(os.path.dirname(os.path.join(root, source)) or root, exist_ok=True)
        shutil.move(os.path.join(root, dest), os.path.join(root, source))
    os.remove(path)
    remove_empty(root)
    print('restored', len(rows))


def remove_empty(root):
    for dirpath, dirs, files in os.walk(root, topdown=False):
        if dirpath != root and not os.listdir(dirpath):
            os.rmdir(dirpath)


def main():
    root = sys.argv[1]
    if '--undo' in sys.argv:
        return undo(root)
    if os.path.exists(os.path.join(root, '_journal.csv')):
        sys.exit('Already organised (_journal.csv exists). Run with --undo first.')

    rows = list(csv.reader(open(os.path.join(HERE, 'docs', 'classement.csv'), encoding='utf-8'), delimiter=';'))[1:]
    planned = {r[0]: r for r in rows}
    kept_hash = {}
    for path, tab, name, *_ in rows:
        if tab:
            kept_hash[read_wav(os.path.join(root, path))[4]] = path  # hash of the audio, not of the whole file

    journal = []
    for dirpath, _, files in os.walk(root):
        for f in sorted(files):
            source = os.path.relpath(os.path.join(dirpath, f), root)
            r = planned.get(source)
            if r and r[1]:
                move(root, source, os.path.join(FOLDERS.get(r[1], r[1]), r[2] + '.wav'), journal)
            elif r and r[4] == 'loop':
                move(root, source, os.path.join('Loops', f), journal)
            elif r and r[4] == 'doublon':
                move(root, source, os.path.join('Doublons', f), journal)
            elif f.lower().endswith('.wav') and read_wav(os.path.join(root, source))[4] in kept_hash:
                move(root, source, os.path.join('Doublons', f), journal)  # an exact copy of a kept file
            elif f.lower().endswith('.wav'):
                move(root, source, os.path.join('Autres', source), journal)  # not in the report: left for review
            elif f != '.DS_Store':
                move(root, source, os.path.join('Autres', source), journal)

    with open(os.path.join(root, '_journal.csv'), 'w', newline='', encoding='utf-8') as out:
        csv.writer(out, delimiter=';').writerows([('avant', 'apres')] + journal)
    for dirpath, _, files in os.walk(root):
        for f in files:
            if f == '.DS_Store':
                os.remove(os.path.join(dirpath, f))
    remove_empty(root)
    print('moved', len(journal))


main()
