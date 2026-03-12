#include "app_config.h"
#include "screen_player_window.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QByteArray>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QScreen>
#include <QTimer>

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_MEDIA_BACKEND")) {
        qputenv("QT_MEDIA_BACKEND", QByteArrayLiteral("windows"));
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("QtMultiPlayer"));
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/app.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt Widgets multi-screen video player"));
    parser.addHelpOption();

    QCommandLineOption configOption(
        QStringList() << QStringLiteral("f") << QStringLiteral("file"),
        QStringLiteral("Path to config JSON."),
        QStringLiteral("file"),
        QStringLiteral("C:/Users/groot/Documents/project/mutilplayer/config_local.json"));
    parser.addOption(configOption);

    QCommandLineOption testModeOption(
        QStringLiteral("test-mode"),
        QStringLiteral("Run in smoke-test mode."));
    parser.addOption(testModeOption);

    QCommandLineOption validateConfigOption(
        QStringLiteral("validate-config"),
        QStringLiteral("Validate config and exit."));
    parser.addOption(validateConfigOption);

    QCommandLineOption quitAfterOption(
        QStringLiteral("quit-after-ms"),
        QStringLiteral("Auto quit after the given milliseconds."),
        QStringLiteral("ms"),
        QStringLiteral("0"));
    parser.addOption(quitAfterOption);

    parser.process(app);

    const QString configPath = QFileInfo(parser.value(configOption)).absoluteFilePath();
    AppConfig config;
    QString errorMessage;
    if (!loadAppConfig(configPath, config, &errorMessage)) {
        QMessageBox::critical(nullptr, QStringLiteral("Config Error"), errorMessage);
        return 1;
    }

    if (parser.isSet(validateConfigOption)) {
        return 0;
    }

    const QList<QScreen *> screens = QGuiApplication::screens();
    QVector<ScreenPlayerWindow *> windows;
    windows.reserve(config.screens.size());
    int remainingWindows = 0;

    for (const ScreenEntry &entry : std::as_const(config.screens)) {
        if (entry.screenIndex < 0 || entry.screenIndex >= screens.size()) {
            continue;
        }

        auto *window = new ScreenPlayerWindow(entry);
        QObject::connect(window, &ScreenPlayerWindow::closedByUser, &app, [&app, &remainingWindows]() {
            --remainingWindows;
            if (remainingWindows <= 0) {
                app.quit();
            }
        });
        window->placeOnScreen(screens.at(entry.screenIndex)->geometry());
        window->show();
        windows.push_back(window);
        ++remainingWindows;
    }

    if (windows.isEmpty()) {
        QMessageBox::critical(nullptr, QStringLiteral("Screen Error"), QStringLiteral("No valid screen configuration could be applied."));
        return 1;
    }

    const bool testMode = parser.isSet(testModeOption);
    const int quitAfterMs = parser.value(quitAfterOption).toInt();
    if (testMode || quitAfterMs > 0) {
        const int delay = quitAfterMs > 0 ? quitAfterMs : 1000;
        QTimer::singleShot(delay, &app, &QCoreApplication::quit);
    }

    const int exitCode = app.exec();
    for (ScreenPlayerWindow *window : std::as_const(windows)) {
        window->stop();
    }
    qDeleteAll(windows);
    return exitCode;
}
