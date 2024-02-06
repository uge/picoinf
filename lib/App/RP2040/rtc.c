/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico.h"

#include "hardware/regs/intctrl.h"
#include "hardware/irq.h"
// #include "hardware/rtc.h"
#include "rtc.h"
#include "hardware/resets.h"
#include "hardware/clocks.h"




// cherry picked some IRQ functionality to get this to work

#include "hardware/sync.h"
#include "hardware/structs/scb.h"


static inline irq_handler_t *get_vtable(void) {
    return (irq_handler_t *) scb_hw->vtor;
}

static void set_raw_irq_handler_and_unlock(uint num, irq_handler_t handler, uint32_t save) {
    // update vtable (vtable_handler may be same or updated depending on cases, but we do it anyway for compactness)
    get_vtable()[VTABLE_FIRST_IRQ + num] = handler;
    __dmb();
    spin_unlock(spin_lock_instance(PICO_SPINLOCK_ID_IRQ), save);
}


void irq_set_exclusive_handler(uint num, irq_handler_t handler) {
    check_irq_param(num);
#if !PICO_NO_RAM_VECTOR_TABLE
    spin_lock_t *lock = spin_lock_instance(PICO_SPINLOCK_ID_IRQ);
    uint32_t save = spin_lock_blocking(lock);
    __unused irq_handler_t current = irq_get_vtable_handler(num);
    // hard_assert(current == __unhandled_user_irq || current == handler);
    set_raw_irq_handler_and_unlock(num, handler, save);
#else
    panic_unsupported();
#endif
}


irq_handler_t irq_get_vtable_handler(uint num) {
    check_irq_param(num);
    return get_vtable()[16 + num];
}


// replaces void irq_set_enabled(uint num, bool enabled)
#include "hardware/regs/m0plus.h"
static void IrqSetEnabled(uint num, bool enabled) 
{
    uint32_t mask = 1u << num;

    if (enabled) {
        // Clear pending before enable
        // (if IRQ is actually asserted, it will immediately re-pend)
        *((io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ICPR_OFFSET)) = mask;
        *((io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ISER_OFFSET)) = mask;
    } else {
        *((io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ICER_OFFSET)) = mask;
    }
}












// Set this when setting an alarm
static rtc_callback_t _callback = NULL;
static bool _alarm_repeats = false;

bool rtc_running(void) {
    return (rtc_hw->ctrl & RTC_CTRL_RTC_ACTIVE_BITS);
}

void rtc_init(void) {
    // Get clk_rtc freq and make sure it is running
    uint rtc_freq = clock_get_hz(clk_rtc);
    assert(rtc_freq != 0);

    // Take rtc out of reset now that we know clk_rtc is running
    reset_block(RESETS_RESET_RTC_BITS);
    unreset_block_wait(RESETS_RESET_RTC_BITS);

    // Set up the 1 second divider.
    // If rtc_freq is 400 then clkdiv_m1 should be 399
    rtc_freq -= 1;

    // Check the freq is not too big to divide
    assert(rtc_freq <= RTC_CLKDIV_M1_BITS);

    // Write divide value
    rtc_hw->clkdiv_m1 = rtc_freq;
}

static bool valid_datetime(datetime_t *t) {
    // Valid ranges taken from RTC doc. Note when setting an RTC alarm
    // these values are allowed to be -1 to say "don't match this value"
    if (!(t->year >= 0 && t->year <= 4095)) return false;
    if (!(t->month >= 1 && t->month <= 12)) return false;
    if (!(t->day >= 1 && t->day <= 31)) return false;
    if (!(t->dotw >= 0 && t->dotw <= 6)) return false;
    if (!(t->hour >= 0 && t->hour <= 23)) return false;
    if (!(t->min >= 0 && t->min <= 59)) return false;
    if (!(t->sec >= 0 && t->sec <= 59)) return false;
    return true;
}

