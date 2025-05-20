#include <stdio.h>
#include "gttcan.h"

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
    write_value_fp_t write_value_fp
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
    gttcan->has_received = false;

    gttcan->rounds_without_shuffling_against_master = 0;
}

void gttcan_start(gttcan_t *gttcan)
{
    gttcan->has_received = false;
    gttcan->is_active = true;
    gttcan->local_schedule_index = 0;
    gttcan->is_time_master = false;
    gttcan->last_lowest_seen_node_id = gttcan->node_id;
    uint32_t start_up_wait_time = ((gttcan->global_schedule_length + (gttcan->node_id * DEFAULT_STARTUP_PAUSE_SLOTS)) * gttcan->slot_duration);
    gttcan->set_timer_int_callback_fp(start_up_wait_time);
}

void gttcan_transmit_next_frame(gttcan_t *gttcan)
{
    if (!gttcan->is_active)
    {
        return;
    }

    uint16_t slot_id = gttcan->local_schedule[gttcan->local_schedule_index].slot_id;
    uint16_t data_id = gttcan->local_schedule[gttcan->local_schedule_index].data_id;

    gttcan->local_schedule_index++;
    if (gttcan->local_schedule_index >= gttcan->local_schedule_length)
    {
        gttcan->local_schedule_index = 0;

        gttcan->is_time_master = (gttcan->last_lowest_seen_node_id == gttcan->current_lowest_seen_node_id) && (gttcan->current_lowest_seen_node_id == gttcan->node_id);

        gttcan->last_lowest_seen_node_id = gttcan->current_lowest_seen_node_id;
        gttcan->current_lowest_seen_node_id = 0;

        if (!gttcan->is_time_master)
        {
            gttcan->reached_end_of_my_schedule_prematurely = true;
        }
    }

    uint32_t time_to_next_transmission = gttcan_get_time_to_next_transmission(slot_id, gttcan);

    gttcan->set_timer_int_callback_fp(time_to_next_transmission);

    if (gttcan->has_received == false)
    {
        gttcan->is_time_master = true;
    }

    uint32_t ext_frame_header = ((uint32_t)slot_id << GTTCAN_NUM_DATA_ID_BITS) | data_id;
    uint64_t data_payload = ((uint64_t)gttcan->slot_duration << 16) | gttcan->node_id; // TODO: Reads real data, not dummy

    if (data_id != REFERENCE_FRAME_DATA_ID || gttcan->is_time_master)
    {
        gttcan->transmit_frame_callback_fp(ext_frame_header, data_payload); 
    }

    if (gttcan->node_id < gttcan->current_lowest_seen_node_id || gttcan->current_lowest_seen_node_id == 0)
    {
        gttcan->current_lowest_seen_node_id = gttcan->node_id;
    }
}

void gttcan_process_frame(gttcan_t *gttcan, uint32_t can_frame_id, uint64_t data)
{
    if (!gttcan->is_initialised)
    {
        return;
    }

    if (!gttcan->has_received)
    {
        gttcan->has_received = true;
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

    bool is_from_master = rx_node_id == gttcan->last_lowest_seen_node_id;

    if ((is_from_master || gttcan->rounds_without_shuffling_against_master > 2) &&
        slot_id > gttcan->local_schedule[gttcan->local_schedule_index].slot_id &&   // If received frame is after my next frame, AND
        gttcan->local_schedule_index > 0 &&                                         // I have transmitted, AND
        !gttcan->reached_end_of_my_schedule_prematurely                             // I haven't already wrapped in this round
    ) {
        gttcan->slot_duration_offset--; // I am slow, speed up
    }

    if ((is_from_master || gttcan->rounds_without_shuffling_against_master > 2) &&
        slot_id < gttcan->local_schedule[gttcan->local_schedule_index - 1].slot_id && // If received frame is before my previous, AND
        gttcan->local_schedule_index > 0 &&                                           // I have transmitted, AND
        !gttcan->reached_end_of_my_schedule_prematurely &&                            // I haven't already wrapped in this round, AND
        slot_id != 0                                                                  // received frame isn't at start of schedule
    ) {
        gttcan->slot_duration_offset++; // I am fast, slow down
    }

    if (!gttcan->is_active && slot_id == 0)
    {
        gttcan->is_active = true;
    }

    if (data_id == REFERENCE_FRAME_DATA_ID)
    {
        gttcan->reached_end_of_my_schedule_prematurely = false;
        if (slot_id == 0 && !gttcan->is_time_master)
        {
            if (gttcan->slot_duration_offset > 0)
            {
                gttcan->slot_duration++;

            }
            if (gttcan->slot_duration_offset < 0)
            {
                gttcan->slot_duration--;
            }
            if (gttcan->slot_duration_offset == 0 && gttcan->rounds_without_shuffling_against_master < 3){
                gttcan->rounds_without_shuffling_against_master++;
            }
        }
        gttcan->slot_duration_offset = 0;

        bool found_next_index = false;
        // Find the first local schedule entry where its slot_id > ref slot_id
        for (int i = 0; i < gttcan->local_schedule_length; i++)
        {
            if (gttcan->local_schedule[i].slot_id > slot_id)
            {
                if ((is_from_master || gttcan->rounds_without_shuffling_against_master > 2) &&
                    !gttcan->reached_end_of_my_schedule_prematurely &&
                    ((gttcan->local_schedule_index < i) ||              // (If I am behind schedule, OR
                    (i == 0 && gttcan->local_schedule_index))           // I didn't complete my schedule)
                ) {
                    gttcan->slot_duration_offset--; // speeding up
                }
                gttcan->local_schedule_index = i;

                found_next_index = true;
                break;
            }
        }

        if (!found_next_index)
        {
            // No slot is greater than the reference slot
            if ((is_from_master || gttcan->rounds_without_shuffling_against_master > 2) &&
                gttcan->local_schedule_index != 0 && !gttcan->reached_end_of_my_schedule_prematurely)
            {
                gttcan->slot_duration_offset--; // Needs a speedup, as I never got to transmit my final frame
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