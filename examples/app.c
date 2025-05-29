/*
 * app.c
 *
 *  Created on: 23 May 2023 by Rene Hexel
 *  Modified on: 29 May 2025 by Daniel De Calmer, Elijah De Calmer, and Draco Zhang
 *  Example application using G-TTCAN for periodic frame transmission.
 */

#include "hal.h"
#include "gpio.h"
#include "timer.h"
#include "gttcan.h"
#include <time.h>

#include "app.h"
#include "can.h"
#include "error.h"
#include "power.h"
#include "uart.h"
#include "usb.h"
#include "dma.h"
#include "spi.h"
#include "global_schedule.h"

gttcan_t gttcan; // G-TTCAN protocol state

// Forward declarations of G-TTCAN callback functions
void set_timer_int(uint32_t time);
void transmit_frame(uint32_t can_frame_id_field, uint64_t data);
uint64_t read_value(uint16_t data_id);
void write_value(uint16_t data_id, uint64_t value);

int main(void)
{
    // Initialize hardware abstraction layer and clock
    HAL_Init();
    SystemClock_Config();

    // Initialize GPIOs, timers, peripherals, and buses
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_DMA_Init();
    MX_CAN_Init();
    MX_USART3_UART_Init();
    MX_USB_OTG_FS_PCD_Init();
    MX_TIM1_Init();
    MX_SPI_Init();

    // Configure CAN filter to accept all messages
    CAN_FilterTypeDef canFilter;
    canFilter.FilterMode = CAN_FILTERMODE_IDMASK;
    canFilter.FilterScale = CAN_FILTERSCALE_32BIT;
    canFilter.FilterIdHigh = 0x0000;
    canFilter.FilterIdLow = 0x0000;
    canFilter.FilterMaskIdHigh = 0x0000;
    canFilter.FilterMaskIdLow = 0x0000;
    canFilter.FilterFIFOAssignment = CAN_RX_FIFO0;
    canFilter.FilterActivation = ENABLE;
    canFilter.SlaveStartFilterBank = 0;
    canFilter.FilterBank = 14;

    // Start CAN peripheral and enable RX/TX interrupts
    HAL_CAN_ConfigFilter(&hcan2, &canFilter);
    HAL_CAN_Start(&hcan2);
    HAL_CAN_ActivateNotification(&hcan2, CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_TX_MAILBOX_EMPTY);

    // Initialize G-TTCAN with node-specific parameters and callbacks
    gttcan_init(&gttcan, 1, global_schedule, MAX_GLOBAL_SCHEDULE_LENGTH, 300, 7,
                transmit_frame, set_timer_int, read_value, write_value);

    gttcan_start(&gttcan); // Start protocol after optional wait

    HAL_TIM_Base_Start_IT(&htim2); // Start timer with interrupt enabled

    while (1)
    {
        // Main loop left intentionally empty:
        // G-TTCAN is fully driven by timer and CAN interrupts
    }
}

// Timer interrupt callback (e.g., called every 300 µs)
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        gttcan_transmit_next_frame(&gttcan); // Schedule and transmit next frame if it's our turn
    }
}

// CAN receive interrupt callback
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    uint32_t hardware_time = __HAL_TIM_GET_COUNTER(&htim2); // Optional time-stamping
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8] = {0};

    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data);

    if (rx_header.IDE == CAN_ID_EXT) {
        const uint64_t *dataptr = (const uint64_t *)rx_data;
        gttcan_process_frame(&gttcan, rx_header.ExtId, *dataptr); // Forward to G-TTCAN logic
    }
}

// Set a timer interrupt after a specific time (used by G-TTCAN to wait between slots)
void set_timer_int(uint32_t time)
{
    __HAL_TIM_DISABLE(&htim2);                       // Stop timer temporarily
    __HAL_TIM_SET_AUTORELOAD(&htim2, time);          // Set new auto-reload value
    __HAL_TIM_SET_COUNTER(&htim2, 0);                // Reset counter
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);   // Clear update flag
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);      // Enable update interrupt
    __HAL_TIM_ENABLE(&htim2);                        // Start timer again
}

// Sends a CAN frame with extended ID and 64-bit data
void transmit_frame(uint32_t can_frame_id, uint64_t data)
{
    CAN_TxHeaderTypeDef tx_header;
    tx_header.IDE = CAN_ID_EXT;
    tx_header.ExtId = can_frame_id;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = 8; // 8 bytes
    tx_header.TransmitGlobalTime = DISABLE;

    uint32_t tx_mbox = 0;
    HAL_CAN_AddTxMessage(&hcan2, &tx_header, (uint8_t *)&data, &tx_mbox);

    HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin); // Toggle LED to indicate activity
}

// Return a data value requested by G-TTCAN, e.g., current time for reference frame
uint64_t read_value(uint16_t data_id)
{
    if (data_id == REFERENCE_FRAME_DATA_ID) {
        return __HAL_TIM_GET_COUNTER(&htim2); // Timestamp based on hardware timer
    } else {
        return 1; // Dummy data
    }
}

// Receive data value from a frame (not used in this example, but defined for extensibility)
void write_value(uint16_t data_id, uint64_t value)
{
    // Placeholder: Implement custom behavior for received data if needed
}
