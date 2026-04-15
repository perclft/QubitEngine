# Load environment variables from .env
if (Test-Path ".env") {
    Get-Content ".env" | ForEach-Object {
        $line = $_.Trim()
        if ($line -and -not $line.StartsWith("#")) {
            if ($line -match '^([^=]+)=(.*)$') {
                $name = $matches[1].Trim()
                $value = $matches[2].Trim()
                [System.Environment]::SetEnvironmentVariable($name, $value, "Process")
                Write-Host "Set env: $name"
            }
        }
    }
}

# Ensure SKIP_AUTH is 1 for dev
$env:QUBIT_ENGINE_SKIP_AUTH = "1"

Write-Host "Starting QubitEngine..."
Start-Process -FilePath ".\bin\Release\qubit_engine.exe" -NoNewWindow

Write-Host "Starting Web Frontend..."
cd web
Start-Process -FilePath "npm.cmd" -ArgumentList "run dev" -NoNewWindow
cd ..

Write-Host "Services started with environment variables."
