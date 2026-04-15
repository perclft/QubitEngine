$PROTOC = "C:\Users\percl\.cargo\registry\src\index.crates.io-1949cf8c6b5b557f\protoc-bin-vendored-win32-3.2.0\bin\protoc.exe"
$PLUGIN = ".\web\node_modules\.bin\protoc-gen-ts_proto.cmd"
$PROTO_DIR = ".\api\proto"
$OUT_DIR = ".\web\src\api"

Write-Host "Regenerating Web Protobufs..."
& $PROTOC --plugin=protoc-gen-ts_proto=$PLUGIN `
         --ts_proto_out=$OUT_DIR `
         --ts_proto_opt=env=node,outputServices=grpc-js `
         -I . `
         "$PROTO_DIR\quantum.proto"

Write-Host "Done."