bool rtc_set_datetime(datetime_t *t) {
    if (!valid_datetime(t)) {
        return false;
    }

    // Disable RTC
    rtc_hw->ctrl = 0;
    // Wait while it is still active
    while (rtc_running()) {
        tight_loop_contents();
    }

    // Write to setup registers
    rtc_hw->setup_0 = (((uint32_t)t->year)  << RTC_SETUP_0_YEAR_LSB ) |
                      (((uint32_t)t->month) << RTC_SETUP_0_MONTH_LSB) |
                      (((uint32_t)t->day)   << RTC_SETUP_0_DAY_LSB);
    rtc_hw->setup_1 = (((uint32_t)t->dotw)  << RTC_SETUP_1_DOTW_LSB) |
                      (((uint32_t)t->hour)  << RTC_SETUP_1_HOUR_LSB) |
                      (((uint32_t)t->min)   << RTC_SETUP_1_MIN_LSB)  |
                      (((uint32_t)t->sec)   << RTC_SETUP_1_SEC_LSB);

    // Load setup values into rtc clock domain
    rtc_hw->ctrl = RTC_CTRL_LOAD_BITS;

    // Enable RTC and wait for it to be running
    rtc_hw->ctrl = RTC_CTRL_RTC_ENABLE_BITS;
    while (!rtc_running()) {
        tight_loop_contents();
    }

    return true;
}

bool rtc_get_datetime(datetime_t *t) {
    // Make sure RTC is running
    if (!rtc_running()) {
        return false;
    }

    // Note: RTC_0 should be read before RTC_1
    uint32_t rtc_0 = rtc_hw->rtc_0;
    uint32_t rtc_1 = rtc_hw->rtc_1;

    t->dotw  = (int8_t) ((rtc_0 & RTC_RTC_0_DOTW_BITS ) >> RTC_RTC_0_DOTW_LSB);
    t->hour  = (int8_t) ((rtc_0 & RTC_RTC_0_HOUR_BITS ) >> RTC_RTC_0_HOUR_LSB);
    t->min   = (int8_t) ((rtc_0 & RTC_RTC_0_MIN_BITS  ) >> RTC_RTC_0_MIN_LSB);
    t->sec   = (int8_t) ((rtc_0 & RTC_RTC_0_SEC_BITS  ) >> RTC_RTC_0_SEC_LSB);
    t->year  = (int16_t) ((rtc_1 & RTC_RTC_1_YEAR_BITS ) >> RTC_RTC_1_YEAR_LSB);
    t->month = (int8_t) ((rtc_1 & RTC_RTC_1_MONTH_BITS) >> RTC_RTC_1_MONTH_LSB);
    t->day   = (int8_t) ((rtc_1 & RTC_RTC_1_DAY_BITS  ) >> RTC_RTC_1_DAY_LSB);

    return true;
}

void rtc_enable_alarm(void) {
    // Set matching and wait for it to be enabled
    hw_set_bits(&rtc_hw->irq_setup_0, RTC_IRQ_SETUP_0_MATCH_ENA_BITS);
    while(!(rtc_hw->irq_setup_0 & RTC_IRQ_SETUP_0_MATCH_ACTIVE_BITS)) {
        tight_loop_contents();
    }
}



static void rtc_irq_handler(void) {
    // Always disable the alarm to clear the current IRQ.
    // Even if it is a repeatable alarm, we don't want it to keep firing.
    // If it matches on a second it can keep firing for that second.
    rtc_disable_alarm();

    if (_alarm_repeats) {
        // If it is a repeatable alarm, re enable the alarm.
        rtc_enable_alarm();
    }

    // Call user callback function
    if (_callback) {
        _callback();
    }
}

static bool rtc_alarm_repeats(datetime_t *t) {
    // If any value is set to -1 then we don't match on that value
    // hence the alarm will eventually repeat
    if (t->year  < 0) return true;
    if (t->month < 0) return true;
    if (t->day   < 0) return true;
    if (t->dotw  < 0) return true;
    if (t->hour  < 0) return true;
    if (t->min   < 0) return true;
    if (t->sec   < 0) return true;
    return false;
}

