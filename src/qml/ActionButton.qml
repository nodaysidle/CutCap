pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import CutCap

// The only button in the app. Dense by construction: height comes from the label plus padding,
// so a label can never be clipped by a Material inset. Keyboard-operable and visibly focused.
Rectangle {
    id: root

    property string text: ""
    property bool highlighted: false
    property bool flat: false
    property color textColor: highlighted ? Design.onAccent : Design.textPrimary
    property color fillColor: highlighted ? Design.accent : Design.surfaceRaised
    property int fontPixelSize: Design.tCaption
    property bool fontBold: false
    property int horizontalPadding: Design.s5
    property int verticalPadding: Design.s2
    property string toolTipText: ""
    readonly property alias hovered: mouse.containsMouse
    readonly property alias pressed: mouse.pressed
    readonly property alias labelItem: label

    signal clicked()

    // Space is play/pause everywhere, except on a control the keyboard moved focus to. A click
    // also focuses the control, and must not turn Space into "press this button again".
    property bool focusFromMouse: false
    onFocusChanged: if (!focus) focusFromMouse = false
    Keys.onShortcutOverride: (event) => {
        if (event.key === Qt.Key_Space && !root.focusFromMouse)
            event.accepted = true
    }

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.description: toolTipText
    Accessible.onPressAction: clicked()

    implicitWidth: Math.max(Design.hCompact, label.implicitWidth + 2 * horizontalPadding)
    implicitHeight: Math.max(Design.hCompact, label.implicitHeight + 2 * verticalPadding)
    radius: Design.rMd
    opacity: enabled ? 1.0 : 0.45
    activeFocusOnTab: enabled
    // Flat actions carry no box: the command strip stays a row of labels, not a tray of buttons.
    border.width: activeFocus ? 2 : (highlighted || flat ? 0 : 1)
    border.color: activeFocus ? Design.focusRing : Design.borderHairline
    color: {
        if (mouse.pressed)
            return highlighted ? Qt.darker(fillColor, 1.15) : Design.mix(Design.surfaceRaised, Design.textPrimary, 0.14)
        if (mouse.containsMouse)
            return highlighted ? Qt.lighter(fillColor, 1.12) : Design.mix(Design.surfaceRaised, Design.textPrimary, 0.08)
        return flat ? "transparent" : fillColor
    }
    Behavior on color { ColorAnimation { duration: Design.msFast; easing.type: Easing.OutQuad } }
    Behavior on opacity { NumberAnimation { duration: Design.msFast; easing.type: Easing.OutQuad } }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.textColor
        font.family: Design.fontUi
        font.pixelSize: root.fontPixelSize
        font.weight: root.fontBold ? Design.wSemibold : Design.wMedium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.focusFromMouse = true
            root.forceActiveFocus()
            root.clicked()
        }
    }

    Keys.onSpacePressed: root.clicked()
    Keys.onReturnPressed: root.clicked()

    ToolTip.visible: toolTipText.length > 0 && mouse.containsMouse
    ToolTip.text: toolTipText
    ToolTip.delay: 400
}
