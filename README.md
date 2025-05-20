# G-TTCAN

A lightweight, time-triggered communication **protocol** for communication between microcontrollers on a CAN Bus.

This repo serves as a platform agnostic implementation of the protocol written in C with a focus on simplicity, low resource overhead and low communication overhead.

#### Overview

In a standard CAN Bus implementation, nodes can transmit ad hoc, and priority based arbitration resolves communication conflicts when they occur. In G-TTCAN, a predefined schedule determines which node transmits, and what they transmit, for each slot in the schedule.

If a node is the only node on the network, it will decide to be the master. Otherwise, the node with the lowest node id will be the master. The master sends a reference frame at the start of schedule, and at any other points in the schedule where a reference frame is defined. Upon receiving these reference frames, nodes synchronise their positions in their local schedule and their timers to be aligned with the reference frame received. Thus, the entire network resynchronises every time a reference frame is sent. Nodes transmit upon receiving an internal timer interrupt which was set either: a) upon receiving a reference frame OR b) upon trasmitting the previous frame in their schedule.

Key Concepts
Time-Triggered Communication
GTTCAN operates on a time-triggered paradigm where communication occurs according to a predetermined schedule rather than in response to events.
Master Selection

If a node is the only one on the network, it becomes the master
Otherwise, the node with the lowest node ID becomes the master
The master sends reference frames to synchronize the network

Scheduling

A global schedule defines the transmission sequence for all nodes
Each node maintains a local schedule with only its own transmission slots
Nodes synchronize based on reference frames sent by the master

Synchronization

The master sends reference frames at designated points in the schedule
Upon receiving a reference frame, nodes synchronize their position in the schedule
Nodes adjust their timing based on synchronized frames to maintain network coherence

Requirements

Each device must have a dedicated timer with interrupt capabilities
Devices must be able to set their timer to interrupt after a specified number of network time units
All devices must support extended CAN frames and share the same CAN bus

