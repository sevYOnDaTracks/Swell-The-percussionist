"""Analyse every WAV under a folder; writes one JSON line per file.
Usage: python3 -I analyze.py <folder> <out.jsonl>"""
import sys, os, json, struct, hashlib
import numpy as np


def read_wav(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise ValueError('not a RIFF/WAVE file')
    pos, fmt, raw = 12, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b'fmt ':
            tag, ch, sr = struct.unpack('<HHI', body[:8])
            bits = struct.unpack('<H', body[14:16])[0]
            if tag == 0xFFFE and len(body) >= 26:
                tag = struct.unpack('<H', body[24:26])[0]
            fmt = (tag, ch, sr, bits)
        elif cid == b'data':
            raw = body
        pos += 8 + size + (size & 1)
    if fmt is None or raw is None:
        raise ValueError('missing fmt or data chunk')
    tag, ch, sr, bits = fmt
    width = bits // 8
    n = len(raw) // (width * ch) * width * ch
    raw = raw[:n]
    if tag == 3:
        x = np.frombuffer(raw, '<f4' if bits == 32 else '<f8').astype(np.float64)
    elif tag == 1:
        if bits == 8:
            x = (np.frombuffer(raw, np.uint8).astype(np.float64) - 128) / 128
        elif bits == 16:
            x = np.frombuffer(raw, '<i2') / 32768.0
        elif bits == 24:
            b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
            v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
            v = np.where(v >= 1 << 23, v - (1 << 24), v)
            x = v / float(1 << 23)
        elif bits == 32:
            x = np.frombuffer(raw, '<i4') / 2147483648.0
        else:
            raise ValueError(f'{bits}-bit PCM')
    else:
        raise ValueError(f'format tag {tag}')
    x = x.reshape(-1, ch)
    return x, sr, ch, bits, hashlib.md5(raw).hexdigest()


def analyse(path):
    x, sr, ch, bits, md5 = read_wav(path)
    mono = x.mean(axis=1)
    out = {'sr': sr, 'ch': ch, 'bits': bits, 'md5': md5, 'dur': len(mono) / sr}
    peak = float(np.max(np.abs(mono))) if len(mono) else 0.0
    out['peak'] = peak
    if peak < 1e-5:
        out['silent'] = True
        return out
    hop = int(sr * .01)
    frames = len(mono) // hop
    env = np.sqrt(np.mean(mono[:frames * hop].reshape(frames, hop) ** 2, axis=1) + 1e-12)
    db = 20 * np.log10(env / env.max())
    # start, peak and the -40 dB end of the sound
    start = int(np.argmax(db > -40))
    top = int(np.argmax(env))
    after = np.where(db[top:] > -40)[0]
    end = top + (int(after[-1]) if len(after) else 0)
    out['attack'] = (top - start) * .01
    out['length'] = (end - start) * .01
    out['tail20'] = float((np.where(db[top:] > -20)[0][-1] if np.any(db[top:] > -20) else 0) * .01)
    # onsets: rises of at least 9 dB over 30 ms, above -30 dB, at least 70 ms apart
    onsets = []
    for i in range(3, frames):
        if db[i] > -30 and db[i] - db[i - 3] > 9 and (not onsets or i - onsets[-1] > 7):
            onsets.append(i)
    out['onsets'] = len(onsets)
    if len(onsets) > 1:
        gaps = np.diff(onsets) * .01
        out['gapMedian'] = float(np.median(gaps))
        out['gapSpread'] = float(np.std(gaps) / max(1e-6, np.mean(gaps)))
    # spectrum of the first 250 ms from the start
    seg = mono[start * hop:start * hop + int(sr * .25)]
    seg = seg * np.hanning(len(seg)) if len(seg) > 32 else seg
    spec = np.abs(np.fft.rfft(seg, n=max(4096, len(seg))))
    freqs = np.fft.rfftfreq(max(4096, len(seg)), 1 / sr)
    power = spec ** 2
    total = power.sum() + 1e-12
    out['centroid'] = float((freqs * power).sum() / total)
    cum = np.cumsum(power) / total
    out['rolloff85'] = float(freqs[np.searchsorted(cum, .85)])
    bands = [0, 150, 400, 1000, 2500, 6000, 24000]
    out['bands'] = [round(float(power[(freqs >= a) & (freqs < b)].sum() / total), 3) for a, b in zip(bands, bands[1:])]
    flat = np.exp(np.mean(np.log(power[1:] + 1e-12))) / (np.mean(power[1:]) + 1e-12)
    out['flatness'] = float(flat)
    # pitch: strongest spectral peak below 1.5 kHz, and how much it stands out
    low = (freqs > 40) & (freqs < 1500)
    if np.any(low):
        k = int(np.argmax(spec * low))
        out['pitch'] = float(freqs[k])
        out['peakiness'] = float(spec[k] / (np.median(spec[low]) + 1e-12))
    seg2 = mono[start * hop:start * hop + int(sr * .1)]
    out['zcr'] = float(np.mean(np.abs(np.diff(np.sign(seg2)))) / 2 * sr) if len(seg2) > 1 else 0
    return out


if __name__ == '__main__':
  root, dest = sys.argv[1], sys.argv[2]
  with open(dest, 'w') as w:
      for dirpath, _, files in os.walk(root):
          for name in sorted(files):
              if not name.lower().endswith('.wav'):
                  continue
              path = os.path.join(dirpath, name)
              rec = {'path': os.path.relpath(path, root)}
              try:
                  rec.update(analyse(path))
              except Exception as e:
                  rec['error'] = str(e)
              w.write(json.dumps(rec) + '\n')
