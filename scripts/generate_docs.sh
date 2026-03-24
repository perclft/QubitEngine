#!/bin/bash
set -e

echo "Generating API Documentation via protoc-gen-doc..."

# Ensure docs directory exists
mkdir -p docs

# Run the official DocGen docker image against our protos
# This avoids needing to install the Go plugin locally
docker run --rm \
  -v "$(pwd)/api/proto:/protos" \
  -v "$(pwd)/docs:/out" \
  pseudomuto/protoc-gen-doc --doc_opt=markdown,api_reference.md

echo "✅ Documentation successfully generated at docs/api_reference.md"
