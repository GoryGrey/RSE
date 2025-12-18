#include "../os/Device.h"
#include "../os/ConsoleDevice.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace os;

void test_device_manager() {
    std::cout << "\n=== Test 1: Device Manager ===" << std::endl;
    
    DeviceManager dm;
    
    // Create some devices
    Device* console = create_console_device();
    
    Device* null_dev = new Device();
    strncpy(null_dev->name, "null", sizeof(null_dev->name) - 1);
    null_dev->type = DeviceType::CHARACTER;
    
    Device* zero_dev = new Device();
    strncpy(zero_dev->name, "zero", sizeof(zero_dev->name) - 1);
    zero_dev->type = DeviceType::CHARACTER;
    
    // Register devices
    assert(dm.registerDevice(console));
    assert(dm.registerDevice(null_dev));
    assert(dm.registerDevice(zero_dev));
    
    assert(dm.count() == 3);
    
    // Look up devices
    assert(dm.lookup("console") == console);
    assert(dm.lookup("null") == null_dev);
    assert(dm.lookup("zero") == zero_dev);
    assert(dm.lookup("nonexistent") == nullptr);
    
    // List devices
    dm.list();
    
    // Unregister
    assert(dm.unregisterDevice("null"));
    assert(dm.count() == 2);
    
    dm.printStats();
    
    // Cleanup
    destroy_console_device(console);
    delete null_dev;
    delete zero_dev;
    
    std::cout << "✅ Test 1 passed!" << std::endl;
}

void test_console_write() {
    std::cout << "\n=== Test 2: Console Write ===" << std::endl;
    
    Device* console = create_console_device();
    
    // Open
    assert(console->open(console) == 0);
    
    // Write
    const char* msg = "Hello from console!\n";
    ssize_t written = console->write(console, msg, strlen(msg));
    assert(written == strlen(msg));
    
    // Close
    assert(console->close(console) == 0);
    
    destroy_console_device(console);
    
    std::cout << "✅ Test 2 passed!" << std::endl;
}

void test_console_read() {
    std::cout << "\n=== Test 3: Console Read ===" << std::endl;
    std::cout << "NOTE: This test requires manual input. Type 'test' and press Enter:" << std::endl;
    
    Device* console = create_console_device();
    
    // Open
    assert(console->open(console) == 0);
    
    // Read
    char buffer[100] = {0};
    ssize_t bytes_read = console->read(console, buffer, sizeof(buffer));
    
    std::cout << "Read " << bytes_read << " bytes: \"" << buffer << "\"" << std::endl;
    
    // Close
    assert(console->close(console) == 0);
    
    destroy_console_device(console);
    
    std::cout << "✅ Test 3 passed!" << std::endl;
}

void test_console_multiple_reads() {
    std::cout << "\n=== Test 4: Console Multiple Reads ===" << std::endl;
    std::cout << "NOTE: Type 'hello world' and press Enter:" << std::endl;
    
    Device* console = create_console_device();
    console->open(console);
    
    // Read in small chunks
    char buffer1[6] = {0};
    char buffer2[6] = {0};
    char buffer3[10] = {0};
    
    ssize_t r1 = console->read(console, buffer1, 5);
    ssize_t r2 = console->read(console, buffer2, 5);
    ssize_t r3 = console->read(console, buffer3, 10);
    
    std::cout << "Read 1 (" << r1 << " bytes): \"" << buffer1 << "\"" << std::endl;
    std::cout << "Read 2 (" << r2 << " bytes): \"" << buffer2 << "\"" << std::endl;
    std::cout << "Read 3 (" << r3 << " bytes): \"" << buffer3 << "\"" << std::endl;
    
    console->close(console);
    destroy_console_device(console);
    
    std::cout << "✅ Test 4 passed!" << std::endl;
}

void test_device_operations() {
    std::cout << "\n=== Test 5: Device Operations ===" << std::endl;
    
    DeviceManager dm;
    Device* console = create_console_device();
    dm.registerDevice(console);
    
    // Look up and use device
    Device* dev = dm.lookup("console");
    assert(dev != nullptr);
    assert(dev == console);
    
    // Open
    dev->open(dev);
    
    // Write
    const char* msg = "Testing device operations\n";
    dev->write(dev, msg, strlen(msg));
    
    // Close
    dev->close(dev);
    
    destroy_console_device(console);
    
    std::cout << "✅ Test 5 passed!" << std::endl;
}

void test_automated() {
    std::cout << "\n=== Test 6: Automated Tests (No Input Required) ===" << std::endl;
    
    // Test 1: Device creation
    Device* console = create_console_device();
    assert(console != nullptr);
    assert(strcmp(console->name, "console") == 0);
    assert(console->type == DeviceType::CHARACTER);
    assert(console->open != nullptr);
    assert(console->close != nullptr);
    assert(console->read != nullptr);
    assert(console->write != nullptr);
    
    // Test 2: Device manager
    DeviceManager dm;
    assert(dm.registerDevice(console));
    assert(dm.count() == 1);
    assert(dm.lookup("console") == console);
    
    // Test 3: Write operation
    console->open(console);
    const char* test_msg = "Automated test message\n";
    ssize_t written = console->write(console, test_msg, strlen(test_msg));
    assert(written == strlen(test_msg));
    console->close(console);
    
    // Cleanup
    destroy_console_device(console);
    
    std::cout << "✅ Test 6 passed!" << std::endl;
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         I/O SYSTEM TEST SUITE                            ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝" << std::endl;
    
    test_device_manager();
    test_console_write();
    test_automated();
    
    // Interactive tests (commented out for automated testing)
    // Uncomment to test console input:
    // test_console_read();
    // test_console_multiple_reads();
    
    test_device_operations();
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         ALL TESTS PASSED ✅                               ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝" << std::endl;
    
    return 0;
}
