# G-TTCAN

A lightweight, time-triggered communication **protocol** for communication between microcontrollers on a CAN Bus.


This repo serves as a platform agnostic implementation of the protocol written in C with a focus on simplicity and low overhead.


#### Practical Overview (What does it look like at runtime)

In a (?regular?) standard CAN Bus implementation, nodes can transmit ad hoc, and priority based arbitration resolves communication conflicts (fact check this). In G-TTCAN, a predefined schedule determines for each slot in the schedule: which node transmits, and what they transmit.


To start the schedule, a Time Master must transmit a Reference Message. When this is received by other nodes on the bus, they will determine how far in the future their next turn to transmit is (slot length * their next position in the schedule). Each node will set an interrupt timer to trigger a transmission in that specified amount of time. When a node transmits, it will calculate the time until its next transmission and set the appropriate interrupt timer (even if it's next transmission is in the next round of the schedule). Once the end of the schedule is reached, the next message will be from the Time Master, and will be the Reference Message commencing the start of the next schedule round. Upon receiving this message, the other nodes discard any timers they are already running and set newly synchronised timers for their next position in the schedule, and so on.




#### Device Requirements

- List device requirements here


#### Review of Alternative Protocols

- Talk about TTCAN
- CanKingdom
- CanOpen
- CSP
- FlexRay
- CAN-TTP


## Technical Details and Setup
