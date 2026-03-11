#include "app_config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
qint64 secondsToMs(const QJsonValue &value)
{
    return static_cast<qint64>(value.toDouble() * 1000.0);
}
}

bool loadAppConfig(const QString &filePath, AppConfig &config, QString *errorMessage)
{
    const QFileInfo configInfo(filePath);
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Cannot open config file: %1").arg(filePath);
        }
        return false;
    }

    const QByteArray raw = file.readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    config = {};
    const QJsonObject root = doc.object();
    config.hwaccel = root.value(QStringLiteral("hwaccel")).toString();

    const QJsonObject screensObject = root.value(QStringLiteral("screens")).toObject();
    for (auto it = screensObject.begin(); it != screensObject.end(); ++it) {
        bool ok = false;
        const int screenIndex = it.key().toInt(&ok);
        if (!ok || !it.value().isObject()) {
            continue;
        }

        const QJsonObject screenObject = it.value().toObject();
        ScreenEntry screen;
        screen.screenIndex = screenIndex;
        screen.cols = screenObject.value(QStringLiteral("cols")).toInt(-1);
        screen.flag = screenObject.value(QStringLiteral("flag")).toInt(0);

        const QJsonArray videosArray = screenObject.value(QStringLiteral("video")).toArray();
        for (const QJsonValue &videoValue : videosArray) {
            if (!videoValue.isObject()) {
                continue;
            }

            const QJsonObject videoObject = videoValue.toObject();
            VideoEntry entry;
            const QString rawPath = videoObject.value(QStringLiteral("path")).toString();
            entry.path = QFileInfo(configInfo.dir(), rawPath).absoluteFilePath();

            const QJsonArray sectionsArray = videoObject.value(QStringLiteral("play_sections")).toArray();
            for (const QJsonValue &sectionValue : sectionsArray) {
                if (!sectionValue.isObject()) {
                    continue;
                }

                const QJsonObject sectionObject = sectionValue.toObject();
                PlaySection section;
                section.startMs = secondsToMs(sectionObject.value(QStringLiteral("start_time")));
                section.durationMs = sectionObject.value(QStringLiteral("duration")).toDouble(-1.0) < 0.0
                    ? -1
                    : secondsToMs(sectionObject.value(QStringLiteral("duration")));
                entry.sections.push_back(section);
            }

            if (!entry.path.isEmpty()) {
                screen.videos.push_back(entry);
            }
        }

        if (!screen.videos.isEmpty()) {
            config.screens.push_back(screen);
        }
    }

    if (config.screens.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No valid screen entries found in config.");
        }
        return false;
    }

    return true;
}
