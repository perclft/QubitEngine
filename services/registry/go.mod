module github.com/perclft/QubitEngine/services/registry

go 1.23

replace github.com/perclft/QubitEngine/api => ../../api

require (
	github.com/google/uuid v1.6.0
	github.com/lib/pq v1.10.9
	github.com/perclft/QubitEngine/api v0.0.0-00010101000000-000000000000
	google.golang.org/grpc v1.68.0
	google.golang.org/protobuf v1.35.2
)

require (
	golang.org/x/net v0.29.0 // indirect
	golang.org/x/sys v0.25.0 // indirect
	golang.org/x/text v0.18.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20240903143218-8af14fe29dc1 // indirect
)
