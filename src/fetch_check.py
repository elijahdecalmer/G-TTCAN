import subprocess

# --- Settings ---
remote_user = "root"
remote_host = "arty.local"
remote_path = "output.txt"
local_path = "output.txt"

# --- 1. SCP the file ---
print("Downloading output.txt from the Arty...")
scp_command = ["scp", f"{remote_user}@{remote_host}:{remote_path}", local_path]
try:
    subprocess.run(scp_command, check=True)
    print("Download complete.")
except subprocess.CalledProcessError:
    print("Warning: SCP download failed. Using local file instead.")
except FileNotFoundError:
    print("Warning: SCP command not found. Using local file instead.")

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

print(f"Found {len(seen_frames)} frames.")

# --- 3. Define the expected global schedule ---

# Define REFERENCE and GENERIC IDs
REFERENCE_FRAME_DATA_ID = 0x0000
GENERIC_DATA_ID = 0x0001

# Schedule: (node_id, slot_id, data_id)
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

print(f"Expecting {len(global_schedule)} frames.")

# --- 4. Compare ---
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

accuracy = (correct / len(global_schedule)) * 100

print(f"\nResults:")
print(f"Correct frames: {correct}/{len(global_schedule)} ({accuracy:.1f}%)")

# Debug: Print all found frames
print("\nFound frames:")
for frame in seen_frames[:min(len(seen_frames), 20)]:  # Limit to first 20 for clarity
    node, slot, data = frame
    print(f"Node {node}, Slot 0x{slot:02X}, Data ID 0x{data:04X}")

# Print missing frames if any
if missing_frames:
    print("\nMissing frames:")
    for (node, slot, data) in missing_frames:
        print(f"Node {node}, Slot 0x{slot:02X}, Data ID 0x{data:04X}")
        
    # Check if they exist in a different format
    for missing in missing_frames:
        miss_node, miss_slot, miss_data = missing
        for seen in seen_frames:
            seen_node, seen_slot, seen_data = seen
            if miss_slot == seen_slot and miss_data == seen_data:
                print(f"  - Found with different node: expected Node {miss_node}, found Node {seen_node}")
            elif miss_node == seen_node and miss_slot == seen_slot:
                print(f"  - Found with different data_id: expected 0x{miss_data:04X}, found 0x{seen_data:04X}")
else:
    print("\nAll frames accounted for!")