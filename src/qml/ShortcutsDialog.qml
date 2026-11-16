import QtQuick
import CutCap
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "shortcutsDialog"
    modal: true
    anchors.centerIn: parent
    width: Math.min(440, parent ? parent.width - Design.s6 * 2 : 440)
    height: Math.min(contentCol.implicitHeight + Design.s5 * 2, parent ? parent.height - Design.s6 * 2 : 520)
    padding: Design.s5
    header: Item { implicitHeight: 0; visible: false }

    Material.theme: theme.darkMode ? Material.Dark : Material.Light
    Material.accent: Design.accent
    Material.background: Design.surfaceOverlay
    Material.foreground: Design.textPrimary

    background: Rectangle {
        color: Design.surfaceOverlay
        radius: Design.rLg
        border.color: Design.borderStrong
        border.width: 1
    }

    contentItem: ColumnLayout {
        id: contentCol
        spacing: Design.s4

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Keyboard Shortcuts"
                color: Design.textPrimary
                font.pixelSize: Design.tTitle
                font.weight: Design.wSemibold
                Layout.fillWidth: true
            }
            Text {
                text: "Global"
                color: Design.textSecondary
                font.pixelSize: Design.tCaption
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Design.borderHairline }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.min(shortcutsCol.implicitHeight, 360)
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: shortcutsCol
                width: parent.width
                spacing: Design.s2

                Repeater {
                    model: [
                        ["Space", "Play / Pause"],
                        ["S or Ctrl+B", "Split clip at playhead"],
                        ["Left / Right", "Step playhead ±0.1s"],
                        ["Ctrl+A", "Select all clips"],
                        ["Esc", "Clear clip selection"],
                        ["Delete / Backspace", "Delete selected clip or text"],
                        ["Shift+Delete", "Ripple delete selected clip"],
                        ["Ctrl+Z / Ctrl+Shift+Z", "Undo / Redo"],
                        ["Ctrl+O", "Import media or project"],
                        ["Ctrl+S", "Save .cutcap project"],
                        ["Ctrl+E", "Export video"],
                        ["?", "Show this shortcuts sheet"],
                        ["Ctrl+Q", "Quit CutCap"]
                    ]
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.s4
                        Rectangle {
                            Layout.preferredWidth: 164
                            implicitHeight: Design.hCompact + Design.s1
                            radius: Design.rSm
                            color: Design.surfacePanel
                            border.color: Design.borderHairline
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: modelData[0]
                                color: Design.accent
                                font.family: "monospace"
                                font.pixelSize: Design.tCaption
                            }
                        }
                        Text {
                            text: modelData[1]
                            color: Design.textPrimary
                            font.pixelSize: Design.tBody
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Design.borderHairline }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            ActionButton {
                objectName: "shortcutsCloseButton"
                text: "Close"
                fontPixelSize: Design.tBody
                horizontalPadding: Design.s5
                verticalPadding: Design.s2
                onClicked: root.close()
            }
        }
    }
}
