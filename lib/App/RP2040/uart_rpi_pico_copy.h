#pragma once

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int uart_rpi_init(const struct device *dev);

#ifdef __cplusplus
}
#endif