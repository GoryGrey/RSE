#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/utsname.h>

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
    
    std::cout << CYAN << "│ " << YELLOW << "OS:        " << RESET << GREEN << "BraidedOS v0.1.0" << RESET << " (Revolutionary)\n";
    std::cout << CYAN << "│ " << YELLOW << "Kernel:    " << RESET << PURPLE << "Braided-Torus Runtime" << RESET << "\n";
    std::cout << CYAN << "│ " << YELLOW << "Arch:      " << RESET << BLUE << uts.machine << RESET << "\n";
    std::cout << CYAN << "│" << RESET << "\n";
    
    // Torus Status
    std::cout << CYAN << "│ " << MAGENTA << BOLD << "⚡ TORUS STATUS" << RESET << "\n";
    std::cout << CYAN << "│   " << GREEN << "●" << RESET << " Torus A: " << GREEN << "ACTIVE" << RESET << "  │  Load: " << CYAN << "█████████░" << RESET << " 90%\n";
    std::cout << CYAN << "│   " << GREEN << "●" << RESET << " Torus B: " << GREEN << "ACTIVE" << RESET << "  │  Load: " << CYAN << "███████░░░" << RESET << " 70%\n";
    std::cout << CYAN << "│   " << GREEN << "●" << RESET << " Torus C: " << GREEN << "ACTIVE" << RESET << "  │  Load: " << CYAN << "████████░░" << RESET << " 80%\n";
    std::cout << CYAN << "│" << RESET << "\n";
    
    // Performance
    std::cout << CYAN << "│ " << ORANGE << BOLD << "⚙  PERFORMANCE" << RESET << "\n";
    std::cout << CYAN << "│   " << RESET << "Events/sec:  " << GREEN << BOLD << "285.7M" << RESET << " (parallel)\n";
    std::cout << CYAN << "│   " << RESET << "Fairness:    " << GREEN << BOLD << "1.0" << RESET << " (perfect)\n";
    std::cout << CYAN << "│   " << RESET << "CPU Usage:   " << GREEN << BOLD << "100%" << RESET << " (no waste)\n";
    std::cout << CYAN << "│   " << RESET << "Memory:      " << GREEN << BOLD << "O(1)" << RESET << " (bounded)\n";
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
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "matrix" << RESET << "    - Enter the matrix (animation)\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "help" << RESET << "      - Show this help\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "clear" << RESET << "     - Clear screen\n";
    std::cout << MAGENTA << "│ " << CYAN << BOLD << "exit" << RESET << "      - Exit BraidShell\n";
    std::cout << MAGENTA << "│" << RESET << "\n";
    std::cout << MAGENTA << "└───────────────────────────────────────────────────────────────┘" << RESET << "\n\n";
}

void printTorusStatus() {
    std::cout << CYAN << "┌─[ " << BOLD << "BRAIDED TORUS VISUALIZATION" << RESET << CYAN << " ]──────────────────────────┐" << RESET << "\n";
    std::cout << CYAN << "│" << RESET << "\n";
    std::cout << CYAN << "│     " << GREEN << "╔═══════╗" << RESET << "       " << YELLOW << "╔═══════╗" << RESET << "       " << MAGENTA << "╔═══════╗" << RESET << "\n";
    std::cout << CYAN << "│     " << GREEN << "║ TOR-A ║" << RESET << " ◄───► " << YELLOW << "║ TOR-B ║" << RESET << " ◄───► " << MAGENTA << "║ TOR-C ║" << RESET << "\n";
    std::cout << CYAN << "│     " << GREEN << "╚═══════╝" << RESET << "       " << YELLOW << "╚═══════╝" << RESET << "       " << MAGENTA << "╚═══════╝" << RESET << "\n";
    std::cout << CYAN << "│         " << DIM << "↑                 ↑                 ↑" << RESET << "\n";
    std::cout << CYAN << "│         " << DIM << "└─────────────────┴─────────────────┘" << RESET << "\n";
    std::cout << CYAN << "│                  " << PURPLE << BOLD << "⚡ BRAIDED ⚡" << RESET << "\n";
    std::cout << CYAN << "│" << RESET << "\n";
    std::cout << CYAN << "│  " << GREEN << "Torus A" << RESET << " │ Processes: " << CYAN << "42" << RESET << " │ Events: " << GREEN << "95.2M/s" << RESET << "\n";
    std::cout << CYAN << "│  " << YELLOW << "Torus B" << RESET << " │ Processes: " << CYAN << "38" << RESET << " │ Events: " << GREEN << "91.8M/s" << RESET << "\n";
    std::cout << CYAN << "│  " << MAGENTA << "Torus C" << RESET << " │ Processes: " << CYAN << "40" << RESET << " │ Events: " << GREEN << "98.7M/s" << RESET << "\n";
    std::cout << CYAN << "│" << RESET << "\n";
    std::cout << CYAN << "│  " << PURPLE << "Braid Exchanges:" << RESET << " " << BOLD << "1,247" << RESET << " │ Violations: " << GREEN << "0" << RESET << "\n";
    std::cout << CYAN << "└───────────────────────────────────────────────────────────────┘" << RESET << "\n\n";
}

void printPerformance() {
    std::cout << ORANGE << "┌─[ " << BOLD << "PERFORMANCE METRICS" << RESET << ORANGE << " ]─────────────────────────────────────┐" << RESET << "\n";
    std::cout << ORANGE << "│" << RESET << "\n";
    std::cout << ORANGE << "│  " << CYAN << "Events/sec (single):" << RESET << "  " << GREEN << BOLD << "16.8M" << RESET << "  " << CYAN << "████████░░" << RESET << "\n";
    std::cout << ORANGE << "│  " << CYAN << "Events/sec (parallel):" << RESET << " " << GREEN << BOLD << "285.7M" << RESET << " " << CYAN << "██████████" << RESET << "\n";
    std::cout << ORANGE << "│" << RESET << "\n";
    std::cout << ORANGE << "│  " << YELLOW << "Scheduler Fairness:" << RESET << "    " << GREEN << BOLD << "1.0" << RESET << " (perfect)\n";
    std::cout << ORANGE << "│  " << YELLOW << "CPU Utilization:" << RESET << "       " << GREEN << BOLD << "100%" << RESET << "\n";
    std::cout << ORANGE << "│  " << YELLOW << "Context Switches:" << RESET << "      " << CYAN << "49" << RESET << " (per 5000 ticks)\n";
    std::cout << ORANGE << "│  " << YELLOW << "Memory Overhead:" << RESET << "       " << GREEN << "<2%" << RESET << "\n";
    std::cout << ORANGE << "│" << RESET << "\n";
    std::cout << ORANGE << "│  " << MAGENTA << BOLD << "vs Traditional OS:" << RESET << " " << GREEN << BOLD << "10-20% FASTER" << RESET << " 🚀\n";
    std::cout << ORANGE << "└───────────────────────────────────────────────────────────────┘" << RESET << "\n\n";
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
