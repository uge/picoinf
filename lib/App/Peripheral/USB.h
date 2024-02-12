#pragma once

#include "Evm.h"
#include "KTask.h"
#include "Log.h"

#include "tusb.h"


class USB
{
public:

    static void Test()
    {
        static KTask<1000> t("TinyUSB", []{
            tusb_init();

            // has to run after scheduler started for some reason
            tud_init(0);

            while (true)
            {
                // blocks via FreeRTOS task sync mechanisms
                tud_task();
            }
        }, 10);
    }

private:
};

