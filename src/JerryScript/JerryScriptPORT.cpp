#include "jerryscript-port.h"

#include "Log.h"
#include "PAL.h"
#include "UART.h"

// https://jerryscript.net/port-api/


// Process management
void jerry_port_init()
{
    // Gets called every time the VM starts
    // Nothing to do
}

void jerry_port_fatal(jerry_fatal_code_t code)
{
    const char *reason = nullptr;

    switch (code)
    {
    case JERRY_FATAL_OUT_OF_MEMORY        : reason = "jerry_port_fatal: Out of Memory"        ; break;
    case JERRY_FATAL_REF_COUNT_LIMIT      : reason = "jerry_port_fatal: RefCount Limit"       ; break;
    case JERRY_FATAL_DISABLED_BYTE_CODE   : reason = "jerry_port_fatal: Disabled Byte Code"   ; break;
    case JERRY_FATAL_UNTERMINATED_GC_LOOPS: reason = "jerry_port_fatal: Unterminated GC Loops"; break;
    case JERRY_FATAL_FAILED_ASSERTION     : reason = "jerry_port_fatal: Failed Assertion"     ; break;
    default                               : reason = "jerry_port_fatal: Unknown"              ; break;
    }

    PAL.Fatal(reason);

    // should never get this far
    while (true);
}

// void jerry_port_sleep(uint32_t sleep_time)
// {

// }


// External context (not required with that feature disabled)
// size_t jerry_port_context_alloc(size_t context_size)
// {
// }

// struct jerry_context_t *jerry_port_context_get()
// {
// }

// void jerry_port_context_free()
// {

// }


// I/O
void jerry_port_log(const char *message)
{
    LogNNL(message);
}

void jerry_port_print_buffer(const jerry_char_t *buf, jerry_size_t bufSize)
{
    UartSend((uint8_t *)buf, (uint16_t)bufSize);
}

// jerry_char_t *jerry_port_line_read(jerry_size_t *out_size_p)
// {

// }

// void jerry_port_line_free(jerry_char_t *buffer_p)
// {

// }


// Filesystem
jerry_char_t *jerry_port_path_normalize(const jerry_char_t *path_p, jerry_size_t path_size)
{
    Log("==================== jerry_port_path_normalize ====================");

    return nullptr;
}

void jerry_port_path_free(jerry_char_t *path_p)
{
    Log("==================== jerry_port_path_free ====================");
}

jerry_size_t jerry_port_path_base(const jerry_char_t *path_p)
{
    Log("==================== jerry_port_path_base ====================");

    return 0;
}

jerry_char_t *jerry_port_source_read(const char *file_name_p, jerry_size_t *out_size_p)
{
    Log("==================== jerry_port_source_read ====================");

    return nullptr;
}

void jerry_port_source_free(jerry_char_t *buffer_p)
{
    Log("==================== jerry_port_source_free ====================");
}


// Date
int32_t jerry_port_local_tza(double unix_ms)
{
    return 0;
}

double jerry_port_current_time()
{
    return PAL.Millis();
}



