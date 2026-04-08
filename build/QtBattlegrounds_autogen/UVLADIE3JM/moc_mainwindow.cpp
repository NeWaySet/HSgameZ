/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/mainwindow.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.4.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
namespace {
struct qt_meta_stringdata_MainWindow_t {
    uint offsetsAndSizes[34];
    char stringdata0[11];
    char stringdata1[12];
    char stringdata2[1];
    char stringdata3[17];
    char stringdata4[14];
    char stringdata5[16];
    char stringdata6[17];
    char stringdata7[19];
    char stringdata8[12];
    char stringdata9[12];
    char stringdata10[15];
    char stringdata11[17];
    char stringdata12[7];
    char stringdata13[10];
    char stringdata14[8];
    char stringdata15[9];
    char stringdata16[18];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_MainWindow_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
        QT_MOC_LITERAL(0, 10),  // "MainWindow"
        QT_MOC_LITERAL(11, 11),  // "hostSession"
        QT_MOC_LITERAL(23, 0),  // ""
        QT_MOC_LITERAL(24, 16),  // "connectToSession"
        QT_MOC_LITERAL(41, 13),  // "refreshTavern"
        QT_MOC_LITERAL(55, 15),  // "buySelectedCard"
        QT_MOC_LITERAL(71, 16),  // "playSelectedCard"
        QT_MOC_LITERAL(88, 18),  // "sellSelectedMinion"
        QT_MOC_LITERAL(107, 11),  // "toggleReady"
        QT_MOC_LITERAL(119, 11),  // "onConnected"
        QT_MOC_LITERAL(131, 14),  // "onDisconnected"
        QT_MOC_LITERAL(146, 16),  // "onNetworkMessage"
        QT_MOC_LITERAL(163, 6),  // "object"
        QT_MOC_LITERAL(170, 9),  // "showError"
        QT_MOC_LITERAL(180, 7),  // "message"
        QT_MOC_LITERAL(188, 8),  // "showInfo"
        QT_MOC_LITERAL(197, 17)   // "updateTransportUi"
    },
    "MainWindow",
    "hostSession",
    "",
    "connectToSession",
    "refreshTavern",
    "buySelectedCard",
    "playSelectedCard",
    "sellSelectedMinion",
    "toggleReady",
    "onConnected",
    "onDisconnected",
    "onNetworkMessage",
    "object",
    "showError",
    "message",
    "showInfo",
    "updateTransportUi"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_MainWindow[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   92,    2, 0x08,    1 /* Private */,
       3,    0,   93,    2, 0x08,    2 /* Private */,
       4,    0,   94,    2, 0x08,    3 /* Private */,
       5,    0,   95,    2, 0x08,    4 /* Private */,
       6,    0,   96,    2, 0x08,    5 /* Private */,
       7,    0,   97,    2, 0x08,    6 /* Private */,
       8,    0,   98,    2, 0x08,    7 /* Private */,
       9,    0,   99,    2, 0x08,    8 /* Private */,
      10,    0,  100,    2, 0x08,    9 /* Private */,
      11,    1,  101,    2, 0x08,   10 /* Private */,
      13,    1,  104,    2, 0x08,   12 /* Private */,
      15,    1,  107,    2, 0x08,   14 /* Private */,
      16,    0,  110,    2, 0x08,   16 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QJsonObject,   12,
    QMetaType::Void, QMetaType::QString,   14,
    QMetaType::Void, QMetaType::QString,   14,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.offsetsAndSizes,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_MainWindow_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'hostSession'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'connectToSession'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'refreshTavern'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'buySelectedCard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'playSelectedCard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'sellSelectedMinion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'toggleReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onConnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDisconnected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNetworkMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QJsonObject &, std::false_type>,
        // method 'showError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'showInfo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'updateTransportUi'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->hostSession(); break;
        case 1: _t->connectToSession(); break;
        case 2: _t->refreshTavern(); break;
        case 3: _t->buySelectedCard(); break;
        case 4: _t->playSelectedCard(); break;
        case 5: _t->sellSelectedMinion(); break;
        case 6: _t->toggleReady(); break;
        case 7: _t->onConnected(); break;
        case 8: _t->onDisconnected(); break;
        case 9: _t->onNetworkMessage((*reinterpret_cast< std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 10: _t->showError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->showInfo((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->updateTransportUi(); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
