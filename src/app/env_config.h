#pragma once

#include <QString>

struct EnvConfig {
  QString backendBaseUrl;
  QString liveKitUrl;

  static EnvConfig fromEnvironment();
};
