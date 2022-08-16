include(../plugins.pri)

QT += serialport

TARGET = $$qtLibraryTarget(nymea_integrationplugindelta)

SOURCES += \
    integrationplugindelta.cpp \
    crc16.cpp


HEADERS += \
    integrationplugindelta.h \
    crc16.h
