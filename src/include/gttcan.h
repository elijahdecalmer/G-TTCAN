
#ifndef GTTCAN_H
#define GTTCAN_H

#include <stdint.h>
#include <stdbool.h>

/* GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH must fit into a uint8_t, so be less than or equal to 255 */ // is this a necessary restriction?
#ifndef GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH
#define GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH 512
#endif

/* REQUIREMENT
 GTTCAN_NUM_SLOT_ID_BITS + GTTCAN_NUM_DATA_ID_BITS <= 29
 This value limits the size of global schedule.
*/
#ifndef GTTCAN_NUM_SLOT_ID_BITS
#define GTTCAN_NUM_SLOT_ID_BITS 13
#endif

/* REQUIREMENT
 GTTCAN_NUM_SLOT_ID_BITS + GTTCAN_NUM_DATA_ID_BITS <= 29
 This value limits the number of whiteboard entries.
 */
#ifndef GTTCAN_NUM_DATA_ID_BITS
#define GTTCAN_NUM_DATA_ID_BITS 16
#endif

#define REFERENCE_FRAME_DATA_ID 0 // ifndef?
#define GENERIC_DATA_ID 1         // ifndef?
#define DEFAULT_STARTUP_PAUSE_SLOTS 2

// Consider making this into masked 29 bit CAN Frame IDs from start, but this struct is more readable
// I think NUM_SLOT_ID_BITS and NUM_DATA_ID_BITS could be up to the end user, but is there any point???
typedef struct local_schedule_entry_tag
{                         // According to claude has no memory overhead vs a uint32_t, but will be one more operation when it comes time to transmit a frame
    uint16_t slot_id;     // could be as large as GTTCAN_MAX_GLOBAL_SCHEDULE_LENGTH
    uint16_t data_id;     // could be as large as the size of the whiteboard //TODO:?
} local_schedule_entry_t; // THIS SHOULD BE USED when it needs to be used in a user defined function

#define MAX_GLOBAL_SCHEDULE_LENGTH 512 // check if number is ok and ifndef

typedef struct global_schedule_entry
{
    uint8_t node_id;
    uint16_t slot_id;
    uint16_t data_id; // didn't use local_schedule_entry_tag didn't want to over-complicate it
} global_schedule_entry_t;

typedef global_schedule_entry_t *global_schedule_ptr_t;

typedef void (*transmit_frame_callback_fp_t)(uint32_t, uint64_t);
typedef void (*set_timer_int_callback_fp_t)(uint32_t);
typedef uint64_t (*read_value_fp_t)(uint16_t);
typedef void (*write_value_fp_t)(uint16_t, uint64_t);
typedef uint32_t (*get_schedule_transmission_time_fp_t)(void);

typedef struct gttcan_tag
{
    uint8_t node_id;
    bool isActive;
    bool is_initialised;
    // Schedule related
    local_schedule_entry_t local_schedule[GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH];
    global_schedule_ptr_t global_schedule_ptr; //could keep a pointer in the future if we need to use global_schedule, i.e. fault tolerance or if global_schedule needs to be dynamically changed?
    uint16_t global_schedule_length;
    uint16_t local_schedule_length;
    uint16_t local_schedule_index;
    uint32_t slot_duration;
    uint32_t timing_offset;
    int slot_duration_offset;

    // Callback functions
    transmit_frame_callback_fp_t transmit_frame_callback_fp;
    set_timer_int_callback_fp_t set_timer_int_callback_fp;
    read_value_fp_t read_value_fp;
    write_value_fp_t write_value_fp;
    get_schedule_transmission_time_fp_t get_schedule_transmission_time_fp;

    // Timing related
    uint32_t last_schedule_transmission_time;
    bool has_adjusted_slots_this_round;

    bool reached_end_of_my_schedule_prematurely;


    // Cascading master
    uint8_t last_lowest_seen_node_id;
    uint8_t current_lowest_seen_node_id;
    bool isTimeMaster;
    bool has_received;

} gttcan_t;

void gttcan_init(
    gttcan_t *gttcan,
    uint8_t node_id,
    global_schedule_ptr_t global_schedule_ptr,
    uint16_t global_schedule_length,
    uint32_t slot_duration,
    uint32_t timing_offset,
    transmit_frame_callback_fp_t transmit_frame_callback_fp,
    set_timer_int_callback_fp_t set_timer_int_callback_fp,
    read_value_fp_t read_value_fp,
    write_value_fp_t write_value_fp,
    get_schedule_transmission_time_fp_t get_schedule_transmission_time_fp
);

void gttcan_start(gttcan_t *gttcan);

void gttcan_transmit_next_frame(gttcan_t *gttcan);

void gttcan_get_local_schedule(gttcan_t *gttcan, global_schedule_ptr_t global_schedule_ptr);

uint16_t gttcan_get_number_of_slots_to_next(uint16_t current_slot_id, uint16_t next_slot_id, uint16_t global_schedule_length);

uint32_t gttcan_get_time_to_next_transmission(uint16_t current_slot_id, gttcan_t *gttcan);

void gttcan_process_frame(gttcan_t *gttcan, uint32_t can_frame_id, uint64_t data);

void gttcan_accumulate_hardware_time(gttcan_t *gttcan, uint32_t hardware_time);

void gttcan_reset_hardware_time(gttcan_t *gttcan);

#endif
