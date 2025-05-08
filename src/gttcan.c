#include <stdio.h>
#include "gttcan.h"

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

    gttcan_get_local_schedule(gttcan, global_schedule_ptr);

    gttcan->transmit_frame_callback_fp = transmit_frame_callback_fp;
    gttcan->set_timer_int_callback_fp = set_timer_int_callback_fp;
    gttcan->read_value_fp = read_value_fp;
    gttcan->write_value_fp = write_value_fp;
    gttcan->get_schedule_transmission_time_fp = get_schedule_transmission_time_fp;

    // gttcan->hardware_time = 0;
    // gttcan->last_reference_frame_hardware_time = 0;
    gttcan->last_schedule_transmission_time = 0;
    gttcan->is_initialised = true;
}

// Transmit frame (blink a blinky LED )
// increment local_schedule_index
// look at my schedule, set time to next transmit
// typedef void (*transmit_frame_callback_fp)(uint32_t, uint64_t);

// SetTimerCallback
// typedef void (*set_timer_int_callback_fp)(uint32_t);
// REMOVE CONTEXT PTR FROM params
// borrow the implementation of this function

// GTTCAN Start (master only function?)
// start schedule, call transmitFrame (blink an LED) to symbolise sending a reference frame
void gttcan_start(
    gttcan_t *gttcan)
{
    gttcan->isActive = true;
    gttcan->local_schedule_index = 0;
    gttcan_transmit_next_frame(gttcan);
}

void gttcan_transmit_next_frame(gttcan_t *gttcan)
{
    if (!gttcan->isActive)
    {
        return;
    }

    uint16_t slot_id = gttcan->local_schedule[gttcan->local_schedule_index].slot_id;
    uint16_t data_id = gttcan->local_schedule[gttcan->local_schedule_index].data_id;

    gttcan->local_schedule_index++;
    if (gttcan->local_schedule_index >= gttcan->local_schedule_length)
    { // TODO MAKE THIS A == ONCE THAT CAN BE ENSURED IS SAFE
        gttcan->local_schedule_index = 0;
    }

    uint32_t time_to_next_transmission = gttcan_get_time_to_next_transmission(slot_id, gttcan);

    gttcan->set_timer_int_callback_fp(time_to_next_transmission);

    uint32_t ext_frame_header;
    ext_frame_header = ((uint32_t)slot_id << GTTCAN_NUM_DATA_ID_BITS) | data_id; // TODO CHECK THE SAFETY OF THIS, should DATA_ID BE ANDED WITH A MASK OF LENGTH DATA_ID????
    gttcan->transmit_frame_callback_fp(ext_frame_header, ((uint64_t)gttcan->slot_duration << 16) | gttcan->node_id);
    // gttcan->transmit_frame_callback_fp(ext_frame_header, 0xDEADBEEF);
}

