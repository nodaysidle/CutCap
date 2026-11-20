#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "theme.h"

class QQmlEngine;
class QJSEngine;

// CutCap design tokens. One source for spacing, type, radii, control heights, motion and the
// semantic colour roles, so QML never invents a 7px margin or a 13px caption. Colour roles are
// derived from the live Omarchy theme, so a theme switch restyles the app without touching QML.
class Design : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // Grid: 2px hairline step, 4px base, 8px rhythm, 24/32 for panel padding.
    Q_PROPERTY(int s1 READ s1 CONSTANT)   // 2
    Q_PROPERTY(int s2 READ s2 CONSTANT)   // 4
    Q_PROPERTY(int s3 READ s3 CONSTANT)   // 6
    Q_PROPERTY(int s4 READ s4 CONSTANT)   // 8
    Q_PROPERTY(int s5 READ s5 CONSTANT)   // 12
    Q_PROPERTY(int s6 READ s6 CONSTANT)   // 16
    Q_PROPERTY(int s7 READ s7 CONSTANT)   // 24
    Q_PROPERTY(int s8 READ s8 CONSTANT)   // 32

    // Type scale for a dense desktop editor. Technical values use the mono face below.
    Q_PROPERTY(QString fontUi READ fontUi CONSTANT)
    Q_PROPERTY(QString fontMono READ fontMono CONSTANT)
    Q_PROPERTY(int tMicro READ tMicro CONSTANT)      // 12 - kbd, ruler, badges
    Q_PROPERTY(int tCaption READ tCaption CONSTANT)  // 12 - labels, metadata
    Q_PROPERTY(int tBody READ tBody CONSTANT)        // 13 - default text
    Q_PROPERTY(int tBodyLg READ tBodyLg CONSTANT)    // 14 - values, clip names
    Q_PROPERTY(int tTitle READ tTitle CONSTANT)      // 18 - panel title
    Q_PROPERTY(int tDisplay READ tDisplay CONSTANT)  // 24 - empty-state headline
    Q_PROPERTY(int wRegular READ wRegular CONSTANT)  // 400
    Q_PROPERTY(int wMedium READ wMedium CONSTANT)    // 500
    Q_PROPERTY(int wSemibold READ wSemibold CONSTANT)// 600

    // Pointer targets never fall below 24 px.
    Q_PROPERTY(int hCompact READ hCompact CONSTANT)  // 24
    Q_PROPERTY(int hSmall READ hSmall CONSTANT)      // 28
    Q_PROPERTY(int hStd READ hStd CONSTANT)          // 32
    Q_PROPERTY(int hLarge READ hLarge CONSTANT)      // 38
    Q_PROPERTY(int rowH READ rowH CONSTANT)          // 34 - list rows
    Q_PROPERTY(int trackH READ trackH CONSTANT)       // 48 - timeline lane
    Q_PROPERTY(int topBarH READ topBarH CONSTANT)     // 44
    Q_PROPERTY(int gutter READ gutter CONSTANT)       // 1 - the only gap between panels

    // Radii: sharp by design. Panels are square; only chips/controls round.
    Q_PROPERTY(int rXs READ rXs CONSTANT)
    Q_PROPERTY(int rSm READ rSm NOTIFY changed)
    Q_PROPERTY(int rMd READ rMd NOTIFY changed)
    Q_PROPERTY(int rLg READ rLg NOTIFY changed)

    // Motion: crisp hover/press only, no loops, no bounce.
    Q_PROPERTY(int msFast READ msFast CONSTANT)   // 90
    Q_PROPERTY(int msBase READ msBase CONSTANT)   // 140
    Q_PROPERTY(int msSlow READ msSlow CONSTANT)   // 220 - panel/dialog reveal

    // Surfaces, lowest to highest.
    Q_PROPERTY(QString surfaceSunken READ surfaceSunken NOTIFY changed)  // canvas, timeline bed
    Q_PROPERTY(QString surfacePanel READ surfacePanel NOTIFY changed)    // default panel
    Q_PROPERTY(QString surfaceRaised READ surfaceRaised NOTIFY changed)  // cards, controls
    Q_PROPERTY(QString surfaceOverlay READ surfaceOverlay NOTIFY changed)// popovers, dialogs

    // Lines: hairlines define structure; shadows are not used for elevation.
    Q_PROPERTY(QString borderHairline READ borderHairline NOTIFY changed)
    Q_PROPERTY(QString borderStrong READ borderStrong NOTIFY changed)
    Q_PROPERTY(QString focusRing READ focusRing NOTIFY changed)

    // Text tiers. Tertiary is for non-essential glyphs only; never for content.
    Q_PROPERTY(QString textPrimary READ textPrimary NOTIFY changed)
    Q_PROPERTY(QString textSecondary READ textSecondary NOTIFY changed)
    Q_PROPERTY(QString textDisabled READ textDisabled NOTIFY changed)

    // Semantics. Accent is spent on: play, the primary action, focus, the playhead.
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString onAccent READ onAccent NOTIFY changed)
    Q_PROPERTY(QString success READ success NOTIFY changed)
    Q_PROPERTY(QString warning READ warning NOTIFY changed)
    Q_PROPERTY(QString danger READ danger NOTIFY changed)
    Q_PROPERTY(QString info READ info NOTIFY changed)

    // Lane identity colours.
    Q_PROPERTY(QString laneV2 READ laneV2 NOTIFY changed)
    Q_PROPERTY(QString laneV1 READ laneV1 NOTIFY changed)
    Q_PROPERTY(QString laneText READ laneText NOTIFY changed)
    Q_PROPERTY(QString laneAudio READ laneAudio NOTIFY changed)

