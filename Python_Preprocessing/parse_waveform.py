
#!/usr/bin/env python3
"""
parse_waveform.py

Read a text file containing repeated blocks that start with
"**Begin Metadata**" followed by metadata lines and then a
"**Begin Waveform**" header and CSV-style waveform lines.

For each pair, this script writes two files into the output
directory (default: <input_stem>_out):

  <input_stem>_001_metadata.csv  - two-column CSV: key,value
  <input_stem>_001_waveform.csv  - waveform CSV (Time,Waveform + data)

If a metadata block appears without a following waveform, the
metadata CSV is still written. Duplicate metadata keys are
preserved by appending _2, _3, ...

Instruction for Usage:
    Prerequirements:
        You must have Python 3 installed.

    Type the following in powershell:
    python parse_waveform.py inputfile 
    this will create a directory named inputfile_out containing the output CSVs.
    The filename must either be in the current working directory or be a full path.

"""

from pathlib import Path
import argparse
import csv
import re
from collections import OrderedDict
from typing import List
import logging


META_HDR_RE = re.compile(r'^\s*\*\*\s*Begin\s+Metadata\s*\*\*', re.IGNORECASE)
WAVE_HDR_RE = re.compile(r'^\s*\*\*\s*Begin\s+Waveform\s*\*\*', re.IGNORECASE)
WAVE_CSV_HDR_RE = re.compile(r'^\s*Time\s*,\s*Waveform\s*$', re.IGNORECASE)


def sanitize_key(k: str) -> str:
    k = k.strip()
    k = re.sub(r'\s+', '_', k)
    k = re.sub(r'[^0-9A-Za-z_\-\.]+', '_', k)
    if re.match(r'^\d', k):
        k = 'k_' + k
    return k or 'key'


def next_unique_key(d: OrderedDict, key: str) -> str:
    if key not in d:
        return key
    i = 2
    while f"{key}_{i}" in d:
        i += 1
    return f"{key}_{i}"


def write_metadata_csv(path: Path, meta: OrderedDict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['key', 'value'])
        for k, v in meta.items():
            writer.writerow([k, v])


