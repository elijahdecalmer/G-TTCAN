#include <stdio.h>
#include "gttcan.h"
#include <stdbool.h>

// GTTCAN INIT
// This function should initialise the local schedule

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
    get_schedule_transmission_time_fp_t get_schedule_transmission_time_fp)
{
    gttcan->isActive = false;
    gttcan->node_id = node_id;
    gttcan->global_schedule_length = global_schedule_length;
    gttcan->slot_duration = slot_duration;
    gttcan->local_schedule_index = 0;
    gttcan->timing_offset = timing_offset;
    // TODO assigned lastframe_on_bus?!?!?!?1
    gttcan->total_nodes = 0;
    gttcan->global_schedule_ptr = global_schedule_ptr;
    gttcan_get_local_schedule(gttcan, global_schedule_ptr);

    gttcan->transmit_frame_callback_fp = transmit_frame_callback_fp;
    gttcan->set_timer_int_callback_fp = set_timer_int_callback_fp;
    gttcan->read_value_fp = read_value_fp;
    gttcan->write_value_fp = write_value_fp;
    // gttcan->get_schedule_transmission_time_fp = get_schedule_transmission_time_fp;

    // gttcan->hardware_time = 0;
    // // gttcan->last_reference_frame_hardware_time = 0;

    gttcan->current_time_master_node_id = 0;
    gttcan->isTimeMaster = false;
    gttcan->last_schedule_transmission_time = 0;
    gttcan->is_initialised = true;
    gttcan->reached_start_of_schedule = false;
    gttcan->time_master_transmitted_last_round = false;
    gttcan->declaration_sent = false;

}


void gttcan_start(
    gttcan_t *gttcan)
{
    gttcan->local_schedule_index = 0;
    uint32_t startup_wait_time = ((gttcan->global_schedule_length + (gttcan->node_id * DEFAULT_STARTUP_PAUSE_SLOTS)) * gttcan->slot_duration); // Wait one full cycle
    // Waits for n rounds of schedule to confirm no higher priority time masters exist.
    gttcan->isTimeMaster = true; // This will be off as soon as it receives a higher priority reserved reference slot. // Will be active once it receives a frame or wait time is over

    gttcan->set_timer_int_callback_fp(startup_wait_time); // could change to single round for all + next_transmission if error allows
}

void gttcan_transmit_next_frame(gttcan_t *gttcan)
{
    if (!gttcan->is_initialised || gttcan->local_schedule_length == 0) {
        // Set timer to a long duration or handle error, then return
        // For simplicity, just returning here. A robust system might set a long poll timer.
        return;
    }

    if (!gttcan->isActive) {
        gttcan->isActive = true;
    }

    local_schedule_entry_t current_ls_entry = gttcan->local_schedule[gttcan->local_schedule_index];
    uint16_t current_slot_id = current_ls_entry.slot_id; 
    uint16_t current_data_id = current_ls_entry.data_id; 

    if(current_slot_id == gttcan->local_schedule[0].slot_id){
        gttcan->reached_start_of_schedule = true;
        gttcan->time_master_transmitted_last_round = false;
    }

    bool should_transmit = false;
    uint64_t payload_data = 0;
    payload_data = ((uint64_t)gttcan->current_time_master_node_id << 16) | gttcan->node_id; 

    // --- Determine if this node should transmit this current_ls_entry ---
    if (current_data_id != REFERENCE_FRAME_DATA_ID) {
        // It's a DATA frame.
        should_transmit = true;

    }else {
        // It's a REFERENCE frame.
        bool is_declaration_slot_range = (current_slot_id < gttcan->total_nodes);

        if (is_declaration_slot_range) {
            // This is a REF frame in a declaration slot.
            // If it's in my local_schedule (per revised gttcan_get_local_schedule), it must be MY declaration.
            if (!gttcan->declaration_sent) {
                // only once per cycle…
                if (gttcan->current_time_master_node_id == 0 ||
                    gttcan->current_time_master_node_id > gttcan->node_id) {
                    gttcan->current_time_master_node_id = gttcan->node_id;
                }
                should_transmit = true;
                gttcan->declaration_sent = true;
            }
        } else {
            // This is a SUBSEQUENT reference frame (after declaration slots).
            // Transmit it ONLY if I am the current Time Master.
            if (gttcan->current_time_master_node_id == gttcan->node_id) {
                should_transmit = true;
            }
        }
    }

    // --- CAN ID Construction & Transmission ---
    uint32_t ext_frame_header = ((uint32_t)current_slot_id << GTTCAN_NUM_DATA_ID_BITS) | current_data_id;

    

    // --- Advance schedule index and set timer for the NEXT event REGARDLESS of transmission ---
    gttcan->local_schedule_index++;
    if (gttcan->local_schedule_index >= gttcan->local_schedule_length) {
        gttcan->local_schedule_index = 0;
        gttcan->current_time_master_node_id = 0; // Reset for next round's election
        gttcan->isTimeMaster = false;
        gttcan->declaration_sent = false;
    }

    // If local_schedule_length is > 0, there's always a next item (could be wrapped)
    // The current_slot_id used here is from the frame just processed (or skipped transmission for).
    // The time to next transmission is calculated from *this* slot to the *next* in the local schedule.
    uint32_t time_to_next_transmission = gttcan_get_time_to_next_transmission(current_slot_id, gttcan);
    gttcan->set_timer_int_callback_fp(time_to_next_transmission);

    if (should_transmit) {
        gttcan->transmit_frame_callback_fp(ext_frame_header, payload_data);
    }
}


