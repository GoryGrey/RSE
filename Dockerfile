# Base Image: Node.js + Build Tools
FROM node:20-slim

# Install C++ Compiler and Build Tools
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    git \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Set Working Directory
WORKDIR /app

# Build C++ kernel (optional)
COPY src/cpp_kernel ./src/cpp_kernel
RUN if [ -f "src/cpp_kernel/CMakeLists.txt" ]; then \
    cmake -S src/cpp_kernel -B build/cpp_kernel -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build/cpp_kernel; \
    fi

# Build Web Dashboard
COPY web_dashboard/package*.json ./web_dashboard/
RUN cd web_dashboard && npm install
COPY web_dashboard ./web_dashboard
RUN cd web_dashboard && npm run build

EXPOSE 4173

# Default Command: serve the built dashboard
CMD ["sh", "-c", "cd web_dashboard && npm run preview -- --host 0.0.0.0 --port 4173"]
