#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>

// ANSI color codes for cyberpunk aesthetic
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"

// Cyberpunk colors
#define CYAN    "\033[38;5;51m"
#define MAGENTA "\033[38;5;201m"
#define GREEN   "\033[38;5;46m"
#define YELLOW  "\033[38;5;226m"
#define RED     "\033[38;5;196m"
#define BLUE    "\033[38;5;33m"
#define PURPLE  "\033[38;5;141m"
#define ORANGE  "\033[38;5;208m"

// Background colors
#define BG_BLACK   "\033[40m"
#define BG_CYAN    "\033[48;5;51m"
#define BG_MAGENTA "\033[48;5;201m"

// Special effects
#define BLINK   "\033[5m"
#define REVERSE "\033[7m"

struct TelemetryFile {
    std::string path;
    std::string contents;
};

bool loadTelemetryFile(TelemetryFile* out) {
    if (!out) {
        return false;
    }
    const char* path = std::getenv("RSE_METRICS_PATH");
    if (!path || path[0] == '\0') {
        return false;
    }
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    out->path = path;
    out->contents = buffer.str();
    return true;
}

void printTelemetrySection(const std::string& title) {
    TelemetryFile telemetry;
    const bool has_telemetry = loadTelemetryFile(&telemetry);

    std::cout << CYAN << "┌─[ " << BOLD << title << RESET << CYAN
              << " ]─────────────────────────────────────────┐" << RESET << "\n";
    if (!has_telemetry) {
        std::cout << CYAN << "│ " << RED << "No telemetry loaded." << RESET << "\n";
        std::cout << CYAN << "│ " << DIM
                  << "Set RSE_METRICS_PATH to a real metrics log."
                  << RESET << "\n";
        std::cout << CYAN << "└───────────────────────────────────────────────┘"
                  << RESET << "\n\n";
        return;
    }

    std::cout << CYAN << "│ " << RESET << "Source: " << telemetry.path << "\n";
    std::cout << CYAN << "│" << RESET << "\n";

    std::istringstream lines(telemetry.contents);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty()) {
            std::cout << CYAN << "│" << RESET << "\n";
        } else {
            std::cout << CYAN << "│ " << RESET << line << "\n";
        }
    }
    std::cout << CYAN << "└───────────────────────────────────────────────┘"
              << RESET << "\n\n";
}

void clearScreen() {
    std::cout << "\033[2J\033[H";
}

void printBanner() {
    std::cout << CYAN << BOLD;
    std::cout << R"(
    ██████╗ ██████╗  █████╗ ██╗██████╗ ███████╗██╗  ██╗███████╗██╗     ██╗     
    ██╔══██╗██╔══██╗██╔══██╗██║██╔══██╗██╔════╝██║  ██║██╔════╝██║     ██║     
    ██████╔╝██████╔╝███████║██║██║  ██║███████╗███████║█████╗  ██║     ██║     
    ██╔══██╗██╔══██╗██╔══██║██║██║  ██║╚════██║██╔══██║██╔══╝  ██║     ██║     
    ██████╔╝██║  ██║██║  ██║██║██████╔╝███████║██║  ██║███████╗███████╗███████╗
    ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝╚═════╝ ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝
    )" << RESET << "\n";
    
    std::cout << MAGENTA << "    ╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "    ║  " << CYAN << "The Future of Computing" << MAGENTA << " │ " << GREEN << "Braided-Torus Architecture" << MAGENTA << "  ║\n";
    std::cout << "    ╚═══════════════════════════════════════════════════════════════╝" << RESET << "\n\n";
}

void printSystemInfo() {
    std::cout << CYAN << "┌─[ " << BOLD << "SYSTEM STATUS" << RESET << CYAN << " ]─────────────────────────────────────────┐" << RESET << "\n";
    
    // OS Info
    struct utsname uts;
    uname(&uts);
    
    std::cout << CYAN << "│ " << YELLOW << "OS:        " << RESET
              << GREEN << uts.sysname << " " << uts.release << RESET << "\n";
    std::cout << CYAN << "│ " << YELLOW << "Kernel:    " << RESET
              << PURPLE << uts.version << RESET << "\n";
    std::cout << CYAN << "│ " << YELLOW << "Arch:      " << RESET
              << BLUE << uts.machine << RESET << "\n";
    std::cout << CYAN << "│" << RESET << "\n";
    TelemetryFile telemetry;
    if (loadTelemetryFile(&telemetry)) {
        std::cout << CYAN << "│ " << ORANGE << BOLD << "⚙  TELEMETRY" << RESET << "\n";
        std::cout << CYAN << "│   " << RESET << "Source: " << telemetry.path << "\n";
    } else {
        std::cout << CYAN << "│ " << ORANGE << BOLD << "⚙  TELEMETRY" << RESET << "\n";
        std::cout << CYAN << "│   " << RED << "No telemetry loaded" << RESET << "\n";
        std::cout << CYAN << "│   " << DIM << "Set RSE_METRICS_PATH to a real metrics log."
                  << RESET << "\n";
    }
    std::cout << CYAN << "│" << RESET << "\n";
    
    // Features
    std::cout << CYAN << "│ " << PURPLE << BOLD << "✨ FEATURES" << RESET << "\n";
    std::cout << CYAN << "│   " << GREEN << "✓" << RESET << " Emergent Scheduling\n";
    std::cout << CYAN << "│   " << GREEN << "✓" << RESET << " Self-Healing (2-of-3)\n";
    std::cout << CYAN << "│   " << GREEN << "✓" << RESET << " Fault Tolerance\n";
    std::cout << CYAN << "│   " << GREEN << "✓" << RESET << " Zero Bottlenecks\n";
    
    std::cout << CYAN << "└───────────────────────────────────────────────────────────────┘" << RESET << "\n\n";
}

