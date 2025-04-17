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
    transmit_frame_callback_fp_t transmit_frame_callback_fp,
    set_timer_int_callback_fp_t set_timer_int_callback_fp
)
{
    gttcan->node_id = node_id;
    gttcan->global_schedule_length = global_schedule_length;
    gttcan->slot_duration = slot_duration;
    gttcan->local_schedule_index = 0;

    gttcan_get_local_schedule(gttcan,global_schedule_ptr);

    gttcan->transmit_frame_callback_fp = transmit_frame_callback_fp;
    gttcan->set_timer_int_callback_fp = set_timer_int_callback_fp;

}




// Transmit frame (blink a blinky LED )
    // increment local_schedule_index
    // look at my schedule, set time to next transmit
    // typedef void (*transmit_frame_callback_fp)(uint32_t, uint64_t);



// SetTimerCallback
// typedef void (*set_timer_int_callback_fp)(uint32_t);
// REMOVE CONTEXT PTR FROM params
// borrow the implementation of this function 



void gttcan_transmit_next_frame(gttcan_t * gttcan)
{

    uint32_t ext_frame_header;

    uint16_t slot_id = gttcan->local_schedule[gttcan->local_schedule_index].slot_id;
    uint16_t data_id = gttcan->local_schedule[gttcan->local_schedule_index].data_id;

    ext_frame_header = ((uint32_t)slot_id << GTTCAN_NUM_DATA_ID_BITS) | data_id; // TODO CHECK THE SAFETY OF THIS, should DATA_ID BE ANDED WITH A MASK OF LENGTH DATA_ID????

    gttcan->local_schedule_index++;
    if (gttcan->local_schedule_index >= gttcan->local_schedule_length){ // TODO MAKE THIS A == ONCE THAT CAN BE ENSURED IS SAFE
        gttcan->local_schedule_index = 0;
    }

    uint16_t next_slot_id = gttcan->local_schedule[gttcan->local_schedule_index].slot_id;

    uint16_t number_of_slots_to_next =  gttcan_get_number_of_slots_to_next(slot_id,next_slot_id,gttcan->global_schedule_length);


    uint32_t time_to_next_transmission = number_of_slots_to_next * gttcan->slot_duration;

    gttcan->set_timer_int_callback_fp(time_to_next_transmission);

    gttcan->transmit_frame_callback_fp(ext_frame_header, (uint64_t)0);


}




// GTTCAN Start (master only function?)
    // start schedule, call transmitFrame (blink an LED) to symbolise sending a reference frame
void gttcan_start(
    gttcan_t *gttcan
)
{
    gttcan->local_schedule_index = 0;
    gttcan_transmit_next_frame(gttcan);


}


void gttcan_get_local_schedule(gttcan_t * gttcan, global_schedule_ptr_t global_schedule_ptr){
    uint8_t local_schedule_index = 0;
    for (int i = 0; i < gttcan->global_schedule_length; i++) {
        if (global_schedule_ptr[i].node_id == gttcan->node_id) {
            gttcan->local_schedule[local_schedule_index].slot_id = global_schedule_ptr[i].slot_id;
            gttcan->local_schedule[local_schedule_index].data_id = global_schedule_ptr[i].data_id;
            local_schedule_index++;
        }
    }
    gttcan->local_schedule_length = local_schedule_index;
}


uint16_t gttcan_get_number_of_slots_to_next(uint16_t current_slot_id, uint16_t next_slot_id, uint16_t global_schedule_length){
    if (current_slot_id < next_slot_id){
        return next_slot_id - current_slot_id;
    } else {
        return global_schedule_length - current_slot_id + next_slot_id;
    }
}