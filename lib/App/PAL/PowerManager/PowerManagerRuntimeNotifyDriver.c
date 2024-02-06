#include "PowerManagerRuntimeNotifyDriver.h"

/*

Has to be in a .c file because the macro magic breaks under c++

*/


/*

tests\subsys\pm\power_mgmt\src\main.c


Ok, so this driver is just participating in runtime power management.

I believe this is ultimately:
- driver is notifying runtime pm when it is used
- when driver isn't used, it is asked to shut down

I believe this differs from non-runtime, where:
- drivers don't keep track of their use state
- drivers are asked to enter a given power state regardless of
  existing utilization (not clear what they do if that isn't workable)

When someone asks it to open, it registers the fact that it is
in use with the pm runtime API.

The reverse when closed.

The open/close are ultimately triggering reference counting within
the pm runtime API.

The events which this device receive via dummy_device_pm_action
should be influenced with whether it is active or not.

The action to be taken in the pm_action callback would be
dependent on the module under management.

For the purposes of power management under my own code, I
don't think I will use this directly.

However it is worth keeping an eye on, because I will be able
to see how other drivers who participate behave under the
conditions the device goes through.

*/


static CbOnInit cbOnInit_ = NULL;
static void SetCbOnInitFn(CbOnInit cb)
{
    cbOnInit_ = cb;
}

static CbOnPmAction cbOnPmAction_ = NULL;
static void SetCbOnPmActionFn(CbOnPmAction cb)
{
    cbOnPmAction_ = cb;
}



static int dummy_open(const struct device *dev)
{
	return pm_device_runtime_get(dev);
}

static int dummy_close(const struct device *dev)
{
	return pm_device_runtime_put(dev);
}

static int dummy_device_pm_action(const struct device *dev,
				                  enum pm_device_action action)
{
    if (cbOnPmAction_)
    {
        cbOnPmAction_(action);
    }

	return 0;
}

int dummy_init(const struct device *dev)
{
    if (cbOnInit_)
    {
        cbOnInit_();
    }

	return pm_device_runtime_enable(dev);
}

PM_DEVICE_DEFINE(dummy_driver, dummy_device_pm_action);


static const struct dummy_driver_api funcs = {
	.open = dummy_open,
	.close = dummy_close,
    .setCbOnInit = SetCbOnInitFn,
    .setCbOnPmAction = SetCbOnPmActionFn,
};


DEVICE_DEFINE(
    dummy_driver,
    DUMMY_DRIVER_NAME,
    &dummy_init,
	PM_DEVICE_GET(dummy_driver),
    NULL,
    NULL,
    APPLICATION,
	CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
    &funcs
);

