import QtQuick
import CutCap
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: win
    visible: true
    width: 1440
    height: 900
    minimumWidth: 880
    minimumHeight: 580
    title: backend.projectTitle + (backend.dirty ? " *" : "") + " — CutCap"
    color: Design.surfaceSunken

    Material.theme: theme.darkMode ? Material.Dark : Material.Light
    Material.accent: Design.accent
    Material.background: Design.surfacePanel
    Material.foreground: Design.textPrimary
    Material.roundedScale: Design.rLg

    readonly property int panelRadius: Design.rLg
    readonly property int controlRadius: Design.rMd
    readonly property bool isTyping: activeFocusItem !== null &&
        (activeFocusItem.toString().indexOf("TextInput") !== -1 ||
         activeFocusItem.toString().indexOf("TextEdit") !== -1 ||
         activeFocusItem.toString().indexOf("TextArea") !== -1 ||
         activeFocusItem.toString().indexOf("TextField") !== -1)

    // Files dropped from outside the app: onto a timeline lane they land at the pointer, elsewhere
    // they are imported like the Import button. Internal media-bin drags are a different key so
    // this area does not swallow them.
    DropArea {
        anchors.fill: parent
        z: 100
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasUrls)
                timelinePanel.dropUrlsAt(drop.x, drop.y, drop.urls)
        }
    }

    // Dialogs (native xdg-desktop-portal on Wayland)
    FileDialog {
        id: importDialog
        title: "Import Media or Project"
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            "Media & Projects (*.mp4 *.mkv *.mov *.webm *.avi *.mp3 *.wav *.ogg *.flac *.png *.jpg *.jpeg *.webp *.srt *.cutcap)",
            "All files (*)"
        ]
        onAccepted: backend.importMedia(selectedFiles)
    }

    FileDialog {
        id: saveProjectDialog
        title: "Save CutCap Project"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CutCap Project (*.cutcap)"]
        onAccepted: backend.saveProject(selectedFile)
    }

    FileDialog {
        id: openProjectDialog
        title: "Open CutCap Project"
        fileMode: FileDialog.OpenFile
        nameFilters: ["CutCap Project (*.cutcap)"]
        onAccepted: backend.openProject(selectedFile)
    }

    FileDialog {
        id: importSrtDialog
        title: "Import Subtitles (.srt)"
        fileMode: FileDialog.OpenFile
        nameFilters: ["SubRip Subtitles (*.srt)"]
        onAccepted: backend.importSrtFile(selectedFile)
    }

    FileDialog {
        id: exportSrtDialog
        title: "Export Subtitles (.srt)"
        fileMode: FileDialog.SaveFile
        nameFilters: ["SubRip Subtitles (*.srt)"]
        onAccepted: backend.exportSrtFile(selectedFile)
    }

    ExportDialog {
        id: exportDialog
    }

    ShortcutsDialog {
        id: shortcutsDialog
    }

    // Keyboard shortcuts
    Shortcut {
        sequence: "Ctrl+A"
        enabled: !win.isTyping
        context: Qt.ApplicationShortcut
        onActivated: backend.selectAllClips()
    }
    Shortcut {
        sequence: "Escape"
        enabled: !win.isTyping && !exportDialog.opened && !shortcutsDialog.opened &&
                 !importDialog.opened && !saveProjectDialog.opened &&
                 !openProjectDialog.opened && !importSrtDialog.opened && !exportSrtDialog.opened
        context: Qt.ApplicationShortcut
        onActivated: backend.clearClipSelection()
    }
    Shortcut {
        sequence: "Space"
        enabled: !win.isTyping
        context: Qt.ApplicationShortcut
        onActivated: backend.playing = !backend.playing
    }
    Shortcut {
        sequence: "S"
        enabled: !win.isTyping
        context: Qt.ApplicationShortcut
        onActivated: backend.splitAtPlayhead()
    }
    Shortcut {
        sequence: "Ctrl+B"
        context: Qt.ApplicationShortcut
        onActivated: backend.splitAtPlayhead()
    }
    Shortcut {
        sequence: "Delete"
        enabled: !win.isTyping
        context: Qt.ApplicationShortcut
        onActivated: backend.deleteSelected(false)
    }
    Shortcut {
        sequence: "Backspace"
        enabled: !win.isTyping
        context: Qt.ApplicationShortcut
        onActivated: backend.deleteSelected(false)
    }
    Shortcut {
        sequence: "Shift+Delete"
        enabled: !win.isTyping
        context: Qt.ApplicationShortcut
        onActivated: backend.deleteSelected(true)
    }
    Shortcut {
        sequences: [StandardKey.Undo]
        context: Qt.ApplicationShortcut
        onActivated: backend.undo()
    }
    Shortcut {
        sequences: [StandardKey.Redo]
        context: Qt.ApplicationShortcut
        onActivated: backend.redo()
    }
    Shortcut {
        sequence: "Ctrl+O"
        context: Qt.ApplicationShortcut
        onActivated: importDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+S"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.projectPath.length > 0) backend.saveProject()
            else saveProjectDialog.open()
        }
    }
    Shortcut {
        sequence: "Ctrl+E"
        context: Qt.ApplicationShortcut
        onActivated: exportDialog.open()
    }
    Shortcut {
        sequence: "Left"
        enabled: !win.isTyping
        context: Qt.ApplicationShortcut
        onActivated: backend.playheadSec = Math.max(0, backend.playheadSec - 0.1)
    }
    Shortcut {
        sequence: "Right"
        enabled: !win.isTyping
        context: Qt.ApplicationShortcut
        onActivated: backend.playheadSec = backend.playheadSec + 0.1
    }
    Shortcut {
        sequence: "?"
        enabled: !win.isTyping
        context: Qt.ApplicationShortcut
        onActivated: shortcutsDialog.open()
    }
    Shortcut {
        sequence: "Ctrl+Q"
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }

    // Error toast
    Rectangle {
        id: errorBanner
        visible: false
        z: 200
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 56
        width: Math.min(620, parent.width - 48)
        height: errText.implicitHeight + 20
        radius: controlRadius
        color: Design.surfaceOverlay
        border.color: Design.danger
        border.width: 1

        Text {
            id: errText
            anchors.fill: parent
            anchors.margins: 10
            color: Design.textPrimary
            font.pixelSize: Design.tBodyLg
            wrapMode: Text.WordWrap
        }

        Timer {
            id: errTimer
            interval: 6000
            onTriggered: errorBanner.visible = false
        }
    }

    Connections {
        target: backend
        function onErrorOccurred(msg) {
            errText.text = msg
            errorBanner.visible = true
            errTimer.restart()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Top Bar: flat command strip. No card, no radius — a hairline under it does the work.
        Rectangle {
            id: topBar
            objectName: "topBar"
            Layout.fillWidth: true
            Layout.preferredHeight: Design.topBarH
            color: Design.surfaceSunken

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Design.s4
                anchors.rightMargin: Design.s4
                spacing: Design.s2

                Rectangle {
                    width: 18
                    height: 18
                    radius: Design.rXs
                    color: Design.accent
                    Text {
                        anchors.centerIn: parent
                        text: "C"
                        color: Design.onAccent
                        font.pixelSize: Design.tCaption
                        font.weight: Design.wSemibold
                    }
                }

                TextField {
                    id: titleField
                    objectName: "titleField"
                    text: backend.projectTitle
                    color: Design.textPrimary
                    font.pixelSize: Design.tBodyLg
                    font.weight: Design.wMedium
                    Layout.minimumWidth: 120
                    Layout.preferredWidth: 170
                    Layout.maximumWidth: 220
                    Layout.fillWidth: false
                    padding: 0
                    topInset: 0
                    bottomInset: 0
                    verticalAlignment: TextInput.AlignVCenter
                    background: Rectangle {
                        color: "transparent"
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: titleField.activeFocus ? Design.accent : "transparent"
                        }
                    }
                    onEditingFinished: backend.setProjectTitle(text)
                }

                Text {
                    objectName: "dirtyBadge"
                    visible: backend.dirty
                    text: "unsaved"
                    color: Design.textSecondary
                    font.pixelSize: Design.tMicro
                    font.letterSpacing: 0.4
                }

                Rectangle { width: 1; height: 14; color: Design.borderHairline }

                ActionButton { text: "Import"; flat: true; horizontalPadding: Design.s3; onClicked: importDialog.open() }
                ActionButton { text: "Open"; flat: true; horizontalPadding: Design.s3; onClicked: openProjectDialog.open() }
                ActionButton {
                    text: "Save"
                    flat: true
                    horizontalPadding: Design.s3
                    onClicked: {
                        if (backend.projectPath.length > 0) backend.saveProject()
                        else saveProjectDialog.open()
                    }
                }
                ActionButton { text: "Undo"; flat: true; horizontalPadding: Design.s3; enabled: backend.canUndo; onClicked: backend.undo() }
                ActionButton { text: "Redo"; flat: true; horizontalPadding: Design.s3; enabled: backend.canRedo; onClicked: backend.redo() }

                Item {
                    Layout.fillWidth: true
                    Layout.minimumWidth: Design.s3
                }

                Text {
                    id: topBarStatus
                    objectName: "topBarStatus"
                    text: backend.statusMessage
                    color: Design.textSecondary
                    font.pixelSize: Design.tCaption
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: 360

                    MouseArea {
                        id: statusHover
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }
                    ToolTip.visible: topBarStatus.truncated && statusHover.containsMouse
                    ToolTip.text: backend.statusMessage
                    ToolTip.delay: 350
                }

                ActionButton {
                    objectName: "shortcutsButton"
                    text: "?"
                    flat: true
                    horizontalPadding: Design.s3
                    toolTipText: "Shortcuts (?)"
                    onClicked: shortcutsDialog.open()
                }
                ActionButton {
                    objectName: "exportButton"
                    text: backend.exporting ? (Math.round(backend.exportProgress * 100) + "%") : "Export"
                    highlighted: true
                    horizontalPadding: Design.s5
                    toolTipText: "Export video (Ctrl+E)"
                    onClicked: exportDialog.open()
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Design.borderHairline
            }
        }

        // Main Workspace: Media Bin | Preview | Inspector
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            MediaBinPanel {
                objectName: "mediaBinPanel"
                SplitView.preferredWidth: 260
                SplitView.minimumWidth: 210
                SplitView.maximumWidth: 380
            }

            // 1px seams are the structure: no cards, no rounded panels.
            Rectangle {
                SplitView.preferredWidth: Design.gutter
                SplitView.fillHeight: true
                color: Design.borderHairline
            }

            PreviewPanel {
                objectName: "previewPanel"
                SplitView.fillWidth: true
                SplitView.minimumWidth: 400
            }

            Rectangle {
                SplitView.preferredWidth: Design.gutter
                SplitView.fillHeight: true
                color: Design.borderHairline
            }

            InspectorPanel {
                objectName: "inspectorPanel"
                SplitView.preferredWidth: 300
                SplitView.minimumWidth: 250
                SplitView.maximumWidth: 420
            }
        }

        // Bottom Timeline
        Rectangle {
            Layout.fillWidth: true
            height: Design.gutter
            color: Design.borderHairline
        }

        TimelinePanel {
            id: timelinePanel
            objectName: "timelinePanel"
            Layout.fillWidth: true
            Layout.preferredHeight: 248
            Layout.minimumHeight: 170
        }
    }
}
