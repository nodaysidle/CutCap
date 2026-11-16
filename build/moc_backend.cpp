/****************************************************************************
** Meta object code from reading C++ file 'backend.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../src/backend.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'backend.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN7BackendE_t {};
} // unnamed namespace

template <> constexpr inline auto Backend::qt_create_metaobjectdata<qt_meta_tag_ZN7BackendE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Backend",
        "projectChanged",
        "",
        "assetsChanged",
        "timelineChanged",
        "selectionChanged",
        "playheadChanged",
        "playbackChanged",
        "viewChanged",
        "previewChanged",
        "previewStateChanged",
        "seekRequested",
        "activeTextsChanged",
        "snapGuideChanged",
        "exportStateChanged",
        "exportFinished",
        "ok",
        "message",
        "voiceoverStateChanged",
        "statusChanged",
        "errorOccurred",
        "newProject",
        "openProject",
        "pathOrUrl",
        "saveProject",
        "setProjectTitle",
        "title",
        "setProjectResolution",
        "width",
        "height",
        "importMedia",
        "QVariantList",
        "pathsOrUrls",
        "importSingleMedia",
        "importMediaAt",
        "track",
        "atSec",
        "setLaneState",
        "key",
        "value",
        "previewSnap",
        "clipId",
        "sec",
        "clearSnapGuide",
        "applyLookPreset",
        "preset",
        "removeAsset",
        "assetId",
        "addAssetToTimeline",
        "selectClip",
        "selectClipAdditive",
        "selectClipRange",
        "selectAllClips",
        "clearClipSelection",
        "selectText",
        "textId",
        "moveSelectedClips",
        "deltaSec",
        "deltaTrack",
        "moveClip",
        "newStartSec",
        "newTrack",
        "setTransitionBetween",
        "firstId",
        "secondId",
        "kind",
        "durationSec",
        "trimClip",
        "newDurationSec",
        "newSourceInSec",
        "splitAtPlayhead",
        "deleteSelected",
        "ripple",
        "updateClipProperty",
        "QVariant",
        "addKeyframeAtPlayhead",
        "updateTransformAtPlayhead",
        "x",
        "y",
        "scale",
        "opacity",
        "rotation",
        "removeKeyframe",
        "timeSec",
        "addTextAtPlayhead",
        "text",
        "isCaption",
        "updateTextProperty",
        "moveText",
        "importSrtFile",
        "exportSrtFile",
        "generateAutoCaptions",
        "language",
        "toggleVoiceoverRecording",
        "generateLocalTts",
        "applyAiBackgroundRemoval",
        "estimateExport",
        "QVariantMap",
        "codec",
        "resolutionPreset",
        "startExport",
        "outputPathOrUrl",
        "cancelExport",
        "undo",
        "redo",
        "syncPlayheadFromMedia",
        "lane",
        "mediaPosMs",
        "laneReachedEnd",
        "formatTimecode",
        "projectTitle",
        "projectPath",
        "dirty",
        "totalDuration",
        "playheadSec",
        "playing",
        "zoomPxPerSec",
        "selectedClipId",
        "selectedClipIds",
        "selectedClipCount",
        "selectedTextId",
        "selectedClip",
        "selectedText",
        "assetsList",
        "clipsList",
        "textsList",
        "transitionPoints",
        "activeTexts",
        "lanesList",
        "snapGuideSec",
        "capabilities",
        "canUndo",
        "canRedo",
        "baseLane",
        "overlayLane",
        "audioLane",
        "clockLane",
        "selectedClipTransform",
        "projectWidth",
        "projectHeight",
        "exporting",
        "exportProgress",
        "recordingVoiceover",
        "statusMessage"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'projectChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'assetsChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'timelineChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'selectionChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'playheadChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'playbackChanged'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'viewChanged'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'previewChanged'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'previewStateChanged'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'seekRequested'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'activeTextsChanged'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'snapGuideChanged'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'exportStateChanged'
        QtMocHelpers::SignalData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'exportFinished'
        QtMocHelpers::SignalData<void(bool, const QString &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 16 }, { QMetaType::QString, 17 },
        }}),
        // Signal 'voiceoverStateChanged'
        QtMocHelpers::SignalData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 17 },
        }}),
        // Method 'newProject'
        QtMocHelpers::MethodData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'openProject'
        QtMocHelpers::MethodData<bool(const QString &)>(22, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 23 },
        }}),
        // Method 'saveProject'
        QtMocHelpers::MethodData<bool(const QString &)>(24, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 23 },
        }}),
        // Method 'saveProject'
        QtMocHelpers::MethodData<bool()>(24, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Bool),
        // Method 'setProjectTitle'
        QtMocHelpers::MethodData<void(const QString &)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 26 },
        }}),
        // Method 'setProjectResolution'
        QtMocHelpers::MethodData<void(int, int)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 28 }, { QMetaType::Int, 29 },
        }}),
        // Method 'importMedia'
        QtMocHelpers::MethodData<void(const QVariantList &)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 31, 32 },
        }}),
        // Method 'importSingleMedia'
        QtMocHelpers::MethodData<void(const QString &)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 23 },
        }}),
        // Method 'importMediaAt'
        QtMocHelpers::MethodData<void(const QVariantList &, int, double)>(34, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 31, 32 }, { QMetaType::Int, 35 }, { QMetaType::Double, 36 },
        }}),
        // Method 'setLaneState'
        QtMocHelpers::MethodData<void(int, const QString &, bool)>(37, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 35 }, { QMetaType::QString, 38 }, { QMetaType::Bool, 39 },
        }}),
        // Method 'previewSnap'
        QtMocHelpers::MethodData<double(const QString &, double, int)>(40, 2, QMC::AccessPublic, QMetaType::Double, {{
            { QMetaType::QString, 41 }, { QMetaType::Double, 42 }, { QMetaType::Int, 35 },
        }}),
        // Method 'clearSnapGuide'
        QtMocHelpers::MethodData<void()>(43, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'applyLookPreset'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(44, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 }, { QMetaType::QString, 45 },
        }}),
        // Method 'removeAsset'
        QtMocHelpers::MethodData<void(const QString &)>(46, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 47 },
        }}),
        // Method 'addAssetToTimeline'
        QtMocHelpers::MethodData<void(const QString &, int, double)>(48, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 47 }, { QMetaType::Int, 35 }, { QMetaType::Double, 36 },
        }}),
        // Method 'addAssetToTimeline'
        QtMocHelpers::MethodData<void(const QString &, int)>(48, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 47 }, { QMetaType::Int, 35 },
        }}),
        // Method 'addAssetToTimeline'
        QtMocHelpers::MethodData<void(const QString &)>(48, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 47 },
        }}),
        // Method 'selectClip'
        QtMocHelpers::MethodData<void(const QString &)>(49, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 },
        }}),
        // Method 'selectClipAdditive'
        QtMocHelpers::MethodData<void(const QString &)>(50, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 },
        }}),
        // Method 'selectClipRange'
        QtMocHelpers::MethodData<void(const QString &)>(51, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 },
        }}),
        // Method 'selectAllClips'
        QtMocHelpers::MethodData<void()>(52, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'clearClipSelection'
        QtMocHelpers::MethodData<void()>(53, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'selectText'
        QtMocHelpers::MethodData<void(const QString &)>(54, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 55 },
        }}),
        // Method 'moveSelectedClips'
        QtMocHelpers::MethodData<bool(double, int)>(56, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Double, 57 }, { QMetaType::Int, 58 },
        }}),
        // Method 'moveClip'
        QtMocHelpers::MethodData<void(const QString &, double, int)>(59, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 }, { QMetaType::Double, 60 }, { QMetaType::Int, 61 },
        }}),
        // Method 'setTransitionBetween'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &, double)>(62, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 63 }, { QMetaType::QString, 64 }, { QMetaType::QString, 65 }, { QMetaType::Double, 66 },
        }}),
        // Method 'setTransitionBetween'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &)>(62, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 63 }, { QMetaType::QString, 64 }, { QMetaType::QString, 65 },
        }}),
        // Method 'trimClip'
        QtMocHelpers::MethodData<void(const QString &, double, double, double)>(67, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 }, { QMetaType::Double, 60 }, { QMetaType::Double, 68 }, { QMetaType::Double, 69 },
        }}),
        // Method 'splitAtPlayhead'
        QtMocHelpers::MethodData<void()>(70, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'deleteSelected'
        QtMocHelpers::MethodData<void(bool)>(71, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 72 },
        }}),
        // Method 'deleteSelected'
        QtMocHelpers::MethodData<void()>(71, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Method 'updateClipProperty'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QVariant &)>(73, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 }, { QMetaType::QString, 38 }, { 0x80000000 | 74, 39 },
        }}),
        // Method 'addKeyframeAtPlayhead'
        QtMocHelpers::MethodData<void(const QString &)>(75, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 },
        }}),
        // Method 'updateTransformAtPlayhead'
        QtMocHelpers::MethodData<void(const QString &, double, double, double, double, double)>(76, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 }, { QMetaType::Double, 77 }, { QMetaType::Double, 78 }, { QMetaType::Double, 79 },
            { QMetaType::Double, 80 }, { QMetaType::Double, 81 },
        }}),
        // Method 'removeKeyframe'
        QtMocHelpers::MethodData<void(const QString &, double)>(82, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 }, { QMetaType::Double, 83 },
        }}),
        // Method 'addTextAtPlayhead'
        QtMocHelpers::MethodData<void(const QString &, bool)>(84, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 85 }, { QMetaType::Bool, 86 },
        }}),
        // Method 'addTextAtPlayhead'
        QtMocHelpers::MethodData<void(const QString &)>(84, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 85 },
        }}),
        // Method 'addTextAtPlayhead'
        QtMocHelpers::MethodData<void()>(84, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
        // Method 'updateTextProperty'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QVariant &)>(87, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 55 }, { QMetaType::QString, 38 }, { 0x80000000 | 74, 39 },
        }}),
        // Method 'moveText'
        QtMocHelpers::MethodData<void(const QString &, double)>(88, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 55 }, { QMetaType::Double, 60 },
        }}),
        // Method 'importSrtFile'
        QtMocHelpers::MethodData<void(const QString &)>(89, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 23 },
        }}),
        // Method 'exportSrtFile'
        QtMocHelpers::MethodData<void(const QString &)>(90, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 23 },
        }}),
        // Method 'generateAutoCaptions'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(91, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 }, { QMetaType::QString, 92 },
        }}),
        // Method 'generateAutoCaptions'
        QtMocHelpers::MethodData<void(const QString &)>(91, 2, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void, {{
            { QMetaType::QString, 41 },
        }}),
        // Method 'toggleVoiceoverRecording'
        QtMocHelpers::MethodData<void()>(93, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'generateLocalTts'
        QtMocHelpers::MethodData<void(const QString &)>(94, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 85 },
        }}),
        // Method 'applyAiBackgroundRemoval'
        QtMocHelpers::MethodData<void(const QString &)>(95, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 },
        }}),
        // Method 'estimateExport'
        QtMocHelpers::MethodData<QVariantMap(const QString &, const QString &, const QString &) const>(96, 2, QMC::AccessPublic, 0x80000000 | 97, {{
            { QMetaType::QString, 45 }, { QMetaType::QString, 98 }, { QMetaType::QString, 99 },
        }}),
        // Method 'startExport'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &, const QString &)>(100, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 101 }, { QMetaType::QString, 45 }, { QMetaType::QString, 98 }, { QMetaType::QString, 99 },
        }}),
        // Method 'cancelExport'
        QtMocHelpers::MethodData<void()>(102, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'undo'
        QtMocHelpers::MethodData<void()>(103, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'redo'
        QtMocHelpers::MethodData<void()>(104, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'syncPlayheadFromMedia'
        QtMocHelpers::MethodData<void(const QString &, qint64)>(105, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 106 }, { QMetaType::LongLong, 107 },
        }}),
        // Method 'laneReachedEnd'
        QtMocHelpers::MethodData<void(const QString &)>(108, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 106 },
        }}),
        // Method 'formatTimecode'
        QtMocHelpers::MethodData<QString(double) const>(109, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::Double, 42 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'projectTitle'
        QtMocHelpers::PropertyData<QString>(110, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'projectPath'
        QtMocHelpers::PropertyData<QString>(111, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'dirty'
        QtMocHelpers::PropertyData<bool>(112, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'totalDuration'
        QtMocHelpers::PropertyData<double>(113, QMetaType::Double, QMC::DefaultPropertyFlags, 2),
        // property 'playheadSec'
        QtMocHelpers::PropertyData<double>(114, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 4),
        // property 'playing'
        QtMocHelpers::PropertyData<bool>(115, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 5),
        // property 'zoomPxPerSec'
        QtMocHelpers::PropertyData<double>(116, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 6),
        // property 'selectedClipId'
        QtMocHelpers::PropertyData<QString>(117, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'selectedClipIds'
        QtMocHelpers::PropertyData<QStringList>(118, QMetaType::QStringList, QMC::DefaultPropertyFlags, 3),
        // property 'selectedClipCount'
        QtMocHelpers::PropertyData<int>(119, QMetaType::Int, QMC::DefaultPropertyFlags, 3),
        // property 'selectedTextId'
        QtMocHelpers::PropertyData<QString>(120, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'selectedClip'
        QtMocHelpers::PropertyData<QVariantMap>(121, 0x80000000 | 97, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'selectedText'
        QtMocHelpers::PropertyData<QVariantMap>(122, 0x80000000 | 97, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'assetsList'
        QtMocHelpers::PropertyData<QVariantList>(123, 0x80000000 | 31, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 1),
        // property 'clipsList'
        QtMocHelpers::PropertyData<QVariantList>(124, 0x80000000 | 31, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'textsList'
        QtMocHelpers::PropertyData<QVariantList>(125, 0x80000000 | 31, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'transitionPoints'
        QtMocHelpers::PropertyData<QVariantList>(126, 0x80000000 | 31, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'activeTexts'
        QtMocHelpers::PropertyData<QVariantList>(127, 0x80000000 | 31, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 10),
        // property 'lanesList'
        QtMocHelpers::PropertyData<QVariantList>(128, 0x80000000 | 31, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'snapGuideSec'
        QtMocHelpers::PropertyData<double>(129, QMetaType::Double, QMC::DefaultPropertyFlags, 11),
        // property 'capabilities'
        QtMocHelpers::PropertyData<QVariantMap>(130, 0x80000000 | 97, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'canUndo'
        QtMocHelpers::PropertyData<bool>(131, QMetaType::Bool, QMC::DefaultPropertyFlags, 2),
        // property 'canRedo'
        QtMocHelpers::PropertyData<bool>(132, QMetaType::Bool, QMC::DefaultPropertyFlags, 2),
        // property 'baseLane'
        QtMocHelpers::PropertyData<QVariantMap>(133, 0x80000000 | 97, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 8),
        // property 'overlayLane'
        QtMocHelpers::PropertyData<QVariantMap>(134, 0x80000000 | 97, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 8),
        // property 'audioLane'
        QtMocHelpers::PropertyData<QVariantMap>(135, 0x80000000 | 97, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 8),
        // property 'clockLane'
        QtMocHelpers::PropertyData<QString>(136, QMetaType::QString, QMC::DefaultPropertyFlags, 8),
        // property 'selectedClipTransform'
        QtMocHelpers::PropertyData<QVariantMap>(137, 0x80000000 | 97, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 8),
        // property 'projectWidth'
        QtMocHelpers::PropertyData<int>(138, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'projectHeight'
        QtMocHelpers::PropertyData<int>(139, QMetaType::Int, QMC::DefaultPropertyFlags, 0),
        // property 'exporting'
        QtMocHelpers::PropertyData<bool>(140, QMetaType::Bool, QMC::DefaultPropertyFlags, 12),
        // property 'exportProgress'
        QtMocHelpers::PropertyData<double>(141, QMetaType::Double, QMC::DefaultPropertyFlags, 12),
        // property 'recordingVoiceover'
        QtMocHelpers::PropertyData<bool>(142, QMetaType::Bool, QMC::DefaultPropertyFlags, 14),
        // property 'statusMessage'
        QtMocHelpers::PropertyData<QString>(143, QMetaType::QString, QMC::DefaultPropertyFlags, 15),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Backend, qt_meta_tag_ZN7BackendE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Backend::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7BackendE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7BackendE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7BackendE_t>.metaTypes,
    nullptr
} };

void Backend::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Backend *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->projectChanged(); break;
        case 1: _t->assetsChanged(); break;
        case 2: _t->timelineChanged(); break;
        case 3: _t->selectionChanged(); break;
        case 4: _t->playheadChanged(); break;
        case 5: _t->playbackChanged(); break;
        case 6: _t->viewChanged(); break;
        case 7: _t->previewChanged(); break;
        case 8: _t->previewStateChanged(); break;
        case 9: _t->seekRequested(); break;
        case 10: _t->activeTextsChanged(); break;
        case 11: _t->snapGuideChanged(); break;
        case 12: _t->exportStateChanged(); break;
        case 13: _t->exportFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 14: _t->voiceoverStateChanged(); break;
        case 15: _t->statusChanged(); break;
        case 16: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 17: _t->newProject(); break;
        case 18: { bool _r = _t->openProject((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 19: { bool _r = _t->saveProject((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 20: { bool _r = _t->saveProject();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 21: _t->setProjectTitle((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 22: _t->setProjectResolution((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 23: _t->importMedia((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1]))); break;
        case 24: _t->importSingleMedia((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 25: _t->importMediaAt((*reinterpret_cast<std::add_pointer_t<QVariantList>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 26: _t->setLaneState((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 27: { double _r = _t->previewSnap((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])));
            if (_a[0]) *reinterpret_cast<double*>(_a[0]) = std::move(_r); }  break;
        case 28: _t->clearSnapGuide(); break;
        case 29: _t->applyLookPreset((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 30: _t->removeAsset((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 31: _t->addAssetToTimeline((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3]))); break;
        case 32: _t->addAssetToTimeline((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 33: _t->addAssetToTimeline((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 34: _t->selectClip((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 35: _t->selectClipAdditive((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 36: _t->selectClipRange((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 37: _t->selectAllClips(); break;
        case 38: _t->clearClipSelection(); break;
        case 39: _t->selectText((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 40: { bool _r = _t->moveSelectedClips((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 41: _t->moveClip((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 42: _t->setTransitionBetween((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4]))); break;
        case 43: _t->setTransitionBetween((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 44: _t->trimClip((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4]))); break;
        case 45: _t->splitAtPlayhead(); break;
        case 46: _t->deleteSelected((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 47: _t->deleteSelected(); break;
        case 48: _t->updateClipProperty((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QVariant>>(_a[3]))); break;
        case 49: _t->addKeyframeAtPlayhead((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 50: _t->updateTransformAtPlayhead((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[6]))); break;
        case 51: _t->removeKeyframe((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 52: _t->addTextAtPlayhead((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 53: _t->addTextAtPlayhead((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 54: _t->addTextAtPlayhead(); break;
        case 55: _t->updateTextProperty((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QVariant>>(_a[3]))); break;
        case 56: _t->moveText((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 57: _t->importSrtFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 58: _t->exportSrtFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 59: _t->generateAutoCaptions((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 60: _t->generateAutoCaptions((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 61: _t->toggleVoiceoverRecording(); break;
        case 62: _t->generateLocalTts((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 63: _t->applyAiBackgroundRemoval((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 64: { QVariantMap _r = _t->estimateExport((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])));
            if (_a[0]) *reinterpret_cast<QVariantMap*>(_a[0]) = std::move(_r); }  break;
        case 65: _t->startExport((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4]))); break;
        case 66: _t->cancelExport(); break;
        case 67: _t->undo(); break;
        case 68: _t->redo(); break;
        case 69: _t->syncPlayheadFromMedia((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[2]))); break;
        case 70: _t->laneReachedEnd((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 71: { QString _r = _t->formatTimecode((*reinterpret_cast<std::add_pointer_t<double>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::projectChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::assetsChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::timelineChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::selectionChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::playheadChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::playbackChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::viewChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::previewChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::previewStateChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::seekRequested, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::activeTextsChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::snapGuideChanged, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::exportStateChanged, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)(bool , const QString & )>(_a, &Backend::exportFinished, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::voiceoverStateChanged, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)()>(_a, &Backend::statusChanged, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (Backend::*)(const QString & )>(_a, &Backend::errorOccurred, 16))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QString*>(_v) = _t->projectTitle(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->projectPath(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->dirty(); break;
        case 3: *reinterpret_cast<double*>(_v) = _t->totalDuration(); break;
        case 4: *reinterpret_cast<double*>(_v) = _t->playheadSec(); break;
        case 5: *reinterpret_cast<bool*>(_v) = _t->playing(); break;
        case 6: *reinterpret_cast<double*>(_v) = _t->zoomPxPerSec(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->selectedClipId(); break;
        case 8: *reinterpret_cast<QStringList*>(_v) = _t->selectedClipIds(); break;
        case 9: *reinterpret_cast<int*>(_v) = _t->selectedClipCount(); break;
        case 10: *reinterpret_cast<QString*>(_v) = _t->selectedTextId(); break;
        case 11: *reinterpret_cast<QVariantMap*>(_v) = _t->selectedClip(); break;
        case 12: *reinterpret_cast<QVariantMap*>(_v) = _t->selectedText(); break;
        case 13: *reinterpret_cast<QVariantList*>(_v) = _t->assetsList(); break;
        case 14: *reinterpret_cast<QVariantList*>(_v) = _t->clipsList(); break;
        case 15: *reinterpret_cast<QVariantList*>(_v) = _t->textsList(); break;
        case 16: *reinterpret_cast<QVariantList*>(_v) = _t->transitionPoints(); break;
        case 17: *reinterpret_cast<QVariantList*>(_v) = _t->activeTexts(); break;
        case 18: *reinterpret_cast<QVariantList*>(_v) = _t->lanesList(); break;
        case 19: *reinterpret_cast<double*>(_v) = _t->snapGuideSec(); break;
        case 20: *reinterpret_cast<QVariantMap*>(_v) = _t->capabilities(); break;
        case 21: *reinterpret_cast<bool*>(_v) = _t->canUndo(); break;
        case 22: *reinterpret_cast<bool*>(_v) = _t->canRedo(); break;
        case 23: *reinterpret_cast<QVariantMap*>(_v) = _t->baseLane(); break;
        case 24: *reinterpret_cast<QVariantMap*>(_v) = _t->overlayLane(); break;
        case 25: *reinterpret_cast<QVariantMap*>(_v) = _t->audioLane(); break;
        case 26: *reinterpret_cast<QString*>(_v) = _t->clockLane(); break;
        case 27: *reinterpret_cast<QVariantMap*>(_v) = _t->selectedClipTransform(); break;
        case 28: *reinterpret_cast<int*>(_v) = _t->projectWidth(); break;
        case 29: *reinterpret_cast<int*>(_v) = _t->projectHeight(); break;
        case 30: *reinterpret_cast<bool*>(_v) = _t->exporting(); break;
        case 31: *reinterpret_cast<double*>(_v) = _t->exportProgress(); break;
        case 32: *reinterpret_cast<bool*>(_v) = _t->recordingVoiceover(); break;
        case 33: *reinterpret_cast<QString*>(_v) = _t->statusMessage(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 4: _t->setPlayheadSec(*reinterpret_cast<double*>(_v)); break;
        case 5: _t->setPlaying(*reinterpret_cast<bool*>(_v)); break;
        case 6: _t->setZoomPxPerSec(*reinterpret_cast<double*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *Backend::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Backend::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7BackendE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Backend::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 72)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 72;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 72)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 72;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 34;
    }
    return _id;
}

// SIGNAL 0
void Backend::projectChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Backend::assetsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Backend::timelineChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void Backend::selectionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void Backend::playheadChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void Backend::playbackChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void Backend::viewChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void Backend::previewChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void Backend::previewStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void Backend::seekRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void Backend::activeTextsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void Backend::snapGuideChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void Backend::exportStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void Backend::exportFinished(bool _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1, _t2);
}

// SIGNAL 14
void Backend::voiceoverStateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void Backend::statusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}

// SIGNAL 16
void Backend::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 16, nullptr, _t1);
}
QT_WARNING_POP
