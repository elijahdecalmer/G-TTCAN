#include <stdint.h>


/* GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH must fit into a uint8_t, so be less than or equal to 255 */ // is this a necessary restriction?
#ifndef GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH
#define GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH 32
#endif


/* REQUIREMENT
 GTTCAN_NUM_SLOT_ID_BITS + GTTCAN_NUM_DATA_ID_BITS <= 29
 This value limits the size of global schedule.
 13 bits gives a possible schedule length of 8192 */
#ifndef GTTCAN_NUM_SLOT_ID_BITS
#define GTTCAN_NUM_SLOT_ID_BITS 13
#endif

/* REQUIREMENT
 GTTCAN_NUM_SLOT_ID_BITS + GTTCAN_NUM_DATA_ID_BITS <= 29
 This value limits the number of whiteboard entries.
 16 bits gives a possible whiteboard length of 65536
 At 8 byte entries, this gives a whiteboard with total
 storage of 512kB.
 
 ????
 max 128kB whiteboard possible IF schedule length must be greater than whiteboard length???
 ie if a slot is needed to populate an entry into the whiteboard
 ????
 */
#ifndef GTTCAN_NUM_DATA_ID_BITS
#define GTTCAN_NUM_DATA_ID_BITS 16
#endif



// Consider making this into masked 29 bit CAN Frame IDs from start, but this struct is more readable
// I think NUM_SLOT_ID_BITS and NUM_DATA_ID_BITS could be up to the end user, but is there any point???
typedef struct local_schedule_entry_tag {
    uint16_t slot_id; // could be as large as GTTCAN_MAX_GLOBAL_SCHEDULE_LENGTH
    uint16_t data_id; // could be as large as the size of the whiteboard
} local_schedule_entry_t;

typedef struct gttcan_tag {
    uint8_t node_id;

    // Schedule related
    local_schedule_entry_t local_schedule[GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH];
    uint16_t global_schedule_length;
    uint8_t local_schedule_length;
    uint32_t slot_duration;






} gttcan_t;


