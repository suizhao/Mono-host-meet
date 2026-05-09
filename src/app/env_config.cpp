#include "app/env_config.h"

#include <QtGlobal>

namespace app {

EnvConfig::EnvConfig()
    : apiBaseUrl_(qEnvironmentVariable("MULTILIVE_API_BASE_URL", "http://127.0.0.1:3000")),
      defaultLiveKitUrl_(qEnvironmentVariable("MULTILIVE_LIVEKIT_URL", "")) {}

const QString& EnvConfig::apiBaseUrl() const {
    return apiBaseUrl_;
}

const QString& EnvConfig::defaultLiveKitUrl() const {
    return defaultLiveKitUrl_;
}

}  // namespace app
