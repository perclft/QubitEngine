#pragma once
#include <grpcpp/grpcpp.h>
#include <string>
#include <map>
#include <spdlog/spdlog.h>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>
#include "qubit_engine_export.h"

namespace qubit_engine {
namespace auth {

class QUBIT_ENGINE_EXPORT AuthInterceptor {
public:
  AuthInterceptor() = default;

  /**
   * @brief Validates the authentication of a gRPC request.
   * @param context The gRPC server context containing client metadata.
   * @return true if authentication is successful or skipped, false otherwise.
   */
  bool ValidateAuth(grpc::ServerContext *context) const;
  void ValidateAuthInternal(const std::map<std::string, std::string>& metadata) const;

private:
};

} // namespace auth
} // namespace qubit_engine
