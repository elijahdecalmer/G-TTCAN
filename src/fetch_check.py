import subprocess
import os
import argparse

# --- Parse command line arguments ---
parser = argparse.ArgumentParser(description='Frame schedule verification tool')
parser.add_argument('--remote-user', default="root", help='Remote username for SCP')
parser.add_argument('--remote-host', default="arty.local", help='Remote hostname for SCP')
parser.add_argument('--remote-path', default="output.txt", help='Remote file path for SCP')
parser.add_argument('--local-path', default="output.txt", help='Local file path')
parser.add_argument('--schedule-file', default="../../../Src/global_schedule.c", help='C file containing the global schedule')
parser.add_argument('--header-file', default="../src/include/gttcan.h", help='Header file with constants and typedefs')
parser.add_argument('--no-download', action='store_true', help='Skip downloading the output file')
args = parser.parse_args()

# --- Settings ---
remote_user = args.remote_user
remote_host = args.remote_host
remote_path = args.remote_path
local_path = args.local_path
schedule_file = args.schedule_file
header_file = args.header_file

# --- 1. SCP the file ---
if not args.no_download:
    print(f"Downloading {remote_path} from {remote_host}...")
    scp_command = ["scp", f"{remote_user}@{remote_host}:{remote_path}", local_path]
    try:
        subprocess.run(scp_command, check=True)
        print("Download complete.")
    except subprocess.CalledProcessError:
        print("Warning: SCP download failed. Using local file instead.")
    except FileNotFoundError:
        print("Warning: SCP command not found. Using local file instead.")
else:
    print(f"Skipping download, using existing {local_path}")

# --- 2. Parse output.txt ---
print("Parsing output.txt...")
seen_frames = []  # List of (node_id, slot_id, data_id)

with open(local_path, "r") as f:
    for line in f:
        line = line.strip()
        if line.startswith("<0x"):
            parts = line.split()
            frame_hex = parts[0][1:-1]  # Remove < >
            data_bytes = parts[2:]  # After [8]
            
            full_id = int(frame_hex, 16)
            slot_id = (full_id >> 16) & 0xFFFF
            data_id = full_id & 0xFFFF
            node_id = int(data_bytes[0], 16)

            # Store as (node_id, slot_id, data_id) to match global_schedule format
            seen_frames.append((node_id, slot_id, data_id))

print(f"\nFound {len(seen_frames)} frames.")

# --- 3. Define the expected global schedule ---

# Define REFERENCE and GENERIC IDs (fallback values)
REFERENCE_FRAME_DATA_ID = 0x0000
GENERIC_DATA_ID = 0x0001

# Import constants from header file
def parse_constants_from_header(header_path):
    constants = {}
    try:
        with open(header_path, 'r') as f:
            content = f.read()
        
        # Find all #define statements
        lines = content.split('\n')
        for line in lines:
            line = line.strip()
            if line.startswith('#define'):
                parts = line.split(maxsplit=2)
                if len(parts) >= 3:
                    name = parts[1].strip()
                    value = parts[2].strip()
                    
                    # Extract just the numeric part before any comments
                    if '//' in value:
                        value = value.split('//')[0].strip()
                    
                    # Convert value to int if possible
                    try:
                        # Handle hex values (0x) and decimal values
                        if value.startswith('0x'):
                            constants[name] = int(value, 16)
                        else:
                            constants[name] = int(value)
                    except ValueError:
                        # If not convertible to int, keep as string
                        constants[name] = value
        
        return constants
    except Exception as e:
        print(f"Warning: Failed to parse header file {header_path}: {e}")
        return {}

