#pragma once

#include "Device.h"
#include <cstring>
#ifdef RSE_KERNEL
#include "KernelStubs.h"
extern "C" void serial_write(const char *s);
#else
#include <iostream>
#endif

/**
 * Console Device Driver for Braided OS
 * 
 * Provides stdin, stdout, stderr through a character device.
 * Host builds use std::cin/cout; kernel builds route through serial output.
 */

namespace os {

/**
 * Console device private data.
 */
struct ConsoleData {
    char input_buffer[1024];   // Line buffer for input
    uint32_t input_size;       // Current size of input buffer
    uint32_t input_pos;        // Current read position
    bool has_input;            // Is there buffered input?
    
    ConsoleData() : input_size(0), input_pos(0), has_input(false) {
        input_buffer[0] = '\0';
    }
};

/**
 * Console device operations.
 */

// Open console
int console_open(Device* dev) {
    (void)dev;
    std::cout << "[Console] Opened" << std::endl;
    return 0;
}

// Close console
int console_close(Device* dev) {
    (void)dev;
    std::cout << "[Console] Closed" << std::endl;
    return 0;
}

// Read from console (stdin)
ssize_t console_read(Device* dev, void* buf, size_t count) {
#ifdef RSE_KERNEL
    (void)dev;
    (void)buf;
    (void)count;
    return 0;
#else
    ConsoleData* data = (ConsoleData*)dev->private_data;
    
    // If no buffered input, read a line
    if (!data->has_input || data->input_pos >= data->input_size) {
        std::cout << "[Console] Reading line..." << std::endl;
        
        // Read line from stdin
        if (!std::cin.getline(data->input_buffer, sizeof(data->input_buffer))) {
            return -1;  // EOF or error
        }
        
        data->input_size = strlen(data->input_buffer);
        
        // Add newline back (getline removes it)
        if (data->input_size < sizeof(data->input_buffer) - 1) {
            data->input_buffer[data->input_size++] = '\n';
            data->input_buffer[data->input_size] = '\0';
        }
        
        data->input_pos = 0;
        data->has_input = true;
    }
    
    // Copy from buffer to user buffer
    size_t available = data->input_size - data->input_pos;
    size_t to_copy = (count < available) ? count : available;
    
    memcpy(buf, data->input_buffer + data->input_pos, to_copy);
    data->input_pos += to_copy;
    
    // If we've consumed all input, mark as no input
    if (data->input_pos >= data->input_size) {
        data->has_input = false;
    }
    
    return to_copy;
#endif
}

// Write to console (stdout/stderr)
ssize_t console_write(Device* dev, const void* buf, size_t count) {
    (void)dev;
#ifdef RSE_KERNEL
    const char* data = (const char*)buf;
    for (size_t i = 0; i < count; ++i) {
        char tmp[2] = {data[i], '\0'};
        serial_write(tmp);
    }
    return (ssize_t)count;
#else
    // Write directly to stdout (no buffering for simplicity)
    std::cout.write((const char*)buf, count);
    std::cout.flush();
    
    return count;
#endif
}

constexpr unsigned long CONSOLE_IOCTL_GET_PENDING = 0x52534510ul;
constexpr unsigned long CONSOLE_IOCTL_CLEAR_INPUT = 0x52534511ul;

// ioctl
int console_ioctl(Device* dev, unsigned long request, void* arg) {
    if (!dev) {
        return -1;
    }
#ifdef RSE_KERNEL
    (void)arg;
    if (request == CONSOLE_IOCTL_GET_PENDING) {
        return 0;
    }
    if (request == CONSOLE_IOCTL_CLEAR_INPUT) {
        return 0;
    }
    return -1;
#else
    ConsoleData* data = (ConsoleData*)dev->private_data;
    if (!data) {
        return -1;
    }
    if (request == CONSOLE_IOCTL_GET_PENDING) {
        if (!arg) {
            return -1;
        }
        uint32_t available = 0;
        if (data->has_input && data->input_size >= data->input_pos) {
            available = data->input_size - data->input_pos;
        }
        *static_cast<uint32_t*>(arg) = available;
        return 0;
    }
    if (request == CONSOLE_IOCTL_CLEAR_INPUT) {
        data->input_size = 0;
        data->input_pos = 0;
        data->has_input = false;
        data->input_buffer[0] = '\0';
        return 0;
    }
    return -1;
#endif
}

/**
 * Create and initialize console device.
 */
Device* create_console_device() {
#ifdef RSE_KERNEL
    static Device dev_pool[8];
    static ConsoleData data_pool[8];
    static uint32_t next_slot = 0;

    if (next_slot >= 8) {
        serial_write("[RSE] console device pool exhausted\n");
        return nullptr;
    }

    Device* dev = &dev_pool[next_slot];
    ConsoleData* data = &data_pool[next_slot];
    next_slot++;

    dev->name[0] = '\0';
    dev->type = DeviceType::CHARACTER;
    dev->private_data = data;
    dev->open = console_open;
    dev->close = console_close;
    dev->read = console_read;
    dev->write = console_write;
    dev->ioctl = console_ioctl;

    data->input_size = 0;
    data->input_pos = 0;
    data->has_input = false;
    data->input_buffer[0] = '\0';

    strncpy(dev->name, "console", sizeof(dev->name) - 1);
    return dev;
#else
    Device* dev = new Device();

    strncpy(dev->name, "console", sizeof(dev->name) - 1);
    dev->type = DeviceType::CHARACTER;
    dev->private_data = new ConsoleData();

    dev->open = console_open;
    dev->close = console_close;
    dev->read = console_read;
    dev->write = console_write;
    dev->ioctl = console_ioctl;

    return dev;
#endif
}

/**
 * Destroy console device.
 */
void destroy_console_device(Device* dev) {
    if (dev->private_data) {
        delete (ConsoleData*)dev->private_data;
    }
    delete dev;
}

} // namespace os
