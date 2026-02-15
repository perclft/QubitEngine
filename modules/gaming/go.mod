module github.com/perclft/QubitEngine/modules/gaming

go 1.24.0

require (
	github.com/perclft/QubitEngine v0.0.0-00010101000000-000000000000
	github.com/perclft/QubitEngine/api v0.0.0-20260208224746-b2f469a30b64
	google.golang.org/grpc v1.77.0
)

replace github.com/perclft/QubitEngine/api => ../../api

replace github.com/perclft/QubitEngine => ../../

require (
	golang.org/x/net v0.46.1-0.20251013234738-63d1a5100f82 // indirect
	golang.org/x/sys v0.37.0 // indirect
	golang.org/x/text v0.30.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20251022142026-3a174f9686a8 // indirect
	google.golang.org/protobuf v1.36.11 // indirect
)