# Import schedule from C file
def parse_schedule_from_c_file(file_path, header_path=None):
    # Set default constants
    constants = {
        "REFERENCE_FRAME_DATA_ID": REFERENCE_FRAME_DATA_ID,
        "GENERIC_DATA_ID": GENERIC_DATA_ID
    }
    
    # Try to load constants from header file if provided
    if header_path and os.path.exists(header_path):
        header_constants = parse_constants_from_header(header_path)
        constants.update(header_constants)
        print(f"Loaded constants from {header_path}: {constants}")
    
    try:
        with open(file_path, 'r') as f:
            content = f.read()
        
        # Extract the section containing the global schedule
        if "global_schedule[" in content:
            start = content.find("global_schedule[")
            start = content.find("{", start)
            end = content.find("};", start)
            schedule_content = content[start+1:end].strip()
            
            # Parse entries
            schedule = []
            lines = schedule_content.split('\n')
            for line in lines:
                line = line.strip()
                if not line or line.startswith('//'):
                    continue
                    
                # Extract the tuple values from {node, slot, data_id}
                if '{' in line and '}' in line:
                    entry = line[line.find('{')+1:line.find('}')]
                    parts = [p.strip() for p in entry.split(',')]
                    
                    if len(parts) >= 3:
                        node_id = int(parts[0])
                        
                        # Handle slot value (decimal)
                        slot_id = int(parts[1])
                        
                        # Handle data_id (might be a named constant)
                        data_id_str = parts[2].strip()
                        if data_id_str in constants:
                            data_id = constants[data_id_str]
                        else:
                            try:
                                data_id = int(data_id_str, 0)  # Auto-detect base (hex, decimal)
                            except ValueError:
                                print(f"Warning: Could not parse data_id '{data_id_str}', using 0")
                                data_id = 0
                        
                        schedule.append((node_id, slot_id, data_id))
            
            return schedule
        else:
            print("Error: Could not find global_schedule in the C file")
            return []
            
    except Exception as e:
        print(f"Error parsing C file: {e}")
        return []

# Try to import schedule from C file, fall back to hardcoded schedule if not found
imported_schedule = parse_schedule_from_c_file(schedule_file, header_file)

if imported_schedule:
    print(f"Successfully imported schedule from {schedule_file}")
    global_schedule = imported_schedule
else:
    print(f"Failed to import schedule from {schedule_file}, using hardcoded schedule")
    # Fallback hardcoded schedule
    global_schedule = [
        (1, 0x00, REFERENCE_FRAME_DATA_ID),
        (1, 0x14, GENERIC_DATA_ID),
        (1, 0x28, REFERENCE_FRAME_DATA_ID),
        (1, 0x37, GENERIC_DATA_ID),
        (1, 0x3C, REFERENCE_FRAME_DATA_ID),
        (2, 0x46, GENERIC_DATA_ID),
        (2, 0x4B, GENERIC_DATA_ID),
        (1, 0x50, GENERIC_DATA_ID),
        (2, 0x55, GENERIC_DATA_ID),
        (2, 0x5A, GENERIC_DATA_ID),
        (1, 0x64, REFERENCE_FRAME_DATA_ID),
        (2, 0x6E, GENERIC_DATA_ID),
    ]

# Display all expected frames
print("\n--- Expected Frames in Global Schedule ---")
print(f"Total frames in schedule: {len(global_schedule)}")
print("Format: Node ID, Slot ID (hex), Data ID (hex), Frame Type")
for i, (node, slot, data) in enumerate(global_schedule):
    # Make sure data is an integer for formatting
    if isinstance(data, str):
        try:
            data = int(data)
        except ValueError:
            data = 0  # Default if conversion fails
    
    frame_type = "REFERENCE" if data == REFERENCE_FRAME_DATA_ID else "GENERIC"
    print(f"{i+1:2}. Node {node}, Slot 0x{slot:02X}, Data ID 0x{data:04X} ({frame_type})")
print("-------------------------------------------\n")

print(f"Expecting {len(global_schedule)} frames per cycle.")

# --- 4. Compare frame presence ---
correct = 0
found = []
missing_frames = []

for expected in global_schedule:
    # Check if this frame exists in the captured data
    if expected in seen_frames:
        correct += 1
        found.append(expected)
    else:
        missing_frames.append(expected)

# --- 5. Analyze cycles throughout the entire file ---
print("\n--- Analyzing All Cycles in Output File ---")

