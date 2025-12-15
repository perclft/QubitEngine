param (
    [string]$Target = "all"
)

$ErrorActionPreference = "Stop"
$Root = Get-Location

function Get-VcpkgPath {
    $VcpkgScript = "scripts\buildsystems\vcpkg.cmake"
    $PossiblePaths = @("C:\vcpkg", "$env:USERPROFILE\vcpkg", "$Root\vcpkg")
    
    foreach ($path in $PossiblePaths) {
        if (Test-Path "$path\$VcpkgScript") {
            return $path
        }
    }
    return $null
}

function Build-Clean {
    Write-Host "🧹 Cleaning build artifacts..." -ForegroundColor Cyan
    $PathsToRemove = @(
        "$Root\backend\build",
        "$Root\cli\internal\generated\*",
        "$Root\bin\*.exe"
    )
    
    foreach ($path in $PathsToRemove) {
        if (Test-Path $path) {
            Write-Host "   Removing $path" -ForegroundColor Gray
            Remove-Item -Recurse -Force $path -ErrorAction SilentlyContinue
        }
    }
    Write-Host "✅ Clean complete." -ForegroundColor Green
}

function Ensure-GoPlugins {
    Write-Host "Checking Go Protobuf plugins..." -ForegroundColor Cyan
    
    $GoPath = go env GOPATH
    if (-not $GoPath) { $GoPath = "$env:USERPROFILE\go" }
    $GoBin = "$GoPath\bin"
    
    # Add to env path just in case
    if ($env:PATH -notmatch [regex]::Escape($GoBin)) {
        $env:PATH += ";$GoBin"
    }

    if (!(Test-Path "$GoBin\protoc-gen-go.exe")) {
        Write-Host "Installing protoc-gen-go..." -ForegroundColor Yellow
        go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
    }
    
    if (!(Test-Path "$GoBin\protoc-gen-go-grpc.exe")) {
        Write-Host "Installing protoc-gen-go-grpc..." -ForegroundColor Yellow
        go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest
    }
    
    return $GoBin
}

function Ensure-CMake {
    if (Get-Command cmake -ErrorAction SilentlyContinue) { return }
    
    Write-Host "CMake not found in PATH. Searching standard locations..." -ForegroundColor Yellow
    
    $SearchPaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
        "C:\Program Files\CMake\bin"
    )

    $VcpkgRoot = Get-VcpkgPath
    if ($VcpkgRoot) {
        $SearchPaths += (Get-ChildItem "$VcpkgRoot\downloads\tools\cmake*" -Recurse -Filter "cmake.exe" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty DirectoryName -Unique)
    }

    foreach ($path in $SearchPaths) {
        if (Test-Path "$path\cmake.exe") {
            Write-Host "Found CMake at $path. Adding to PATH for this session." -ForegroundColor Green
            $env:PATH += ";$path"
            return
        }
    }
    
    Write-Error "CMake not found. Please install CMake."
}

function Build-Proto {
    Write-Host "Generating Protobufs..." -ForegroundColor Cyan
    $GoBin = Ensure-GoPlugins

    $ProtoDir = "$Root\api\proto"
    $GoOut = "$Root\cli\internal\generated"
    
    if (!(Test-Path $GoOut)) { New-Item -ItemType Directory -Force -Path $GoOut | Out-Null }
    
    $ProtocExe = "protoc"
    if (!(Get-Command protoc -ErrorAction SilentlyContinue)) {
        $VcpkgRoot = Get-VcpkgPath
        if ($VcpkgRoot) {
            $PotentialProtoc = "$VcpkgRoot\installed\x64-windows\tools\protobuf\protoc.exe"
            if (Test-Path $PotentialProtoc) { $ProtocExe = $PotentialProtoc }
            else { Write-Error "protoc not found in VCPKG." }
        } else { Write-Error "protoc not found." }
    }

    # Use explicit plugins to avoid PATH issues
    $PluginGo = "$GoBin\protoc-gen-go.exe"
    $PluginGrpc = "$GoBin\protoc-gen-go-grpc.exe"

    if (!(Test-Path $PluginGo) -or !(Test-Path $PluginGrpc)) {
        Write-Error "Go plugins not found in $GoBin"
    }

    & $ProtocExe -I $ProtoDir `
        --plugin=protoc-gen-go=$PluginGo --go_out=$GoOut --go_opt=paths=source_relative `
        --plugin=protoc-gen-go-grpc=$PluginGrpc --go-grpc_out=$GoOut --go-grpc_opt=paths=source_relative `
        "$ProtoDir\quantum.proto"
        
    Write-Host "Protobufs generated." -ForegroundColor Green
}

function Build-CPP {
    Write-Host "Building C++ Engine (Native Windows)..." -ForegroundColor Cyan
    Ensure-CMake
    
    $BuildDir = "$Root\backend\build"
    
    if (!(Test-Path $BuildDir)) { New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null }
    
    Push-Location $BuildDir
    
    $VcpkgRoot = Get-VcpkgPath
    if (-not $VcpkgRoot) {
        Write-Warning "Could not find vcpkg automatically. Assuming C:\vcpkg..."
        $VcpkgRoot = "C:\vcpkg"
    }
    
    $VcpkgToolchain = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
    Write-Host "Using VCPKG Toolchain: $VcpkgToolchain" -ForegroundColor Gray
    
    # Quote arguments to prevent variable expansion issues in cache
    cmake .. "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain" "-DVCPKG_TARGET_TRIPLET=x64-windows"
    cmake --build . --config Release
    
    Pop-Location
    Write-Host "C++ Engine built." -ForegroundColor Green
}

function Build-Go {
    Write-Host "Building Go Services..." -ForegroundColor Cyan
    
    if (Test-Path "$Root\modules\gaming") {
        cd "$Root\modules\gaming"
        go build -o "$Root\bin\gaming_service.exe" .
        Write-Host "   Built gaming_service.exe" -ForegroundColor Gray
    }
    
    if (Test-Path "$Root\cli") {
        cd "$Root\cli"
        go build -o "$Root\bin\qctl.exe" ./cmd/qctl
        Write-Host "   Built qctl.exe" -ForegroundColor Gray
    }
    
    cd $Root
    Write-Host "Go services built." -ForegroundColor Green
}

function Run-Tests {
    Write-Host "Running Tests..." -ForegroundColor Cyan
    
    $TestExe = "$Root\backend\build\Release\tests.exe"
    
    if (Test-Path $TestExe) {
        Write-Host "Executing: $TestExe" -ForegroundColor Gray
        & $TestExe
    } else {
        # Fallback to CTest if exe not found directly
        if (Test-Path "$Root\backend\build") {
            Push-Location "$Root\backend\build"
            ctest -C Release --output-on-failure
            Pop-Location
        } else {
            Write-Error "Build directory not found."
        }
    }
}

# Router
switch ($Target) {
    "clean" { Build-Clean }
    "proto" { Build-Proto }
    "cpp"   { Build-CPP }
    "go"    { Build-Go }
    "test"  { Run-Tests }
    "all"   { Build-Clean; Build-Proto; Build-CPP; Build-Go }
    default { Write-Host "Usage: .\build.ps1 [clean|proto|cpp|go|test|all]" -ForegroundColor Red }
}
