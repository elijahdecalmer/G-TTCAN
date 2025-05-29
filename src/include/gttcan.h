
#ifndef GTTCAN_H
#define GTTCAN_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Maximum number of entries in a node's local transmission schedule
 * 
 * Defines the size of the local schedule array that stores slot/data ID pairs
 * for frames this node will transmit. Each entry represents one scheduled
 * transmission opportunity within the global transmission cycle.
 */
#ifndef GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH
#define GTTCAN_MAX_LOCAL_SCHEDULE_LENGTH 512
#endif

/**
 * @brief Maximum number of entries in the global schedule
 * 
 * Defines the total number of time slots in the global schedule that coordinates
 * all nodes in the G-TTCAN network. This represents the complete schedule cycle
 * length before it repeats.
 * 
 * @note Larger values allow more nodes or more frequent transmissions but increase
 * cycle time. 
 * @note Must accommodate all nodes' transmission needs.
 */

#ifndef MAX_GLOBAL_SCHEDULE_LENGTH
#define MAX_GLOBAL_SCHEDULE_LENGTH 512
#endif

/* REQUIREMENT
 GTTCAN_NUM_SLOT_ID_BITS + GTTCAN_NUM_DATA_ID_BITS <= 29
 This value limits the size of global schedule.
*/

/**
 * @brief Number of bits allocated for slot ID in CAN frame identifier
 * 
 * Determines the maximum number of time slots in the global schedule.
 * Slot ID is embedded in the upper bits of the 29-bit extended CAN ID.
 * 
 * Maximum slots = 2^GTTCAN_NUM_SLOT_ID_BITS
 * With 13 bits: up to 8192 slots possible
 * 
 * CONSTRAINT: GTTCAN_NUM_SLOT_ID_BITS + GTTCAN_NUM_DATA_ID_BITS <= 29
 * (Must fit within extended CAN frame ID)
 */
#ifndef GTTCAN_NUM_SLOT_ID_BITS
#define GTTCAN_NUM_SLOT_ID_BITS 13
#endif

/**
 * @brief Number of bits allocated for data ID in CAN frame identifier
 * 
 * Determines the maximum number of different data types/messages that can
 * be distinguished within each time slot. Data ID is embedded in the lower
 * bits of the 29-bit extended CAN ID.
 * 
 * Maximum data IDs = 2^GTTCAN_NUM_DATA_ID_BITS
 * With 16 bits: up to 65536 different data types possible
 * 
 * CONSTRAINT: GTTCAN_NUM_SLOT_ID_BITS + GTTCAN_NUM_DATA_ID_BITS <= 29
 * (Must fit within extended CAN frame ID)
 */
#ifndef GTTCAN_NUM_DATA_ID_BITS
#define GTTCAN_NUM_DATA_ID_BITS 16
#endif


/**
 * @brief Data ID reserved for reference/synchronization frames
 * 
 * Special data ID used for frames that provide timing reference and
 * synchronization information to maintain global time alignment across
 * all nodes in the G-TTCAN network.
 * 
 * @note These frames are critical for GTTCAN operation
 */
#ifndef REFERENCE_FRAME_DATA_ID
#define REFERENCE_FRAME_DATA_ID 0
#endif


/**
 * @brief Data ID for general-purpose data frames
 * 
 * Default data ID used for standard application data that doesn't require
 * special handling or synchronization properties. Can be used as a fallback
 * or default value when specific data typing isn't needed.
 * @note This data ID only serves as an example, the user should define their own
 * data IDs to fit their usage scenarios
 */
#ifndef GENERIC_DATA_ID
#define GENERIC_DATA_ID 1
#endif


/**
 * @brief Number of time slots to pause during network startup
 * 
 * When a node first joins the G-TTCAN network or after a reset, it waits
 * this many slots before beginning normal transmission. This node specific 
 * startup delay allows the node to:
 * - Synchronize with the global time reference (if exists)
 * - Learn the current schedule state (if exists)
 * - Avoid collisions during initialization
 * 
 * @note Too small: risk of startup collisions
 * @note Too large: delayed network participation
 */
#ifndef DEFAULT_STARTUP_PAUSE_SLOTS
#define DEFAULT_STARTUP_PAUSE_SLOTS 2
#endif



/**
 * @brief Threshold for switching to all-node synchronization adjustment
 * 
 * Number of schedule rounds (complete schedule cycles) to wait before changing
 * from individual node time adjustment to system-wide synchronization mode.
 * 
 * This allows individual nodes to initially adjust their local timing from the time 
 * master, then switches to a mode where the nodes aligns itself based off every node
 * for better overall network synchronization stability.
 * 
 * Affects convergence time vs. stability trade-off in time synchronization.
 */
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

