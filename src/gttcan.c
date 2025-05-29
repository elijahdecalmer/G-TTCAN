#include <stdio.h>
#include "gttcan.h"

/**
 * @brief Initialize a G-TTCAN instance with configuration parameters and callbacks
 * 
 * This function must be called before any other G-TTCAN operations. It sets up
 * the node configuration, extracts the local schedule from the global schedule,
 * and registers all necessary callback functions for hardware interaction.
 * 
 * @param gttcan Pointer to an uninitialized gttcan_t structure to be used
 * @param node_id Unique node identifier (1-255) used for master election and scheduling (node id cannot be 0)
 * @param global_schedule_ptr Pointer to array of global_schedule_entry_t defining network-wide schedule
 * @param global_schedule_length Number of entries in the global schedule array
 * @param slot_duration Duration of each time slot in system time units
 * @param interrupt_timing_offset Time offset applied before transmission to compensate for processing delays.
 *          This should be equal to the time taken between two points A and B; A) the calling of process_frame() with a
 *          received reference frame, and B) the subsequent calling of set_timer_int_callback_fp (specifically the line
 *          that sets your interrupt timer).
 * @param transmit_frame_callback_fp Function pointer for CAN frame transmission (see transmit_frame_callback_fp_t)
 * @param set_timer_int_callback_fp Function pointer for timer interrupt setup (see set_timer_int_callback_fp_t)
 * @param read_value_fp Function pointer for reading data values (see read_value_fp_t)
 * @param write_value_fp Function pointer for writing received data (see write_value_fp_t)
 * @param dynamic_slot_duration_correction Enable automatic slot duration adjustment for
 *          timing corrections. Disable for more deterministic behavior, enable for dynamic adjustment
 *          to account for clock frequency variations between nodes, or over time.
 * 
 * @note Node ID must be unique across the network and cannot be 0.
 * @note The global_schedule_ptr must remain valid for the lifetime of the gttcan instance
 * @note All callback functions must be implemented and functional before calling gttcan_start()
 * @note The slot_duration must be set to a value that is suitable for the network and hardware
 *          capabilities, and must be larger than the time it takes for transmission of a can frame.
 *          Reccomended slot duration is AT LEAST 1.5 times the time it takes to transmit a CAN frame, 
 *          to allow for processing time and some margin for error.
 * 
 */
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
) {
    gttcan->is_active = false;
    gttcan->node_id = node_id;
    gttcan->global_schedule_length = global_schedule_length;
    gttcan->slot_duration = slot_duration;
    gttcan->local_schedule_index = 0;
    gttcan->interrupt_timing_offset = interrupt_timing_offset;

    gttcan->global_schedule_ptr = global_schedule_ptr;
    gttcan_get_local_schedule(gttcan, global_schedule_ptr);

    gttcan->transmit_frame_callback_fp = transmit_frame_callback_fp;
    gttcan->set_timer_int_callback_fp = set_timer_int_callback_fp;
    gttcan->read_value_fp = read_value_fp;
    gttcan->write_value_fp = write_value_fp;

    gttcan->is_initialised = true;

    gttcan->slot_duration_offset = 0;
    gttcan->reached_end_of_my_schedule_prematurely = false;

    gttcan->last_lowest_seen_node_id = 0;
    gttcan->current_lowest_seen_node_id = 0;

    gttcan->rounds_without_shuffling_against_master = 0;
    gttcan->dynamic_slot_duration_correction = dynamic_slot_duration_correction;
}

/**
 * @brief Start G-TTCAN protocol operation and begin network participation
 * 
 * Activates the G-TTCAN instance and initiates network communication. The node
 * will wait for a startup delay period before beginning transmission to prevent
 * simultaneous network entry by multiple nodes. The node will either join into the
 * schedule, or become the time master and transmit if it is the only node in the network.
 * 
 * @param gttcan Pointer to initialized gttcan_t structure
 * 
 * @note gttcan_init() must be called successfully before this function
 * @note Startup delay is calculated as:
 *          (global_schedule_length + (node_id * DEFAULT_STARTUP_PAUSE_SLOTS)) * slot_duration
 *          to stagger the start of the node's transmission
 * @note After startup delay, the node will begin transmitting according to its local schedule
 */
