# Project Genesis Launcher
Write-Host "🌌 Initializing Project Genesis..." -ForegroundColor Cyan

# 1. Build CLI
Write-Host "🛠️  Building COG CLI..." -ForegroundColor Yellow
cd "$PSScriptRoot\COG\cli"
npm install
npm run build
if ($LASTEXITCODE -ne 0) { Write-Error "CLI Build Failed"; exit 1 }

# 2. Build Visor
Write-Host "🕶️  Building Visor..." -ForegroundColor Yellow
cd "$PSScriptRoot\COG\visor"
npm install
# We don't need to build for dev, just install

# 3. Instruction
Write-Host "`n✅ Build Complete." -ForegroundColor Green
Write-Host "To run the demo, open 3 PowerShell terminals:`n"
Write-Host "1. [DAEMON]   cd COG\cli; node dist/index.js spin -p 3000" -ForegroundColor Magenta
Write-Host "2. [VISOR]    cd COG\visor; npm run dev" -ForegroundColor Magenta
Write-Host "3. [GENESIS]  cd COG\genesis; node ..\cli\dist\index.js deploy" -ForegroundColor Magenta
