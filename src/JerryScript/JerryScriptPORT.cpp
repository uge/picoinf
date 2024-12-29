#include "jerryscript-port.h"

// https://jerryscript.net/port-api/


// Process management
void jerry_port_init(void)
{
    // required
}

void jerry_port_fatal(jerry_fatal_code_t code)
{
    // TODO -- Fatal handler

    while (1);
}

// void jerry_port_sleep(uint32_t sleep_time)
// {

// }


// External context (not required with that feature disabled)
// size_t jerry_port_context_alloc(size_t context_size)
// {
// }

// struct jerry_context_t *jerry_port_context_get(void)
// {
// }

// void jerry_port_context_free(void)
// {

// }


// I/O
void jerry_port_log(const char *message_p)
{
    // required
}

void jerry_port_print_buffer(const jerry_char_t *buffer_p, jerry_size_t buffer_size)
{
    // required
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
    return nullptr;
}

void jerry_port_path_free(jerry_char_t *path_p)
{
    // required
}

jerry_size_t jerry_port_path_base(const jerry_char_t *path_p)
{
    return 0;
}

jerry_char_t *jerry_port_source_read(const char *file_name_p, jerry_size_t *out_size_p)
{
    return nullptr;
}

void jerry_port_source_free(jerry_char_t *buffer_p)
{
    // required
}


// Date
int32_t jerry_port_local_tza(double unix_ms)
{
    return 0;
}

double jerry_port_current_time(void)
{
    return 0;
}



