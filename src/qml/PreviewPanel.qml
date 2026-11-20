pragma ComponentBehavior: Bound

import QtQuick
import CutCap
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    signal importRequested()
    color: Design.surfaceSunken
    radius: 0
    border.width: 0
    clip: true

    readonly property real aspectRatio: Backend.projectWidth / Math.max(1, Backend.projectHeight)

    function syncLanes(forceSeek) {
        baseLane.sync(forceSeek)
        overlayLane.sync(forceSeek)
        audioLane.sync(forceSeek)
    }

    Component.onCompleted: syncLanes(true)

    Connections {
        target: Backend
        function onPreviewChanged() { root.syncLanes(!Backend.playing) }
        function onSeekRequested() { root.syncLanes(true) }
        function onPlaybackChanged() { root.syncLanes(false) }
    }

    PreviewLane {
        id: audioLane
        role: "audio"
        audioOnly: true
        lane: Backend.audioLane
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Design.s5
        spacing: Design.s3

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Design.hSmall
            spacing: Design.s2

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 6
                implicitHeight: 6
                radius: width / 2
                color: Backend.playing ? Design.accent : Design.success
            }
            Text {
                text: Backend.playing ? "PROGRAM" : "VIEWER"
                color: Design.textPrimary
                font.family: Design.fontMono
                font.pixelSize: Design.tMicro
                font.weight: Design.wMedium
                font.letterSpacing: 0.8
            }
            Text {
                text: Backend.projectWidth + " × " + Backend.projectHeight
                color: Design.textSecondary
                font.family: Design.fontMono
                font.pixelSize: Design.tMicro
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "LOCAL"
                color: Design.textSecondary
                font.family: Design.fontMono
                font.pixelSize: Design.tMicro
                font.letterSpacing: 0.8
            }
        }

        Item {
            id: stage
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                id: canvasBox
                objectName: "previewCanvas"
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height * root.aspectRatio)
                height: width / root.aspectRatio
                // An empty project has no picture, so it shows the themed bed and a prompt in theme
                // ink; black would make the prompt unreadable in a light theme. Playback is black.
                readonly property bool emptyProject: Backend.clipCount === 0 && Backend.textCount === 0
                color: emptyProject ? Design.surfacePanel : "#000000"
                clip: true
                border.color: Design.borderHairline
                border.width: 1

                readonly property real pixelScale: height / Math.max(1, Backend.projectHeight)

                PreviewLane {
                    id: baseLane
                    objectName: "baseLane"
                    role: "base"
                    anchors.fill: parent
                    lane: Backend.baseLane
                }

                PreviewLane {
                    id: overlayLane
                    objectName: "overlayLane"
                    role: "overlay"
                    anchors.fill: parent
                    fitToMedia: true
                    lane: Backend.overlayLane
                }

                // A gap in the timeline plays as black, like the export. Only an empty project gets
                // the intake slate, with one clear action instead of competing empty states.
                Column {
                    anchors.centerIn: parent
                    spacing: Design.s3
                    visible: parent.emptyProject
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Start with a cut."
                        color: Design.textPrimary
                        font.pixelSize: Design.tDisplay
                        font.weight: Design.wSemibold
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Drop video, images, or audio here.\nCtrl+O also opens files."
                        color: Design.textSecondary
                        font.pixelSize: Design.tBody
                        horizontalAlignment: Text.AlignHCenter
                    }
                    ActionButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Import Media"
                        highlighted: true
                        fontPixelSize: Design.tBody
                        horizontalPadding: Design.s6
                        verticalPadding: Design.s3
                        onClicked: root.importRequested()
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Design.s4
                    visible: parent.emptyProject
                    text: "00:00:00"
                    color: Design.textSecondary
                    opacity: 0.18
                    font.family: Design.fontMono
                    font.pixelSize: Design.tDisplay
                }

                Repeater {
                    model: Backend.activeTexts
                    delegate: Rectangle {
                        required property var modelData
                        id: txtBox
                        x: txtBox.modelData.x * canvasBox.width - width / 2
                        y: txtBox.modelData.y * canvasBox.height - height / 2
                        width: overlayLabel.implicitWidth + 20
                        height: overlayLabel.implicitHeight + 10
                        radius: Design.rMd
                        color: Design.alpha(txtBox.modelData.bgColor, txtBox.modelData.bgOpacity)
                        border.color: Backend.selectedTextId === txtBox.modelData.id ? Design.accent : "transparent"
                        border.width: 1

                        Text {
                            id: overlayLabel
                            anchors.centerIn: parent
                            text: txtBox.modelData.text
                            color: txtBox.modelData.color
                            font.pixelSize: Math.max(10, Math.round(txtBox.modelData.fontSize * canvasBox.pixelScale))
                            font.bold: txtBox.modelData.bold
                            horizontalAlignment: Text.AlignHCenter
                        }

                        MouseArea {
                            anchors.fill: parent
                            drag.target: txtBox
                            onPressed: Backend.selectText(txtBox.modelData.id)
                            onReleased: {
                                const nx = (txtBox.x + txtBox.width / 2) / canvasBox.width
                                const ny = (txtBox.y + txtBox.height / 2) / canvasBox.height
                                Backend.updateTextProperty(txtBox.modelData.id, "x", nx)
                                Backend.updateTextProperty(txtBox.modelData.id, "y", ny)
                            }
                        }
                    }
                }
            }

            // Crop marks sit on the matte, never over the exported picture.
            Rectangle {
                x: canvasBox.x - Design.s5; y: canvasBox.y
                width: Design.s5; height: 1
                color: Design.textSecondary; opacity: 0.55
            }
            Rectangle {
                x: canvasBox.x; y: canvasBox.y - Design.s5
                width: 1; height: Design.s5
                color: Design.textSecondary; opacity: 0.55
            }
            Rectangle {
                x: canvasBox.x + canvasBox.width; y: canvasBox.y
                width: Design.s5; height: 1
                color: Design.textSecondary; opacity: 0.55
            }
            Rectangle {
                x: canvasBox.x + canvasBox.width - 1; y: canvasBox.y - Design.s5
                width: 1; height: Design.s5
                color: Design.textSecondary; opacity: 0.55
            }
            Rectangle {
                x: canvasBox.x - Design.s5; y: canvasBox.y + canvasBox.height
                width: Design.s5; height: 1
                color: Design.textSecondary; opacity: 0.55
            }
            Rectangle {
                x: canvasBox.x; y: canvasBox.y + canvasBox.height
                width: 1; height: Design.s5
                color: Design.textSecondary; opacity: 0.55
            }
            Rectangle {
                x: canvasBox.x + canvasBox.width; y: canvasBox.y + canvasBox.height
                width: Design.s5; height: 1
                color: Design.textSecondary; opacity: 0.55
            }
            Rectangle {
                x: canvasBox.x + canvasBox.width - 1; y: canvasBox.y + canvasBox.height
                width: 1; height: Design.s5
                color: Design.textSecondary; opacity: 0.55
            }
        }

        // Transport: a flat strip on a hairline, timecode left, transport centre, format right.
        Rectangle {
            objectName: "previewTransport"
            Layout.fillWidth: true
            Layout.preferredHeight: Design.hLarge + Design.s3
            color: "transparent"

            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Design.borderHairline }

            RowLayout {
                id: transportRow
                anchors.fill: parent
                anchors.leftMargin: Design.s3
                anchors.rightMargin: Design.s3
                spacing: Design.s2

                Text {
                    objectName: "transportPlayhead"
                    text: Backend.formatTimecode(Backend.playheadSec)
                    color: Design.textPrimary
                    font.family: Design.fontMono
                    font.pixelSize: Design.tBody
                    font.weight: Design.wMedium
                }
                Text {
                    objectName: "transportDuration"
                    // Secondary metadata: shown only when the row can hold it next to every control.
                    visible: transportRow.width >= 400
                    text: "/ " + Backend.formatTimecode(Backend.totalDuration)
                    color: Design.textSecondary
                    font.family: Design.fontMono
                    font.pixelSize: Design.tMicro
                }

                Item { Layout.fillWidth: true }

                Rectangle { implicitWidth: 1; implicitHeight: Design.s4; color: Design.borderHairline }

                ActionButton {
                    objectName: "transportStepBack"
                    text: "−1s"
                    flat: true
                    horizontalPadding: Design.s2
                    fontPixelSize: Design.tBody
                    onClicked: Backend.playheadSec = Math.max(0, Backend.playheadSec - 1.0)
                }

                ActionButton {
                    objectName: "transportPlay"
                    text: Backend.playing ? "Pause" : "Play"
                    highlighted: true
                    horizontalPadding: Design.s4
                    fontPixelSize: Design.tBody
                    fontBold: true
                    onClicked: Backend.playing = !Backend.playing
                }

                ActionButton {
                    objectName: "transportStepForward"
                    text: "+1s"
                    flat: true
                    horizontalPadding: Design.s2
                    fontPixelSize: Design.tBody
                    onClicked: Backend.playheadSec = Backend.playheadSec + 1.0
                }

                Rectangle { implicitWidth: 1; implicitHeight: Design.s4; color: Design.borderHairline }

                Item { Layout.fillWidth: true }

                ComboBox {
                    id: aspectCombo
                    objectName: "aspectCombo"
                    model: ["16:9", "9:16", "1:1"]
                    font.family: Design.fontMono
                    font.pixelSize: Design.tCaption
                    implicitHeight: Design.hSmall
                    Layout.preferredWidth: 88
                    Layout.maximumWidth: 96
                    leftPadding: Design.s2
                    topInset: 0
                    bottomInset: 0
                    currentIndex: Backend.projectWidth === Backend.projectHeight
                                  ? 2 : (Backend.projectWidth < Backend.projectHeight ? 1 : 0)
                    onActivated: (index) => {
                        if (index === 0)
                            Backend.setProjectResolution(1920, 1080)
                        else if (index === 1)
                            Backend.setProjectResolution(1080, 1920)
                        else
                            Backend.setProjectResolution(1080, 1080)
                    }
                }
            }
        }
    }
}