public:
    explicit Design(Theme &theme, QObject *parent = nullptr);
    // Fallback for a QML-only load: main registers its own instance (which takes precedence).
    static Design *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    int s1() const { return 2; }
    int s2() const { return 4; }
    int s3() const { return 6; }
    int s4() const { return 8; }
    int s5() const { return 12; }
    int s6() const { return 16; }
    int s7() const { return 24; }
    int s8() const { return 32; }

    QString fontUi() const { return QStringLiteral("Nimbus Sans"); }
    QString fontMono() const { return QStringLiteral("JetBrains Mono"); }

    int tMicro() const { return 12; }
    int tCaption() const { return 12; }
    int tBody() const { return 13; }
    int tBodyLg() const { return 14; }
    int tTitle() const { return 18; }
    int tDisplay() const { return 24; }
    int wRegular() const { return 400; }
    int wMedium() const { return 500; }
    int wSemibold() const { return 600; }

    int hCompact() const { return 24; }
    int hSmall() const { return 28; }
    int hStd() const { return 32; }
    int hLarge() const { return 38; }
    int rowH() const { return 34; }
    int trackH() const { return 48; }
    int topBarH() const { return 44; }
    int gutter() const { return 1; }

    int rXs() const;
    int rSm() const;
    int rMd() const;
    int rLg() const;

    int msFast() const { return 90; }
    int msBase() const { return 140; }
    int msSlow() const { return 220; }

    QString surfaceSunken() const;
    QString surfacePanel() const;
    QString surfaceRaised() const;
    QString surfaceOverlay() const;
    QString borderHairline() const;
    QString borderStrong() const;
    QString focusRing() const;
    QString textPrimary() const;
    QString textSecondary() const;
    QString textDisabled() const;
    QString accent() const;
    QString onAccent() const;
    QString success() const;
    QString warning() const;
    QString danger() const;
    QString info() const;
    QString laneV2() const;
    QString laneV1() const;
    QString laneText() const;
    QString laneAudio() const;

    // #RRGGBBAA helper for scrims and tints, so QML never hardcodes rgba() literals.
    Q_INVOKABLE QString alpha(const QString &hex, double alpha) const;
    Q_INVOKABLE QString mix(const QString &a, const QString &b, double t) const;

signals:
    void changed();

private:
    Theme &m_theme;
};
