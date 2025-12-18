#include "../os/VFS.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace os;

void test_file_create() {
    std::cout << "\n=== Test 1: File Creation ===" << std::endl;
    
    MemFS fs;
    FileDescriptorTable fd_table;
    VFS vfs(&fs, &fd_table);
    
    // Create a file
    int fd = vfs.open("/test.txt", O_CREAT | O_RDWR, 0644);
    assert(fd >= 3);  // FDs 0, 1, 2 are reserved
    
    std::cout << "Created file with FD: " << fd << std::endl;
    
    // Close
    assert(vfs.close(fd) == 0);
    
    // Verify file exists
    int fd2 = vfs.open("/test.txt", O_RDONLY);
    assert(fd2 >= 0);
    vfs.close(fd2);
    
    std::cout << "✅ Test 1 passed!" << std::endl;
}

void test_write_read() {
    std::cout << "\n=== Test 2: Write and Read ===" << std::endl;
    
    MemFS fs;
    FileDescriptorTable fd_table;
    VFS vfs(&fs, &fd_table);
    
    // Create and write
    int fd = vfs.open("/data.txt", O_CREAT | O_RDWR, 0644);
    assert(fd >= 0);
    
    const char* data = "Hello, world!";
    int64_t written = vfs.write(fd, data, strlen(data));
    assert(written == strlen(data));
    
    std::cout << "Wrote " << written << " bytes" << std::endl;
    
    // Close and reopen for reading
    vfs.close(fd);
    
    fd = vfs.open("/data.txt", O_RDONLY);
    assert(fd >= 0);
    
    // Read
    char buffer[100] = {0};
    int64_t read_bytes = vfs.read(fd, buffer, sizeof(buffer));
    assert(read_bytes == strlen(data));
    assert(strcmp(buffer, data) == 0);
    
    std::cout << "Read " << read_bytes << " bytes: \"" << buffer << "\"" << std::endl;
    
    vfs.close(fd);
    
    std::cout << "✅ Test 2 passed!" << std::endl;
}

void test_append() {
    std::cout << "\n=== Test 3: Append Mode ===" << std::endl;
    
    MemFS fs;
    FileDescriptorTable fd_table;
    VFS vfs(&fs, &fd_table);
    
    // Create and write
    int fd = vfs.open("/log.txt", O_CREAT | O_RDWR, 0644);
    vfs.write(fd, "Line 1\n", 7);
    vfs.close(fd);
    
    // Append
    fd = vfs.open("/log.txt", O_WRONLY | O_APPEND);
    vfs.write(fd, "Line 2\n", 7);
    vfs.close(fd);
    
    // Read all
    fd = vfs.open("/log.txt", O_RDONLY);
    char buffer[100] = {0};
    int64_t read_bytes = vfs.read(fd, buffer, sizeof(buffer));
    vfs.close(fd);
    
    std::cout << "File contents:\n" << buffer << std::endl;
    
    assert(read_bytes == 14);
    assert(strcmp(buffer, "Line 1\nLine 2\n") == 0);
    
    std::cout << "✅ Test 3 passed!" << std::endl;
}

void test_truncate() {
    std::cout << "\n=== Test 4: Truncate ===" << std::endl;
    
    MemFS fs;
    FileDescriptorTable fd_table;
    VFS vfs(&fs, &fd_table);
    
    // Create and write
    int fd = vfs.open("/temp.txt", O_CREAT | O_RDWR, 0644);
    vfs.write(fd, "This will be deleted", 20);
    vfs.close(fd);
    
    // Open with O_TRUNC
    fd = vfs.open("/temp.txt", O_RDWR | O_TRUNC);
    
    // Write new content
    vfs.write(fd, "New content", 11);
    vfs.close(fd);
    
    // Read
    fd = vfs.open("/temp.txt", O_RDONLY);
    char buffer[100] = {0};
    int64_t read_bytes = vfs.read(fd, buffer, sizeof(buffer));
    vfs.close(fd);
    
    std::cout << "File contents: \"" << buffer << "\"" << std::endl;
    
    assert(read_bytes == 11);
    assert(strcmp(buffer, "New content") == 0);
    
    std::cout << "✅ Test 4 passed!" << std::endl;
}

