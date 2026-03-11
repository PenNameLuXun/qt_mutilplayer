#pragma once

#include <QString>
#include <QVector>

struct PlaySection
{
    qint64 startMs = 0;
    qint64 durationMs = -1;
};

struct VideoEntry
{
    QString path;
    QVector<PlaySection> sections;
};

struct ScreenEntry
{
    int screenIndex = 0;
    int cols = -1;
    int flag = 0;
    QVector<VideoEntry> videos;
};

struct AppConfig
{
    QString hwaccel;
    QVector<ScreenEntry> screens;
};

bool loadAppConfig(const QString &filePath, AppConfig &config, QString *errorMessage);
