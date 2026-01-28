/*
 * Copyright (c) 2026 Vaisala Oyj
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define UART_NODE DT_ALIAS(test_uart)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_NODE);

// takes around 5.73 ms to send.
static uint8_t tx_buf[] = "Hello World";

/* Cycle through different delays. 6000 us (5th iteration) jams the program */
const uint32_t delays_us[] = {5000, 5000, 10000, 10000, 6000};

static volatile bool tx_done;
static volatile bool tx_aborted;

K_SEM_DEFINE(tx_sem, 0, 1);

static void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(user_data);

	switch (evt->type) {
	case UART_TX_DONE:
		printk("TX_DONE: sent %d bytes\n", evt->data.tx.len);
		tx_done = true;
		k_sem_give(&tx_sem);
		break;

	case UART_TX_ABORTED:
		printk("TX_ABORTED\n");
		tx_aborted = true;
		k_sem_give(&tx_sem);
		break;

	default:
		break;
	}
}

int main(void)
{
	int ret;

	printk("\n");
	printk("============================================\n");
	printk("UART Async TX Abort Bug Demonstration\n");
	printk("Board: b_u585i_iot02a (STM32U5)\n");
	printk("Test UART: %s\n", uart_dev->name);
	printk("============================================\n");
	printk("\n");

	if (!device_is_ready(uart_dev)) {
		printk("ERROR: UART device not ready\n");
		return -1;
	}

	/* Register callback */
	ret = uart_callback_set(uart_dev, uart_callback, NULL);
	if (ret) {
		printk("ERROR: Failed to set callback: %d\n", ret);
		return -1;
	}

	for (unsigned int iteration = 0;; ++iteration) {

		const uint32_t delay_us = delays_us[iteration % ARRAY_SIZE(delays_us)];

		printk("=== Iteration %u: delay=%u us ===\n", iteration + 1, delay_us);

		tx_done = false;
		tx_aborted = false;

		/* Start TX */
		uart_tx(uart_dev, tx_buf, sizeof(tx_buf), SYS_FOREVER_US);

		/* Wait before abort */
		k_busy_wait(delay_us);

		uart_tx_abort(uart_dev);
		printk("uart_tx_abort() returned (no hang)\n");

		/* Wait for completion or abort */
		if (k_sem_take(&tx_sem, K_MSEC(1000)) == 0) {
			if (tx_aborted) {
				printk("Result: TX was aborted\n");
			} else if (tx_done) {
				printk("Result: TX finished before abort\n");
			}
		} else {
			printk("Result: TIMEOUT - no callback received\n");
		}

		k_msleep(50); /* Small delay between iterations */
	}
}