void gttcan_start(gttcan_t *gttcan)
{
    gttcan->is_active = true;
    gttcan->local_schedule_index = 0;
    gttcan->is_time_master = false;
    gttcan->last_lowest_seen_node_id = gttcan->node_id;
    uint32_t start_up_wait_time = ((gttcan->global_schedule_length + (gttcan->node_id * DEFAULT_STARTUP_PAUSE_SLOTS)) * gttcan->slot_duration);
    gttcan->set_timer_int_callback_fp(start_up_wait_time);
}

/**
 * @brief Transmit the next scheduled frame and configure timing for subsequent transmission
 * 
 * This function should be called from the timer interrupt handler. It handles
 * master election logic, constructs and transmits the next frame in the local
 * schedule, and sets up the timer for the following transmission.
 * 
 * @param gttcan Pointer to active gttcan_t structure
 * 
 * @note Must be called from timer interrupt context
 * @note Automatically constructs extended CAN frame header from slot_id and data_id from schedule
 * @note Data payload is retrieved by calling read_value_fp with the data_id from the schedule
 * @note Reference frames are only transmitted by the current time master
 * @note Updates master election state and schedules next transmission via timer callback
 */
void gttcan_transmit_next_frame(gttcan_t *gttcan)
{
    if (!gttcan->is_active)
    {
        return;
    }

    uint16_t slot_id = gttcan->local_schedule[gttcan->local_schedule_index].slot_id;
    uint16_t data_id = gttcan->local_schedule[gttcan->local_schedule_index].data_id;

    if (gttcan->local_schedule_index == 0){
        gttcan->is_time_master = (gttcan->last_lowest_seen_node_id == gttcan->current_lowest_seen_node_id) && (gttcan->current_lowest_seen_node_id == gttcan->node_id);
        gttcan->last_lowest_seen_node_id = gttcan->current_lowest_seen_node_id;
        gttcan->current_lowest_seen_node_id = 0;
    }

    gttcan->local_schedule_index++;
    if (gttcan->local_schedule_index >= gttcan->local_schedule_length)
    {
        gttcan->local_schedule_index = 0;

        if (!gttcan->is_time_master)
        {
            gttcan->reached_end_of_my_schedule_prematurely = true;
        }
    }

    uint32_t time_to_next_transmission = gttcan_get_time_to_next_transmission(slot_id, gttcan);

    gttcan->set_timer_int_callback_fp(time_to_next_transmission);

    uint32_t ext_frame_header = ((uint32_t)slot_id << GTTCAN_NUM_DATA_ID_BITS) | data_id;

    int ISTIMEMASTER;
    if (gttcan->is_time_master){
        ISTIMEMASTER = 1;
    } else {
        ISTIMEMASTER = 3;
    }

    uint64_t data_payload = gttcan->read_value_fp(data_id);

    if (data_id != REFERENCE_FRAME_DATA_ID || gttcan->is_time_master)
    {
        gttcan->transmit_frame_callback_fp(ext_frame_header, data_payload); 
    }

    if (gttcan->node_id < gttcan->current_lowest_seen_node_id || gttcan->current_lowest_seen_node_id == 0)
    {
        gttcan->current_lowest_seen_node_id = gttcan->node_id;
    }
}

/**
 * @brief Process received CAN frames for synchronization and data handling
 * 
 * Handles all incoming CAN frames, performing synchronization with reference frames
 * and processing data frames. Updates timing corrections, master election status,
 * and stores received data via the write_value callback.
 * 
 * @param gttcan Pointer to gttcan_t structure
 * @param can_frame_id 29-bit extended CAN frame identifier containing slot_id and data_id from the received CAN frame
 * @param data 64-bit data payload from the received CAN frame
 * 
 * @note Should be called for every received CAN frame on the bus
 * @note Reference frames (data_id == REFERENCE_FRAME_DATA_ID) trigger schedule synchronization
 * @note Data frames are passed to write_value_fp callback for application processing
 * @note Implements dynamic timing correction based on received frame timing (if dynamic_slot_duration_correction is enabled)
 * @note Updates master election by tracking lowest node IDs seen in consecutive rounds
 * @note If the node is not initialized, this function does nothing. This is to prevent 
 *          processing frames before the G-TTCAN instance is fully set up, if the interrupt handler fires before gttcan_start() is called.
 */