void gttcan_process_frame(gttcan_t *gttcan, uint32_t can_frame_id, uint64_t data)
{
    if (!gttcan->is_initialised)
    {
        return;
    }

    uint16_t slot_id = (can_frame_id >> GTTCAN_NUM_DATA_ID_BITS) & 0xFFFF;
    uint16_t data_id = can_frame_id & 0xFFFF;

    if (gttcan->local_schedule_index > 0 && slot_id > gttcan->local_schedule[gttcan->local_schedule_index].slot_id)
    {
        // If received frame is greater than the next one I want to send, and I haven't wrapped, then I am slow
        gttcan->slot_duration_offset--; // SPEED ME UP
    }
    if (gttcan->local_schedule_index > 0 && slot_id != 0 && slot_id < gttcan->local_schedule[gttcan->local_schedule_index - 1].slot_id)
    {
        // If received frame is less than one I have already transmitted (and I've transmitted already this schedule)
        gttcan->slot_duration_offset++; // SLOW ME DOWN, I TRANSMITTED BEFORE SOMEONE ELSE HAD THE CHANCE, WHICH CAN'T BE POSSIBLE IF I AM AT ZERO
    }

    if (!gttcan->isActive && slot_id == 0)
    {
        gttcan->isActive = true;
        // uint32_t time_to_next_transmission = gttcan_get_time_to_next_transmission(slot_id,gttcan); //offset correction
        // gttcan->set_timer_int_callback_fp(time_to_next_transmission);
    }

    if (slot_id == 0)
    {
        if (gttcan->last_schedule_transmission_time == 0)
        {
            gttcan->last_schedule_transmission_time = gttcan->get_schedule_transmission_time_fp();
        }
        else
        {
            // uint32_t expected_time_elapsed = (uint32_t)gttcan->global_schedule_length * gttcan->slot_duration;
            // gttcan->slot_duration = gttcan->last_schedule_transmission_time / (uint32_t)gttcan->global_schedule_length;
        }
    }

    if (data_id == REFERENCE_FRAME_DATA_ID)
    {
        int32_t time_difference = 0;

        // NEW CODE START HERE!!!!!!!!!!
        int found_next_index = -1;
        // find the first local schedule entry where its slot_id > ref slot_id
        for (int i = 0; i < gttcan->local_schedule_length; i++)
        {
            if (gttcan->local_schedule[i].slot_id > slot_id)
            {
                // Found the next slot, position the index at the previous entry
                if (gttcan->local_schedule_index < i)
                {
                    gttcan->slot_duration_offset--; // NEEDS A SPEEDUP, because I didn't get to transmit a frame
                }
                if (i == 0 && gttcan->local_schedule_index)
                {
                    gttcan->slot_duration_offset--; // Needs a speedup, as I never got to transmit my final frame
                }
                gttcan->local_schedule_index = i;

                found_next_index = 1;
                break;
            }
        }

        if (found_next_index < 0)
        {
            // No slot is greater than the reference slot
            if (gttcan->local_schedule_index != 0)
            {
                gttcan->slot_duration_offset--; // Needs a speedup, as I never got to transmit my final frame
            }
            gttcan->local_schedule_index = 0;
        }

        // NEW CODE END HERE!!!!!!!!!!!

        // uint32_t expected_time_elapsed = (uint32_t)gttcan->global_schedule_length * gttcan->slot_duration;
        // if(gttcan->last_reference_frame_hardware_time > 0){ // If a reference frame time has been recorded
        //     uint32_t actual_time_elapsed = gttcan->last_reference_frame_hardware_time - gttcan->hardware_time;
        //     int32_t time_difference = expected_time_elapsed - actual_time_elapsed;
        // }

        // gttcan->last_reference_frame_hardware_time = gttcan->hardware_time;
        // gttcan_reset_hardware_time(gttcan);

        uint32_t time_to_next_transmission = gttcan_get_time_to_next_transmission(slot_id, gttcan) + time_difference; // offset correction
        gttcan->set_timer_int_callback_fp(time_to_next_transmission);
    }
    else
    {
        // Write data using user defined functions
        // [data_id data]
        gttcan->write_value_fp(data_id, data);
    }
    // future per frame clock correction functionalities maybe
}

void gttcan_get_local_schedule(gttcan_t *gttcan, global_schedule_ptr_t global_schedule_ptr)
{
    uint16_t local_schedule_index = 0;
    for (int i = 0; i < gttcan->global_schedule_length; i++)
    {
        if (global_schedule_ptr[i].node_id == gttcan->node_id)
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

    if ((gttcan->slot_duration_offset/50) < -1*gttcan->slot_duration || (gttcan->slot_duration_offset/50) > gttcan->slot_duration ){
        gttcan->slot_duration_offset = 0;
    }

    gttcan->slot_duration += gttcan->slot_duration_offset / 50;
    gttcan->slot_duration_offset = 0;

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

// void gttcan_reset_hardware_time(gttcan_t *gttcan){
//     gttcan->hardware_time = 0;
// }