void gttcan_process_frame(gttcan_t *gttcan, uint32_t can_frame_id, uint64_t data)
{
    if (!gttcan->is_initialised) {
        return;
    }

    uint32_t data_id_mask = (1U << GTTCAN_NUM_DATA_ID_BITS) - 1;
    uint16_t received_slot_id = (can_frame_id >> GTTCAN_NUM_DATA_ID_BITS);
    uint16_t received_data_id = can_frame_id & data_id_mask;

    // Extract the sender node ID from the data field
    uint8_t sender_node_id = (uint8_t)(data & 0xFF);
    uint8_t time_master_node_id = (uint8_t)((data >> 16) & 0xFF);

    // --- Master Election Phase (when a declaration frame is received) ---
    bool is_declaration_slot_range = (received_slot_id < gttcan->total_nodes);

    if (received_data_id == REFERENCE_FRAME_DATA_ID) {
        // Handle any reference frame (declaration or regular)
        
        if (is_declaration_slot_range) {
            // This is a declaration frame - strict enforcement of lowest node ID wins
            if (sender_node_id < gttcan->current_time_master_node_id || gttcan->current_time_master_node_id == 0) {
                gttcan->current_time_master_node_id = sender_node_id;
                gttcan->isTimeMaster = (sender_node_id == gttcan->node_id);
            }
        } else {
            // Regular reference frame - confirm the time master
            if (sender_node_id == time_master_node_id && 
                time_master_node_id != 0) {
                gttcan->current_time_master_node_id = time_master_node_id;
                gttcan->isTimeMaster = (time_master_node_id == gttcan->node_id);
            }
        }

        // --- Synchronization to Time Master's Reference Frames ---
        // Only synchronize to the actual elected time master
        if (sender_node_id == gttcan->current_time_master_node_id) {
            if (!gttcan->isActive) {
                gttcan->isActive = true;
            }

            // Resynchronize local_schedule_index
            int found_next_local_schedule_item = -1;
            for (int i = 0; i < gttcan->local_schedule_length; i++) {
                if (gttcan->local_schedule[i].slot_id > received_slot_id) {
                    gttcan->local_schedule_index = i;
                    found_next_local_schedule_item = 1;
                    break;
                }
            }
            if (found_next_local_schedule_item < 0) {
                gttcan->local_schedule_index = 0; // Wrap around
            }

            // Calculate time to next transmission
            uint32_t time_to_next_transmission = gttcan_get_time_to_next_transmission(received_slot_id, gttcan);
            gttcan->set_timer_int_callback_fp(time_to_next_transmission);
        }
    } else if (received_data_id != REFERENCE_FRAME_DATA_ID) {
        // Regular data frame processing
        if (gttcan->isActive) {
            gttcan->write_value_fp(received_data_id, data);
        }
    }
}

void gttcan_get_local_schedule(gttcan_t *gttcan, global_schedule_ptr_t global_schedule_ptr)
{
    uint8_t highest_node_id = 0;
    uint16_t current_local_schedule_length = 0;

    for (int i = 0; i < gttcan->global_schedule_length; i++) {
        if (global_schedule_ptr[i].node_id > highest_node_id) {
            highest_node_id = global_schedule_ptr[i].node_id;
        }

        global_schedule_entry_t current_global_entry = global_schedule_ptr[i];
        bool add_to_local = false;

        // Condition 1: Is it this node's OWN frame (declaration or data)?
        if (current_global_entry.node_id == gttcan->node_id) {
            add_to_local = true;
        }
        // Condition 2: Is it a SUBSEQUENT reference frame (globally Node 1's, after declaration phase)?
        // These are added to every node's local schedule as potential transmission candidates if they become master.
        else if (current_global_entry.node_id == 1 && // Originally Node 1's
                 current_global_entry.data_id == REFERENCE_FRAME_DATA_ID &&
                 current_global_entry.slot_id >= gttcan->node_id) { // After declaration slots
            add_to_local = true;
        }
        // Note: Node 1's declaration frame (slot_id < total_nodes) is NOT added to other nodes'

        if (add_to_local) {
            if (current_local_schedule_length < GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH) {
                gttcan->local_schedule[current_local_schedule_length].slot_id = current_global_entry.slot_id;
                gttcan->local_schedule[current_local_schedule_length].data_id = current_global_entry.data_id;
                current_local_schedule_length++;
            }
        }
    }
    gttcan->total_nodes = highest_node_id;
    gttcan->local_schedule_length = current_local_schedule_length;
}

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

uint32_t gttcan_get_time_to_next_transmission(uint16_t current_slot_id, gttcan_t *gttcan)
{
    uint16_t next_slot_id = gttcan->local_schedule[gttcan->local_schedule_index].slot_id;
    uint16_t number_of_slots_to_next = gttcan_get_number_of_slots_to_next(current_slot_id, next_slot_id, gttcan->global_schedule_length);

    uint32_t time = (uint32_t)number_of_slots_to_next * gttcan->slot_duration;

    if (time > gttcan->timing_offset)
    {
        return time - gttcan->timing_offset;
    }
    else
    {
        return 1;
    }
}

// void gttcan_accumulate_hardware_time(gttcan_t *gttcan, uint32_t hardware_time)
// {
//     gttcan->hardware_time += hardware_time;
// }

// // void gttcan_reset_hardware_time(gttcan_t *gttcan)
// {
// //     gttcan->hardware_time = 0;
// // }