void gttcan_process_frame(gttcan_t *gttcan, uint32_t can_frame_id, uint64_t data)
{
    if (!gttcan->is_initialised)
    {
        return;
    }

    uint16_t slot_id = can_frame_id >> GTTCAN_NUM_DATA_ID_BITS;
    uint16_t data_id = can_frame_id & 0xFFFF;

    uint8_t rx_node_id = 0;
    for (int i = 0; i < gttcan->global_schedule_length; i++)
    {
        if (gttcan->global_schedule_ptr[i].slot_id == slot_id)
        {
            rx_node_id = gttcan->global_schedule_ptr[i].node_id;
            break;
        }
    }

    bool is_from_master = (rx_node_id == gttcan->last_lowest_seen_node_id) && (rx_node_id == gttcan->current_lowest_seen_node_id) && (gttcan->last_lowest_seen_node_id != 0);

    if ((is_from_master || (gttcan->rounds_without_shuffling_against_master >= NUM_ROUNDS_BEFORE_SWITCHING_TO_ALL_NODE_ADJUST)) &&
        slot_id > gttcan->local_schedule[gttcan->local_schedule_index].slot_id &&   // If received frame is after my next frame, AND
        gttcan->local_schedule_index > 0 &&                                         // I have transmitted, AND
        !gttcan->reached_end_of_my_schedule_prematurely                             // I haven't already wrapped in this round
    ) {
        gttcan->slot_duration_offset--; // I am slow, speed up
        if (is_from_master){
            gttcan->rounds_without_shuffling_against_master = 0;
        }
    }

    if ((is_from_master || (gttcan->rounds_without_shuffling_against_master >= NUM_ROUNDS_BEFORE_SWITCHING_TO_ALL_NODE_ADJUST)) &&
        slot_id < gttcan->local_schedule[gttcan->local_schedule_index - 1].slot_id && // If received frame is before my previous, AND
        gttcan->local_schedule_index > 0 &&                                           // I have transmitted, AND
        !gttcan->reached_end_of_my_schedule_prematurely &&                            // I haven't already wrapped in this round, AND
        slot_id != 0                                                                  // received frame isn't at start of schedule
    ) {
        gttcan->slot_duration_offset++; // I am fast, slow down
        if (is_from_master){
            gttcan->rounds_without_shuffling_against_master = 0;
        }
    }

    if (!gttcan->is_active && slot_id == 0)
    {
        gttcan->is_active = true;
    }

    if (data_id == REFERENCE_FRAME_DATA_ID)
    {
        
        if (slot_id == 0 && !gttcan->is_time_master)
        {
            if (gttcan->dynamic_slot_duration_correction && gttcan->slot_duration_offset > 0)
            {
                gttcan->slot_duration++;

            }
            if (gttcan->dynamic_slot_duration_correction && gttcan->slot_duration_offset < 0)
            {
                gttcan->slot_duration--;
            }
            if (
                gttcan->slot_duration_offset == 0 && 
                gttcan->rounds_without_shuffling_against_master < NUM_ROUNDS_BEFORE_SWITCHING_TO_ALL_NODE_ADJUST){
                gttcan->rounds_without_shuffling_against_master++;
            }
            gttcan->slot_duration_offset = 0;
            gttcan->reached_end_of_my_schedule_prematurely = false;

        }

        bool found_next_index = false;
        // Find the first local schedule entry where its slot_id > ref slot_id
        for (int i = 0; i < gttcan->local_schedule_length; i++)
        {
            if (gttcan->local_schedule[i].slot_id > slot_id)
            {
                if ((is_from_master || (gttcan->rounds_without_shuffling_against_master >= NUM_ROUNDS_BEFORE_SWITCHING_TO_ALL_NODE_ADJUST)) &&
                    !gttcan->reached_end_of_my_schedule_prematurely &&
                    ((gttcan->local_schedule_index < i) ||              // (If I am behind schedule, OR
                    (i == 0 && gttcan->local_schedule_index))           // I didn't complete my schedule)
                ) {
                    gttcan->slot_duration_offset--; // speeding up
                    if (is_from_master){
                        gttcan->rounds_without_shuffling_against_master = 0;
                    }
                }
                gttcan->local_schedule_index = i;

                found_next_index = true;
                break;
            }
        }

        if (!found_next_index)
        {
            // No slot is greater than the reference slot
            if ((is_from_master || (gttcan->rounds_without_shuffling_against_master >= NUM_ROUNDS_BEFORE_SWITCHING_TO_ALL_NODE_ADJUST)) &&
                gttcan->local_schedule_index != 0 && !gttcan->reached_end_of_my_schedule_prematurely)
            {
                gttcan->slot_duration_offset--; // Needs a speedup, as I never got to transmit my final frame
                if (is_from_master){
                    gttcan->rounds_without_shuffling_against_master = 0;
                }
            }
            gttcan->local_schedule_index = 0;
        }

        uint32_t time_to_next_transmission = gttcan_get_time_to_next_transmission(slot_id, gttcan);
        gttcan->set_timer_int_callback_fp(time_to_next_transmission);
    }
    else
    {
        gttcan->write_value_fp(data_id, data);
    }

    // Here onwards is for determining master


    if ((rx_node_id < gttcan->current_lowest_seen_node_id || gttcan->current_lowest_seen_node_id == 0))
    {
        gttcan->current_lowest_seen_node_id = rx_node_id;
    }
}

