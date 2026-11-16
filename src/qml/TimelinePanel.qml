import QtQuick
import CutCap
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root
    color: Design.surfacePanel
    clip: true

    readonly property int headerWidth: 148
    readonly property int trackHeight: Design.trackH
    readonly property real pxPerSec: Backend.zoomPxPerSec
    readonly property real timelineWidth: Math.max(timelineFlick.width, (Backend.totalDuration + 10) * pxPerSec)

    // Every live clip delegate, keyed by clip id. Captured once per drag so a group move can
    // translate the whole selection while the grabbed clip owns the pointer.
    function clipItemSnapshot() {
        const map = ({})
        function walk(item) {
            const kids = item.children
            for (let i = 0; i < kids.length; ++i) {
                const child = kids[i]
                if (child.objectName === "clipBody" && child.clipId)
                    map[child.clipId] = child
                walk(child)
            }
        }
        walk(root)
        return map
    }

    // Where a drop at a scene point lands: the lane under the cursor and the time under it.
    // The row coordinates are content coordinates, so a point is only a hit if it is visible in the
    // Flickable's viewport; the labels column and the area outside the scrolled view are not.
    function dropTargetFor(sceneX, sceneY) {
        const view = timelineFlick.mapFromItem(null, sceneX, sceneY)
        if (view.x < 0 || view.y < 0 || view.x > timelineFlick.width || view.y > timelineFlick.height)
            return null
        const rows = [{ row: v2Row, track: 1 }, { row: v1Row, track: 0 }, { row: a1Row, track: 2 }]
        for (const entry of rows) {
            const p = entry.row.mapFromItem(timelineFlick, view)
            if (p.x >= 0 && p.x <= entry.row.width && p.y >= 0 && p.y <= entry.row.height)
                return { track: entry.track, atSec: Math.max(0, (view.x + timelineFlick.contentX) / root.pxPerSec) }
        }
        return null
    }

    // Files dropped from outside the app land on the lane and time under the pointer.
    function dropUrlsAt(sceneX, sceneY, urls) {
        const target = dropTargetFor(sceneX, sceneY)
        if (!target)
            Backend.importMedia(urls)
        else
            Backend.importMediaAt(urls, target.track, target.atSec)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Timeline Toolbar
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            ActionButton {
                text: "Split (S)"
                onClicked: Backend.splitAtPlayhead()
            }
            ActionButton {
                text: "Delete"
                onClicked: Backend.deleteSelected(false)
            }
            ActionButton {
                text: "Ripple Del"
                onClicked: Backend.deleteSelected(true)
            }

            Rectangle { Layout.preferredWidth: 1; Layout.preferredHeight: 18; color: Design.borderHairline }

            ActionButton {
                text: "+ Text"
                onClicked: Backend.addTextAtPlayhead("Title", false)
            }
            ActionButton {
                text: "+ Caption"
                onClicked: Backend.addTextAtPlayhead("Caption", true)
            }

            Text {
                visible: Backend.selectedClipCount > 1
                text: Backend.selectedClipCount + " clips selected"
                color: Design.textSecondary
                font.pixelSize: Design.tCaption
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Zoom"
                color: Design.textSecondary
                font.pixelSize: Design.tCaption
            }
            ActionButton {
                text: "−"
                flat: true
                fontPixelSize: Design.tTitle
                implicitWidth: 30
                onClicked: Backend.zoomPxPerSec = Backend.zoomPxPerSec * 0.8
            }
            ActionButton {
                text: "+"
                flat: true
                fontPixelSize: Design.tTitle
                implicitWidth: 30
                onClicked: Backend.zoomPxPerSec = Backend.zoomPxPerSec * 1.25
            }
        }

        // Tracks + Scrollable Ruler
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left Track Labels (synced vertically with timelineFlick)
            Flickable {
                Layout.preferredWidth: root.headerWidth
                Layout.fillHeight: true
                interactive: false
                clip: true
                contentWidth: root.headerWidth
                contentHeight: timelineFlick.contentHeight
                contentY: timelineFlick.contentY

            Column {
                width: root.headerWidth
                spacing: 2

                Rectangle {
                    width: root.headerWidth; height: 24
                    color: Design.surfaceSunken
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: Design.s3
                        anchors.verticalCenter: parent.verticalCenter
                        text: "TIME"
                        color: Design.textSecondary
                        font.pixelSize: Design.tMicro
                        font.letterSpacing: 0.6
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Design.borderHairline }
                }
                Repeater {
                    model: [
                        { label: "V2 Overlay", col: Design.laneV2, track: 1 },
                        { label: "V1 Video", col: Design.laneV1, track: 0 },
                        { label: "T1 Text", col: Design.warning, track: -1 },
                        { label: "A1 Audio", col: Design.success, track: 2 }
                    ]
                    delegate: Rectangle {
                        id: delegateRoot5
                        required property var modelData
                        readonly property var laneState: delegateRoot5.modelData.track >= 0
                                                            ? (Backend.lanesList[delegateRoot5.modelData.track] || ({}))
                                                            : ({})
                        width: root.headerWidth
                        height: root.trackHeight
                        color: Design.surfaceSunken
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 3
                            color: delegateRoot5.modelData.col
                            opacity: laneState.muted === true ? 0.35 : 1.0
                        }
                        Column {
                            objectName: "laneLabel_" + delegateRoot5.modelData.label.replace(" ", "")
                            // Reserve the strip the toggles occupy so a long lane name can never
                            // run under the controls; both columns are the same width on every row.
                            width: root.headerWidth - Design.s5 - laneToggles.width - Design.s3
                            anchors.left: parent.left
                            anchors.leftMargin: Design.s5
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 0
                            Text {
                                width: parent.width
                                text: delegateRoot5.modelData.label
                                elide: Text.ElideRight
                                color: laneState.locked === true ? Design.textSecondary : Design.textPrimary
                                font.pixelSize: Design.tCaption
                                font.weight: Design.wMedium
                            }
                            Text {
                                width: parent.width
                                text: delegateRoot5.modelData.track < 0 ? "text"
                                      : (delegateRoot5.modelData.track === 2 ? (laneState.muted ? "muted" : "audio")
                                                               : (laneState.locked ? "locked" : (laneState.muted ? "muted" : "video")))
                                elide: Text.ElideRight
                                color: Design.textSecondary
                                font.pixelSize: Design.tMicro
                            }
                        }
                        Row {
                            id: laneToggles
                            width: 3 * (Design.hCompact - 4) + 2 * Design.s1
                            anchors.right: parent.right
                            anchors.rightMargin: Design.s2
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Design.s1
                            visible: delegateRoot5.modelData.track >= 0
                            LaneToggle {
                                objectName: "laneToggle_" + delegateRoot5.modelData.label.replace(" ", "") + "_M"
                                label: "M"
                                active: laneState.muted === true
                                activeColor: Design.warning
                                toolTipText: "Mute this lane"
                                onToggled: Backend.setLaneState(delegateRoot5.modelData.track, "muted", !(laneState.muted === true))
                            }
                            LaneToggle {
                                objectName: "laneToggle_" + delegateRoot5.modelData.label.replace(" ", "") + "_S"
                                label: "S"
                                active: laneState.solo === true
                                activeColor: Design.accent
                                toolTipText: "Solo this lane"
                                onToggled: Backend.setLaneState(delegateRoot5.modelData.track, "solo", !(laneState.solo === true))
                            }
                            LaneToggle {
                                objectName: "laneToggle_" + delegateRoot5.modelData.label.replace(" ", "") + "_L"
                                label: "L"
                                active: laneState.locked === true
                                activeColor: Design.danger
                                toolTipText: "Lock this lane against edits"
                                onToggled: Backend.setLaneState(delegateRoot5.modelData.track, "locked", !(laneState.locked === true))
                            }
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Design.borderHairline }
                    }
                }
            }
            }

            // Scrollable Timeline Area
            Flickable {
                id: timelineFlick
                objectName: "timelineFlick"
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: root.timelineWidth
                contentHeight: 24 + 4 * (root.trackHeight + 2) + (root.timelineWidth > width ? 10 : 0)
                clip: true

                ScrollBar.horizontal: ScrollBar {
                    id: timelineHScroll
                    objectName: "timelineHScroll"
                    policy: ScrollBar.AsNeeded
                    height: 8
                    contentItem: Rectangle {
                        implicitHeight: 6
                        radius: height / 2
                        color: timelineHScroll.pressed ? Design.accent : Design.borderStrong
                    }
                    background: Rectangle { color: Design.surfaceSunken }
                }

                ScrollBar.vertical: ScrollBar {
                    id: timelineVScroll
                    objectName: "timelineVScroll"
                    policy: ScrollBar.AsNeeded
                    width: 8
                    contentItem: Rectangle {
                        implicitWidth: 6
                        radius: width / 2
                        color: timelineVScroll.pressed ? Design.accent : Design.borderStrong
                    }
                    background: Rectangle { color: Design.surfaceSunken }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onPressed: (mouse) => {
                        Backend.playheadSec = Math.max(0, mouse.x / root.pxPerSec)
                    }
                    onPositionChanged: (mouse) => {
                        if (pressed)
                            Backend.playheadSec = Math.max(0, mouse.x / root.pxPerSec)
                    }
                    onWheel: (wheel) => {
                        const maxX = Math.max(0, timelineFlick.contentWidth - timelineFlick.width)
                        const delta = wheel.angleDelta.x !== 0 ? wheel.angleDelta.x : wheel.angleDelta.y
                        if (maxX > 0 && (wheel.angleDelta.x !== 0 || (wheel.modifiers & Qt.ShiftModifier) || timelineFlick.contentHeight <= timelineFlick.height)) {
                            timelineFlick.contentX = Math.max(0, Math.min(maxX, timelineFlick.contentX - delta * 0.5))
                            wheel.accepted = true
                        } else {
                            wheel.accepted = false
                        }
                    }
                }

                Rectangle {
                    width: timelineFlick.contentWidth
                    height: timelineFlick.contentHeight
                    color: Design.surfaceSunken
                    z: -2
                }

                // Time grid, drawn behind the clips.
                Repeater {
                    model: Math.ceil(root.timelineWidth / (2 * root.pxPerSec))
                    delegate: Rectangle {
                        x: index * 2 * root.pxPerSec
                        y: 24
                        width: 1
                        height: timelineFlick.contentHeight - 24
                        color: Design.borderHairline
                        opacity: 0.5
                        z: -1
                    }
                }

                Column {
                    id: tracksColumn
                    width: root.timelineWidth
                    spacing: 2

                    // Time Ruler
                    Rectangle {
                        width: root.timelineWidth
                        height: 24
                        color: Design.surfaceSunken
                        border.color: Design.borderHairline
                        border.width: 1

                        Repeater {
                            model: Math.ceil(root.timelineWidth / (2 * root.pxPerSec))
                            delegate: Item {
                                x: index * 2 * root.pxPerSec
                                height: 24
                                Rectangle { width: 1; height: 8; anchors.bottom: parent.bottom; color: Design.borderStrong }
                                Text {
                                    // The playhead handle is 13 px wide and centred on t=0, so the
                                    // first tick needs to clear it; later ticks keep the hairline inset.
                                    x: index === 0 ? Design.s4 + Design.s1 : Design.s3
                                    y: 4
                                    text: Math.floor(index * 2 / 60) + ":" + String(index * 2 % 60).padStart(2, "0")
                                    color: Design.textSecondary
                                    font.pixelSize: Design.tMicro
                                }
                            }
                        }
                    }

                    // Track 1: V2 Overlay
                    Rectangle {
                        id: v2Row
                        width: root.timelineWidth
                        height: root.trackHeight
                        color: v2Drop.hovering ? Design.alpha(Design.textPrimary, 0.05) : "transparent"
                        border.color: v2Drop.hovering ? Design.laneV2 : Design.borderHairline
                        border.width: 1

                        TrackDropZone {
                            id: v2Drop
                            objectName: "dropV2"
                            track: 1
                            laneColor: Design.laneV2
                        }

                        Repeater {
                            model: Backend.clipsList
                            delegate: Loader {
                                id: delegateRoot1
                                required property var modelData
                                active: delegateRoot1.modelData.track === 1
                                sourceComponent: clipDelegate
                                property var clipData: delegateRoot1.modelData
                                property color laneColor: Design.laneV2
                            }
                        }

                        TransitionJunctions { laneTrack: 1 }
                    }

                    // Track 0: V1 Main Video
                    Rectangle {
                        id: v1Row
                        width: root.timelineWidth
                        height: root.trackHeight
                        color: v1Drop.hovering ? Design.alpha(Design.textPrimary, 0.05) : "transparent"
                        border.color: v1Drop.hovering ? Design.laneV1 : Design.borderHairline
                        border.width: 1

                        TrackDropZone {
                            id: v1Drop
                            objectName: "dropV1"
                            track: 0
                            laneColor: Design.laneV1
                        }

                        Repeater {
                            model: Backend.clipsList
                            delegate: Loader {
                                id: delegateRoot2
                                required property var modelData
                                active: delegateRoot2.modelData.track === 0
                                sourceComponent: clipDelegate
                                property var clipData: delegateRoot2.modelData
                                property color laneColor: Design.laneV1
                            }
                        }

                        TransitionJunctions { laneTrack: 0 }
                    }

                    // Track T1: Text & Captions
                    Rectangle {
                        width: root.timelineWidth
                        height: root.trackHeight
                        color: "transparent"
                        border.color: Design.borderHairline
                        border.width: 1

                        Repeater {
                            model: Backend.textsList
                            delegate: Rectangle {
                                required property var modelData
                                id: textItem
                                x: textItem.modelData.startSec * root.pxPerSec
                                y: 4
                                width: Math.max(28, textItem.modelData.durationSec * root.pxPerSec)
                                height: root.trackHeight - 8
                                radius: Design.rSm
                                color: Qt.darker(Design.warning, 1.8)
                                border.color: Backend.selectedTextId === textItem.modelData.id ? Design.textPrimary : Design.warning
                                border.width: Backend.selectedTextId === textItem.modelData.id ? 2 : 1

                                Text {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    text: (textItem.modelData.isCaption ? "CC: " : "T: ") + textItem.modelData.text
                                    color: Design.textPrimary
                                    font.pixelSize: Design.tCaption
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideRight
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    preventStealing: true
                                    drag.target: textItem
                                    drag.axis: Drag.XAxis
                                    drag.minimumX: 0
                                    onPressed: Backend.selectText(textItem.modelData.id)
                                    onReleased: Backend.moveText(textItem.modelData.id, textItem.x / root.pxPerSec)
                                }
                            }
                        }
                    }

                    // Track 2: A1 Audio / Voiceover
                    Rectangle {
                        id: a1Row
                        width: root.timelineWidth
                        height: root.trackHeight
                        color: a1Drop.hovering ? Design.alpha(Design.textPrimary, 0.05) : "transparent"
                        border.color: a1Drop.hovering ? Design.success : Design.borderHairline
                        border.width: 1

                        TrackDropZone {
                            id: a1Drop
                            objectName: "dropA1"
                            track: 2
                            laneColor: Design.success
                        }

                        Repeater {
                            model: Backend.clipsList
                            delegate: Loader {
                                id: delegateRoot3
                                required property var modelData
                                active: delegateRoot3.modelData.track === 2
                                sourceComponent: clipDelegate
                                property var clipData: delegateRoot3.modelData
                                property color laneColor: Design.success
                            }
                        }

                        TransitionJunctions { laneTrack: 2 }
                    }
                }

                // Snap guide: appears when a drag is within snapping distance of an edge.
                Rectangle {
                    visible: Backend.snapGuideSec >= 0
                    x: Backend.snapGuideSec * root.pxPerSec
                    y: 24
                    width: 1
                    height: timelineFlick.contentHeight - 24
                    color: Design.accent
                    opacity: 0.9
                    z: 40
                }

                // Playhead: 1px needle with a grab handle you can drag.
                Item {
                    id: playhead
                    x: Backend.playheadSec * root.pxPerSec - width / 2
                    y: 0
                    width: 13
                    height: parent.height
                    z: 50

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 1
                        height: parent.height
                        color: Design.accent
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 0
                        width: 13
                        height: 16
                        radius: Design.rXs
                        color: Design.accent
                        border.color: Design.alpha(Design.surfaceSunken, 0.5)
                        border.width: 1
                    }
                    MouseArea {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 0
                        width: 15
                        height: 16
                        cursorShape: Qt.SizeHorCursor
                        drag.target: playhead
                        drag.axis: Drag.XAxis
                        drag.minimumX: -width / 2
                        drag.maximumX: timelineFlick.contentWidth - width / 2
                        onPositionChanged: {
                            if (drag.active)
                                Backend.playheadSec = Math.max(0, (playhead.x + width / 2) / root.pxPerSec)
                        }
                    }
                }
            }
        }
    }

    // Small square lane control (M / S / L). Reads as state, not as a button row.
    component LaneToggle: Rectangle {
        id: toggle
        property string label: "M"
        property bool active: false
        property color activeColor: Design.accent
        property string toolTipText: ""
        implicitWidth: Design.hCompact - 4
        implicitHeight: Design.hCompact - 6
        radius: Design.rXs
        color: active ? activeColor : "transparent"
        border.width: active ? 0 : 1
        border.color: Design.borderHairline
        Text {
            anchors.centerIn: parent
            text: toggle.label
            color: toggle.active ? Design.onAccent : Design.textSecondary
            font.pixelSize: Design.tMicro
            font.weight: Design.wSemibold
        }
        MouseArea {
            id: toggleMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: toggle.toggled()
        }
        ToolTip.visible: hovered && toggle.toolTipText.length > 0
        ToolTip.text: toggle.toolTipText
        ToolTip.delay: 400
        readonly property bool hovered: toggleMouse.containsMouse
        signal toggled()
    }

    // A junction where two clips touch on one lane. The chip sets the transition that carries the
    // left clip into the right one, using the same kinds the exporter chains through xfade.
    component TransitionJunctions: Item {
        id: junctions
        property int laneTrack: 0
        anchors.fill: parent

        Repeater {
            model: Backend.transitionPoints
            delegate: Item {
                id: delegateRoot6
                required property var modelData
                visible: delegateRoot6.modelData.track === junctions.laneTrack
                x: delegateRoot6.modelData.timeSec * root.pxPerSec - width / 2
                y: 0
                width: 20
                height: root.trackHeight
                z: 30

                Rectangle {
                    visible: delegateRoot6.modelData.kind !== "none"
                    x: parent.width / 2 - width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: delegateRoot6.modelData.durationSec * root.pxPerSec
                    height: parent.height - 8
                    radius: Design.rXs
                    color: Design.alpha(Design.accent, 0.22)
                    border.width: 1
                    border.color: Design.accent
                }

                Rectangle {
                    objectName: "transitionChip_" + delegateRoot6.modelData.secondId
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    radius: Design.rXs
                    color: chipMouse.containsMouse ? Design.surfaceOverlay : Design.surfaceRaised
                    border.width: 1
                    border.color: delegateRoot6.modelData.kind !== "none" ? Design.accent : Design.borderStrong

                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        color: delegateRoot6.modelData.kind !== "none" ? Design.accent : Design.textSecondary
                        font.pixelSize: Design.tMicro
                        font.weight: Design.wSemibold
                    }
                    MouseArea {
                        id: chipMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: chipMenu.open()
                    }
                    Menu {
                        id: chipMenu
                        objectName: "transitionMenu_" + delegateRoot6.modelData.secondId
                        // The model rebuilds when the transition changes, so commit after the menu closes.
                        MenuItem {
                            objectName: "transitionOption_none"
                            text: "No transition"
                            onTriggered: Qt.callLater(() => Backend.setTransitionBetween(delegateRoot6.modelData.firstId, delegateRoot6.modelData.secondId, "none"))
                        }
                        MenuSeparator {}
                        MenuItem {
                            objectName: "transitionOption_fade"
                            text: "Fade"
                            onTriggered: Qt.callLater(() => Backend.setTransitionBetween(delegateRoot6.modelData.firstId, delegateRoot6.modelData.secondId, "fade"))
                        }
                        MenuItem {
                            objectName: "transitionOption_dissolve"
                            text: "Dissolve"
                            onTriggered: Qt.callLater(() => Backend.setTransitionBetween(delegateRoot6.modelData.firstId, delegateRoot6.modelData.secondId, "dissolve"))
                        }
                        MenuItem {
                            objectName: "transitionOption_wipeleft"
                            text: "Wipe left"
                            onTriggered: Qt.callLater(() => Backend.setTransitionBetween(delegateRoot6.modelData.firstId, delegateRoot6.modelData.secondId, "wipeleft"))
                        }
                        MenuItem {
                            objectName: "transitionOption_wiperight"
                            text: "Wipe right"
                            onTriggered: Qt.callLater(() => Backend.setTransitionBetween(delegateRoot6.modelData.firstId, delegateRoot6.modelData.secondId, "wiperight"))
                        }
                        MenuItem {
                            objectName: "transitionOption_slideleft"
                            text: "Slide left"
                            onTriggered: Qt.callLater(() => Backend.setTransitionBetween(delegateRoot6.modelData.firstId, delegateRoot6.modelData.secondId, "slideleft"))
                        }
                        MenuItem {
                            objectName: "transitionOption_slideright"
                            text: "Slide right"
                            onTriggered: Qt.callLater(() => Backend.setTransitionBetween(delegateRoot6.modelData.firstId, delegateRoot6.modelData.secondId, "slideright"))
                        }
                    }
                }
            }
        }
    }

    // Drop target for media-bin cards. Coordinates are local to the track row, which already
    // lives in the Flickable's content space, so they include the horizontal scroll offset.
    component TrackDropZone: DropArea {
        id: zone
        property int track: 0
        property color laneColor: Design.laneV1
        property bool acceptsSource: false
        property real ghostX: 0
        readonly property bool hovering: containsDrag && acceptsSource

        anchors.fill: parent
        z: 10
        keys: ["cutcap/asset"]

        onEntered: (drag) => {
            const src = drag.source
            zone.acceptsSource = !!src && src.assetId !== undefined
                                 && ((zone.track === 2) === (src.assetKind === "audio"))
            drag.accepted = zone.acceptsSource
            zone.ghostX = drag.x
        }
        onPositionChanged: (drag) => {
            const raw = Math.max(0, drag.x / root.pxPerSec)
            zone.ghostX = Backend.previewSnap("", raw, zone.track) * root.pxPerSec
        }
        onExited: {
            zone.acceptsSource = false
            Backend.clearSnapGuide()
        }
        onDropped: (drop) => {
            if (zone.acceptsSource)
                Backend.addAssetToTimeline(drop.source.assetId, zone.track, Math.max(0, drop.x / root.pxPerSec))
            zone.acceptsSource = false
            Backend.clearSnapGuide()
        }

        Rectangle {
            visible: zone.hovering
            x: Math.max(0, zone.ghostX)
            y: 3
            width: Math.max(24, (zone.drag.source ? zone.drag.source.assetDuration : 0) * root.pxPerSec)
            height: root.trackHeight - 6
            radius: Design.rSm
            color: Design.alpha(zone.laneColor, 0.28)
            border.color: zone.laneColor
            border.width: 1

            Text {
                anchors.fill: parent
                anchors.margins: 5
                text: zone.drag.source ? zone.drag.source.assetName : ""
                color: Design.textPrimary
                font.pixelSize: Design.tCaption
                elide: Text.ElideRight
            }
        }
    }

    Component {
        id: clipDelegate
        Rectangle {
            id: clipBox
            objectName: "clipBody"
            readonly property string clipId: clipData ? clipData.id : ""
            readonly property bool selected: Backend.selectedClipIds.indexOf(clipData.id) >= 0
            readonly property bool locked: (Backend.lanesList[clipData.track] || ({})).locked === true
            property bool hovered: false
            property var dragOrigins: ({})
            property var dragItems: ({})
            opacity: locked ? 0.55 : 1.0
            readonly property bool visual: clipData.kind !== "audio" && clipData.track !== 2
            readonly property real sourceDuration: Math.max(0.001, clipData.sourceDurationSec)
            // Source time (seconds) shown at a horizontal position inside the clip.
            function sourceTimeAt(px) {
                return clipData.sourceInSec + (px / root.pxPerSec) * clipData.speed
            }

            x: clipData.timelineStartSec * root.pxPerSec
            y: 3
            width: Math.max(24, clipData.durationSec * root.pxPerSec)
            height: root.trackHeight - 6
            radius: Design.rXs
            color: Qt.darker(laneColor, 2.6)
            clip: true

            // Video/image: frames from the filmstrip at the source time under each tile.
            Row {
                id: strip
                objectName: "clipStrip"
                readonly property real tileWidth: height * 16 / 9
                anchors.fill: parent
                visible: clipBox.visual && (clipData.filmstripPath || "").length > 0
                Repeater {
                    model: strip.visible && strip.tileWidth > 0 ? Math.ceil(clipBox.width / strip.tileWidth) : 0
                    delegate: Image {
                        readonly property int frames: clipData.filmstripFrames || 1
                        readonly property int tileWidth: clipData.filmstripTileWidth || 1
                        readonly property int tileHeight: clipData.filmstripTileHeight || 1
                        readonly property int frameIndex: Math.max(0, Math.min(frames - 1,
                            Math.floor(clipBox.sourceTimeAt((index + 0.5) * strip.tileWidth) / clipBox.sourceDuration * frames)))
                        width: strip.tileWidth
                        height: strip.height
                        source: "file://" + clipData.filmstripPath
                        sourceClipRect: Qt.rect(frameIndex * tileWidth, 0, tileWidth, tileHeight)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: true
                    }
                }
            }

            // Audio: waveform of the trimmed source segment. Only the visible slice is drawn, so a
            // long clip at max zoom stays one item instead of one per three pixels.
            Canvas {
                id: wave
                objectName: "clipWave"
                readonly property var peaks: clipData.waveformPeaks || []
                readonly property real viewLeft: timelineFlick.contentX - clipBox.x
                readonly property real firstX: Math.max(2, viewLeft)
                readonly property real lastX: Math.min(clipBox.width - 2, viewLeft + timelineFlick.width)
                x: firstX
                y: 16
                width: Math.max(0, lastX - firstX)
                height: clipBox.height - 19
                visible: !clipBox.visual && peaks.length > 0 && width > 1
                onVisibleChanged: if (visible) requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPeaksChanged: requestPaint()
                Connections {
                    target: timelineFlick
                    function onContentXChanged() {
                        if (wave.visible)
                            wave.requestPaint()
                    }
                }
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    if (peaks.length === 0 || width <= 0 || height <= 0)
                        return
                    ctx.fillStyle = laneColor
                    for (let px = 0; px < width; px += 3) {
                        const frac = clipBox.sourceTimeAt(firstX + px + 1) / clipBox.sourceDuration
                        const idx = Math.max(0, Math.min(peaks.length - 1, Math.floor(frac * peaks.length)))
                        const h = Math.max(2, Math.min(height, peaks[idx] * height))
                        ctx.fillRect(px, (height - h) / 2, 2, h)
                    }
                }
            }

            // Lane identity line: marks the track a clip belongs to, not decoration.
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 2
                color: laneColor
                opacity: 0.9
            }

            // Name chip on a scrim so it reads over any frame.
            Rectangle {
                x: 4
                y: 3
                width: Math.min(nameLabel.implicitWidth + 10, clipBox.width - 8)
                height: nameLabel.implicitHeight + 4
                radius: Design.rSm
                color: Design.alpha(Design.surfaceSunken, 0.72)
                visible: clipBox.width > 36
                Text {
                    id: nameLabel
                    anchors.fill: parent
                    anchors.leftMargin: 5
                    anchors.rightMargin: 5
                    verticalAlignment: Text.AlignVCenter
                    text: clipData.name
                    color: Design.textPrimary
                    font.pixelSize: Design.tCaption
                    font.weight: Design.wSemibold
                    elide: Text.ElideRight
                }
            }

            // Keyframe markers
            Repeater {
                model: clipData.keyframes ? clipData.keyframes : []
                delegate: Rectangle {
                    id: delegateRoot4
                    required property var modelData
                    x: Math.max(2, Math.min(clipBox.width - 10, delegateRoot4.modelData.timeSec * root.pxPerSec - 4))
                    y: clipBox.height - 12
                    width: 8
                    height: 8
                    rotation: 45
                    color: Design.warning
                    border.color: Design.surfaceSunken
                    border.width: 1
                }
            }

            // Lane colour / selection outline drawn above the frames; hover lifts the surface.
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: clipBox.hovered && !clipBox.selected ? Design.alpha(Design.textPrimary, 0.06) : "transparent"
                border.color: clipBox.selected ? Design.textPrimary : (clipBox.hovered ? Design.mix(laneColor, Design.textPrimary, 0.35) : laneColor)
                border.width: clipBox.selected ? 2 : 1
                Behavior on color { ColorAnimation { duration: Design.msFast; easing.type: Easing.OutQuad } }
            }
            HoverHandler { onHoveredChanged: clipBox.hovered = hovered }

            // Left Trim Handle
            Rectangle {
                width: 7
                height: parent.height
                anchors.left: parent.left
                color: leftTrimMouse.containsMouse ? Design.accent : "transparent"

                MouseArea {
                    id: leftTrimMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: Qt.SizeHorCursor
                    property real startGlobalX: 0
                    property real origX: 0
                    property real origW: 0
                    onPressed: (mouse) => {
                        if (!clipBox.selected)
                            Backend.selectClip(clipData.id)
                        startGlobalX = mapToGlobal(mouse.x, 0).x
                        origX = clipBox.x
                        origW = clipBox.width
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed) return
                        const dx = mapToGlobal(mouse.x, 0).x - startGlobalX
                        const maxDx = origW - 24
                        const clampedDx = Math.max(-origX, Math.min(maxDx, dx))
                        clipBox.x = origX + clampedDx
                        clipBox.width = origW - clampedDx
                    }
                    onReleased: {
                        const newStart = Math.max(0, clipBox.x / root.pxPerSec)
                        const deltaSec = newStart - clipData.timelineStartSec
                        const newDur = Math.max(0.2, clipBox.width / root.pxPerSec)
                        const newSourceIn = Math.max(0, clipData.sourceInSec + deltaSec * clipData.speed)
                        Backend.trimClip(clipData.id, newStart, newDur, newSourceIn)
                    }
                }
            }

            // Drag body (horizontal + vertical track switch) + Right-click Menu
            Menu {
                id: clipContextMenu
                MenuItem {
                    text: "Split at Playhead (S)"
                    onTriggered: Backend.splitAtPlayhead()
                }
                MenuItem {
                    text: clipData.track === 0 ? "Move to V2 Overlay (PiP)" : "Move to V1 Main Video"
                    visible: clipData.kind !== "audio" && clipData.track !== 2
                    onTriggered: Backend.moveClip(clipData.id, clipData.timelineStartSec, clipData.track === 0 ? 1 : 0)
                }
                MenuSeparator {}
                MenuItem {
                    text: "Delete Clip (Del)"
                    onTriggered: {
                        Backend.selectClip(clipData.id)
                        Backend.deleteSelected(false)
                    }
                }
                MenuItem {
                    text: "Ripple Delete (Shift+Del)"
                    onTriggered: {
                        Backend.selectClip(clipData.id)
                        Backend.deleteSelected(true)
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                anchors.leftMargin: 7
                anchors.rightMargin: 7
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                preventStealing: true
                drag.target: clipBox
                drag.axis: Drag.XAxis
                drag.minimumX: 0
                property real pressGlobalY: 0
                property real pressStartSec: 0
                property int pressTrack: 0
                onPressed: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        if (!clipBox.selected)
                            Backend.selectClip(clipData.id)
                        clipContextMenu.popup()
                        return
                    }
                    if (mouse.modifiers & Qt.ControlModifier)
                        Backend.selectClipAdditive(clipData.id)
                    else if (mouse.modifiers & Qt.ShiftModifier)
                        Backend.selectClipRange(clipData.id)
                    else if (!clipBox.selected)
                        Backend.selectClip(clipData.id)

                    pressGlobalY = mapToGlobal(0, mouse.y).y
                    pressStartSec = clipData.timelineStartSec
                    pressTrack = clipData.track
                    // Capture every selected clip's own position so the group keeps its spacing.
                    clipBox.dragItems = root.clipItemSnapshot()
                    const origins = ({})
                    for (const id of Backend.selectedClipIds) {
                        const item = clipBox.dragItems[id]
                        if (item)
                            origins[id] = item.x
                    }
                    clipBox.dragOrigins = origins
                }
                onPositionChanged: (mouse) => {
                    if (!drag.active || clipBox.locked)
                        return
                    // Same snapping rule the commit will use, so the guide never lies.
                    const snapped = Backend.previewSnap(clipData.id, Math.max(0, clipBox.x / root.pxPerSec), clipData.track)
                    clipBox.x = snapped * root.pxPerSec
                    const grabX = clipBox.dragOrigins[clipData.id]
                    if (grabX === undefined)
                        return
                    const deltaPx = clipBox.x - grabX
                    for (const id of Backend.selectedClipIds) {
                        const item = clipBox.dragItems[id]
                        if (item && item !== clipBox && clipBox.dragOrigins[id] !== undefined)
                            item.x = Math.max(0, clipBox.dragOrigins[id] + deltaPx)
                    }
                }
                onReleased: (mouse) => {
                    Backend.clearSnapGuide()
                    if (mouse.button === Qt.RightButton)
                        return
                    let targetTrack = pressTrack
                    if (clipData.kind !== "audio" && clipData.track !== 2) {
                        const dy = mapToGlobal(0, mouse.y).y - pressGlobalY
                        if (dy < -root.trackHeight * 0.55)
                            targetTrack = 1
                        else if (dy > root.trackHeight * 0.55)
                            targetTrack = 0
                    }
                    Backend.moveSelectedClips(clipBox.x / root.pxPerSec - pressStartSec, targetTrack - pressTrack)
                    clipBox.dragOrigins = ({})
                    clipBox.dragItems = ({})
                }
            }

            // Right Trim Handle
            Rectangle {
                width: 7
                height: parent.height
                anchors.right: parent.right
                color: rightTrimMouse.containsMouse ? Design.accent : "transparent"

                MouseArea {
                    id: rightTrimMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: Qt.SizeHorCursor
                    property real startX: 0
                    property real origDur: 0
                    onPressed: (mouse) => {
                        if (!clipBox.selected)
                            Backend.selectClip(clipData.id)
                        startX = mapToGlobal(mouse.x, 0).x
                        origDur = clipData.durationSec
                    }
                    onPositionChanged: (mouse) => {
                        if (!pressed) return
                        const dx = mapToGlobal(mouse.x, 0).x - startX
                        clipBox.width = Math.max(24, (origDur + dx / root.pxPerSec) * root.pxPerSec)
                    }
                    onReleased: {
                        const newDur = Math.max(0.2, clipBox.width / root.pxPerSec)
                        Backend.trimClip(clipData.id, clipData.timelineStartSec, newDur, clipData.sourceInSec)
                    }
                }
            }
        }
    }
}
