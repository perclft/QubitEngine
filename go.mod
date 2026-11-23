module github.com/perclft/QubitEngine

go 1.27.3

require (
	github.com/perclft/QubitEngine/api v0.0.0-20261122225608-b917fe819ca4
	golang.org/x/sys v0.49.0
	google.golang.org/grpc v1.83.2
)

require (
	github.com/golang-jwt/jwt/v5 v5.3.1 // indirect
	golang.org/x/net v0.62.0 // indirect
	golang.org/x/text v0.43.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20260526163538-3dc84a4a5aaa // indirect
	google.golang.org/protobuf v1.36.11 // indirect
)

replace github.com/perclft/QubitEngine/pkg/ipc => ./pkg/ipc

replace google.golang.org/grpc => google.golang.org/grpc v1.85.0