/**
 * @brief Extract node-specific schedule entries from the global schedule
 * 
 * Creates a local schedule containing only the transmission slots assigned to
 * this node plus any reference frame slots. This reduces memory usage and
 * simplifies schedule traversal during operation.
 * 
 * @param gttcan Pointer to gttcan_t structure to populate with local schedule
 * @param global_schedule_ptr Pointer to the complete global schedule array
 * 
 * @note Called automatically during gttcan_init()
 * @note Local schedule includes slots where node_id matches or data_id is REFERENCE_FRAME_DATA_ID
 * @note Populates gttcan->local_schedule array and sets gttcan->local_schedule_length
 * @note Local schedule entries maintain original slot_id values for timing calculations
 */
void gttcan_get_local_schedule(gttcan_t *gttcan, global_schedule_ptr_t global_schedule_ptr)
{
    uint16_t local_schedule_index = 0;
    for (int i = 0; i < gttcan->global_schedule_length; i++)
    {
        if (global_schedule_ptr[i].node_id == gttcan->node_id || global_schedule_ptr[i].data_id == REFERENCE_FRAME_DATA_ID)
        {
            gttcan->local_schedule[local_schedule_index].slot_id = global_schedule_ptr[i].slot_id;
            gttcan->local_schedule[local_schedule_index].data_id = global_schedule_ptr[i].data_id;
            local_schedule_index++;
        }
    }
    gttcan->local_schedule_length = local_schedule_index;
}

/**
 * @brief Calculate number of schedule slots between two positions with wraparound handling
 * 
 * Computes the shortest path from current slot to next slot, accounting for
 * schedule wraparound when the next slot has a lower ID than the current slot.
 * 
 * @param current_slot_id Current position in the global schedule
 * @param next_slot_id Target slot position in the global schedule  
 * @param global_schedule_length Total number of slots in the global schedule
 * 
 * @return Number of slots to advance from current to next position
 * 
 * @note Handles schedule wraparound (e.g., slot 10 to slot 2 in 12-slot schedule = 4 slots)
 * @note Used internally for timing calculations
 */
uint16_t gttcan_get_number_of_slots_to_next(uint16_t current_slot_id, uint16_t next_slot_id, uint16_t global_schedule_length)
{
    if (current_slot_id < next_slot_id)
    {
        return next_slot_id - current_slot_id;
    }
    else
    {
        return global_schedule_length - current_slot_id + next_slot_id;
    }
}

/**
 * @brief Calculate time delay until next scheduled transmission
 * 
 * Determines the exact time delay needed before the next transmission interrupt
 * should occur, accounting for slot duration, interrupt timing offset, and
 * schedule position.
 * 
 * @param current_slot_id Current slot position in the schedule (reference point)
 * @param gttcan Pointer to gttcan_t structure containing timing configuration
 * 
 * @return Time delay in system time units until next transmission should occur
 * 
 * @note Accounts for interrupt_timing_offset to compensate for processing delays
 * @note Returns minimum delay of 1 time unit if calculated delay would be too small
 * @note Used internally by gttcan_transmit_next_frame() and gttcan_process_frame()
 * @note Time calculation: (slots_to_next * slot_duration) - interrupt_timing_offset
 */
uint32_t gttcan_get_time_to_next_transmission(uint16_t current_slot_id, gttcan_t *gttcan)
{
    uint16_t next_slot_id = gttcan->local_schedule[gttcan->local_schedule_index].slot_id;
    uint16_t number_of_slots_to_next = gttcan_get_number_of_slots_to_next(current_slot_id, next_slot_id, gttcan->global_schedule_length);

    uint32_t time_to_next_transmission = (uint32_t)number_of_slots_to_next * gttcan->slot_duration;

    if (time_to_next_transmission > gttcan->interrupt_timing_offset)
    {
        return time_to_next_transmission - gttcan->interrupt_timing_offset;
    }
    else
    {
        return 1;
    }
}