# Function to identify cycles in the output
def identify_cycles(frames, schedule):
    if len(frames) == 0 or len(schedule) == 0:
        return []
    
    schedule_length = len(schedule)
    cycles = []
    current_cycle = []
    expected_idx = 0
    
    # Try to find the first frame matching the start of the schedule
    start_frame = schedule[0]
    start_idx = 0
    
    # Try each possible frame in the schedule as a starting point
    for s_idx in range(len(schedule)):
        potential_start = schedule[s_idx]
        for f_idx, frame in enumerate(frames):
            if frame == potential_start:
                start_frame = potential_start
                start_idx = s_idx
                expected_idx = s_idx
                
                # Start a new cycle with this frame
                current_cycle = [frame]
                
                # See if we can follow the full schedule from this point
                i = f_idx + 1  # Next frame in output
                seen_schedule_items = 1
                
                while i < len(frames) and seen_schedule_items < schedule_length:
                    next_expected = (expected_idx + 1) % schedule_length
                    if frames[i] == schedule[next_expected]:
                        current_cycle.append(frames[i])
                        expected_idx = next_expected
                        seen_schedule_items += 1
                        i += 1
                    else:
                        break
                
                # If we found a full cycle, use this as our starting point
                if seen_schedule_items == schedule_length:
                    print(f"Found initial cycle starting with {start_frame}")
                    break
            
        # If we found a full cycle, break out of the outer loop too
        if len(current_cycle) == schedule_length:
            break
    
    # If we couldn't find a full cycle, give up
    if len(current_cycle) < schedule_length:
        print("Could not find a complete initial cycle matching the schedule.")
        return cycles
    
    # Now process all frames, detecting cycles
    cycles.append(current_cycle)
    current_cycle = []
    
    # Process remaining frames
    for frame in frames[frames.index(start_frame) + schedule_length:]:
        next_expected = (expected_idx + 1) % schedule_length
        
        if frame == schedule[next_expected]:
            # This frame is expected next in the schedule
            current_cycle.append(frame)
            expected_idx = next_expected
            
            # If we've completed a cycle, save it
            if len(current_cycle) == schedule_length:
                cycles.append(current_cycle)
                current_cycle = []
        else:
            # Find where this frame should be in the schedule
            found = False
            for i, expected_frame in enumerate(schedule):
                if frame == expected_frame:
                    # Start a new cycle from this point
                    current_cycle = [frame]
                    expected_idx = i
                    found = True
                    break
            
            # If we didn't find this frame in the schedule, it's unexpected
            if not found:
                if current_cycle:
                    print(f"Unexpected frame detected. Current cycle interrupted after {len(current_cycle)} frames.")
                    current_cycle = []
    
    return cycles

cycles = identify_cycles(seen_frames, global_schedule)

# Print cycle analysis
print(f"\nFound {len(cycles)} complete cycles in the output file.")

