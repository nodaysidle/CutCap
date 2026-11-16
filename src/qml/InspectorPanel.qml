import QtQuick
import CutCap
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: root
    color: Design.surfacePanel
    clip: true

    readonly property var selClip: backend.selectedClip
    readonly property var selText: backend.selectedText
    readonly property bool hasClip: backend.selectedClipId.length > 0 && selClip.id !== undefined
    readonly property bool hasText: backend.selectedTextId.length > 0 && selText.id !== undefined
    readonly property var selXf: backend.selectedClipTransform

    // Section label: small, secondary, sentence case, above a hairline rule. No shouting.
    component SectionLabel: Text {
        color: Design.textSecondary
        font.pixelSize: Design.tCaption
        font.weight: Design.wMedium
        font.letterSpacing: 0.2
    }

    // One property, editable two ways: type an exact number or drag. Reset returns the default.
    component ValueRow: ColumnLayout {
        id: row
        property string label: ""
        property real value: 0
        property real from: 0
        property real to: 1
        property real step: 0.01
        property real defaultValue: 0
        property int decimals: 2
        signal edited(real value)

        Layout.fillWidth: true
        spacing: Design.s1

        RowLayout {
            Layout.fillWidth: true
            spacing: Design.s2
            SectionLabel { text: row.label; Layout.fillWidth: true; elide: Text.ElideRight }
            TextField {
                id: entry
                Layout.preferredWidth: 58
                implicitHeight: Design.hCompact
                horizontalAlignment: TextInput.AlignRight
                verticalAlignment: TextInput.AlignVCenter
                color: Design.textPrimary
                font.pixelSize: Design.tCaption
                font.family: "monospace"
                text: Number(row.value).toFixed(row.decimals)
                selectByMouse: true
                leftPadding: Design.s2
                rightPadding: Design.s2
                topPadding: 0
                bottomPadding: 0
                topInset: 0
                bottomInset: 0
                background: Rectangle {
                    color: Design.surfaceSunken
                    radius: Design.rXs
                    border.width: 1
                    border.color: entry.activeFocus ? Design.focusRing : Design.borderHairline
                }
                validator: DoubleValidator { bottom: row.from; top: row.to; decimals: 4 }
                onEditingFinished: row.edited(Math.max(row.from, Math.min(row.to, Number.fromLocaleString(text))))
                Connections {
                    target: row
                    function onValueChanged() {
                        if (!entry.activeFocus)
                            entry.text = Number(row.value).toFixed(row.decimals)
                    }
                }
            }
            Rectangle {
                implicitWidth: Design.hCompact - 4
                implicitHeight: Design.hCompact - 4
                radius: Design.rXs
                color: resetMouse.containsMouse ? Design.surfaceRaised : "transparent"
                border.width: 1
                border.color: Design.borderHairline
                Text { anchors.centerIn: parent; text: "↺"; color: Design.textSecondary; font.pixelSize: Design.tCaption }
                MouseArea {
                    id: resetMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: row.edited(row.defaultValue)
                }
                ToolTip.visible: resetMouse.containsMouse
                ToolTip.text: "Reset to " + Number(row.defaultValue).toFixed(row.decimals)
                ToolTip.delay: 400
            }
        }
        CompactSlider {
            Layout.fillWidth: true
            from: row.from
            to: row.to
            stepSize: row.step
            value: row.value
            onMoved: row.edited(value)
        }
    }

    component Divider: Rectangle {
        Layout.fillWidth: true
        height: 1
        color: Design.borderHairline
    }

    // Material's touch-sized controls (48-56px) spread the inspector out; desktop-dense versions.
    component CompactSlider: Slider {
        implicitHeight: Design.hSmall
    }

    component CompactComboBox: ComboBox {
        implicitHeight: Design.hStd
        topInset: 0
        bottomInset: 0
        font.pixelSize: Design.tBody
    }

    component CollapsibleSection: Item {
        id: section
        property string title: ""
        property bool expanded: true
        default property alias content: bodyColumn.data
        property alias headerActions: actionsSlot.data

        width: parent ? parent.width : 0
        implicitHeight: sectionLayout.implicitHeight

        ColumnLayout {
            id: sectionLayout
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Design.s2

            Rectangle {
                id: headerBar
                Layout.fillWidth: true
                implicitHeight: Design.hSmall
                radius: Design.rXs
                color: headerMouse.containsMouse ? Design.mix(Design.surfacePanel, Design.textPrimary, 0.04) : "transparent"
                border.width: activeFocus ? 1 : 0
                border.color: Design.focusRing
                activeFocusOnTab: true

                Keys.onSpacePressed: section.expanded = !section.expanded
                Keys.onReturnPressed: section.expanded = !section.expanded

                RowLayout {
                    anchors.fill: parent
                    spacing: Design.s2

                    MouseArea {
                        id: headerMouse
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            headerBar.forceActiveFocus()
                            section.expanded = !section.expanded
                        }

                        RowLayout {
                            anchors.fill: parent
                            spacing: Design.s2
                            Text {
                                text: section.expanded ? "▾" : "▸"
                                color: Design.textSecondary
                                font.pixelSize: Design.tCaption
                            }
                            SectionLabel {
                                text: section.title
                                color: section.expanded ? Design.textPrimary : Design.textSecondary
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                    }

                    RowLayout {
                        id: actionsSlot
                        spacing: Design.s1
                    }
                }
            }

            ColumnLayout {
                id: bodyColumn
                Layout.fillWidth: true
                visible: section.expanded
                spacing: Design.s3
            }

            Divider {}
        }
    }

    component InspectorScrollBar: ScrollBar {
        id: sb
        policy: ScrollBar.AsNeeded
        width: 8
        contentItem: Rectangle {
            implicitWidth: 6
            radius: width / 2
            color: sb.pressed ? Design.accent : Design.borderStrong
            opacity: sb.active || sb.hovered || sb.size < 1.0 ? 0.85 : 0.0
        }
        background: Rectangle {
            color: "transparent"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

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
                    text: hasText ? "Text" : (hasClip ? selClip.name : "Inspector")
                    color: Design.textPrimary
                    font.weight: Design.wSemibold
                    font.pixelSize: Design.tBodyLg
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    visible: backend.selectedClipCount > 1
                    text: backend.selectedClipCount + " selected"
                    color: Design.textSecondary
                    font.pixelSize: Design.tCaption
                }
                ActionButton {
                    text: "Delete"
                    visible: hasClip || hasText
                    flat: true
                    textColor: Design.danger
                    onClicked: backend.deleteSelected(false)
                }
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Design.borderHairline }
        }

        // Empty state
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !hasClip && !hasText
            Text {
                anchors.centerIn: parent
                width: parent.width - Design.s6 * 2
                text: "Select a clip or text overlay\non the timeline to edit\neffects, audio, or keyframes"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: Design.textSecondary
                font.pixelSize: Design.tBody
            }
        }

        // Text / Caption Inspector
        ScrollView {
            id: textScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: hasText
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: InspectorScrollBar { objectName: "textInspectorScrollBar" }

            ColumnLayout {
                x: Design.s4
                width: Math.max(100, textScroll.availableWidth - Design.s4 * 2)
                spacing: Design.s3

                Item { implicitHeight: Design.s1 }

                SectionLabel { text: "Content" }
                TextArea {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    text: hasText ? selText.text : ""
                    wrapMode: Text.WordWrap
                    font.pixelSize: Design.tBody
                    color: Design.textPrimary
                    background: Rectangle {
                        color: Design.surfaceOverlay
                        radius: Design.rMd
                        border.color: Design.borderHairline
                    }
                    onTextChanged: {
                        if (hasText && activeFocus && text !== selText.text)
                            backend.updateTextProperty(selText.id, "text", text)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Font Size: " + (hasText ? selText.fontSize : 44); color: Design.textSecondary; font.pixelSize: Design.tCaption }
                    Item { Layout.fillWidth: true }
                    CheckBox {
                        text: "Bold"
                        checked: hasText ? selText.bold : true
                        font.pixelSize: Design.tCaption
                        onToggled: backend.updateTextProperty(selText.id, "bold", checked)
                    }
                }
                CompactSlider {
                    Layout.fillWidth: true
                    from: 18; to: 110; stepSize: 2
                    value: hasText ? selText.fontSize : 44
                    onMoved: backend.updateTextProperty(selText.id, "fontSize", Math.round(value))
                }

                ValueRow {
                    label: "Duration (s)"
                    value: hasText ? selText.durationSec : 3.0
                    from: 0.5; to: 20.0; step: 0.1; defaultValue: 3.0; decimals: 1
                    onEdited: (v) => backend.updateTextProperty(selText.id, "durationSec", v)
                }

                ValueRow {
                    label: "Box Opacity"
                    value: hasText ? selText.bgOpacity : 0.65
                    from: 0.0; to: 1.0; step: 0.05; defaultValue: 0.65; decimals: 2
                    onEdited: (v) => backend.updateTextProperty(selText.id, "bgOpacity", v)
                }

                Text { text: "Text Color"; color: Design.textSecondary; font.pixelSize: Design.tCaption }
                RowLayout {
                    spacing: Design.s3
                    Repeater {
                        model: ["#eceff4", Design.warning, Design.info, Design.success, Design.danger, Design.laneV2]
                        delegate: Rectangle {
                            width: 24; height: 24; radius: width / 2
                            color: modelData
                            border.color: (hasText && selText.color === modelData) ? Design.accent : Design.borderHairline
                            border.width: 2
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: backend.updateTextProperty(selText.id, "color", modelData)
                            }
                        }
                    }
                }

                Item { implicitHeight: Design.s3 }
            }
        }

        // One scrolling panel; collapsible sections keep the workspace scannable at small heights.
        ScrollView {
            id: clipScroll
            objectName: "clipInspectorScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: hasClip
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: InspectorScrollBar {
                id: clipScrollBar
                objectName: "clipInspectorScrollBar"
            }

            // A ScrollView positions one child; the clip sections must stack in a single column
            // or every section draws on top of the others.
            Column {
                id: clipColumn
                objectName: "clipInspectorColumn"
                x: Design.s4
                y: Design.s2
                width: Math.max(100, clipScroll.availableWidth - Design.s4 * 2)
                bottomPadding: Design.s4
                spacing: Design.s3

                CollapsibleSection {
                    objectName: "sectionTiming"
                    title: "Clip & Timing"
                    expanded: true

                    RowLayout {
                        Layout.fillWidth: true
                        visible: hasClip && selClip.kind !== "audio" && selClip.track !== 2
                        Text { text: "Track"; color: Design.textSecondary; font.pixelSize: Design.tCaption }
                        Item { Layout.fillWidth: true }
                        ActionButton {
                            text: selClip.track === 0 ? "V1 Main → V2" : "V2 Overlay → V1"
                            onClicked: backend.moveClip(selClip.id, selClip.timelineStartSec, selClip.track === 0 ? 1 : 0)
                        }
                    }

                    ValueRow {
                        label: "Speed ×"
                        value: hasClip ? selClip.speed : 1.0
                        from: 0.25; to: 4.0; step: 0.05; defaultValue: 1.0; decimals: 2
                        onEdited: (v) => backend.updateClipProperty(selClip.id, "speed", v)
                    }

                    Text { text: "Transition In"; color: Design.textSecondary; font.pixelSize: Design.tCaption }
                    CompactComboBox {
                        Layout.fillWidth: true
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: "None", value: "none" },
                            { text: "Fade", value: "fade" },
                            { text: "Dissolve", value: "dissolve" },
                            { text: "Wipe left", value: "wipeleft" },
                            { text: "Wipe right", value: "wiperight" },
                            { text: "Slide left", value: "slideleft" },
                            { text: "Slide right", value: "slideright" }
                        ]
                        currentIndex: hasClip ? Math.max(0, indexOfValue(selClip.transitionIn)) : 0
                        onActivated: backend.updateClipProperty(selClip.id, "transitionIn", currentValue)
                    }
                }

                CollapsibleSection {
                    objectName: "sectionLook"
                    title: "Color & Effects"
                    expanded: true
                    headerActions: ActionButton {
                        objectName: "resetLooksButton"
                        text: "Reset"
                        flat: true
                        horizontalPadding: Design.s2
                        toolTipText: "Reset brightness, contrast, saturation, blur and vignette"
                        onClicked: backend.applyLookPreset(selClip.id, "neutral")
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Design.s1
                        Repeater {
                            model: ["Neutral", "Punch", "Mono", "Warm", "Soft"]
                            delegate: ActionButton {
                                text: modelData
                                flat: true
                                horizontalPadding: Design.s2
                                toolTipText: "Apply the " + modelData + " look"
                                onClicked: backend.applyLookPreset(selClip.id, modelData.toLowerCase())
                            }
                        }
                    }

                    ValueRow {
                        label: "Brightness"
                        value: hasClip ? selClip.brightness : 0.0
                        from: -0.5; to: 0.5; step: 0.02; defaultValue: 0.0; decimals: 2
                        onEdited: (v) => backend.updateClipProperty(selClip.id, "brightness", v)
                    }

                    ValueRow {
                        label: "Contrast"
                        value: hasClip ? selClip.contrast : 1.0
                        from: 0.5; to: 2.0; step: 0.05; defaultValue: 1.0; decimals: 2
                        onEdited: (v) => backend.updateClipProperty(selClip.id, "contrast", v)
                    }

                    ValueRow {
                        label: "Saturation"
                        value: hasClip ? selClip.saturation : 1.0
                        from: 0.0; to: 2.0; step: 0.05; defaultValue: 1.0; decimals: 2
                        onEdited: (v) => backend.updateClipProperty(selClip.id, "saturation", v)
                    }

                    ValueRow {
                        label: "Blur"
                        value: hasClip ? selClip.blur : 0.0
                        from: 0.0; to: 20.0; step: 0.5; defaultValue: 0.0; decimals: 1
                        onEdited: (v) => backend.updateClipProperty(selClip.id, "blur", v)
                    }

                    CheckBox {
                        text: "Vignette"
                        checked: hasClip ? selClip.vignette : false
                        font.pixelSize: Design.tBody
                        onToggled: backend.updateClipProperty(selClip.id, "vignette", checked)
                    }
                }

                CollapsibleSection {
                    objectName: "sectionTransform"
                    title: "Transform & Keyframes"
                    expanded: true
                    headerActions: ActionButton {
                        text: "+ Keyframe"
                        flat: true
                        horizontalPadding: Design.s2
                        onClicked: backend.addKeyframeAtPlayhead(selClip.id)
                    }

                    ValueRow {
                        label: "Position X"
                        value: selXf.x
                        from: 0.0; to: 1.0; step: 0.01; defaultValue: 0.5; decimals: 2
                        onEdited: (v) => backend.updateTransformAtPlayhead(selClip.id, v, selXf.y, selXf.scale, selXf.opacity, selXf.rotation)
                    }

                    ValueRow {
                        label: "Position Y"
                        value: selXf.y
                        from: 0.0; to: 1.0; step: 0.01; defaultValue: 0.5; decimals: 2
                        onEdited: (v) => backend.updateTransformAtPlayhead(selClip.id, selXf.x, v, selXf.scale, selXf.opacity, selXf.rotation)
                    }

                    ValueRow {
                        label: "Scale"
                        value: selXf.scale
                        from: 0.2; to: 2.5; step: 0.05; defaultValue: 1.0; decimals: 2
                        onEdited: (v) => backend.updateTransformAtPlayhead(selClip.id, selXf.x, selXf.y, v, selXf.opacity, selXf.rotation)
                    }

                    ValueRow {
                        label: "Opacity"
                        value: selXf.opacity
                        from: 0.0; to: 1.0; step: 0.05; defaultValue: 1.0; decimals: 2
                        onEdited: (v) => backend.updateTransformAtPlayhead(selClip.id, selXf.x, selXf.y, selXf.scale, v, selXf.rotation)
                    }

                    ValueRow {
                        label: "Rotation (°)"
                        value: selXf.rotation
                        from: -180; to: 180; step: 1; defaultValue: 0.0; decimals: 0
                        onEdited: (v) => backend.updateTransformAtPlayhead(selClip.id, selXf.x, selXf.y, selXf.scale, selXf.opacity, v)
                    }

                    Repeater {
                        model: hasClip ? selClip.keyframes : []
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "◆ " + Number(modelData.timeSec).toFixed(2) + "s  scale=" + Number(modelData.scale).toFixed(2)
                                color: Design.warning
                                font.pixelSize: Design.tCaption
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            ActionButton {
                                text: "×"
                                flat: true
                                implicitWidth: 26
                                fontPixelSize: Design.tBodyLg
                                textColor: Design.danger
                                toolTipText: "Remove keyframe"
                                onClicked: backend.removeKeyframe(selClip.id, modelData.timeSec)
                            }
                        }
                    }
                }

                CollapsibleSection {
                    objectName: "sectionAudio"
                    title: "Audio & Cleanup"
                    expanded: hasClip && selClip.kind === "audio"

                    ValueRow {
                        label: "Volume"
                        value: hasClip ? selClip.volume : 1.0
                        from: 0.0; to: 2.0; step: 0.05; defaultValue: 1.0; decimals: 2
                        onEdited: (v) => backend.updateClipProperty(selClip.id, "volume", v)
                    }

                    CheckBox {
                        text: "Mute Clip Audio"
                        checked: hasClip ? selClip.muted : false
                        font.pixelSize: Design.tBody
                        onToggled: backend.updateClipProperty(selClip.id, "muted", checked)
                    }

                    SectionLabel { text: "Offline Audio Cleanup (FFmpeg DSP)" }

                    CheckBox {
                        text: "Denoise (afftdn)"
                        checked: hasClip ? selClip.audioDenoise : false
                        font.pixelSize: Design.tBody
                        onToggled: backend.updateClipProperty(selClip.id, "audioDenoise", checked)
                    }
                    CheckBox {
                        text: "Voice Clarity (High-pass + EQ)"
                        checked: hasClip ? selClip.audioVoiceClarity : false
                        font.pixelSize: Design.tBody
                        onToggled: backend.updateClipProperty(selClip.id, "audioVoiceClarity", checked)
                    }
                    CheckBox {
                        text: "Compressor (Even Speech)"
                        checked: hasClip ? selClip.audioCompress : false
                        font.pixelSize: Design.tBody
                        onToggled: backend.updateClipProperty(selClip.id, "audioCompress", checked)
                    }
                    CheckBox {
                        text: "Loudness Normalize (-14 LUFS)"
                        checked: hasClip ? selClip.audioNormalize : false
                        font.pixelSize: Design.tBody
                        onToggled: backend.updateClipProperty(selClip.id, "audioNormalize", checked)
                    }
                }

                CollapsibleSection {
                    objectName: "sectionBackground"
                    title: "Background & AI"
                    expanded: false

                    SectionLabel { text: "Removal Mode" }
                    CompactComboBox {
                        Layout.fillWidth: true
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: "None", value: "none" },
                            { text: "Chroma key", value: "chromakey" },
                            { text: "AI cutout (rembg)", value: "ai_rembg" }
                        ]
                        currentIndex: hasClip ? Math.max(0, indexOfValue(selClip.bgRemovalMode)) : 0
                        onActivated: backend.updateClipProperty(selClip.id, "bgRemovalMode", currentValue)
                    }

                    ValueRow {
                        label: "Chroma Similarity"
                        value: hasClip ? selClip.chromaSimilarity : 0.18
                        from: 0.02; to: 0.5; step: 0.01; defaultValue: 0.18; decimals: 2
                        onEdited: (v) => backend.updateClipProperty(selClip.id, "chromaSimilarity", v)
                    }

                    ValueRow {
                        label: "Chroma Blend"
                        value: hasClip ? selClip.chromaBlend : 0.08
                        from: 0.0; to: 0.4; step: 0.01; defaultValue: 0.08; decimals: 2
                        onEdited: (v) => backend.updateClipProperty(selClip.id, "chromaBlend", v)
                    }

                    RowLayout {
                        spacing: Design.s3
                        Text { text: "Key Color:"; color: Design.textSecondary; font.pixelSize: Design.tCaption }
                        Repeater {
                            model: [
                                { value: "#00ff00", name: "Green" },
                                { value: "#0000ff", name: "Blue" },
                                { value: "#14161a", name: "Graphite" },
                                { value: "#eceff4", name: "White" }
                            ]
                            delegate: Item {
                                id: swatch
                                width: 24
                                height: 24
                                activeFocusOnTab: true
                                Accessible.role: Accessible.RadioButton
                                Accessible.name: modelData.name + " key colour"
                                Accessible.checked: hasClip && selClip.chromaColor === modelData.value
                                Accessible.onPressAction: backend.updateClipProperty(selClip.id, "chromaColor", modelData.value)
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 20
                                    height: 20
                                    radius: Design.rXs
                                    color: modelData.value
                                    border.color: (hasClip && selClip.chromaColor === modelData.value) ? Design.accent : Design.borderHairline
                                    border.width: 2
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    radius: Design.rSm
                                    color: "transparent"
                                    border.width: swatch.activeFocus ? 2 : 0
                                    border.color: Design.focusRing
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        swatch.forceActiveFocus()
                                        backend.updateClipProperty(selClip.id, "chromaColor", modelData.value)
                                    }
                                }
                                Keys.onSpacePressed: backend.updateClipProperty(selClip.id, "chromaColor", modelData.value)
                                Keys.onReturnPressed: backend.updateClipProperty(selClip.id, "chromaColor", modelData.value)
                            }
                        }
                    }

                    ActionButton {
                        text: "Run Local AI Remove BG (rembg)"
                        Layout.fillWidth: true
                        fontPixelSize: Design.tBody
                        enabled: backend.capabilities.rembg !== "" && selClip.kind === "image" && !backend.busy
                        toolTipText: backend.capabilities.rembg === ""
                                     ? "Install rembg (uv tool install rembg[cli]) for local AI cutout; Chroma Key works without it"
                                     : (selClip.kind !== "image"
                                        ? "AI cutout works on image clips; Chroma Key removes a colour from video"
                                        : "Remove this image's background locally")
                        onClicked: backend.applyAiBackgroundRemoval(selClip.id)
                    }
                    Text {
                        visible: backend.busy
                        text: backend.statusMessage
                        color: Design.textSecondary
                        font.pixelSize: Design.tCaption
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    Text {
                        text: backend.capabilities.rembg
                              ? "rembg detected locally"
                              : "Chroma Key works built-in; install rembg for AI cutout"
                        color: backend.capabilities.rembg ? Design.success : Design.textSecondary
                        font.pixelSize: Design.tCaption
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                CollapsibleSection {
                    objectName: "sectionCaptionsVoice"
                    title: "Captions & Voice"
                    expanded: false

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.s1
                        TextField {
                            id: captionLang
                            Accessible.name: "Caption language"
                            text: "en"
                            Layout.preferredWidth: 52
                            implicitHeight: Design.hSmall
                            color: Design.textPrimary
                            font.pixelSize: Design.tCaption
                            leftPadding: Design.s2
                            rightPadding: Design.s2
                            topPadding: 0
                            bottomPadding: 0
                            topInset: 0
                            bottomInset: 0
                            verticalAlignment: TextInput.AlignVCenter
                            background: Rectangle {
                                color: Design.surfaceSunken
                                radius: Design.rXs
                                border.width: 1
                                border.color: captionLang.activeFocus ? Design.focusRing : Design.borderHairline
                            }
                        }
                        ActionButton {
                            text: "Auto captions"
                            Layout.fillWidth: true
                            enabled: backend.capabilities.whisper !== "" && !backend.busy
                            toolTipText: backend.capabilities.whisper !== ""
                                         ? "Transcribe this clip offline with " + backend.capabilities.whisper
                                         : "Install whisper.cpp or faster-whisper for offline captions"
                            onClicked: backend.generateAutoCaptions(selClip.id, captionLang.text)
                        }
                    }
                    Text {
                        visible: backend.busy
                        text: backend.statusMessage
                        color: Design.textSecondary
                        font.pixelSize: Design.tCaption
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.s1
                        ActionButton { text: "Import .srt"; Layout.fillWidth: true; onClicked: importSrtDialog.open() }
                        ActionButton { text: "Export .srt"; Layout.fillWidth: true; onClicked: exportSrtDialog.open() }
                    }

                    ActionButton {
                        text: backend.recordingVoiceover ? "Stop recording" : "Record voiceover"
                        Layout.fillWidth: true
                        fillColor: backend.recordingVoiceover ? Design.danger : Design.surfaceRaised
                        textColor: backend.recordingVoiceover ? Design.onAccent : Design.textPrimary
                        enabled: backend.recordingVoiceover || backend.capabilities.pwRecord
                        toolTipText: backend.recordingVoiceover || backend.capabilities.pwRecord
                                     ? "Record a voiceover onto the audio lane"
                                     : "Install pipewire (pw-record) or pulseaudio-utils (parec) to record"
                        onClicked: backend.toggleVoiceoverRecording()
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Design.s1
                        TextField {
                            id: ttsField
                            Accessible.name: "Text to speak"
                            placeholderText: "Text to speak locally"
                            Layout.fillWidth: true
                            implicitHeight: Design.hSmall
                            color: Design.textPrimary
                            font.pixelSize: Design.tCaption
                            leftPadding: Design.s2
                            rightPadding: Design.s2
                            topPadding: 0
                            bottomPadding: 0
                            topInset: 0
                            bottomInset: 0
                            verticalAlignment: TextInput.AlignVCenter
                            background: Rectangle {
                                color: Design.surfaceSunken
                                radius: Design.rXs
                                border.width: 1
                                border.color: ttsField.activeFocus ? Design.focusRing : Design.borderHairline
                            }
                        }
                        ActionButton {
                            text: "Speak"
                            enabled: ttsField.text.length > 0 && backend.capabilities.tts !== "" && !backend.busy
                            toolTipText: backend.capabilities.tts === ""
                                         ? "Install piper-tts or espeak-ng for offline speech"
                                         : "Generate a voice clip with the local TTS engine"
                            onClicked: backend.generateLocalTts(ttsField.text)
                        }
                    }
                }
            }
        }
    }
}
