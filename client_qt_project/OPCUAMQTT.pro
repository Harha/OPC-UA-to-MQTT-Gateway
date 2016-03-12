#-------------------------------------------------
#
# Project created by QtCreator 2015-12-18T23:23:40
#
#-------------------------------------------------

QT     += core gui
CONFIG += console

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# C++11
QMAKE_CXXFLAGS += -std=c++11 -Werror=return-type

# Optimization for release
QMAKE_CXXFLAGS_RELEASE += -Ofast

# Includes
INCLUDEPATH += C:/boost/boost_mingw # boost
INCLUDEPATH += ./include # freeopcua, mqtt

# Libraries
LIBS += -LC:/boost/boost_mingw/stage/lib \ # boost
        -lws2_32 \ # winsock
        -lboost_system-mgw49-mt-d-1_60 # boost system

LIBS += -L"$$PWD/lib/opcua" \ # freeopcua
        -lopcuacore \
        -lopcuaprotocol \
        -lopcuaclient

LIBS += -L"$$PWD/lib/mqtt" \ # mqtt
        -lmosquitto \
        -lmosquittopp

TARGET = OPCUAMQTT
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    aboutdialog.cpp \
    opcuaclient.cpp \
    coupleritem.cpp \
    mqttclient.cpp \
    opcuaepwrapper.cpp

HEADERS  += mainwindow.h \
    aboutdialog.h \
    config.h \
    opcuaclient.h \
    coupleritem.h \
    mqttclient.h \
    clientstates.h \
    opcuaepwrapper.h

FORMS    += mainwindow.ui \
    aboutdialog.ui

RESOURCES += \
    resources.qrc
