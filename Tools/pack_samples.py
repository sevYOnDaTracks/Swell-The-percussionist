"""Builds the plugin's sample pack from the sorted list.

Usage: python3 -I Tools/pack_samples.py <classified.json> <source folder>
(run Tools/analyze_samples.py, then Tools/classify_samples.py first)

Reads the classification (one entry per unique sound, with its category), trims leading and trailing silence,
normalises each sound to -1 dBFS, converts it to 16-bit FLAC in assets/samples/pack/, and writes
Source/SampleIndex.h (the list the plugin embeds) and docs/classement.csv (what went where, for review).
"""
import sys, os, re, json, csv, subprocess, struct, wave, tempfile, shutil
import numpy as np

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from analyze_samples import read_wav  # noqa: E402

# Order of the tabs, and which sorted group goes into each.
TABS = [
    ('Djembe', ['Djembe']), ('Congas', ['Congas']), ('Talking', ['Talking']), ('Percs', ['Percs']),
    ('Toms', ['Toms']), ('Rims', ['Rims']), ('Shakers', ['Shakers']), ('Claps', ['Claps']),
    ('Metal', ['Metal', 'Bois']), ('FX', ['FX']),
]
SOUND_TO_TAB = {'Rims': 'Rims', 'Shakers': 'Shakers', 'Toms': 'Toms', 'Claps': 'Claps'}  # anything else heard -> Percs
MAX_SECONDS = 8.0


def tab_of(r):
    if r['how'] in ('nom', 'dossier'):
        return next(t for t, groups in TABS if r['cat'] in groups)
    return SOUND_TO_TAB.get(r['cat'], 'Percs')


# What each pad is called: the kind of sound (from the file name when it says so, otherwise the tab), in this order on the pads.
KINDS = {
    'Djembe': [(None, 'Djemb\u00e9')],
    'Congas': [(r'bongo', 'Bongo'), (None, 'Conga')],
    'Talking': [(None, 'Talking')],
    'Percs': [(None, 'Perc')],
    'Toms': [(r'kick|808|\bbd\b', 'Kick'), (r'bombo', 'Bombo'), (None, 'Tom')],
    'Rims': [(None, 'Rim')],
    'Shakers': [(r'tamb', 'Tambourin'), (r'\bhh|hat|^h\d|\bcl\d|open|close', 'Hi-Hat'), (None, 'Shaker')],
    'Claps': [(r'snap', 'Snap'), (r'snare', 'Snare'), (None, 'Clap')],
    'Metal': [(r'cowbell|cow bell', 'Cowbell'), (r'triang|\btri\b', 'Triangle'), (r'gong', 'Gong'), (r'clave', 'Clave'),
              (r'wood|block|blok', 'Wood Block'), (r'ting|bell|cloche', 'Cloche'), (None, 'M\u00e9tal')],
    'FX': [(r'siffl|siflet|whistle', 'Sifflet'), (r'vox|voix|naija|uh oh|mamphoza', 'Voix'), (r'impact|transition|hit|drop', 'Impact'), (None, 'FX')],
}


def kind(tab, path):
    name = os.path.basename(path).lower()
    return next(i for i, (pattern, _) in enumerate(KINDS[tab]) if pattern is None or re.search(pattern, name))


