#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>

class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString darkBackground READ darkBackground NOTIFY changed)
    Q_PROPERTY(QString darkerBackground READ darkerBackground NOTIFY changed)
    Q_PROPERTY(QString lighterBackground READ lighterBackground NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString darkForeground READ darkForeground NOTIFY changed)
    Q_PROPERTY(QString secondaryForeground READ secondaryForeground NOTIFY changed)
    Q_PROPERTY(QString lightForeground READ lightForeground NOTIFY changed)
    Q_PROPERTY(QString brightForeground READ brightForeground NOTIFY changed)
    Q_PROPERTY(QString selection READ selection NOTIFY changed)
    Q_PROPERTY(QString muted READ muted NOTIFY changed)
    Q_PROPERTY(QString red READ red NOTIFY changed)
    Q_PROPERTY(QString yellow READ yellow NOTIFY changed)
    Q_PROPERTY(QString orange READ orange NOTIFY changed)
    Q_PROPERTY(QString green READ green NOTIFY changed)
    Q_PROPERTY(QString cyan READ cyan NOTIFY changed)
    Q_PROPERTY(QString blue READ blue NOTIFY changed)
    Q_PROPERTY(QString magenta READ magenta NOTIFY changed)
    Q_PROPERTY(bool darkMode READ darkMode NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    QString background() const { return m_background; }
    QString darkBackground() const { return m_darkBackground; }
    QString darkerBackground() const { return m_darkerBackground; }
    QString lighterBackground() const { return m_lighterBackground; }
    QString foreground() const { return m_foreground; }
    QString darkForeground() const { return m_darkForeground; }
    QString secondaryForeground() const;
    QString lightForeground() const { return m_lightForeground; }
    QString brightForeground() const { return m_brightForeground; }
    QString selection() const { return m_selection; }
    QString muted() const { return m_muted; }
    QString red() const { return m_red; }
    QString yellow() const { return m_yellow; }
    QString orange() const { return m_orange; }
    QString green() const { return m_green; }
    QString cyan() const { return m_cyan; }
    QString blue() const { return m_blue; }
    QString magenta() const { return m_magenta; }
    bool darkMode() const { return m_darkMode; }
    int radius() const { return m_radius; }

    static QString readColor(const QString &content, const QString &key, const QString &fallback);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();
    QString resolveColorsPath() const;

    QString m_directory;
    QString m_accent = "#7393a7";
    QString m_background = "#14161a";
    QString m_darkBackground = "#0e1013";
    QString m_darkerBackground = "#090a0c";
    QString m_lighterBackground = "#1d2026";
    QString m_foreground = "#d8dde6";
    QString m_darkForeground = "#6f7785";
    QString m_lightForeground = "#b8c0cc";
    QString m_brightForeground = "#eceff4";
    QString m_selection = "#282d36";
    QString m_muted = "#555c69";
    QString m_red = "#c96f73";
    QString m_yellow = "#d1b379";
    QString m_orange = "#cf8e6d";
    QString m_green = "#7ea68d";
    QString m_cyan = "#6ea3a8";
    QString m_blue = "#7393a7";
    QString m_magenta = "#9c82a8";
    bool m_darkMode = true;

    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 8;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll;
    QTimer m_roundingTimeout;
};
