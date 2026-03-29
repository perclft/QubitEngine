#include "../src/ServiceImpl.hpp"
#include "quantum.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>
#include <jwt-cpp/jwt.h>
#include <cstdlib>
#include <map>
#include <string>

using namespace qubit_engine;

class SecurityIntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
#ifdef _WIN32
    _putenv_s("QUBIT_ENGINE_JWT_SECRET", "test-secret-123");
    _putenv_s("QUBIT_ENGINE_SKIP_AUTH", "");
#else
    setenv("QUBIT_ENGINE_JWT_SECRET", "test-secret-123", 1);
    unsetenv("QUBIT_ENGINE_SKIP_AUTH");
#endif
  }

  std::string generate_valid_token() {
    auto token = jwt::create()
        .set_issuer("qubit-engine")
        .set_type("JWT")
        .sign(jwt::algorithm::hs256{"test-secret-123"});
    return token;
  }

  QubitEngineServiceImpl service;
};

TEST_F(SecurityIntegrationTest, ValidateAuthInternal_NoToken_Throws) {
  std::map<std::string, std::string> metadata;
  EXPECT_THROW(service.ValidateAuthInternal(metadata), std::runtime_error);
}

TEST_F(SecurityIntegrationTest, ValidateAuthInternal_ValidToken_Success) {
  std::map<std::string, std::string> metadata;
  metadata["authorization"] = "Bearer " + generate_valid_token();
  EXPECT_NO_THROW(service.ValidateAuthInternal(metadata));
}

TEST_F(SecurityIntegrationTest, ValidateAuthInternal_InvalidToken_Throws) {
  std::map<std::string, std::string> metadata;
  auto bad_token = jwt::create().sign(jwt::algorithm::hs256{"wrong-secret"});
  metadata["authorization"] = "Bearer " + bad_token;
  EXPECT_THROW(service.ValidateAuthInternal(metadata), std::exception);
}

TEST_F(SecurityIntegrationTest, ValidateAuthInternal_WrongIssuer_Throws) {
  std::map<std::string, std::string> metadata;
  auto bad_token = jwt::create()
      .set_issuer("wrong-issuer")
      .sign(jwt::algorithm::hs256{"test-secret-123"});
  metadata["authorization"] = "Bearer " + bad_token;
  // Currently we expect "qubit-engine" as issuer in the implementation
  EXPECT_THROW(service.ValidateAuthInternal(metadata), std::exception);
}
