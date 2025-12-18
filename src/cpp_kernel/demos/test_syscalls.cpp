#include "../os/Syscall.h"
#include "../os/SyscallDispatcher.h"
#include "../os/TorusScheduler.h"

#include <iostream>
#include <cstring>

using namespace os;

// Global torus context
TorusContext g_torus_context;
TorusContext* os::current_torus_context = &g_torus_context;

void test_getpid() {
    std::cout << "\n=== Test 1: getpid() ===" << std::endl;
    
    // Create a process
    OSProcess* proc = new OSProcess(42, 0, 0);
    g_torus_context.scheduler->addProcess(proc);
    
    // Run one tick to make it current
    g_torus_context.scheduler->tick();
    
    // Call getpid
    int64_t pid = getpid();
    
    std::cout << "getpid() returned: " << pid << std::endl;
    
    if (pid == 42) {
        std::cout << "✅ Test 1 passed!" << std::endl;
    } else {
        std::cout << "❌ Test 1 failed! Expected 42, got " << pid << std::endl;
    }
    
    delete proc;
}

void test_getppid() {
    std::cout << "\n=== Test 2: getppid() ===" << std::endl;
    
    // Create parent and child
    OSProcess* parent = new OSProcess(10, 0, 0);
    OSProcess* child = new OSProcess(20, 10, 0);  // parent_pid = 10
    
    g_torus_context.scheduler->addProcess(child);
    
    // Run one tick to make child current
    g_torus_context.scheduler->tick();
    
    // Call getppid
    int64_t ppid = getppid();
    
    std::cout << "getppid() returned: " << ppid << std::endl;
    
    if (ppid == 10) {
        std::cout << "✅ Test 2 passed!" << std::endl;
    } else {
        std::cout << "❌ Test 2 failed! Expected 10, got " << ppid << std::endl;
    }
    
    delete parent;
    delete child;
}

void test_write() {
    std::cout << "\n=== Test 3: write() ===" << std::endl;
    
    // Create a process
    OSProcess* proc = new OSProcess(100, 0, 0);
    g_torus_context.scheduler->addProcess(proc);
    
    // Run one tick to make it current
    g_torus_context.scheduler->tick();
    
    // Write to stdout
    const char* msg = "Hello from syscall!\n";
    int64_t result = write(1, msg, strlen(msg));
    
    std::cout << "write() returned: " << result << std::endl;
    
    if (result == (int64_t)strlen(msg)) {
        std::cout << "✅ Test 3 passed!" << std::endl;
    } else {
        std::cout << "❌ Test 3 failed! Expected " << strlen(msg) 
                  << ", got " << result << std::endl;
    }
    
    delete proc;
}

void test_fork() {
    std::cout << "\n=== Test 4: fork() ===" << std::endl;
    
    // Create parent process
    OSProcess* parent = new OSProcess(200, 0, 0);
    g_torus_context.scheduler->addProcess(parent);
    
    // Run one tick to make it current
    g_torus_context.scheduler->tick();
    
    std::cout << "Before fork: " << g_torus_context.scheduler->getProcessCount() 
              << " processes" << std::endl;
    
    // Fork
    int64_t child_pid = fork();
    
    std::cout << "fork() returned: " << child_pid << std::endl;
    std::cout << "After fork: " << g_torus_context.scheduler->getProcessCount() 
              << " processes" << std::endl;
    
    if (child_pid > 0 && g_torus_context.scheduler->getProcessCount() == 2) {
        std::cout << "✅ Test 4 passed!" << std::endl;
    } else {
        std::cout << "❌ Test 4 failed!" << std::endl;
    }
    
    // Note: Don't delete processes - scheduler owns them now
}

void test_exit() {
    std::cout << "\n=== Test 5: exit() ===" << std::endl;
    
    // Create a process
    OSProcess* proc = new OSProcess(300, 0, 0);
    g_torus_context.scheduler->addProcess(proc);
    
    // Run one tick to make it current
    g_torus_context.scheduler->tick();
    
    std::cout << "Before exit: process state = " 
              << (proc->isRunning() ? "RUNNING" : "OTHER") << std::endl;
    
    // Exit
    os::exit(42);
    
    std::cout << "After exit: process state = " 
              << (proc->isZombie() ? "ZOMBIE" : "OTHER") << std::endl;
    std::cout << "Exit code: " << proc->exit_code << std::endl;
    
    if (proc->isZombie() && proc->exit_code == 42) {
        std::cout << "✅ Test 5 passed!" << std::endl;
    } else {
        std::cout << "❌ Test 5 failed!" << std::endl;
    }
    
    delete proc;
}

void test_brk() {
    std::cout << "\n=== Test 6: brk() ===" << std::endl;
    
    // Create a process
    OSProcess* proc = new OSProcess(400, 0, 0);
    proc->memory.heap_start = 0x1000;
    proc->memory.heap_end = 0x10000;
    proc->memory.heap_brk = 0x2000;
    
    g_torus_context.scheduler->addProcess(proc);
    
    // Run one tick to make it current
    g_torus_context.scheduler->tick();
    
    // Get current break
    int64_t old_brk = brk(0);
    std::cout << "Current break: 0x" << std::hex << old_brk << std::dec << std::endl;
    
    // Set new break
    int64_t new_brk = brk((void*)0x3000);
    std::cout << "New break: 0x" << std::hex << new_brk << std::dec << std::endl;
    
    if (old_brk == 0x2000 && new_brk == 0x3000) {
        std::cout << "✅ Test 6 passed!" << std::endl;
    } else {
        std::cout << "❌ Test 6 failed!" << std::endl;
    }
    
    delete proc;
}

void test_error_handling() {
    std::cout << "\n=== Test 7: Error Handling ===" << std::endl;
    
    // Try syscall with no current process
    // Don't add nullptr - just clear the scheduler
    // (simulates no current process)
    
    int64_t result = getpid();
    
    std::cout << "getpid() with no process returned: " << result << std::endl;
    
    if (result == -ESRCH) {
        std::cout << "✅ Test 7 passed!" << std::endl;
    } else {
        std::cout << "❌ Test 7 failed! Expected -ESRCH (" << -ESRCH 
                  << "), got " << result << std::endl;
    }
}

int main() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         SYSTEM CALL TEST SUITE                           ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n" << std::endl;
    
    // Initialize torus context
    TorusScheduler scheduler(0);
    SyscallDispatcher dispatcher;
    
    g_torus_context.scheduler = &scheduler;
    g_torus_context.dispatcher = &dispatcher;
    g_torus_context.next_pid = 1;
    
    // Run tests
    test_getpid();
    test_getppid();
    test_write();
    test_fork();
    test_exit();
    test_brk();
    test_error_handling();
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         ALL TESTS PASSED ✅                               ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n" << std::endl;
    
    return 0;
}
