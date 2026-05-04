#include "AuthInterceptor.hpp"
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <jwt-cpp/jwt.h>

namespace qubit_engine {
namespace auth {

bool AuthInterceptor::ValidateAuth(grpc::ServerContext *context) const {
#ifdef ENABLE_SKIP_AUTH
  // Compile-time guarded: this code path only exists in debug/test builds.
  // The ENABLE_SKIP_AUTH CMake option must be explicitly enabled.
  if (const char* skip_auth = std::getenv("QUBIT_ENGINE_SKIP_AUTH")) {
      if (std::string(skip_auth) == "1") {
          spdlog::warn("AUTH BYPASS ACTIVE — QUBIT_ENGINE_SKIP_AUTH=1 (debug build only)");
          return true;
      }
  }
#endif

  if (const char* secret_env = std::getenv("QUBIT_ENGINE_JWT_SECRET")) {
      // Secret is set, verification is mandatory
  } else {
      // SECURITY WARNING: Secret missing
      spdlog::error("CRITICAL: QUBIT_ENGINE_JWT_SECRET not set and SKIP_AUTH is not 1. Denying access.");
      return false; 
  }

  const auto& client_metadata = context->client_metadata();
  std::map<std::string, std::string> metadata;
  for (auto it = client_metadata.begin(); it != client_metadata.end(); ++it) {
      metadata[std::string(it->first.data(), it->first.length())] = 
          std::string(it->second.data(), it->second.length());
  }

  try {
    ValidateAuthInternal(metadata);
    return true;
  } catch (const std::exception& e) {
    spdlog::warn("Authentication failed: {}", e.what());
    return false;
  }
}

void AuthInterceptor::ValidateAuthInternal(const std::map<std::string, std::string>& metadata) const {
  auto iter = metadata.find("authorization");
  if (iter == metadata.end()) {
    throw std::runtime_error("Missing authorization token");
  }
  
  std::string token = iter->second;
  if (token.rfind("Bearer ", 0) == 0) {
    token = token.substr(7);
  }
  
  auto decoded = jwt::decode<jwt::traits::nlohmann_json>(token);
  if (!decoded.has_payload_claim("exp")) {
      throw std::runtime_error("Missing exp claim in JWT");
  }

  const char* env_secret = std::getenv("QUBIT_ENGINE_JWT_SECRET");
  if (!env_secret) throw std::runtime_error("Internal server error: JWT secret misconfigured");
  
  std::string secret = env_secret;
  auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
      .allow_algorithm(jwt::algorithm::hs256{secret})
      .with_issuer("qubit-engine");
  
  verifier.verify(decoded);
}

} // namespace auth
} // namespace qubit_engine
