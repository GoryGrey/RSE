
# RSE Verification Container
# Use this to verify O(1) memory scaling in an isolated environment.

FROM node:20-alpine

WORKDIR /app

# Copy Config
COPY package*.json ./
COPY tsconfig*.json ./

# Install Dependencies
RUN npm ci

# Copy Source & Scripts
COPY src ./src
COPY scripts ./scripts
COPY benchmarks ./benchmarks

# Build TypeScript
RUN npx tsc

# Default Command: Run Scientific Validation
CMD ["npx", "tsx", "scripts/validate_scientific.ts"]
