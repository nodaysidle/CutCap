import QtQuick
import CutCap
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "exportDialog"
    modal: true
    anchors.centerIn: parent
    width: Math.min(440, parent ? parent.width - Design.s6 * 2 : 440)
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

    property string selectedPath: ""
    readonly property string selectedPreset: presetCombo.currentValue || "high"
    readonly property string selectedCodec: codecCombo.currentValue || "h264"
    readonly property string selectedResolution: resCombo.currentValue || "project"
    readonly property var estimate: {
        // Re-evaluate whenever timeline clips, project resolution, or combo selections change.
        const _clips = backend.clipsList.length
        const _w = backend.projectWidth
        const _h = backend.projectHeight
        return backend.estimateExport(selectedPreset, selectedCodec, selectedResolution)
    }

    FileDialog {
        id: saveExportDialog
        title: "Choose Export Output File"
        fileMode: FileDialog.SaveFile
        nameFilters: ["MP4 Video (*.mp4)", "Matroska (*.mkv)", "QuickTime (*.mov)"]
        onAccepted: {
            root.selectedPath = selectedFile.toString()
            backend.startExport(root.selectedPath, root.selectedPreset, root.selectedCodec, root.selectedResolution)
        }
    }

    Connections {
        target: backend
        function onExportFinished(ok, message) {
            if (ok) {
                resultText.color = Design.success
                resultText.text = message
            } else {
                resultText.color = Design.danger
                resultText.text = "Error: " + message
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Design.s4

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Export Video"
                color: Design.textPrimary
                font.pixelSize: Design.tTitle
                font.weight: Design.wSemibold
                Layout.fillWidth: true
            }
            Text {
                text: "100% local · FFmpeg"
                color: Design.textSecondary
                font.pixelSize: Design.tCaption
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Design.borderHairline }

        GridLayout {
            columns: 2
            columnSpacing: Design.s5
            rowSpacing: Design.s3
            Layout.fillWidth: true

            Text { text: "Quality Preset"; color: Design.textSecondary; font.pixelSize: Design.tBody }
            ComboBox {
                id: presetCombo
                objectName: "exportPresetCombo"
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: "High (CRF 18)", value: "high" },
                    { text: "Medium (CRF 22)", value: "medium" },
                    { text: "Fast (CRF 25)", value: "fast" }
                ]
                Layout.fillWidth: true
                implicitHeight: Design.hStd
                topInset: 0
                bottomInset: 0
                font.pixelSize: Design.tBody
            }

            Text { text: "Video Codec"; color: Design.textSecondary; font.pixelSize: Design.tBody }
            ComboBox {
                id: codecCombo
                objectName: "exportCodecCombo"
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: "H.264 (libx264)", value: "h264" },
                    { text: "H.265 / HEVC (libx265)", value: "h265" },
                    { text: "AV1 (libsvtav1)", value: "av1" }
                ]
                Layout.fillWidth: true
                implicitHeight: Design.hStd
                topInset: 0
                bottomInset: 0
                font.pixelSize: Design.tBody
            }

            Text { text: "Resolution"; color: Design.textSecondary; font.pixelSize: Design.tBody }
            ComboBox {
                id: resCombo
                objectName: "exportResCombo"
                textRole: "text"
                valueRole: "value"
                model: [
                    { text: "Project (" + backend.projectWidth + "×" + backend.projectHeight + ")", value: "project" },
                    { text: "4K UHD (3840×2160)", value: "4k" },
                    { text: "Vertical (1080×1920)", value: "vertical" }
                ]
                Layout.fillWidth: true
                implicitHeight: Design.hStd
                topInset: 0
                bottomInset: 0
                font.pixelSize: Design.tBody
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: summaryGrid.implicitHeight + Design.s4 * 2
            radius: Design.rSm
            color: Design.surfacePanel
            border.color: Design.borderHairline
            border.width: 1

            GridLayout {
                id: summaryGrid
                anchors.fill: parent
                anchors.margins: Design.s4
                columns: 2
                columnSpacing: Design.s4
                rowSpacing: Design.s2

                Text { text: "Output"; color: Design.textSecondary; font.pixelSize: Design.tCaption }
                Text {
                    objectName: "exportEstimateText"
                    text: root.estimate.resolutionLabel + " · " + root.estimate.durationLabel + " · " + root.estimate.estimatedSizeLabel
                    color: Design.textPrimary
                    font.family: "monospace"
                    font.pixelSize: Design.tCaption
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Text { text: "Encoder"; color: Design.textSecondary; font.pixelSize: Design.tCaption }
                Text {
                    objectName: "exportCodecStatus"
                    text: root.estimate.codecSupported
                          ? (root.estimate.encoder + " available" + (root.estimate.hasAudio ? " + AAC" : ""))
                          : (root.estimate.encoder + " unavailable in ffmpeg")
                    color: root.estimate.codecSupported ? Design.success : Design.danger
                    font.pixelSize: Design.tCaption
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            visible: backend.exporting || backend.exportProgress > 0
            value: backend.exportProgress
        }

        Text {
            id: resultText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: Design.tCaption
            visible: text.length > 0
        }

        Text {
            visible: backend.clipsList.length === 0
            Layout.fillWidth: true
            text: "Add at least one clip to the timeline before exporting."
            color: Design.warning
            font.pixelSize: Design.tCaption
            wrapMode: Text.WordWrap
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Design.borderHairline }

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.s3
            Item { Layout.fillWidth: true }
            ActionButton {
                objectName: "exportCancelButton"
                text: backend.exporting ? "Cancel Export" : "Close"
                fontPixelSize: Design.tBody
                horizontalPadding: Design.s5
                verticalPadding: Design.s2
                onClicked: {
                    if (backend.exporting) backend.cancelExport()
                    else root.close()
                }
            }
            ActionButton {
                objectName: "exportStartButton"
                text: "Choose File & Export"
                enabled: !backend.exporting && backend.clipsList.length > 0 && root.estimate.codecSupported
                highlighted: true
                fontPixelSize: Design.tBody
                horizontalPadding: Design.s5
                verticalPadding: Design.s2
                onClicked: saveExportDialog.open()
            }
        }
    }
}
