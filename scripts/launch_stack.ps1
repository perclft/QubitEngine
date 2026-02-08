<#
.SYNOPSIS
    Launches the QubitEngine Webstack using Rancher Desktop (rdctl).
.DESCRIPTION
    This script executes docker-compose UP inside the Rancher Desktop VM.
    It bypasses Windows Docker pipe issues by running the build/launch directly in the VM.
.EXAMPLE
    .\scripts\launch_stack.ps1
#>

$ProjectRoot = Resolve-Path "$PSScriptRoot\.."
$RancherPath = "/mnt/c/Users/percl/projects/QubitEngine"

Write-Host "Starting QubitEngine Webstack via Rancher Desktop..." -ForegroundColor Cyan
Write-Host "Target Path (VM): $RancherPath" -ForegroundColor Gray

# Check for rdctl
if (-not (Get-Command "rdctl" -ErrorAction SilentlyContinue)) {
    Write-Error "Error: 'rdctl' command not found. Please verify Rancher Desktop is installed and in your PATH."
    exit 1
}

# The command to run inside the VM
# We use 'sh -c' to chain commands reliably
$RemoteCmd = "cd $RancherPath && docker-compose -f deploy/docker/docker-compose.yaml up -d"

Write-Host "Executing: $RemoteCmd" -ForegroundColor DarkGray

# Execute
rdctl shell sh -c "$RemoteCmd"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nSuccess! The stack is starting up." -ForegroundColor Green
    Write-Host "Frontend will be available at: http://localhost:5173" -ForegroundColor Green
    Write-Host "Use 'rdctl shell docker ps' to check status." -ForegroundColor Gray
} else {
    Write-Error "Failed to start stack. Exit Code: $LASTEXITCODE"
}
