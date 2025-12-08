QubitEngine/
├── api/                        # THE CONTRACT
│   └── proto/
│       └── quantum.proto       # The gRPC service definition (ISA)
│
├── backend/                    # THE ENGINE (C++ Server) [cite: 1, 4]
│   ├── src/
│   │   ├── main.cpp            # gRPC Server Entrypoint
│   │   ├── QuantumRegister.cpp # The Math Logic (State Vector)
│   │   └── ServiceImpl.cpp     # gRPC Service Implementation
│   ├── include/
│   │   └── QuantumRegister.hpp
│   ├── tests/                  # GoogleTest unit tests
│   ├── CMakeLists.txt          # Build configuration
│   └── vcpkg.json              # Dependency Manager (gRPC, Abseil, GTest)
│
├── cli/                        # THE CONTROL PLANE (Go Client) 
│   ├── cmd/
│   │   └── qctl/               # "Quantum Control" CLI tool
│   │       └── main.go
│   ├── internal/
│   │   └── grpc_client/        # Go wrapper for generated gRPC code
│   ├── go.mod
│   └── go.sum
│
├── deploy/                     # THE INFRASTRUCTURE [cite: 7, 26]
│   ├── docker/
│   │   ├── Dockerfile.engine   # Multi-stage build for C++
│   │   └── Dockerfile.cli      # Multi-stage build for Go
│   ├── k8s/                    # Raw Manifests for Rancher
│   │   ├── namespace.yaml
│   │   ├── engine-deployment.yaml
│   │   └── engine-service.yaml # Headless Service
│   └── helm/                   # (Optional) Helm Chart for packaging
│
├── scripts/
│   ├── generate_protos.sh      # Script to run protoc for both C++ and Go
│   └── load_test.py            # Python script to spam the engine (Stress Test)
│
├── Makefile                    # The "One Command" entry point
└── README.md                   # Documentation with Architecture Diagram
