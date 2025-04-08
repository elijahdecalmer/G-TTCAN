#include <stdio.h>
#include "gttcan.h"

#define REFERENCE_FRAME_DATA_ID 0
#define GENERIC_DATA_ID 1 

// GTTCAN INIT
// This function should initialise the local schedule

void gttcan_init(
    gttcan_t *gttcan,
    uint8_t node_id,
    uint16_t global_schedule_length,
    uint8_t local_schedule_length,
    uint32_t slot_duration,
    transmit_frame_callback_fp_t transmit_callback_fp,
    set_timer_int_callback_fp_t set_timer_int_callback_fp
)
{
    gttcan->node_id = node_id;
    gttcan->global_schedule_length = global_schedule_length;
    gttcan->local_schedule_length = local_schedule_length;
    gttcan->slot_duration = slot_duration;

    gttcan->transmit_frame_callback_fp = transmit_callback_fp;
    gttcan->set_timer_int_callback_fp = set_timer_int_callback_fp;

    gttcan->local_schedule_index;

    gttcan->local_schedule[0].slot_id = 0;
    gttcan->local_schedule[0].data_id = REFERENCE_FRAME_DATA_ID;

    gttcan->local_schedule[1].slot_id = 1;
    gttcan->local_schedule[1].data_id = GENERIC_DATA_ID;

    gttcan->local_schedule[2].slot_id = 2;
    gttcan->local_schedule[2].data_id = GENERIC_DATA_ID;

    gttcan->local_schedule[3].slot_id = 3;
    gttcan->local_schedule[3].data_id = GENERIC_DATA_ID;

    gttcan->local_schedule[5].slot_id = 5;
    gttcan->local_schedule[5].data_id = GENERIC_DATA_ID;

    gttcan->local_schedule[8].slot_id = 8;
    gttcan->local_schedule[8].data_id = GENERIC_DATA_ID;

    gttcan->local_schedule[13].slot_id = 13;
    gttcan->local_schedule[13].data_id = GENERIC_DATA_ID;

    gttcan->local_schedule[21].slot_id = 21;
    gttcan->local_schedule[21].data_id = GENERIC_DATA_ID;

    gttcan->local_schedule[34].slot_id = 34;
    gttcan->local_schedule[34].data_id = GENERIC_DATA_ID;

    gttcan->local_schedule[55].slot_id = 55;
    gttcan->local_schedule[55].data_id = GENERIC_DATA_ID;

    gttcan->local_schedule[89].slot_id = 89;
    gttcan->local_schedule[89].data_id = GENERIC_DATA_ID;

    gttcan->local_schedule_length = 11;

}




// Transmit frame (blink a blinky LED )
    // increment local_schedule_index
    // look at my schedule, set time to next transmit
    // typedef void (*transmit_callback_fp)(uint32_t, uint64_t);



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

    uint16_t number_of_slots_to_next; // TODO CHECK THAT THIS IS LESS THAN MAX GLOBAL SCHEDULE LENGTH

    if (slot_id < next_slot_id){
        number_of_slots_to_next = next_slot_id - slot_id;
    } else {
        number_of_slots_to_next = gttcan->global_schedule_length - slot_id + next_slot_id;
    }


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