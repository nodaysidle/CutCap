#include "design.h"

#include <algorithm>
#include <cmath>

Design::Design(Theme &theme, QObject *parent) : QObject(parent), m_theme(theme) {
    connect(&theme, &Theme::changed, this, &Design::changed);
}

int Design::rXs() const {
    return 2;
}

int Design::rSm() const {
    return std::min(3, std::max(1, m_theme.radius()));
}

int Design::rMd() const {
    return std::min(4, std::max(2, m_theme.radius()));
}

int Design::rLg() const {
    return std::min(6, std::max(3, m_theme.radius()));
}

QString Design::surfaceSunken() const {
    return m_theme.darkerBackground();
}

QString Design::surfacePanel() const {
    return m_theme.background();
}

QString Design::surfaceRaised() const {
    return m_theme.lighterBackground();
}

QString Design::surfaceOverlay() const {
    return m_theme.darkBackground();
}

QString Design::borderHairline() const {
    return m_theme.selection();
}

QString Design::borderStrong() const {
    return m_theme.muted();
}

QString Design::focusRing() const {
    return m_theme.accent();
}

QString Design::textPrimary() const {
    return m_theme.brightForeground();
}

QString Design::textSecondary() const {
    return m_theme.secondaryForeground();
}

QString Design::textDisabled() const {
    return alpha(m_theme.secondaryForeground(), 0.55);
}

QString Design::accent() const {
    return m_theme.accent();
}

QString Design::onAccent() const {
    return m_theme.accentForeground();
}

QString Design::success() const {
    return m_theme.green();
}

QString Design::warning() const {
    return m_theme.yellow();
}

QString Design::danger() const {
    return m_theme.red();
}

QString Design::info() const {
    return m_theme.cyan();
}

QString Design::laneV2() const {
    return m_theme.magenta();
}

QString Design::laneV1() const {
    return m_theme.blue();
}

QString Design::laneText() const {
    return m_theme.yellow();
}

QString Design::laneAudio() const {
    return m_theme.green();
}

QString Design::alpha(const QString &hex, double a) const {
    QColor c(hex);
    if (!c.isValid())
        return hex;
    return QString("%1%2").arg(c.name(), QString("%1").arg(int(std::lround(std::clamp(a, 0.0, 1.0) * 255.0)), 2, 16, QChar('0')));
}

QString Design::mix(const QString &a, const QString &b, double t) const {
    const QColor from(a);
    const QColor to(b);
    if (!from.isValid() || !to.isValid())
        return a;
    const double k = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * k, from.greenF() + (to.greenF() - from.greenF()) * k,
                            from.blueF() + (to.blueF() - from.blueF()) * k)
        .name();
}
