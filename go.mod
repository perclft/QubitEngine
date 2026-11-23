module github.com/perclft/QubitEngine

go 1.27.3

require (
	github.com/perclft/QubitEngine/api v0.0.0-20261123010519-d207178fa81d
	golang.org/x/sys v0.49.0
	google.golang.org/grpc v1.84.0
)

require (
	github.com/golang-jwt/jwt/v5 v5.3.1 // indirect
	golang.org/x/net v0.62.0 // indirect
	golang.org/x/text v0.43.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20261110000943-09e21b237f2c // indirect
	google.golang.org/protobuf v1.36.12 // indirect
)

replace github.com/perclft/QubitEngine/pkg/ipc => ./pkg/ipc

replace google.golang.org/grpc => google.golang.org/grpc v1.85.0