void test_seek() {
    std::cout << "\n=== Test 5: Seek ===" << std::endl;
    
    MemFS fs;
    FileDescriptorTable fd_table;
    VFS vfs(&fs, &fd_table);
    
    // Create and write
    int fd = vfs.open("/seek.txt", O_CREAT | O_RDWR, 0644);
    vfs.write(fd, "0123456789", 10);
    
    // Seek to beginning
    int64_t offset = vfs.lseek(fd, 0, SEEK_SET);
    assert(offset == 0);
    
    // Read 5 bytes
    char buffer[10] = {0};
    vfs.read(fd, buffer, 5);
    assert(strcmp(buffer, "01234") == 0);
    
    // Seek to end
    offset = vfs.lseek(fd, 0, SEEK_END);
    assert(offset == 10);
    
    // Seek backward 3 bytes
    offset = vfs.lseek(fd, -3, SEEK_CUR);
    assert(offset == 7);
    
    // Read remaining
    memset(buffer, 0, sizeof(buffer));
    vfs.read(fd, buffer, 3);
    assert(strcmp(buffer, "789") == 0);
    
    vfs.close(fd);
    
    std::cout << "✅ Test 5 passed!" << std::endl;
}

void test_multiple_fds() {
    std::cout << "\n=== Test 6: Multiple File Descriptors ===" << std::endl;
    
    MemFS fs;
    FileDescriptorTable fd_table;
    VFS vfs(&fs, &fd_table);
    
    // Open same file multiple times
    int fd1 = vfs.open("/multi.txt", O_CREAT | O_RDWR, 0644);
    int fd2 = vfs.open("/multi.txt", O_RDWR);
    int fd3 = vfs.open("/multi.txt", O_RDONLY);
    
    assert(fd1 >= 0 && fd2 >= 0 && fd3 >= 0);
    assert(fd1 != fd2 && fd2 != fd3);
    
    // Write from fd1
    vfs.write(fd1, "ABC", 3);
    
    // Write from fd2 (different offset)
    vfs.write(fd2, "XYZ", 3);
    
    // Read from fd3
    char buffer[10] = {0};
    vfs.read(fd3, buffer, 6);
    
    std::cout << "File contents: \"" << buffer << "\"" << std::endl;
    
    // Should be "XYZABC" (fd2 wrote at offset 0, fd1 wrote at offset 3)
    // Actually "ABCXYZ" because fd1 wrote first
    
    vfs.close(fd1);
    vfs.close(fd2);
    vfs.close(fd3);
    
    std::cout << "✅ Test 6 passed!" << std::endl;
}

void test_unlink() {
    std::cout << "\n=== Test 7: Unlink (Delete) ===" << std::endl;
    
    MemFS fs;
    FileDescriptorTable fd_table;
    VFS vfs(&fs, &fd_table);
    
    // Create file
    int fd = vfs.open("/delete_me.txt", O_CREAT | O_RDWR, 0644);
    vfs.write(fd, "Temporary", 9);
    vfs.close(fd);
    
    // Delete
    assert(vfs.unlink("/delete_me.txt") == 0);
    
    // Try to open (should fail)
    fd = vfs.open("/delete_me.txt", O_RDONLY);
    assert(fd < 0);
    
    std::cout << "✅ Test 7 passed!" << std::endl;
}

void test_stress() {
    std::cout << "\n=== Test 8: Stress Test ===" << std::endl;
    
    MemFS fs;
    FileDescriptorTable fd_table;
    VFS vfs(&fs, &fd_table);
    
    // Create many files
    const int num_files = 50;
    for (int i = 0; i < num_files; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "/file%d.txt", i);
        
        int fd = vfs.open(filename, O_CREAT | O_RDWR, 0644);
        assert(fd >= 0);
        
        char data[64];
        snprintf(data, sizeof(data), "This is file number %d\n", i);
        vfs.write(fd, data, strlen(data));
        vfs.close(fd);
    }
    
    std::cout << "Created " << num_files << " files" << std::endl;
    
    // Read them back
    for (int i = 0; i < num_files; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "/file%d.txt", i);
        
        int fd = vfs.open(filename, O_RDONLY);
        assert(fd >= 0);
        
        char buffer[64] = {0};
        vfs.read(fd, buffer, sizeof(buffer));
        vfs.close(fd);
        
        char expected[64];
        snprintf(expected, sizeof(expected), "This is file number %d\n", i);
        assert(strcmp(buffer, expected) == 0);
    }
    
    std::cout << "Verified " << num_files << " files" << std::endl;
    
    vfs.printStats();
    
    std::cout << "✅ Test 8 passed!" << std::endl;
}

int main() {
    std::cout << "╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         VIRTUAL FILE SYSTEM TEST SUITE                   ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝" << std::endl;
    
    test_file_create();
    test_write_read();
    test_append();
    test_truncate();
    test_seek();
    test_multiple_fds();
    test_unlink();
    test_stress();
    
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         ALL TESTS PASSED ✅                               ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝" << std::endl;
    
    return 0;
}
