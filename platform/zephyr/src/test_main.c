/**
 * Copyright (C) 2023 EEMBC
 * Copyright (C) 2026 Arm Limited
 *
 * All EEMBC Benchmark Software are products of EEMBC and are provided under the
 * terms of the EEMBC Benchmark License Agreements. The EEMBC Benchmark Software
 * are proprietary intellectual properties of EEMBC and its Members and is
 * protected under all applicable laws, including all applicable copyright laws.
 */

#include <stdio.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

int audiomark_test_main(int argc, char *argv[]);

#if defined(TF_INTERPRETER) && !defined(CONFIG_ETHOS_U)
int
cmsis_nn_init(void)
{
    return 0;
}

int
classify_on_cmsis_nn(const int8_t *in_data, int8_t *out_data)
{
    ARG_UNUSED(in_data);
    ARG_UNUSED(out_data);
    return -1;
}
#endif

static void
emit_eot(void)
{
    fflush(stdout);

#if DT_HAS_CHOSEN(zephyr_console)
    const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

    if (device_is_ready(uart))
    {
        uart_poll_out(uart, 0x04);
        k_msleep(10);
        return;
    }
#endif

    printf("\x04");
    fflush(stdout);
    k_msleep(10);
}

int
main(void)
{
    int ret = audiomark_test_main(0, NULL);

    emit_eot();

    return ret;
}