void printHelp() {
    std::cout << MAGENTA << "┌─[ " << BOLD << "AVAILABLE COMMANDS" << RESET << MAGENTA << " ]──────────────────────────────────────┐" << RESET << "\n";
    std::cout << MAGENTA << "│" << RESET << "\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "info" << RESET << "      - Show system information\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "torus" << RESET << "     - Display torus status\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "perf" << RESET << "      - Show performance metrics\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "stat" << RESET << "      - Show file metadata\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "matrix" << RESET << "    - Enter the matrix (animation)\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "help" << RESET << "      - Show this help\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "clear" << RESET << "     - Clear screen\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "exit" << RESET << "      - Exit BraidShell\n";
    std::cout << MAGENTA << "│" << RESET << "\n";
    std::cout << MAGENTA << "│ " << DIM << "Telemetry: set RSE_METRICS_PATH to a real metrics log" << RESET << "\n";
    std::cout << MAGENTA << "│" << RESET << "\n";
    std::cout << MAGENTA << "└───────────────────────────────────────────────────────────────┘" << RESET << "\n\n";
}

void printTorusStatus() {
    printTelemetrySection("TORUS TELEMETRY");
}

void printPerformance() {
    printTelemetrySection("PERFORMANCE METRICS");
}

void printStat(const std::string& path) {
    if (path.empty()) {
        std::cout << RED << "  ✗ stat requires a path" << RESET << "\n\n";
        return;
    }
    struct stat st {};
    if (::stat(path.c_str(), &st) != 0) {
        std::cout << RED << "  ✗ stat failed for: " << RESET << BOLD << path << RESET << "\n\n";
        return;
    }
    const char* type = "unknown";
    if (S_ISREG(st.st_mode)) {
        type = "file";
    } else if (S_ISDIR(st.st_mode)) {
        type = "dir";
    } else if (S_ISCHR(st.st_mode)) {
        type = "char";
    } else if (S_ISBLK(st.st_mode)) {
        type = "block";
    }
    std::cout << CYAN << "  stat " << RESET << path << "\n";
    std::cout << CYAN << "    size: " << RESET << st.st_size << "\n";
    std::cout << CYAN << "    type: " << RESET << type << "\n\n";
}

void printMatrix() {
    std::cout << GREEN;
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 80; j++) {
            char c = "01"[rand() % 2];
            std::cout << c;
        }
        std::cout << "\n";
        usleep(50000);
    }
    std::cout << RESET;
}

void printPrompt() {
    std::cout << CYAN << "┌─[" << RESET << MAGENTA << BOLD << "braid" << RESET << CYAN << "@" << RESET << GREEN << BOLD << "future" << RESET << CYAN << "]" << RESET << "\n";
    std::cout << CYAN << "└─" << PURPLE << "▶" << RESET << " ";
}

int main() {
    clearScreen();
    printBanner();
    
    std::cout << YELLOW << "  Welcome to the future of computing." << RESET << "\n";
    std::cout << DIM << "  Type 'help' for available commands." << RESET << "\n\n";
    
    std::string line;
    
    while (true) {
        printPrompt();
        
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty()) {
            continue;
        }
        
        std::cout << "\n";
        
        if (line == "exit" || line == "quit") {
            std::cout << MAGENTA << "  Exiting BraidShell..." << RESET << "\n";
            std::cout << CYAN << "  Stay degen. Stay future. 🚀" << RESET << "\n\n";
            break;
        }
        else if (line == "help") {
            printHelp();
        }
        else if (line == "info") {
            printSystemInfo();
        }
        else if (line == "torus") {
            printTorusStatus();
        }
        else if (line == "perf" || line == "performance") {
            printPerformance();
        }
        else if (line.rfind("stat ", 0) == 0) {
            std::string path = line.substr(5);
            path.erase(0, path.find_first_not_of(" \t"));
            path.erase(path.find_last_not_of(" \t") + 1);
            printStat(path);
        }
        else if (line == "clear") {
            clearScreen();
            printBanner();
        }
        else if (line == "matrix") {
            printMatrix();
        }
        else {
            std::cout << RED << "  ✗ Unknown command: " << RESET << BOLD << line << RESET << "\n";
            std::cout << DIM << "  Type 'help' for available commands." << RESET << "\n\n";
        }
    }
    
    return 0;
}