/**
 * @brief Callback function pointer for transmitting CAN frames
 * 
 * This function is called by G-TTCAN when it needs to transmit a frame on the CAN bus.
 * The implementation must handle the actual transmission using the platform's CAN driver.
 * 
 * @param can_frame_id 29-bit extended CAN frame identifier to be transmitted
 *                     Format: [slot_id (GTTCAN_NUM_SLOT_ID_BITS bits) | data_id (GTTCAN_NUM_DATA_ID_BITS bits)]
 * @param data 64-bit data payload to be transmitted in the CAN frame
 * 
 * @note Must be non-blocking or have minimal execution time to avoid timing issues
 * @note Should handle transmission errors gracefully (e.g., bus-off conditions)
 * @note Called from interrupt context in gttcan_transmit_next_frame()
 * @note Frame format must be extended (29-bit identifier) CAN frame
 * @note Implementation should not modify the parameters
 * 
 * Example implementation:
 * @code
 * void my_transmit_callback(uint32_t can_id, uint64_t data) {
 *     can_message_t msg;
 *     msg.id = can_id;
 *     msg.extended = true;
 *     msg.data_length = 8;
 *     memcpy(msg.data, &data, 8);
 *     can_transmit(&msg);
 * }
 * @endcode
 */
typedef void (*transmit_frame_callback_fp_t)(uint32_t, uint64_t);

/**
 * @brief Callback function pointer for setting timer interrupts
 * 
 * This function is called by G-TTCAN to schedule the next transmission.
 * The implementation must configure a hardware timer to generate an interrupt
 * after the specified delay, which should then call gttcan_transmit_next_frame().
 * 
 * @param time_in_stu Time delay in system time units until the interrupt should occur
 *                    Must be > 0; G-TTCAN ensures minimum value of 1
 * 
 * @note The interrupt handler must call gttcan_transmit_next_frame() when triggered
 * @note Should handle timer overflow conditions for large delay values
 * @note Must be accurate and have low jitter for proper network synchronization
 * @note Should overwrite any existing timer and set the new delay, rather than queuing multiple interrupts
 * @note This function is called from the G-TTCAN context, so it should not take long to execute
 * @note System time units must be consistent across all nodes in the network
 * 
 * Example implementation:
 * @code
 * void my_timer_callback(uint32_t delay_stu) {
 *     // Stop existing timer
 *     timer_stop();
 *     // Set to zero
 *     timer_set_value(0); 
 *     // Set new timer value (convert STU to hardware timer units if needed)
 *     timer_set_period(delay_stu * STU_TO_TIMER_RATIO);
 *     // Start timer
 *     timer_start();
 * }
 * 
 * // In timer interrupt handler:
 * void timer_isr(void) {
 *     gttcan_transmit_next_frame(&my_gttcan);
 * }
 * @endcode
 */
typedef void (*set_timer_int_callback_fp_t)(uint32_t);

/**
 * @brief Callback function pointer for reading data values for transmission
 * 
 * This function is called by G-TTCAN when it needs to read data for transmission
 * in a scheduled slot. The implementation should return the current value associated
 * with the specified data identifier.
 * 
 * @param data_id Data identifier from the global schedule indicating which data to read
 *                Values are application-specific and defined in the global schedule
 * 
 * @return 64-bit data value to be transmitted in the CAN frame payload
 * 
 * @note Called from interrupt context in gttcan_transmit_next_frame()
 * @note Should be fast and non-blocking to maintain timing accuracy, so where possible read from memory or registers directly instead of performing I/O operations
 * @note Return value will be transmitted exactly as returned (no formatting by G-TTCAN)
 * @note May be called multiple times for the same data_id if node has multiple slots
 * @note Should handle unknown data_id values gracefully (return 0 or error value)
 * 
 * Example implementation:
 * @code
 * uint64_t my_read_callback(uint16_t data_id) {
 *     switch(data_id) {
 *         case SENSOR_TEMPERATURE:
 *             return read_temperature_sensor(); // Reading from a register or memory ideally
 *         case MOTOR_SPEED:
 *             return get_motor_rpm(); // Reading from a register or memory ideally
 *         case STATUS_FLAGS:
 *             return get_system_status(); // Reading from a register or memory ideally
 *         default:
 *             return 0; // Unknown data ID
 *     }
 * }
 * @endcode
 */
typedef uint64_t (*read_value_fp_t)(uint16_t);

/**
 * @brief Callback function pointer for storing received data values
 * 
 * This function is called by G-TTCAN when it receives a data frame from another node.
 * The implementation should store or process the received data according to the
 * application's requirements.
 * 
 * @param data_id Data identifier from the received frame indicating the type of data
 *                Values are application-specific and defined in the global schedule
 * @param data 64-bit data value received from the transmitting node
 * 
 * @note Called from interrupt context in gttcan_process_frame()
 * @note Should be fast and non-blocking to maintain timing accuracy
 * @note May queue data for later processing if complex operations are needed
 * @note Should handle unknown data_id values gracefully (ignore or log error)
 * @note Data value is exactly as transmitted (no processing by G-TTCAN)
 * @note May receive data from multiple nodes with the same data_id
 * 
 * Example implementation:
 * @code
 * void my_write_callback(uint16_t data_id, uint64_t data) {
 *     switch(data_id) {
 *         case SENSOR_TEMPERATURE:
 *             update_temperature_reading(data);
 *             break;
 *         case MOTOR_SPEED:
 *             update_motor_rpm_display(data);
 *             break;
 *         case STATUS_FLAGS:
 *             process_system_status(data);
 *             break;
 *         default:
 *             // Log unknown data ID or ignore
 *             break;
 *     }
 * }
 * @endcode
 */
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
