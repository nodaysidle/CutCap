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
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // Panel header: title, count, import. No view switch — captions and voiceover live in the
        // inspector; text and captions are added from the timeline toolbar.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Design.topBarH
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Design.s5
                anchors.rightMargin: Design.s3
                spacing: Design.s2

                Text {
                    text: "Media"
                    color: Design.textPrimary
                    font.pixelSize: Design.tBody
                    font.weight: Design.wSemibold
                }
                Text {
                    text: assetsListView.count + (assetsListView.count === 1 ? " item" : " items")
                    color: Design.textSecondary
                    font.pixelSize: Design.tMicro
                    Layout.fillWidth: true
                }
                ActionButton {
                    text: "Import"
                    flat: true
                    toolTipText: "Import media, projects or subtitles (Ctrl+O)"
                    onClicked: root.importRequested()
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Design.borderHairline }
        }

    ListView {
        id: assetsListView
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 0
        clip: true
        model: Backend.assetsList

        Label {
            anchors.centerIn: parent
            visible: parent.count === 0
            text: "Drop video, audio, or\nimage files here"
            horizontalAlignment: Text.AlignHCenter
            color: Design.textSecondary
            font.pixelSize: Design.tBodyLg
        }

        delegate: Item {
            id: delegateRoot1
            required property int index
            required property var modelData
            width: assetsListView.width
            height: 64

            Rectangle {
                id: assetCard
                objectName: "assetCard"
                width: parent.width
                height: 64
                radius: 0
                color: assetDragArea.containsMouse || assetDragArea.drag.active
                       ? Design.mix(Design.surfacePanel, Design.textPrimary, 0.05) : "transparent"

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Design.borderHairline
                    visible: index < assetsListView.count - 1
                }

                property string assetId: delegateRoot1.modelData.id
                property string assetKind: delegateRoot1.modelData.kind
                property string assetName: delegateRoot1.modelData.name
                property real assetDuration: delegateRoot1.modelData.durationSec
                property bool hovered: assetDragArea.containsMouse || assetDragArea.drag.active

                Drag.active: assetDragArea.drag.active
                Drag.keys: ["cutcap/asset"]
                // Drop position = pointer position, so the timeline ghost starts under the cursor.
                Drag.hotSpot.x: assetDragArea.mouseX
                Drag.hotSpot.y: assetDragArea.mouseY

                states: State {
                    when: assetDragArea.drag.active
                    ParentChange { target: assetCard; parent: Window.window.contentItem }
                    PropertyChanges { assetCard.z: 500; assetCard.opacity: 0.85 }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Design.s4
                    spacing: Design.s1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            Layout.preferredWidth: 64
                            Layout.preferredHeight: 36
                            radius: Design.rXs
                            color: Design.surfaceSunken
                            clip: true

                            Image {
                                anchors.fill: parent
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                source: delegateRoot1.modelData.thumbnailPath ? ("file://" + delegateRoot1.modelData.thumbnailPath) : ""
                                visible: delegateRoot1.modelData.kind !== "audio"
                            }

                            Row {
                                id: thumbWave
                                readonly property var peaks: delegateRoot1.modelData.waveformPeaks || []
                                anchors.centerIn: parent
                                height: parent.height - 10
                                spacing: 1
                                visible: delegateRoot1.modelData.kind === "audio"
                                Repeater {
                                    model: thumbWave.visible ? 22 : 0
                                    delegate: Item {
                                        width: 2
                                        height: thumbWave.height
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 2
                                            radius: width / 2
                                            height: Math.max(2, (thumbWave.peaks.length
                                                ? thumbWave.peaks[Math.floor(index / 22 * thumbWave.peaks.length)] : 0.1) * parent.height)
                                            color: Design.success
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                objectName: "assetName"
                                text: delegateRoot1.modelData.name
                                color: Design.textPrimary
                                font.pixelSize: Design.tBody
                                font.weight: Design.wSemibold
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            // One slot, two states: the metadata line is replaced by the actions on
                            // hover. Nothing floats over the text and the row never changes height.
                            Item {
                                id: metaSlot
                                Layout.fillWidth: true
                                Layout.preferredHeight: Design.hCompact

                                readonly property int seconds: Math.round(delegateRoot1.modelData.durationSec)
                                readonly property string baseMeta: (delegateRoot1.modelData.kind === "audio" ? "Audio" : delegateRoot1.modelData.kind === "image" ? "Image" : "Video")
                                                                   + " · " + Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0")
                                readonly property string fullMeta: delegateRoot1.modelData.kind !== "audio"
                                                                   ? (baseMeta + " · " + delegateRoot1.modelData.width + "×" + delegateRoot1.modelData.height)
                                                                   : baseMeta

                                TextMetrics {
                                    id: fullMetaMetrics
                                    font.pixelSize: Design.tCaption
                                    text: metaSlot.fullMeta
                                }

                                Text {
                                    objectName: "assetMeta"
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    // Drop dimensions before eliding kind and duration at narrow widths.
                                    text: fullMetaMetrics.advanceWidth <= width ? metaSlot.fullMeta : metaSlot.baseMeta
                                    color: Design.textSecondary
                                    font.pixelSize: Design.tCaption
                                    elide: Text.ElideRight
                                    visible: !assetCard.hovered
                                }

                                RowLayout {
                                    objectName: "assetActions"
                                    anchors.fill: parent
                                    spacing: Design.s1
                                    visible: assetCard.hovered

                                    ActionButton {
                                        text: delegateRoot1.modelData.kind === "audio" ? "→ A1" : "→ V1"
                                        flat: true
                                        toolTipText: "Append to " + (delegateRoot1.modelData.kind === "audio" ? "A1 audio" : "V1 main video")
                                        onClicked: Backend.addAssetToTimeline(delegateRoot1.modelData.id, delegateRoot1.modelData.kind === "audio" ? 2 : 0, -1)
                                    }
                                    ActionButton {
                                        text: "→ V2"
                                        flat: true
                                        visible: delegateRoot1.modelData.kind !== "audio"
                                        toolTipText: "Place on V2 overlay at the playhead"
                                        onClicked: Backend.addAssetToTimeline(delegateRoot1.modelData.id, 1, Backend.playheadSec)
                                    }
                                    ActionButton {
                                        text: "×"
                                        flat: true
                                        implicitWidth: Design.hCompact
                                        textColor: Design.danger
                                        toolTipText: "Remove from media bin"
                                        onClicked: Backend.removeAsset(delegateRoot1.modelData.id)
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }

                    }

                }

                MouseArea {
                    id: assetDragArea
                    anchors.fill: parent
                    z: -1
                    hoverEnabled: true
                    preventStealing: true
                    drag.target: assetCard
                    onReleased: {
                        assetCard.Drag.drop()
                        assetCard.x = 0
                        assetCard.y = 0
                    }
                }

                ToolTip.visible: assetDragArea.containsMouse && !assetDragArea.drag.active
                ToolTip.text: delegateRoot1.modelData.name + " (" + (delegateRoot1.modelData.kind !== "audio"
                    ? ((delegateRoot1.modelData.kind === "image" ? "Image" : "Video") + " · "
                       + Math.floor(Math.round(delegateRoot1.modelData.durationSec) / 60) + ":"
                       + String(Math.round(delegateRoot1.modelData.durationSec) % 60).padStart(2, "0")
                       + " · " + delegateRoot1.modelData.width + "×" + delegateRoot1.modelData.height)
                    : ("Audio · " + Math.floor(Math.round(delegateRoot1.modelData.durationSec) / 60) + ":"
                       + String(Math.round(delegateRoot1.modelData.durationSec) % 60).padStart(2, "0"))) + ")"
                ToolTip.delay: 450
            }
        }
    }
    }
}
