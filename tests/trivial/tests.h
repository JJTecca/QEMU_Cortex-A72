/******************************************************************************
 * File: tests.h
 * Project: RPi5-Industrial-Gateway Bare Metal Development
 *
 * Copyright (c) 2026
 * Author: Maior Cristian
 *****************************************************************************/

 /***************************************************
 * Macro Definitions
 **************************************************/
#ifndef TESTS_H
#define TESTS_H

#define TEST_PING_DATA_BASE     0x1000
#define TEST_DATA_BASE          0xDEAD0000
#define TEST_DATA_STEP          0x100
#define TEST_ACK_POLL_ROUNDS    10
#define TEST_DELAY_SHORT        2000000
#define TEST_DELAY_MEDIUM       3000000
#define TEST_DELAY_LONG         10000000

/**************************************************
 * HELPER FUNCTIONS
 ***************************************************/

/******************************************************************************
 * Function: test1_ping_all_cores
 * Description: Sends PING messages from Core 0 to all cores (1-3)
 * Returns: 0 on success, -1 on failure
 *****************************************************************************/
int  test1_ping_all_cores(void);

/******************************************************************************
 * Function: test2_send_data_messages
 * Description: Sends DATA messages to all cores and waits for ACKs
 * Returns: 0 on success, -1 if no ACK received
 *****************************************************************************/
int  test2_send_data_messages(void);

/******************************************************************************
 * Function: test3_uart_rx_keyboard_simulation
 * Description: Announces live UART keyboard simulation, then idles Core 0
 * Returns: None (enters WFE loop)
 *****************************************************************************/
void test3_uart_rx_keyboard_simulation(void);

/******************************************************************************
 * Function: run_all_tests
 * Description: Master runner — executes all unit tests in order
 * Returns: None
 *****************************************************************************/
void run_all_tests(void);

#endif /* TESTS_H */
