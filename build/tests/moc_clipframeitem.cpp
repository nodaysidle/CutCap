/****************************************************************************
** Meta object code from reading C++ file 'clipframeitem.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/clipframeitem.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'clipframeitem.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13ClipFrameItemE_t {};
} // unnamed namespace

template <> constexpr inline auto ClipFrameItem::qt_create_metaobjectdata<qt_meta_tag_ZN13ClipFrameItemE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ClipFrameItem",
        "imageSourceChanged",
        "",
        "lookChanged",
        "hasFrameChanged",
        "videoSink",
        "QVideoSink*",
        "imageSource",
        "brightness",
        "contrast",
        "saturation",
        "blur",
        "referenceHeight",
        "vignette",
        "chromaKey",
        "keyColor",
        "QColor",
        "similarity",
        "blend",
        "hasFrame"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'imageSourceChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'lookChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'hasFrameChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'videoSink'
        QtMocHelpers::PropertyData<QVideoSink*>(5, 0x80000000 | 6, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'imageSource'
        QtMocHelpers::PropertyData<QString>(7, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 0),
        // property 'brightness'
        QtMocHelpers::PropertyData<double>(8, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'contrast'
        QtMocHelpers::PropertyData<double>(9, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'saturation'
        QtMocHelpers::PropertyData<double>(10, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'blur'
        QtMocHelpers::PropertyData<double>(11, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'referenceHeight'
        QtMocHelpers::PropertyData<double>(12, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'vignette'
        QtMocHelpers::PropertyData<bool>(13, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'chromaKey'
        QtMocHelpers::PropertyData<bool>(14, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'keyColor'
        QtMocHelpers::PropertyData<QColor>(15, 0x80000000 | 16, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag | QMC::StdCppSet, 1),
        // property 'similarity'
        QtMocHelpers::PropertyData<double>(17, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'blend'
        QtMocHelpers::PropertyData<double>(18, QMetaType::Double, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'hasFrame'
        QtMocHelpers::PropertyData<bool>(19, QMetaType::Bool, QMC::DefaultPropertyFlags, 2),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ClipFrameItem, qt_meta_tag_ZN13ClipFrameItemE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ClipFrameItem::staticMetaObject = { {
    QMetaObject::SuperData::link<QQuickItem::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ClipFrameItemE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ClipFrameItemE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13ClipFrameItemE_t>.metaTypes,
    nullptr
} };

void ClipFrameItem::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ClipFrameItem *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->imageSourceChanged(); break;
        case 1: _t->lookChanged(); break;
        case 2: _t->hasFrameChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ClipFrameItem::*)()>(_a, &ClipFrameItem::imageSourceChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ClipFrameItem::*)()>(_a, &ClipFrameItem::lookChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ClipFrameItem::*)()>(_a, &ClipFrameItem::hasFrameChanged, 2))
            return;
    }
    if (_c == QMetaObject::RegisterPropertyMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 0:
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QVideoSink* >(); break;
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVideoSink**>(_v) = _t->videoSink(); break;
        case 1: *reinterpret_cast<QString*>(_v) = _t->imageSource(); break;
        case 2: *reinterpret_cast<double*>(_v) = _t->m_brightness; break;
        case 3: *reinterpret_cast<double*>(_v) = _t->m_contrast; break;
        case 4: *reinterpret_cast<double*>(_v) = _t->m_saturation; break;
        case 5: *reinterpret_cast<double*>(_v) = _t->m_blur; break;
        case 6: *reinterpret_cast<double*>(_v) = _t->m_referenceHeight; break;
        case 7: *reinterpret_cast<bool*>(_v) = _t->m_vignette; break;
        case 8: *reinterpret_cast<bool*>(_v) = _t->m_chromaKey; break;
        case 9: *reinterpret_cast<QColor*>(_v) = _t->m_keyColor; break;
        case 10: *reinterpret_cast<double*>(_v) = _t->m_similarity; break;
        case 11: *reinterpret_cast<double*>(_v) = _t->m_blend; break;
        case 12: *reinterpret_cast<bool*>(_v) = _t->hasFrame(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 1: _t->setImageSource(*reinterpret_cast<QString*>(_v)); break;
        case 2: _t->setBrightness(*reinterpret_cast<double*>(_v)); break;
        case 3: _t->setContrast(*reinterpret_cast<double*>(_v)); break;
        case 4: _t->setSaturation(*reinterpret_cast<double*>(_v)); break;
        case 5: _t->setBlur(*reinterpret_cast<double*>(_v)); break;
        case 6: _t->setReferenceHeight(*reinterpret_cast<double*>(_v)); break;
        case 7: _t->setVignette(*reinterpret_cast<bool*>(_v)); break;
        case 8: _t->setChromaKey(*reinterpret_cast<bool*>(_v)); break;
        case 9: _t->setKeyColor(*reinterpret_cast<QColor*>(_v)); break;
        case 10: _t->setSimilarity(*reinterpret_cast<double*>(_v)); break;
        case 11: _t->setBlend(*reinterpret_cast<double*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *ClipFrameItem::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ClipFrameItem::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ClipFrameItemE_t>.strings))
        return static_cast<void*>(this);
    return QQuickItem::qt_metacast(_clname);
}

int ClipFrameItem::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QQuickItem::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    }
    return _id;
}

// SIGNAL 0
void ClipFrameItem::imageSourceChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ClipFrameItem::lookChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void ClipFrameItem::hasFrameChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
