#-------------------------------------------------
#
# Project created by QtCreator 2014-09-12T14:14:50
#
#-------------------------------------------------

QT       += core network

QT       -= gui

TARGET = APNSd
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += src/main.cpp \
    src/capnsd.cpp

HEADERS += \
    src/capnsd.h \
    src/shared.h
