# Build Wasm Kernel using Docker
Write-Host "🐳 Checking Docker..." -ForegroundColor Cyan
docker --version
if ($LASTEXITCODE -ne 0) {
    Write-Error "Docker is not found! Please install Docker Desktop."
    exit 1
}

Write-Host "🛠️  Building Builder Image..." -ForegroundColor Yellow
cd "$PSScriptRoot\COG\wasm"
docker build -t cog-wasm-builder .
if ($LASTEXITCODE -ne 0) {
    Write-Error "Docker Build Failed. Is Docker Desktop running?"
    exit 1
}

Write-Host "🚀 Compiling C++ Kernel to Wasm..." -ForegroundColor Green
cd "$PSScriptRoot"
# Mount root so we can access COG/genesis_cpp
docker run --rm -v "${PWD}:/src" cog-wasm-builder
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation Failed"
    exit 1
}

Write-Host "✅ Wasm Artifacts Generated in output/" -ForegroundColor Green
# Copy to Visor public dir
Copy-Item "output\genesis.js" "COG\visor\public\" -Force
Copy-Item "output\genesis.wasm" "COG\visor\public\" -Force
