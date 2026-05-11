#include "app/env_config.h"

#include <QProcessEnvironment>

namespace {
constexpr auto kDefaultBackendBaseUrl = "http://127.0.0.1:3000";
constexpr auto kDefaultLiveKitUrl = "wss://127.0.0.1:7880";
} // namespace

EnvConfig EnvConfig::fromEnvironment() {
  const auto env = QProcessEnvironment::systemEnvironment();
  EnvConfig config;
  config.backendBaseUrl =
      env.value("BACKEND_BASE_URL", kDefaultBackendBaseUrl);
  config.liveKitUrl = env.value("LIVEKIT_URL", kDefaultLiveKitUrl);
  return config;
}
