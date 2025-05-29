
#ifndef GTTCAN_H
#define GTTCAN_H

#include <stdint.h>
#include <stdbool.h>

/* GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH must fit into a uint8_t, so be less than or equal to 255 */ // is this a necessary restriction?
#ifndef GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH
#define GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH 512
#endif

#ifndef MAX_GLOBAL_SCHEDULE_LENGTH
#define MAX_GLOBAL_SCHEDULE_LENGTH 512
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

#ifndef REFERENCE_FRAME_DATA_ID
#define REFERENCE_FRAME_DATA_ID 0
#endif

#ifndef GENERIC_DATA_ID
#define GENERIC_DATA_ID 1
#endif

#ifndef DEFAULT_STARTUP_PAUSE_SLOTS
#define DEFAULT_STARTUP_PAUSE_SLOTS 2
#endif

#ifndef NUM_ROUNDS_BEFORE_SWITCHING_TO_ALL_NODE_ADJUST
#define NUM_ROUNDS_BEFORE_SWITCHING_TO_ALL_NODE_ADJUST 2
#endif

typedef struct local_schedule_entry_tag
{
    uint16_t slot_id;
    uint16_t data_id;
} local_schedule_entry_t;

typedef struct global_schedule_entry
{
    uint8_t node_id;
    uint16_t slot_id;
    uint16_t data_id;
} global_schedule_entry_t;

typedef global_schedule_entry_t *global_schedule_ptr_t;

typedef void (*transmit_frame_callback_fp_t)(uint32_t, uint64_t);
typedef void (*set_timer_int_callback_fp_t)(uint32_t);
typedef uint64_t (*read_value_fp_t)(uint16_t);
typedef void (*write_value_fp_t)(uint16_t, uint64_t);

typedef struct gttcan_tag
{
    // Node related
    uint8_t node_id;
    bool is_active;
    bool is_initialised;
    uint32_t slot_duration;
    uint32_t interrupt_timing_offset;

    // Schedule related
    local_schedule_entry_t local_schedule[GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH];
    global_schedule_ptr_t global_schedule_ptr;
    uint16_t global_schedule_length;
    uint16_t local_schedule_length;
    uint16_t local_schedule_index;

    // Callback functions
    transmit_frame_callback_fp_t transmit_frame_callback_fp;
    set_timer_int_callback_fp_t set_timer_int_callback_fp;
    read_value_fp_t read_value_fp;
    write_value_fp_t write_value_fp;

    // Shuffle correction
    bool dynamic_slot_duration_correction;
    bool reached_end_of_my_schedule_prematurely;
    int slot_duration_offset;
    int rounds_without_shuffling_against_master;

    // Cascading master
    uint8_t last_lowest_seen_node_id;
    uint8_t current_lowest_seen_node_id;
    bool is_time_master;

} gttcan_t;

void gttcan_init(
    gttcan_t *gttcan,
    uint8_t node_id,
    global_schedule_ptr_t global_schedule_ptr,
    uint16_t global_schedule_length,
    uint32_t slot_duration,
    uint32_t interrupt_timing_offset,
    transmit_frame_callback_fp_t transmit_frame_callback_fp,
    set_timer_int_callback_fp_t set_timer_int_callback_fp,
    read_value_fp_t read_value_fp,
    write_value_fp_t write_value_fp,
    bool dynamic_slot_duration_correction
);

void gttcan_start(gttcan_t *gttcan);

void gttcan_transmit_next_frame(gttcan_t *gttcan);

void gttcan_get_local_schedule(gttcan_t *gttcan, global_schedule_ptr_t global_schedule_ptr);

uint16_t gttcan_get_number_of_slots_to_next(uint16_t current_slot_id, uint16_t next_slot_id, uint16_t global_schedule_length);

uint32_t gttcan_get_time_to_next_transmission(uint16_t current_slot_id, gttcan_t *gttcan);

void gttcan_process_frame(gttcan_t *gttcan, uint32_t can_frame_id, uint64_t data);

#endif
