#pragma once

#include <stdexcept>
#include <string>
#include "qubit_engine_export.h"

namespace qubit_engine {

class QUBIT_ENGINE_EXPORT Exception : public std::runtime_error {
public:
  explicit Exception(const std::string &message)
      : std::runtime_error(message) {}
};

class QUBIT_ENGINE_EXPORT QubitOutOfRangeException : public Exception {
public:
  explicit QubitOutOfRangeException(const std::string &message)
      : Exception(message) {}
};

class QUBIT_ENGINE_EXPORT BackendUnavailableException : public Exception {
public:
  explicit BackendUnavailableException(const std::string &message)
      : Exception(message) {}
};

class QUBIT_ENGINE_EXPORT InvalidArgumentException : public Exception {
public:
  explicit InvalidArgumentException(const std::string &message)
      : Exception(message) {}
};

class QUBIT_ENGINE_EXPORT FeatureNotSupportedException : public Exception {
public:
  explicit FeatureNotSupportedException(const std::string &message)
      : Exception(message) {}
};

class QUBIT_ENGINE_EXPORT NonCliffordGateException : public Exception {
public:
  explicit NonCliffordGateException(const std::string &message)
      : Exception(message) {}
};

} // namespace qubit_engine