if len(cycles) > 0:
    # Check for cycle correctness
    correct_cycles = 0
    order_issues = 0
    
    for i, cycle in enumerate(cycles):
        # Print information for every cycle
        print(f"\nCycle {i+1}:")
        print("--------------------------------------------------")
        
        # Check frame order
        order_correct = True
        for j, frame in enumerate(cycle):
            expected_frame = global_schedule[j]
            
            # Make sure data is an integer for formatting
            exp_node, exp_slot, exp_data = expected_frame
            if isinstance(exp_data, str):
                try:
                    exp_data = int(exp_data)
                except ValueError:
                    exp_data = 0
                    
            act_node, act_slot, act_data = frame
            
            # Display the frame
            frame_type = "REFERENCE" if act_data == REFERENCE_FRAME_DATA_ID else "GENERIC"
            print(f"{j+1:2}. Node {act_node}, Slot 0x{act_slot:02X}, Data ID 0x{act_data:04X} ({frame_type})")
            
            # Check if it matches expected
            if frame != expected_frame:
                order_correct = False
                print(f"   MISMATCH: Expected Node {exp_node}, Slot 0x{exp_slot:02X}, Data ID 0x{exp_data:04X}")
        
        # Summary for this cycle
        if order_correct:
            print("Order check: PASSED ✓")
            correct_cycles += 1
        else:
            print("Order check: FAILED ✗")
            order_issues += 1
    
    # Overall statistics
    print("\n--------------------------------------------------")
    print(f"Total cycles analyzed: {len(cycles)}")
    print(f"Correct cycles: {correct_cycles}/{len(cycles)} ({(correct_cycles/len(cycles))*100:.1f}%)")
    print(f"Cycles with ordering issues: {order_issues}")
    
    # Overall statistics for frames
    total_frames = len(seen_frames)
    total_expected = len(cycles) * len(global_schedule)
    print(f"\nTotal frames seen: {total_frames}")
    print(f"Total frames expected across all cycles: {total_expected}")
    
    # Frame presence statistics
    frame_presence = {}
    for node, slot, data_id in global_schedule:
        key = (node, slot, data_id)
        frame_presence[key] = 0
    
    for frame in seen_frames:
        if frame in frame_presence:
            frame_presence[frame] += 1
    
    print("\nFrame presence analysis:")
    for frame, count in frame_presence.items():
        node, slot, data = frame
        expected_count = len(cycles)  # Once per cycle
        presence_pct = (count / expected_count) * 100 if expected_count > 0 else 0
        
        # Make sure data is an integer for formatting
        if isinstance(data, str):
            try:
                data = int(data)
            except ValueError:
                data = 0
        
        frame_type = "REFERENCE" if data == REFERENCE_FRAME_DATA_ID else "GENERIC"
        print(f"Node {node}, Slot 0x{slot:02X}, Data ID 0x{data:04X} ({frame_type}): {count}/{expected_count} ({presence_pct:.1f}%)")

else:
    print("No complete cycles found in the output file.")
    
    # Summary of seen frames
    print("\nFrames seen (not in complete cycles):")
    for i, frame in enumerate(seen_frames[:20]):  # Show first 20 frames
        node, slot, data = frame
        frame_type = "REFERENCE" if data == REFERENCE_FRAME_DATA_ID else "GENERIC"
        print(f"{i+1:2}. Node {node}, Slot 0x{slot:02X}, Data ID 0x{data:04X} ({frame_type})")
    
    if len(seen_frames) > 20:
        print(f"... and {len(seen_frames) - 20} more frames ...")

print("\n--- Verification Summary ---")
# Basic frame count check
frame_count_expected = len(global_schedule)
frame_count_seen = len(set(seen_frames))
print(f"Unique frame types: {frame_count_seen}/{frame_count_expected}")

# Frame presence stats
presence_accuracy = (correct / len(global_schedule)) * 100 if global_schedule else 0
print(f"Frame presence accuracy: {correct}/{len(global_schedule)} ({presence_accuracy:.1f}%)")

# Cycle stats
if len(cycles) > 0:
    print(f"Complete cycles detected: {len(cycles)}")
    print(f"Cycles with correct frame order: {correct_cycles}/{len(cycles)} ({(correct_cycles/len(cycles))*100:.1f}%)")
else:
    print("No complete cycles detected.")

if missing_frames:
    print("\nMissing frames:")
    for (node, slot, data) in missing_frames:
        # Make sure data is an integer for formatting
        if isinstance(data, str):
            try:
                data = int(data)
            except ValueError:
                data = 0
        
        print(f"Node {node}, Slot 0x{slot:02X}, Data ID 0x{data:04X}")
        
        # Check if they exist in a different format
        for seen in seen_frames:
            seen_node, seen_slot, seen_data = seen
            if slot == seen_slot and data == seen_data:
                print(f"  - Found with different node: expected Node {node}, found Node {seen_node}")
            elif node == seen_node and slot == seen_slot:
                print(f"  - Found with different data_id: expected 0x{data:04X}, found 0x{seen_data:04X}")
else:
    print("\nAll expected frame types were present in the output file.")