def pad_names(tab, items):
    """Groups the sounds of a tab by kind, low to high inside each, and names them 'Conga Grave 3'.
    The register (Grave, Médium, Aigu) is only added when a kind has enough sounds for it to help."""
    groups = [[] for _ in KINDS[tab]]
    for r in items:
        groups[kind(tab, r['path'])].append(r)
    named = []
    for k, group in enumerate(groups):
        label = KINDS[tab][k][1]
        group.sort(key=lambda r: r.get('pitch', 0) * .3 + r['centroid'] * .7)
        counts = {}
        for i, r in enumerate(group):
            register = ''
            if len(group) >= 6:
                register = ['Grave', 'M\u00e9dium', 'Aigu'][min(2, i * 3 // len(group))]
            key = register
            counts[key] = counts.get(key, 0) + 1
            total = sum(1 for j in range(len(group)) if len(group) < 6 or ['Grave', 'M\u00e9dium', 'Aigu'][min(2, j * 3 // len(group))] == register)
            number = '' if total == 1 else f' {counts[key]}'
            named.append((r, ' '.join(x for x in [label, register] if x) + number))
    return named


def near_duplicate(a, b):
    na = re.sub(r'(_2|_o)$', '', os.path.splitext(os.path.basename(a['path']))[0].lower())
    nb = re.sub(r'(_2|_o)$', '', os.path.splitext(os.path.basename(b['path']))[0].lower())
    return na == nb and abs(a['dur'] - b['dur']) < .01 * max(a['dur'], b['dur']) + .005 \
        and abs(a['centroid'] - b['centroid']) < .05 * max(a['centroid'], b['centroid'])


def prepare(x, sr):
    mono = np.abs(x).max(axis=1)
    peak = mono.max()
    above = np.where(mono > peak * 10 ** (-50 / 20))[0]
    start = max(0, above[0] - int(sr * .002))
    hop = int(sr * .005)
    env = np.array([mono[i:i + hop].max() for i in range(0, len(mono), hop)])
    alive = np.where(env > peak * 10 ** (-60 / 20))[0]
    end = min(len(mono), (alive[-1] + 1) * hop + int(sr * .03))
    end = min(end, start + int(sr * MAX_SECONDS))
    y = x[start:end].copy()
    fade = min(len(y), int(sr * .01))
    y[-fade:] *= np.linspace(1, 0, fade)[:, None]
    y *= 10 ** (-1 / 20) / max(1e-9, np.abs(y).max())
    return y


def write_flac(y, sr, dest):
    # afconvert writes an empty FLAC for anything shorter than one 4608-frame packet: pad very short sounds with silence.
    if len(y) < 4608:
        y = np.vstack([y, np.zeros((4608 - len(y), y.shape[1]))])
    dither = (np.random.random(y.shape) - np.random.random(y.shape)) / 32768
    pcm = np.clip(np.round((y + dither) * 32767), -32768, 32767).astype('<i2')
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
        name = tmp.name
    with wave.open(name, 'wb') as w:
        w.setnchannels(y.shape[1]); w.setsampwidth(2); w.setframerate(sr); w.writeframes(pcm.tobytes())
    subprocess.run(['afconvert', '-f', 'flac', '-d', 'flac', name, dest], check=True)
    os.remove(name)
    if os.path.getsize(dest) < 200:
        raise RuntimeError(f'empty FLAC written for {dest}')


FOLDERS = {'Djembe': 'Djemb\u00e9', 'Metal': 'M\u00e9tal'}  # tab id -> folder name, when they differ


def sync_folder(root, moves):
    """Moves every sound to <Tab folder>/<pad name>.wav (loops and duplicates to their folders), so the sample folder
    always matches the plugin. Updates _journal.csv (original path -> current path) and returns current -> original."""
    journal_path = os.path.join(root, '_journal.csv')
    journal = list(csv.reader(open(journal_path, encoding='utf-8'), delimiter=';'))[1:] if os.path.exists(journal_path) else []
    origin = {after: before for before, after in journal}
    moves = [(a, b) for a, b in moves if a != b]
    for a, _ in moves:  # two steps, so that renames inside a folder never overwrite each other
        os.rename(os.path.join(root, a), os.path.join(root, a + '.moving'))
    for a, b in moves:
        dest = os.path.join(root, b)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        base, ext, n = dest[:-4], dest[-4:], 2
        while os.path.exists(dest):
            dest, n = f'{base} ({n}){ext}', n + 1
        os.rename(os.path.join(root, a + '.moving'), dest)
        origin[os.path.relpath(dest, root)] = origin.pop(a, a)
    origin = {c: o for c, o in origin.items() if os.path.exists(os.path.join(root, c))}  # forget deleted files
    with open(journal_path, 'w', newline='', encoding='utf-8') as f:
        csv.writer(f, delimiter=';').writerows([('avant', 'apres')] + [(o, c) for c, o in origin.items()])
    for dirpath, dirs, files in os.walk(root, topdown=False):
        if dirpath != root and not [f for f in os.listdir(dirpath) if f != '.DS_Store']:
            shutil.rmtree(dirpath)
    return origin


def main():
    rows = json.load(open(sys.argv[1]))
    source = sys.argv[2]
    out = os.path.join(HERE, 'assets', 'samples', 'pack')
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)

    kept, skipped = [], []
    for r in rows:
        if r['cat'] == 'LOOP':
            skipped.append((r, 'loop'))
        elif any(near_duplicate(r, k) for k in kept):
            skipped.append((r, 'doublon'))
        else:
            kept.append(r)

    tabs = {t: [] for t, _ in TABS}
    for r in kept:
        r['tab'] = tab_of(r)
        tabs[r['tab']].append(r)

    report = [('fichier', 'onglet', 'nom du pad', 'classe par', 'page/pad')]
    moves = []
    index = []
    for c, (tab, _) in enumerate(TABS):
        entries = []
        for i, (r, name) in enumerate(pad_names(tab, tabs[tab])):
            x, sr, *_ = read_wav(os.path.join(source, r['path']))
            resource = f'c{c}_{i:03d}.flac'
            write_flac(prepare(x, sr), sr, os.path.join(out, resource))
            entries.append((resource.replace('.', '_'), name, r['md5'][:16]))
            report.append((r['path'], tab, name, r['how'], f"{i // 16 + 1}/{i % 16 + 1}"))
            moves.append((r['path'], os.path.join(FOLDERS.get(tab, tab), name + '.wav')))
        index.append((tab, entries))
        print(f'{tab:8} {len(entries):4}')
    for r, why in skipped:
        report.append((r['path'], '', '', '', why))
        if r['path'].split('/')[0] not in ('Loops', 'Doublons'):
            moves.append((r['path'], os.path.join('Loops' if why == 'loop' else 'Doublons', os.path.basename(r['path']))))
    # Exact copies dropped while sorting are still in the tab folders: move them to Doublons too.
    known = {r['path'] for r in rows}
    for folder in set(FOLDERS.get(t, t) for t, _ in TABS):
        for f in sorted(os.listdir(os.path.join(source, folder))) if os.path.isdir(os.path.join(source, folder)) else []:
            path = os.path.join(folder, f)
            if f.lower().endswith('.wav') and path not in known:
                moves.append((path, os.path.join('Doublons', f)))
                report.append((path, '', '', '', 'doublon'))
    origins = sync_folder(source, moves)
    report = [report[0] + ("fichier d'origine",)] + [
        (dict(moves).get(row[0], row[0]),) + tuple(row[1:]) + (origins.get(dict(moves).get(row[0], row[0]), row[0]),) for row in report[1:]]

    with open(os.path.join(HERE, 'Source', 'SampleIndex.h'), 'w') as h:
        h.write('#pragma once\n// Generated by Tools/pack_samples.py: do not edit by hand.\n\n')
        h.write('// id: from the audio content, so it stays the same when a sound is renamed or moved (favourites use it).\n')
        h.write('struct SampleEntry { const char* resource; const char* name; const char* id; };\n\n')
        for c, (tab, entries) in enumerate(index):
            h.write(f'inline const SampleEntry samples{c}[] {{\n')
            for res, name, sid in entries:
                h.write(f'    {{ "{res}", u8"{name.replace(chr(34), "")}", "{sid}" }},\n')
            h.write('};\n')
        h.write('\nstruct SampleList { const SampleEntry* entries; int count; };\n')
        h.write(f'inline const SampleList sampleLists[{len(index)}] {{\n')
        for c, (tab, entries) in enumerate(index):
            h.write(f'    {{ samples{c}, {len(entries)} }}, // {tab}\n')
        h.write('};\n')

    os.makedirs(os.path.join(HERE, 'docs'), exist_ok=True)
    with open(os.path.join(HERE, 'docs', 'classement.csv'), 'w', newline='', encoding='utf-8') as f:
        csv.writer(f, delimiter=';').writerows(report)
    size = sum(os.path.getsize(os.path.join(out, f)) for f in os.listdir(out))
    print('kept', len(kept), 'skipped', len(skipped), 'pack', round(size / 1e6, 1), 'MB')


main()
