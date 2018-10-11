#-------------------------------------------------
#
# Project created by QtCreator 2018-09-25T14:41:27
#
#-------------------------------------------------

QT       += core gui  sql  network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = agvservice
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

LIBS += -lsqlite3
LIBS += -ljsoncpp
LIBS += -lboost_system
LIBS += -lboost_thread




HEADERS  += mainwindow.h \
    mapmap/mapelement.h \
    mapmap/mappoint.h \
    mapmap/mappath.h \
    mapmap/mapbackground.h \
    mapmap/onemap.h \
    util/common.h \
    mapmap/mapmanager.h \
    network/sessionbuffer.h \
    network/protocol.h \
    network/session.h \
    network/acceptor.h \
    sql/sql.h \
    sql/cppsqlite3.h \
    network/tcpacceptor.h \
    util/qyhbuffer.h \
    network/tcpsession.h \
    agv/agv.h \
    network/sessionmanager.h \
    task/taskmaker.h \
    task/taskmanager.h \
    task/agvtasknodedothing.h \
    task/agvtask.h \
    network/tcpclient.h \
    util/base64.h \
    user/msgprocess.h \
    user/userlogmanager.h \
    util/bezierarc.h \
    agv/realagv.h \
    user/usermanager.h \
    agv/virtualagv.h \
    impl/linepath.h \
    agv/rosagv.h \
    task/agvtasknode.h \
    agv/agvmanager.h \
    agv/virtualrosagv.h \
    agv/rosagvutils.h






SOURCES += main.cpp\
        mainwindow.cpp \
    mapmap/mapelement.cpp \
    mapmap/mappoint.cpp \
    mapmap/mappath.cpp \
    mapmap/mapbackground.cpp \
    mapmap/onemap.cpp \
    util/common.cpp \
    mapmap/mapmanager.cpp \
    network/sessionbuffer.cpp \
    network/session.cpp \
    network/acceptor.cpp \
    sql/sql.cpp \
    sql/cppsqlite3.cpp \
    network/tcpacceptor.cpp \
    util/qyhbuffer.cpp \
    network/tcpsession.cpp \
    network/sessionmanager.cpp \
    task/taskmaker.cpp \
    task/taskmanager.cpp \
    network/tcpclient.cpp \
    util/base64.cpp \
    user/msgprocess.cpp \
    user/userlogmanager.cpp \
    util/bezierarc.cpp \
    agv/realagv.cpp \
    user/usermanager.cpp \
    impl/linepath.cpp \
    agv/rosagv.cpp \
    agv/agv.cpp \
    agv/agvmanager.cpp \
    agv/virtualrosagv.cpp





FORMS    += mainwindow.ui


CONFIG   += console
CONFIG   -= app_bundle
CONFIG += c++11
CONFIG += c++14
