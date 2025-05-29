# G-TTCAN

A lightweight, time-triggered communication **protocol** for communication between microcontrollers on a CAN Bus.

This repo serves as a platform agnostic implementation of the protocol written in C with a focus on simplicity, low resource overhead and low communication overhead.

**DISCLAIMER:**

**This protocol is a work in progress and is provided "as-is" without any warranties or guarantees.** G-TTCAN is currently experimental and not suitable for production use, safety-critical applications, or systems where reliability is essential. The protocol does not guarantee deterministic timing, fault tolerance, data integrity, or consistent performance across different hardware platforms or network conditions. Users should thoroughly test and validate the implementation in their specific environment before any deployment. The authors assume no responsibility for any damage, data loss, system failures, or other consequences resulting from the use of this protocol. Use at your own risk and discretion.

#### Overview

In a standard CAN Bus implementation, nodes can transmit ad hoc, and priority based arbitration resolves communication conflicts when they occur. In G-TTCAN, a predefined schedule determines which node transmits, and what data they should transmit, for each slot in the schedule.

If a node is the only node on the network, it will decide to be the master. Otherwise, the node with the lowest node id that transmits in the schedule will be the master for the next round, and so on. The master sends a reference frame at the start of schedule, and at any other points in the schedule where a reference frame is defined. Upon receiving these reference frames, all other nodes synchronise their positions in their local schedule and their timers to be aligned with the reference frame received. Thus, the entire network resynchronises every time a reference frame is sent. Nodes transmit upon an internal timer interrupt which was set either: a) upon receiving a reference frame OR b) upon their transmitting the previous frame in their schedule.

#### Key Concepts

**Time-Triggered Communication**

GTTCAN operates on a time-triggered paradigm where communication occurs according to a predetermined schedule rather than in response to events.

**Master Selection**

If a node is the only one on the network, it becomes the master. Otherwise, the node with the lowest node ID becomes the master. If a node with a lower node ID rejoins the network, they will become the master. The master sends reference frames wherever they appear in the schedule.

**Scheduling**

A global schedule defines the transmission sequence for all nodes.
Each node maintains a local schedule with only its own transmission slots and reference slots.
Nodes synchronize based on reference frames sent by the master.

**Synchronization**

The master sends reference frames at designated points in the schedule.
Upon receiving a reference frame, nodes synchronize their position in their schedule.
Upon receiving a reference frame, nodes recalculate their next time to transmit, and overwrite their relevant timer to trigger an interrupt in this amount of time.

**System Time Units**
System Time Units (STU) are the fundamental timing unit used throughout G-TTCAN for all time-related parameters and calculations. All G-TTCAN timing parameters - including slot_duration, interrupt_timing_offset, and timer delays - must use the same units as your hardware timer. When you specify a slot_duration of 300 STU and G-TTCAN later calls your timer callback with a delay of 600 STU, both values must be in identical units that your timer can directly understand without conversion. Whether STU represents microseconds, milliseconds, timer ticks, or any other unit doesn't matter to G-TTCAN, but this unit must be consistent between your timing configuration and timer implementation across all nodes in the network.

**CAN Frame Extended ID**

The 29-bit extended CAN frame identifier in G-TTCAN is composed of two fields: the slot_id and data_id from the schedule entry. The slot_id occupies the most significant bits (default: 13 bits) and represents the position in the global schedule, while the data_id occupies the least significant bits (default: 16 bits) and identifies the type of data being transmitted. The frame ID is constructed using the formula: `frame_id = (slot_id << GTTCAN_NUM_DATA_ID_BITS) | data_id`. For example, with slot_id = 5 and data_id = 100, the resulting frame ID would be `(5 << 16) | 100 = 0x50064`. The bit allocation is configurable through the `GTTCAN_NUM_SLOT_ID_BITS`and`GTTCAN_NUM_DATA_ID_BITS` constants, but their sum must not exceed 29 bits to fit within the extended CAN frame identifier. This encoding allows receivers to extract both the schedule position and data type from a single frame identifier using bit shifting and masking operations.

#### Examples

See the Examples folder in the code repository for hardware-specific example implementations of G-TTCAN.

**Schedule**

```c
global_schedule_entry_t global_schedule[MAX_GLOBAL_SCHEDULE_LENGTH] = {
    // {node_id, slot_id, data_id},
    {1, 0, REFERENCE_FRAME_DATA_ID}, // data_id of 0 is interpreted as a reference frame, send at the start, and throughout the schedule as necessary
    {2, 1, TEMP_DATA},
    {3, 2, SPEED_DATA},
    {1, 3, GPS_DATA_1},
    {1, 4, GPS_DATA_2},
    {2, 5, VOLTAGE_DATA},
    {3, 6, ORIENTATION_DATA},
    {1, 7, STATUS_DATA},
};
```

#### Configuration Guidelines

****Slot Duration****

The `slot_duration` parameter defines the time allocated for each entry in the schedule and must be longer than the time required to transmit a CAN frame. The recommended slot duration is at least 1.5 times the CAN frame transmission time to allow for processing overhead and timing margins. For example, if a CAN frame takes 200 microseconds to transmit, set `slot_duration` to at least 300 microseconds.

**Interrupt Timing Offset**

The `interrupt_timing_offset` parameter compensates for processing delays between frame reception/transmission and timer configuration. This value should be measured on each hardware platform by timing from point A (the calling of `gttcan_process_frame()` with a received reference frame) to point B (the execution of the line in your `set_timer_int_callback_fp` implementation that actually sets the interrupt timer). This offset is applied every time G-TTCAN sets a timer to account for the processing time required, ensuring that timer interrupts occur closer to the correct moments relative to the schedule.

#### Requirements

- Each device must have a dedicated timer with interrupt capabilities
- Devices must be able to set their timer to interrupt after a specified number of System Time Units
- All devices must support extended CAN frames and share the same CAN bus
