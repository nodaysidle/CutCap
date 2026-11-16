#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cmath>

Theme::Theme(const QString &directory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.config/omarchy/current" : directory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    m_hyprctl = !hyprctl.isEmpty()
                    ? hyprctl
                    : (directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
                           ? QStandardPaths::findExecutable("hyprctl")
                           : QString());

    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this] { m_roundingTimeout.stop(); });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (code != 0 || status != QProcess::NormalExit || !value.isDouble() || value.toInt(-1) < 0)
            return;
        if (value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    if (!m_hyprctl.isEmpty()) {
        const QString signature = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
        const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        if (!signature.isEmpty() && !runtime.isEmpty()) {
            const QString socketPath = QString("%1/hypr/%2/.socket2.sock").arg(runtime, signature);
            connect(&m_hyprSocket, &QLocalSocket::readyRead, this, [this] {
                if (QString::fromUtf8(m_hyprSocket.readAll()).contains("configreloaded"))
                    refreshRounding();
            });
            connect(&m_hyprSocket, &QLocalSocket::connected, this, [this] { m_hyprReconnect.stop(); });
            connect(&m_hyprSocket, &QLocalSocket::disconnected, this, [this] { m_hyprReconnect.start(); });
            connect(&m_hyprSocket, &QLocalSocket::errorOccurred, this, [this] { m_hyprReconnect.start(); });
            m_hyprReconnect.setInterval(30000);
            connect(&m_hyprReconnect, &QTimer::timeout, this, [this, socketPath] {
                if (m_hyprSocket.state() == QLocalSocket::UnconnectedState)
                    m_hyprSocket.connectToServer(socketPath);
            });
            m_hyprSocket.connectToServer(socketPath);
        }
    }

    reload();
}

Theme::~Theme() {
    m_hyprSocket.abort();
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

QString Theme::readColor(const QString &content, const QString &key, const QString &fallback) {
    const QRegularExpression expression(
        QString(R"re(^\s*%1\s*=\s*["'](#[0-9a-fA-F]{6})["'])re").arg(QRegularExpression::escape(key)),
        QRegularExpression::MultilineOption);
    const auto match = expression.match(content);
    return match.hasMatch() ? match.captured(1) : fallback;
}

static double relativeLuminance(const QColor &c) {
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    return .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
}

static double contrastRatio(const QColor &a, const QColor &b) {
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    return (std::max(la, lb) + .05) / (std::min(la, lb) + .05);
}

QString Theme::accentForeground() const {
    return relativeLuminance(QColor(m_accent)) > .179 ? "#090a0c" : "#eceff4";
}

// Labels, hints and timecodes: the theme's dark_foreground, pushed away from the panel surfaces until
// it reads at WCAG AA (4.5:1) on every one of them, so muted text stays legible in any theme.
QString Theme::secondaryForeground() const {
    const QList<QColor> surfaces{QColor(m_darkerBackground), QColor(m_darkBackground), QColor(m_background),
                                 QColor(m_lighterBackground), QColor(m_selection)};
    const QColor from(m_darkForeground);
    const QColor to(m_foreground);
    auto worstRatio = [&surfaces](const QColor &c) {
        double worst = 21.0;
        for (const auto &surface : surfaces)
            worst = std::min(worst, contrastRatio(c, surface));
        return worst;
    };
    for (int step = 0; step <= 20; ++step) {
        const double t = step / 20.0;
        const QColor mixed = QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * t,
                                              from.greenF() + (to.greenF() - from.greenF()) * t,
                                              from.blueF() + (to.blueF() - from.blueF()) * t);
        if (worstRatio(mixed) >= 4.6)
            return mixed.name();
    }
    return m_foreground;
}

QString Theme::resolveColorsPath() const {
    const QString primary = m_directory + "/theme/colors.toml";
    if (QFileInfo::exists(primary))
        return primary;
    const QString fallback = QDir::homePath() + "/.local/state/omarchy/current/theme/colors.toml";
    if (QFileInfo::exists(fallback))
        return fallback;
    return primary;
}

void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates) {
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);
    }

    QFile file(resolveColorsPath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QString content = QString::fromUtf8(file.readAll());
    bool anyChanged = false;
    auto update = [&](QString &target, const QString &key, const QString &fallback) {
        const QString next = readColor(content, key, fallback);
        if (next != target) {
            target = next;
            anyChanged = true;
        }
    };

    update(m_accent, "accent", "#7393a7");
    update(m_background, "background", "#14161a");
    update(m_darkBackground, "dark_background", "#0e1013");
    update(m_darkerBackground, "darker_background", "#090a0c");
    update(m_lighterBackground, "lighter_background", "#1d2026");
    update(m_foreground, "foreground", "#d8dde6");
    update(m_darkForeground, "dark_foreground", "#6f7785");
    update(m_lightForeground, "light_foreground", "#b8c0cc");
    update(m_brightForeground, "bright_foreground", "#eceff4");
    update(m_selection, "selection", "#282d36");
    update(m_muted, "muted", "#555c69");
    update(m_red, "red", "#c96f73");
    update(m_yellow, "yellow", "#d1b379");
    update(m_orange, "orange", "#cf8e6d");
    update(m_green, "green", "#7ea68d");
    update(m_cyan, "cyan", "#6ea3a8");
    update(m_blue, "blue", "#7393a7");
    update(m_magenta, "magenta", "#9c82a8");

    const QRegularExpression modeRe(R"re(^\s*mode\s*=\s*["'](light|dark)["'])re",
                                    QRegularExpression::MultilineOption);
    const auto modeMatch = modeRe.match(content);
    const bool nextDark = !modeMatch.hasMatch() || modeMatch.captured(1) == "dark";
    if (nextDark != m_darkMode) {
        m_darkMode = nextDark;
        anyChanged = true;
    }

    if (anyChanged)
        emit changed();
}