#include "zephyr/irq.h"
void rtc_set_alarm(datetime_t *t, rtc_callback_t user_callback) {
    rtc_disable_alarm();

    // Only add to setup if it isn't -1
    rtc_hw->irq_setup_0 = ((t->year  < 0) ? 0 : (((uint32_t)t->year)  << RTC_IRQ_SETUP_0_YEAR_LSB )) |
                          ((t->month < 0) ? 0 : (((uint32_t)t->month) << RTC_IRQ_SETUP_0_MONTH_LSB)) |
                          ((t->day   < 0) ? 0 : (((uint32_t)t->day)   << RTC_IRQ_SETUP_0_DAY_LSB  ));
    rtc_hw->irq_setup_1 = ((t->dotw  < 0) ? 0 : (((uint32_t)t->dotw)  << RTC_IRQ_SETUP_1_DOTW_LSB)) |
                          ((t->hour  < 0) ? 0 : (((uint32_t)t->hour)  << RTC_IRQ_SETUP_1_HOUR_LSB)) |
                          ((t->min   < 0) ? 0 : (((uint32_t)t->min)   << RTC_IRQ_SETUP_1_MIN_LSB )) |
                          ((t->sec   < 0) ? 0 : (((uint32_t)t->sec)   << RTC_IRQ_SETUP_1_SEC_LSB ));

    // Set the match enable bits for things we care about
    if (t->year  >= 0) hw_set_bits(&rtc_hw->irq_setup_0, RTC_IRQ_SETUP_0_YEAR_ENA_BITS);
    if (t->month >= 0) hw_set_bits(&rtc_hw->irq_setup_0, RTC_IRQ_SETUP_0_MONTH_ENA_BITS);
    if (t->day   >= 0) hw_set_bits(&rtc_hw->irq_setup_0, RTC_IRQ_SETUP_0_DAY_ENA_BITS);
    if (t->dotw  >= 0) hw_set_bits(&rtc_hw->irq_setup_1, RTC_IRQ_SETUP_1_DOTW_ENA_BITS);
    if (t->hour  >= 0) hw_set_bits(&rtc_hw->irq_setup_1, RTC_IRQ_SETUP_1_HOUR_ENA_BITS);
    if (t->min   >= 0) hw_set_bits(&rtc_hw->irq_setup_1, RTC_IRQ_SETUP_1_MIN_ENA_BITS);
    if (t->sec   >= 0) hw_set_bits(&rtc_hw->irq_setup_1, RTC_IRQ_SETUP_1_SEC_ENA_BITS);

    // Does it repeat? I.e. do we not match on any of the bits
    _alarm_repeats = rtc_alarm_repeats(t);

    // Store function pointer we can call later
    _callback = user_callback;

    irq_set_exclusive_handler(RTC_IRQ, rtc_irq_handler);

    // 
    // For some reason the interrupt attaching isn't working from the sdk code, so
    // with a "direct" connection, the sdk is back working as though native, and
    // zephyr stays out of it.
    // 
    // https://docs.zephyrproject.org/3.1.0/kernel/services/interrupts.html
    //
    // IRQ_CONNECT(RTC_IRQ, 7, rtc_irq_handler_zephyr_wrapper, 0, 0);
    IRQ_DIRECT_CONNECT(RTC_IRQ, 7, rtc_irq_handler, 0);


    // Enable the IRQ at the peri
    rtc_hw->inte = RTC_INTE_RTC_BITS;

    // Enable the IRQ at the proc
    IrqSetEnabled(RTC_IRQ, true);

    rtc_enable_alarm();
}

void rtc_disable_alarm(void) {
    // Disable matching and wait for it to stop being active
    hw_clear_bits(&rtc_hw->irq_setup_0, RTC_IRQ_SETUP_0_MATCH_ENA_BITS);
    while(rtc_hw->irq_setup_0 & RTC_IRQ_SETUP_0_MATCH_ACTIVE_BITS) {
        tight_loop_contents();
    }
}
