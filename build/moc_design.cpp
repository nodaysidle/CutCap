/****************************************************************************
** Meta object code from reading C++ file 'design.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../src/design.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'design.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6DesignE_t {};
} // unnamed namespace

template <> constexpr inline auto Design::qt_create_metaobjectdata<qt_meta_tag_ZN6DesignE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Design",
        "changed",
        "",
        "alpha",
        "hex",
        "mix",
        "a",
        "b",
        "t",
        "s1",
        "s2",
        "s3",
        "s4",
        "s5",
        "s6",
        "s7",
        "s8",
        "tMicro",
        "tCaption",
        "tBody",
        "tBodyLg",
        "tTitle",
        "tDisplay",
        "wRegular",
        "wMedium",
        "wSemibold",
        "hCompact",
        "hSmall",
        "hStd",
        "hLarge",
        "rowH",
        "trackH",
        "topBarH",
        "gutter",
        "rXs",
        "rSm",
        "rMd",
        "rLg",
        "msFast",
        "msBase",
        "msSlow",
        "surfaceSunken",
        "surfacePanel",
        "surfaceRaised",
        "surfaceOverlay",
        "borderHairline",
        "borderStrong",
        "focusRing",
        "textPrimary",
        "textSecondary",
        "textDisabled",
        "accent",
        "onAccent",
        "success",
        "warning",
        "danger",
        "info",
        "laneV2",
        "laneV1",
        "laneText",
        "laneAudio"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'changed'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'alpha'
        QtMocHelpers::MethodData<QString(const QString &, double) const>(3, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 4 }, { QMetaType::Double, 3 },
        }}),
        // Method 'mix'
        QtMocHelpers::MethodData<QString(const QString &, const QString &, double) const>(5, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::QString, 6 }, { QMetaType::QString, 7 }, { QMetaType::Double, 8 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 's1'
        QtMocHelpers::PropertyData<int>(9, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 's2'
        QtMocHelpers::PropertyData<int>(10, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 's3'
        QtMocHelpers::PropertyData<int>(11, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 's4'
        QtMocHelpers::PropertyData<int>(12, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 's5'
        QtMocHelpers::PropertyData<int>(13, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 's6'
        QtMocHelpers::PropertyData<int>(14, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 's7'
        QtMocHelpers::PropertyData<int>(15, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 's8'
        QtMocHelpers::PropertyData<int>(16, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'tMicro'
        QtMocHelpers::PropertyData<int>(17, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'tCaption'
        QtMocHelpers::PropertyData<int>(18, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'tBody'
        QtMocHelpers::PropertyData<int>(19, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'tBodyLg'
        QtMocHelpers::PropertyData<int>(20, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'tTitle'
        QtMocHelpers::PropertyData<int>(21, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'tDisplay'
        QtMocHelpers::PropertyData<int>(22, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'wRegular'
        QtMocHelpers::PropertyData<int>(23, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'wMedium'
        QtMocHelpers::PropertyData<int>(24, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'wSemibold'
        QtMocHelpers::PropertyData<int>(25, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'hCompact'
        QtMocHelpers::PropertyData<int>(26, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'hSmall'
        QtMocHelpers::PropertyData<int>(27, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'hStd'
        QtMocHelpers::PropertyData<int>(28, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'hLarge'
        QtMocHelpers::PropertyData<int>(29, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'rowH'
        QtMocHelpers::PropertyData<int>(30, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'trackH'
        QtMocHelpers::PropertyData<int>(31, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'topBarH'
        QtMocHelpers::PropertyData<int>(32, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'gutter'
        QtMocHelpers::PropertyData<int>(33, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'rXs'
        QtMocHelpers::PropertyData<int>(34, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'rSm'
        QtMocHelpers::PropertyData<int>(35, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'rMd'
        QtMocHelpers::PropertyData<int>(36, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'rLg'
        QtMocHelpers::PropertyData<int>(37, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'msFast'
        QtMocHelpers::PropertyData<int>(38, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'msBase'
        QtMocHelpers::PropertyData<int>(39, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'msSlow'
        QtMocHelpers::PropertyData<int>(40, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'surfaceSunken'
        QtMocHelpers::PropertyData<QString>(41, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'surfacePanel'
        QtMocHelpers::PropertyData<QString>(42, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'surfaceRaised'
        QtMocHelpers::PropertyData<QString>(43, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'surfaceOverlay'
        QtMocHelpers::PropertyData<QString>(44, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'borderHairline'
        QtMocHelpers::PropertyData<QString>(45, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'borderStrong'
        QtMocHelpers::PropertyData<QString>(46, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'focusRing'
        QtMocHelpers::PropertyData<QString>(47, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'textPrimary'
        QtMocHelpers::PropertyData<QString>(48, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'textSecondary'
        QtMocHelpers::PropertyData<QString>(49, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'textDisabled'
        QtMocHelpers::PropertyData<QString>(50, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'accent'
        QtMocHelpers::PropertyData<QString>(51, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'onAccent'
        QtMocHelpers::PropertyData<QString>(52, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'success'
        QtMocHelpers::PropertyData<QString>(53, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'warning'
        QtMocHelpers::PropertyData<QString>(54, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'danger'
        QtMocHelpers::PropertyData<QString>(55, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'info'
        QtMocHelpers::PropertyData<QString>(56, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'laneV2'
        QtMocHelpers::PropertyData<QString>(57, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'laneV1'
        QtMocHelpers::PropertyData<QString>(58, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'laneText'
        QtMocHelpers::PropertyData<QString>(59, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'laneAudio'
        QtMocHelpers::PropertyData<QString>(60, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Design, qt_meta_tag_ZN6DesignE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Design::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6DesignE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6DesignE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6DesignE_t>.metaTypes,
    nullptr
} };

void Design::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Design *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->changed(); break;
        case 1: { QString _r = _t->alpha((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 2: { QString _r = _t->mix((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Design::*)()>(_a, &Design::changed, 0))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<int*>(_v) = _t->s1(); break;
        case 1: *reinterpret_cast<int*>(_v) = _t->s2(); break;
        case 2: *reinterpret_cast<int*>(_v) = _t->s3(); break;
        case 3: *reinterpret_cast<int*>(_v) = _t->s4(); break;
        case 4: *reinterpret_cast<int*>(_v) = _t->s5(); break;
        case 5: *reinterpret_cast<int*>(_v) = _t->s6(); break;
        case 6: *reinterpret_cast<int*>(_v) = _t->s7(); break;
        case 7: *reinterpret_cast<int*>(_v) = _t->s8(); break;
        case 8: *reinterpret_cast<int*>(_v) = _t->tMicro(); break;
        case 9: *reinterpret_cast<int*>(_v) = _t->tCaption(); break;
        case 10: *reinterpret_cast<int*>(_v) = _t->tBody(); break;
        case 11: *reinterpret_cast<int*>(_v) = _t->tBodyLg(); break;
        case 12: *reinterpret_cast<int*>(_v) = _t->tTitle(); break;
        case 13: *reinterpret_cast<int*>(_v) = _t->tDisplay(); break;
        case 14: *reinterpret_cast<int*>(_v) = _t->wRegular(); break;
        case 15: *reinterpret_cast<int*>(_v) = _t->wMedium(); break;
        case 16: *reinterpret_cast<int*>(_v) = _t->wSemibold(); break;
        case 17: *reinterpret_cast<int*>(_v) = _t->hCompact(); break;
        case 18: *reinterpret_cast<int*>(_v) = _t->hSmall(); break;
        case 19: *reinterpret_cast<int*>(_v) = _t->hStd(); break;
        case 20: *reinterpret_cast<int*>(_v) = _t->hLarge(); break;
        case 21: *reinterpret_cast<int*>(_v) = _t->rowH(); break;
        case 22: *reinterpret_cast<int*>(_v) = _t->trackH(); break;
        case 23: *reinterpret_cast<int*>(_v) = _t->topBarH(); break;
        case 24: *reinterpret_cast<int*>(_v) = _t->gutter(); break;
        case 25: *reinterpret_cast<int*>(_v) = _t->rXs(); break;
        case 26: *reinterpret_cast<int*>(_v) = _t->rSm(); break;
        case 27: *reinterpret_cast<int*>(_v) = _t->rMd(); break;
        case 28: *reinterpret_cast<int*>(_v) = _t->rLg(); break;
        case 29: *reinterpret_cast<int*>(_v) = _t->msFast(); break;
        case 30: *reinterpret_cast<int*>(_v) = _t->msBase(); break;
        case 31: *reinterpret_cast<int*>(_v) = _t->msSlow(); break;
        case 32: *reinterpret_cast<QString*>(_v) = _t->surfaceSunken(); break;
        case 33: *reinterpret_cast<QString*>(_v) = _t->surfacePanel(); break;
        case 34: *reinterpret_cast<QString*>(_v) = _t->surfaceRaised(); break;
        case 35: *reinterpret_cast<QString*>(_v) = _t->surfaceOverlay(); break;
        case 36: *reinterpret_cast<QString*>(_v) = _t->borderHairline(); break;
        case 37: *reinterpret_cast<QString*>(_v) = _t->borderStrong(); break;
        case 38: *reinterpret_cast<QString*>(_v) = _t->focusRing(); break;
        case 39: *reinterpret_cast<QString*>(_v) = _t->textPrimary(); break;
        case 40: *reinterpret_cast<QString*>(_v) = _t->textSecondary(); break;
        case 41: *reinterpret_cast<QString*>(_v) = _t->textDisabled(); break;
        case 42: *reinterpret_cast<QString*>(_v) = _t->accent(); break;
        case 43: *reinterpret_cast<QString*>(_v) = _t->onAccent(); break;
        case 44: *reinterpret_cast<QString*>(_v) = _t->success(); break;
        case 45: *reinterpret_cast<QString*>(_v) = _t->warning(); break;
        case 46: *reinterpret_cast<QString*>(_v) = _t->danger(); break;
        case 47: *reinterpret_cast<QString*>(_v) = _t->info(); break;
        case 48: *reinterpret_cast<QString*>(_v) = _t->laneV2(); break;
        case 49: *reinterpret_cast<QString*>(_v) = _t->laneV1(); break;
        case 50: *reinterpret_cast<QString*>(_v) = _t->laneText(); break;
        case 51: *reinterpret_cast<QString*>(_v) = _t->laneAudio(); break;
        default: break;
        }
    }
}

const QMetaObject *Design::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Design::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6DesignE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Design::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
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
        _id -= 52;
    }
    return _id;
}

// SIGNAL 0
void Design::changed()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