def write_waveform_csv(path: Path, lines: List[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', encoding='utf-8', newline='') as f:
        # Ensure header exists
        if not lines:
            f.write('Time,Waveform\n')
            return
        # Some files include 'Time, Waveform' header; normalize to 'Time,Waveform'
        first = lines[0].strip()
        if WAVE_CSV_HDR_RE.match(first):
            f.write('Time,Waveform\n')
            for ln in lines[1:]:
                f.write(ln.rstrip('\r\n') + '\n')
        else:
            f.write('Time,Waveform\n')
            for ln in lines:
                f.write(ln.rstrip('\r\n') + '\n')


def filter_monotonic_time(lines: List[str]) -> List[str]:
    """Remove waveform rows where timestamp is not strictly monotonic.

    Rules:
    - Remove an entry if its timestamp is smaller than the previous entry.
    - Remove an entry if the next entry's timestamp is smaller than the current entry.

    Input lines are raw CSV lines like '1439551,57640\n' (may include header).
    Returns a filtered list of lines preserving original order.
    """
    # Parse timestamps into list of tuples (idx, timestamp, line)
    entries = []
    for idx, raw in enumerate(lines):
        s = raw.strip()
        if not s:
            continue
        # skip header if present
        if WAVE_CSV_HDR_RE.match(s):
            continue
        parts = s.split(',', 1)
        try:
            t = int(parts[0].strip())
        except Exception:
            # non-integer timestamp -> keep as-is (conservative)
            t = None
        entries.append((idx, t, raw))

    if not entries:
        return []

    n = len(entries)
    keep = [True] * n

    for j in range(n):
        t = entries[j][1]
        if t is None:
            # can't evaluate; keep
            continue
        # previous
        if j > 0:
            t_prev = entries[j-1][1]
            if t_prev is not None and t < t_prev:
                keep[j] = False
                continue
        # next
        if j < n-1:
            t_next = entries[j+1][1]
            if t_next is not None and t_next < t:
                keep[j] = False

    # Build filtered lines preserving original order
    filtered = [entries[j][2] for j in range(n) if keep[j]]
    return filtered


class SignalSaturationDetected(Exception):
    """Raised when a signal shows saturation-like behavior: many low values and values near the top scale."""


def filter_and_detect_values(lines: List[str], *, low_thresh: int = 10, high_thresh: int = 2**16, near_top_delta: int = 500, low_count_trigger: int = 5, raise_on_detect: bool = True) -> List[str]:
    """Filter waveform rows by value range and detect potential saturation.

    - Discard any row where value < low_thresh or value > high_thresh.
    - Count values within `near_top_delta` of `high_thresh` (i.e., >= high_thresh - near_top_delta).
    - Count values < 200 (for detector condition as requested).
    - If the signal contains any near-top values AND more than `low_count_trigger` values < 200, raise SignalSaturationDetected.

    Returns filtered lines (CSV strings) preserving original order and formatting for kept rows.
    """
    kept = []
    near_top_count = 0
    low_under_200 = 0

    for raw in lines:
        s = raw.strip()
        if not s:
            continue
        # skip header if present
        if WAVE_CSV_HDR_RE.match(s):
            continue
        parts = s.split(',', 1)
        if len(parts) < 2:
            # malformed line; keep conservatively
            kept.append(raw)
            continue
        t_str = parts[0].strip()
        v_str = parts[1].strip()
        try:
            v = int(v_str)
        except Exception:
            # Non-integer value; keep conservatively
            kept.append(raw)
            continue

        # Detector counts (always computed on original values)
        if v >= high_thresh - near_top_delta:
            near_top_count += 1
        if v < 200:
            low_under_200 += 1

        # Discard values outside allowed range
        if v < low_thresh or v > high_thresh:
            logging.debug(f"Discarding value {v} at time {t_str}: outside [{low_thresh}, {high_thresh}]")
            continue

        # Keep the original raw line (preserve whitespace/newline)
        kept.append(raw)

    # Apply detector: if there are near-top values and more than low_count_trigger low values -> raise or flag
    if near_top_count > 0 and low_under_200 > low_count_trigger:
        if raise_on_detect:
            # The user requested to 'throw' but not do anything for now — raise a specific exception so callers can catch it.
            raise SignalSaturationDetected(f"Detected {near_top_count} near-top values and {low_under_200} values <200")
        else:
            logging.info(f"Signal saturation detected (near_top={near_top_count}, low_lt_200={low_under_200}) -- not raising by request")

    return kept


def parse_wavefile(infile: Path, outdir: Path) -> None:
    outdir.mkdir(parents=True, exist_ok=True)
    with infile.open('r', encoding='utf-8', errors='replace') as f:
        lines = f.readlines()

    i = 0
    n = len(lines)
    block_idx = 1

    while i < n:
        line = lines[i].rstrip('\r\n')
        # Look for metadata header
        if META_HDR_RE.match(line):
            meta = OrderedDict()
            i += 1
            # collect metadata until waveform header or EOF
            while i < n and not WAVE_HDR_RE.match(lines[i]):
                raw = lines[i].rstrip('\r\n')
                i += 1
                if not raw.strip() or raw.strip().startswith('#'):
                    continue
                # key: value on same line
                # Accept either "Key: Value" or CSV-style "Key, Value" metadata lines.
                kv_key = None
                kv_val = None
                if ':' in raw:
                    parts = raw.split(':', 1)
                    kv_key = parts[0].strip()
                    kv_val = parts[1].strip()
                elif ',' in raw:
                    # CSV-style key,value
                    parts = raw.split(',', 1)
                    kv_key = parts[0].strip()
                    kv_val = parts[1].strip()

                if kv_key is not None:
                    # If value is empty, try to read the next non-empty non-header line
                    if kv_val == '':
                        j = i
                        while j < n and (not lines[j].strip() or META_HDR_RE.match(lines[j]) or WAVE_HDR_RE.match(lines[j])):
                            j += 1
                        if j < n:
                            kv_val = lines[j].strip()
                            i = j + 1

                    # strip optional surrounding quotes for CSV-style values
                    if (kv_val.startswith('"') and kv_val.endswith('"')) or (kv_val.startswith("'") and kv_val.endswith("'")):
                        kv_val = kv_val[1:-1]

                    key = sanitize_key(kv_key)
                    key = next_unique_key(meta, key)
                    meta[key] = kv_val
                else:
                    # free-form metadata line; store under meta_line_N
                    key = next_unique_key(meta, 'meta_line')
                    meta[key] = raw.strip()

            # At this point either we're at a waveform header or EOF
            # We'll collect waveform first (if present) so we can add a saturation flag to metadata
            saturation_flag = 0

            # If waveform header follows, collect waveform
            if i < n and WAVE_HDR_RE.match(lines[i]):
                # consume waveform header line
                i += 1
                wave_lines = []
                while i < n and not META_HDR_RE.match(lines[i]):
                    raw = lines[i]
                    i += 1
                    if not raw.strip() or raw.strip().startswith('#'):
                        continue
                    wave_lines.append(raw)
                # Filter waveform lines for non-monotonic timestamps
                filtered_time = filter_monotonic_time(wave_lines)
                # Then filter values (discard out-of-range) and run detector; do not raise so we can set metadata flag
                try:
                    filtered = filter_and_detect_values(filtered_time, raise_on_detect=True)
                except SignalSaturationDetected as e:
                    logging.warning(f"SignalSaturationDetected: {e}")
                    saturation_flag = 1
                    # Re-run filtering but don't raise so we get the cleaned data to write
                    filtered = filter_and_detect_values(filtered_time, raise_on_detect=False)

                wave_path = outdir / f"{infile.stem}_{block_idx:03d}_waveform.csv"
                write_waveform_csv(wave_path, filtered)
                print(f"[+] Wrote waveform: {wave_path} (rows: {len(filtered)} of {len(wave_lines)})")

            # After waveform processing (or EOF with no waveform), write metadata and include saturation flag
            meta['saturation_detected'] = '1' if saturation_flag else '0'
            meta_path = outdir / f"{infile.stem}_{block_idx:03d}_metadata.csv"
            write_metadata_csv(meta_path, meta)
            print(f"[+] Wrote metadata: {meta_path}")

            block_idx += 1
            continue

        # Not a metadata header; skip line
        i += 1

    print(f"Done. Parsed {block_idx-1} metadata blocks. Output: {outdir}")


def main():
    ap = argparse.ArgumentParser(description='Parse pulsewave test runs into metadata and waveform CSVs')
    ap.add_argument('input', type=Path, help='Input text file')
    ap.add_argument('--out', type=Path, default=None, help='Output directory (default: <input_stem>_out)')
    args = ap.parse_args()

    infile = args.input
    if not infile.exists():
        raise SystemExit(f"Input not found: {infile}")
    outdir = args.out if args.out is not None else infile.parent / f"{infile.stem}_out"
    parse_wavefile(infile, outdir)


if __name__ == '__main__':
    main()
