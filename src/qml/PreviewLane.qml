import QtQuick
import QtMultimedia
import CutCap

// One timeline lane (V1 base, V2 overlay or A1 audio) rendered with its own MediaPlayer.
Item {
    id: root

    property string role: "base"
    property var lane: ({})
    property bool audioOnly: false
    property bool fitToMedia: false

    readonly property bool active: lane.active === true
    readonly property bool isImage: active && lane.isImage === true
    readonly property var xf: active ? lane.transform : ({ x: 0.5, y: 0.5, scale: 1.0, opacity: 1.0, rotation: 0.0 })
    // Any look change routes frames through ClipFrameItem, which applies the export filters' math.
    readonly property bool processed: active && (Math.abs(lane.brightness) > 0.001
                                                 || Math.abs(lane.contrast - 1.0) > 0.001
                                                 || Math.abs(lane.saturation - 1.0) > 0.001
                                                 || lane.blur > 0.5
                                                 || lane.vignette === true
                                                 || lane.chromaKey === true)
    readonly property alias player: player

    visible: active && !audioOnly

    function currentLane() {
        if (role === "base")
            return backend.baseLane
        if (role === "overlay")
            return backend.overlayLane
        return backend.audioLane
    }

    // forceSeek: the user moved the playhead (or edited the clip while paused).
    // Otherwise the clock lane is never re-seeked while playing, so audio stays continuous.
    function sync(forceSeek) {
        const d = currentLane()
        if (!d.active || d.isImage) {
            if (player.playbackState === MediaPlayer.PlayingState)
                player.pause()
            return
        }
        if (player.source.toString() !== d.url) {
            player.source = d.url
            return
        }
        if (player.mediaStatus === MediaPlayer.NoMedia || player.mediaStatus === MediaPlayer.LoadingMedia)
            return

        const playingClock = backend.playing && backend.clockLane === role
        const drift = Math.abs(player.position - d.positionMs)
        let seeked = false
        if (forceSeek ? drift > 15 : (!playingClock && drift > 300)) {
            player.position = d.positionMs
            seeked = true
        }

        if (backend.playing) {
            const ended = player.mediaStatus === MediaPlayer.EndOfMedia && !seeked
            if (player.playbackState !== MediaPlayer.PlayingState && !ended)
                player.play()
        } else if (player.playbackState !== MediaPlayer.PausedState) {
            // pause() from StoppedState decodes and shows the frame at position without audio.
            player.pause()
        }
    }

    MediaDevices {
        id: devices
    }

    AudioOutput {
        id: audioOut
        device: devices.defaultAudioOutput
        volume: root.active ? root.lane.volume : 1.0
        muted: root.active && root.lane.muted === true
    }

    MediaPlayer {
        id: player
        audioOutput: audioOut
        videoOutput: root.audioOnly ? null : (root.processed ? processor.videoSink : videoOut)
        playbackRate: root.active ? root.lane.speed : 1.0

        onPositionChanged: (pos) => {
            if (backend.playing && playbackState === MediaPlayer.PlayingState && backend.clockLane === root.role)
                backend.syncPlayheadFromMedia(root.role, pos)
        }
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia) {
                const d = root.currentLane()
                if (!d.active || source.toString() !== d.url)
                    return
                position = d.positionMs
                if (backend.playing)
                    play()
                else
                    pause()
            } else if (mediaStatus === MediaPlayer.EndOfMedia) {
                backend.laneReachedEnd(root.role)
            }
        }
    }

    Item {
        id: placement
        width: parent.width
        height: parent.height
        x: (root.xf.x - 0.5) * width
        y: (root.xf.y - 0.5) * height
        scale: root.xf.scale
        opacity: root.xf.opacity
        rotation: root.xf.rotation
        visible: !root.audioOnly

        Item {
            id: frame
            readonly property real mediaAspect: (root.lane.mediaWidth > 0 && root.lane.mediaHeight > 0)
                                                ? root.lane.mediaWidth / root.lane.mediaHeight
                                                : parent.width / Math.max(1, parent.height)
            // Height of this clip once fitted into the export canvas (what boxblur's radius refers to).
            readonly property real exportHeight: Math.min(backend.projectHeight, backend.projectWidth / mediaAspect)
            anchors.centerIn: parent
            width: root.fitToMedia ? Math.min(parent.width, parent.height * mediaAspect) : parent.width
            height: root.fitToMedia ? width / mediaAspect : parent.height

            VideoOutput {
                id: videoOut
                objectName: "videoOut"
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
                visible: !root.isImage && !root.processed
            }

            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                source: root.isImage && !root.processed ? root.lane.url : ""
                visible: root.isImage && !root.processed
            }

            ClipFrameItem {
                id: processor
                objectName: "clipFrame"
                anchors.fill: parent
                visible: root.processed
                imageSource: root.processed && root.isImage ? root.lane.url : ""
                brightness: root.processed ? root.lane.brightness : 0.0
                contrast: root.processed ? root.lane.contrast : 1.0
                saturation: root.processed ? root.lane.saturation : 1.0
                blur: root.processed ? root.lane.blur : 0.0
                referenceHeight: frame.exportHeight
                vignette: root.processed && root.lane.vignette === true
                chromaKey: root.processed && root.lane.chromaKey === true
                keyColor: root.processed && root.lane.chromaKey ? root.lane.chromaColor : "#00ff00"
                similarity: root.processed && root.lane.chromaKey ? root.lane.chromaSimilarity : 0.18
                blend: root.processed && root.lane.chromaKey ? root.lane.chromaBlend : 0.08
            }
        }
    }
}
