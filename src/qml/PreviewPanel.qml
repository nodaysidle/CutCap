pragma ComponentBehavior: Bound

import QtQuick
import CutCap
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root

    signal importRequested()
    color: Design.surfacePanel
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
        anchors.margins: 10
        spacing: 8

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                id: canvasBox
                objectName: "previewCanvas"
                anchors.centerIn: parent
                width: Math.min(parent.width, parent.height * root.aspectRatio)
                height: width / root.aspectRatio
                color: "#000000"
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

                // A gap in the timeline plays as black, like the export. Only an empty project gets a prompt.
                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    visible: Backend.clipsList.length === 0
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Drop video, audio or images here"
                        color: Design.textPrimary
                        font.pixelSize: Design.tBodyLg
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "or press Ctrl+O to choose files"
                        color: Design.textSecondary
                        font.pixelSize: Design.tBody
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
                    font.family: "monospace"
                    font.pixelSize: Design.tBody
                    font.weight: Design.wMedium
                }
                Text {
                    objectName: "transportDuration"
                    // Secondary metadata: shown only when the row can hold it next to every control.
                    visible: transportRow.width >= 400
                    text: "/ " + Backend.formatTimecode(Backend.totalDuration)
                    color: Design.textSecondary
                    font.family: "monospace"
                    font.pixelSize: Design.tMicro
                }

                Item { Layout.fillWidth: true }

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

                Item { Layout.fillWidth: true }

                ComboBox {
                    id: aspectCombo
                    objectName: "aspectCombo"
                    model: ["16:9 1080p", "9:16 shorts", "1:1 square"]
                    font.pixelSize: Design.tCaption
                    implicitHeight: Design.hSmall
                    Layout.preferredWidth: 120
                    Layout.maximumWidth: 132
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
