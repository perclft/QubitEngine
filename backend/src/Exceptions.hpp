#pragma once

#include <stdexcept>
#include <string>

namespace qubit_engine {

class Exception : public std::runtime_error {
public:
  explicit Exception(const std::string &message)
      : std::runtime_error(message) {}
};

class QubitOutOfRangeException : public Exception {
public:
  explicit QubitOutOfRangeException(const std::string &message)
      : Exception(message) {}
};

class BackendUnavailableException : public Exception {
public:
  explicit BackendUnavailableException(const std::string &message)
      : Exception(message) {}
};

class InvalidArgumentException : public Exception {
public:
  explicit InvalidArgumentException(const std::string &message)
      : Exception(message) {}
};

class FeatureNotSupportedException : public Exception {
public:
  explicit FeatureNotSupportedException(const std::string &message)
      : Exception(message) {}
};

class NonCliffordGateException : public Exception {
public:
  explicit NonCliffordGateException(const std::string &message)
      : Exception(message) {}
};

} // namespace qubit_engine
