#!/usr/bin/env python3
import re
import argparse
import sys

def parse_c_schedule(path):
    """Parses a .c file to extract CAN IDs from the global_schedule structure."""
    with open(path, 'r') as f:
        content = f.read()

    pattern = r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*([A-Z_]+)\s*\}'
    matches = re.findall(pattern, content)
    if not matches:
        print("Error: No valid {node_id, slot_id, data_id} entries found in .c schedule.")
        sys.exit(1)

    id_map = {
        "REFERENCE_FRAME_DATA_ID": 0,
        "GENERIC_DATA_ID": 1
    }
    can_ids = []
    for _, slot_id, data_id_const in matches:
        slot_hex = int(slot_id) & 0xFFFF
        data_id_value = id_map.get(data_id_const, 0)
        can_id = (slot_hex << 16) | data_id_value
        can_ids.append(f"0x{can_id:08x}")
    return can_ids

def parse_log_file(path):
    """Parses a CAN bus log file to extract frame IDs with their line numbers."""
    entries = []
    with open(path, 'r') as f:
        for line_no, line in enumerate(f, 1):
            m = re.search(r'<(0x[0-9a-fA-F]{8})>', line)
            if m:
                entries.append((m.group(1).lower(), line_no))
    return entries

def analyze_schedule(reference_ids, log_entries):
    """Analyzes trimmed cycles for missing, duplicate, and out-of-order frames,"""
    N = len(reference_ids)
    idx_map = {cid: i for i, cid in enumerate(reference_ids)}
    # Map log entries to indices
    indexed = [(idx_map[e], ln) for e, ln in log_entries if e in idx_map]
    if not indexed:
        print("No valid frames from the log match the schedule.")
        return

    total_log = len(log_entries)
    boundaries = [i for i in range(1, len(indexed)) if indexed[i-1][0] > indexed[i][0]]
    if len(boundaries) >= 2:
        indexed = indexed[boundaries[0]:boundaries[-1]]

    total_present = len(indexed)

    boundaries = [i for i in range(1, len(indexed)) if indexed[i-1][0] > indexed[i][0]]
    cycles = []
    start = 0
    for b in boundaries:
        cycles.append(indexed[start:b])
        start = b
    cycles.append(indexed[start:])

    completed_cycles = len(cycles)
    total_expected = completed_cycles * N

    total_missing = 0
    total_out_of_order = 0
    for cycle in cycles:
        prev = cycle[0][0]
        for idx, _ in cycle[1:]:
            step = (idx - prev) % N
            if step > 1:
                total_missing += step - 1
            elif step == 0 or step > N - 1:
                total_out_of_order += 1
            prev = idx

    present_percent = (total_present / total_expected * 100) if total_expected else 0
    missing_percent = (total_missing / total_expected * 100) if total_expected else 0

    print(f"Reference Schedule Size:      {N}")
    print(f"Log Frame Count:              {total_log}")
    print(f"Frames After Trimming:        {total_present}")
    print(f"Missing Frames:               {total_missing}")
    print(f"Out-of-Order Events:          {total_out_of_order}")
    print(f"Completed Cycles Detected:    {completed_cycles}")
    print(f"Frame Presence:               {present_percent:.2f}%")
    print(f"Frame Missing Rate:           {missing_percent:.2f}%\n")

    for i, cycle in enumerate(cycles, 1):
        missing = []
        duplicates = []
        out_of_order = []
        seen = set()
        prev_idx, prev_ln = cycle[0]
        seen.add(prev_idx)

        for idx, ln in cycle[1:]:
            if idx == prev_idx:
                duplicates.append(ln)
            else:
                step = (idx - prev_idx) % N
                if step == 1:
                    pass
                elif step > 1:
                    for _ in range(step - 1):
                        missing.append(ln)
                else:
                    out_of_order.append(ln)
            seen.add(idx)
            prev_idx = idx

        if not missing and not duplicates and not out_of_order:
            continue

        print(f"Cycle {i} (lines {cycle[0][1]}-{cycle[-1][1]}):")
        if missing:
            print(f"  Missing Frames:      {len(missing)} at lines {missing}")
        if duplicates:
            print(f"  Duplicate Frames:    {len(duplicates)} at lines {duplicates}")
        if out_of_order:
            print(f"  Out-of-Order:        {len(out_of_order)} at lines {out_of_order}\n")

def main():
    p = argparse.ArgumentParser(description="Detect missing, duplicate, and out-of-order frames per cycle.")
    p.add_argument("schedule_c_file", help=".c file with global_schedule")
    p.add_argument("log_txt_file", help=".txt log file from CAN bus")
    args = p.parse_args()
    try:
        ref_ids = parse_c_schedule(args.schedule_c_file)
        log_entries = parse_log_file(args.log_txt_file)
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    analyze_schedule(ref_ids, log_entries)

if __name__ == "__main__":
    main()