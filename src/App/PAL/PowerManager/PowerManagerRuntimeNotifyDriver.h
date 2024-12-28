#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/pm.h>


#define DUMMY_DRIVER_NAME	"dummy_driver"

typedef int (*dummy_api_open_t)(const struct device *dev);

typedef int (*dummy_api_close_t)(const struct device *dev);


typedef void (*CbOnInit)();
typedef void (*SetCbOnInit)(CbOnInit cb);
typedef void (*CbOnPmAction)(enum pm_device_action action);
typedef void (*SetCbOnPmAction)(CbOnPmAction cb);

struct dummy_driver_api {
	dummy_api_open_t open;
	dummy_api_close_t close;

    SetCbOnInit setCbOnInit;
    SetCbOnPmAction setCbOnPmAction;
};



