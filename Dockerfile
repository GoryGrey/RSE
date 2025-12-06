# Base Image: Node.js + Build Tools
FROM node:20-slim

# Install C++ Compiler and Build Tools
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    git \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Set Working Directory
WORKDIR /app

# Copy Project Files
COPY package*.json ./
COPY . .

# Install Node Dependencies
RUN npm install

# Create C++ Build Directory
RUN mkdir -p build
WORKDIR /app/build

# Compile BettiOS Kernel (C++)
# If CMakeLists.txt exists in src/cpp_kernel, use it
RUN if [ -f "../src/cpp_kernel/CMakeLists.txt" ]; then \
    cmake ../src/cpp_kernel && \
    make; \
    fi

WORKDIR /app

# Default Command: Run Verify Suite (JS + Metal)
CMD ["sh", "-c", "npx tsx scripts/verify_os_capability.ts && if [ -f build/bettios_kernel ]; then ./build/bettios_kernel; else echo '[Skipped] C++ Kernel not built'; fi"]
