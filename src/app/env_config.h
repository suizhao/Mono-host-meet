#pragma once

#include <QString>

namespace app {

class EnvConfig {
public:
    EnvConfig();

    [[nodiscard]] const QString& apiBaseUrl() const;
    [[nodiscard]] const QString& defaultLiveKitUrl() const;

private:
    QString apiBaseUrl_;
    QString defaultLiveKitUrl_;
};

}  // namespace app
