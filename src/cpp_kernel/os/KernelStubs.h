#ifndef RSE_KERNEL_STUBS_H
#define RSE_KERNEL_STUBS_H

#ifdef RSE_KERNEL

#include <stddef.h>

namespace std {

struct ostream {
    ostream& operator<<(const char*) { return *this; }
    ostream& operator<<(char) { return *this; }
    ostream& operator<<(int) { return *this; }
    ostream& operator<<(unsigned int) { return *this; }
    ostream& operator<<(long) { return *this; }
    ostream& operator<<(unsigned long) { return *this; }
    ostream& operator<<(long long) { return *this; }
    ostream& operator<<(unsigned long long) { return *this; }
    ostream& operator<<(double) { return *this; }
    ostream& operator<<(void*) { return *this; }
    ostream& operator<<(const void*) { return *this; }
    typedef ostream& (*manip_t)(ostream&);
    ostream& operator<<(manip_t) { return *this; }
    ostream& flush() { return *this; }
};

struct istream {
    istream& read(char*, size_t) { return *this; }
    size_t gcount() const { return 0; }
    bool getline(char*, size_t) { return false; }
};

inline ostream cout;
inline ostream cerr;
inline istream cin;

struct mutex {
    void lock() {}
    void unlock() {}
};

template <typename Mutex>
class lock_guard {
public:
    explicit lock_guard(Mutex& m) : m_(m) { m_.lock(); }
    ~lock_guard() { m_.unlock(); }

    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;

private:
    Mutex& m_;
};

inline ostream& endl(ostream& os) { return os; }
inline ostream& flush(ostream& os) { return os; }
inline ostream& hex(ostream& os) { return os; }
inline ostream& dec(ostream& os) { return os; }

} // namespace std

#endif

#endif
