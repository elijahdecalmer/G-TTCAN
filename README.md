# G-TTCAN

A lightweight, time-triggered communication **protocol** for communication between microcontrollers on a CAN Bus.

This repo serves as a platform agnostic implementation of the protocol written in C with a focus on simplicity, low resource overhead and low communication overhead.

DISCLAIMER: This protocol is a work in progress, and is currently not guaranteed in any capacity around reliability, deterministic timing, fault tolerance, resource usage, etc. All testing should be 

#### Overview

In a standard CAN Bus implementation, nodes can transmit ad hoc, and priority based arbitration resolves communication conflicts when they occur. In G-TTCAN, a predefined schedule determines which node transmits, and what data they should transmit, for each slot in the schedule.

If a node is the only node on the network, it will decide to be the master. Otherwise, the node with the lowest node id that transmits in two consecutive rounds of the schedule will be the master for the next round, and so on. The master sends a reference frame at the start of schedule, and at any other points in the schedule where a reference frame is defined. Upon receiving these reference frames, all other nodes synchronise their positions in their local schedule and their timers to be aligned with the reference frame received. Thus, the entire network resynchronises every time a reference frame is sent. Nodes transmit upon an internal timer interrupt which was set either: a) upon receiving a reference frame OR b) upon their trasmitting the previous frame in their schedule.

#### Key Concepts

**Time-Triggered Communication**

GTTCAN operates on a time-triggered paradigm where communication occurs according to a predetermined schedule rather than in response to events.

**Master Selection**

If a node is the only one on the network, it becomes the master. Otherwise, the node with the lowest node ID becomes the master. If a node with a lower node ID rejoins the network, they will become the master. The master sends a reference frame at the start of the schedule, and optional reference frames throughout the schedule depending on length, to synchronize all nodes on the network.

**Scheduling**

A global schedule defines the transmission sequence for all nodes
Each node maintains a local schedule with only its own transmission slots
Nodes synchronize based on reference frames sent by the master

**Synchronization**

The master sends reference frames at designated points in the schedule.
Upon receiving a reference frame, nodes synchronize their position in their schedule.
Upon receiving a reference frame, nodes recalculate their next time to transmit, and overwrite their relevant timer to trigger an interrupt in this amount of time.

#### Requirements

Each device must have a dedicated timer with interrupt capabilities
Devices must be able to set their timer to interrupt after a specified number of network time units
All devices must support extended CAN frames and share the same CAN bus







- TALK ABOUT THE HASH DEFINES
- TALK ABOUT THE STRUCT STYLE and give an example
- Explain system time units
- MODIFY READ AnD WRITE DATA TO ACTUALLY BE USED