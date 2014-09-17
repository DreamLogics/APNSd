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


SOURCES += main.cpp \
    capnsd.cpp

HEADERS += \
    capnsd.h \
    shared.h
