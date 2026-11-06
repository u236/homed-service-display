include(../homed-common/homed-color.pri)
include(../homed-common/homed-common.pri)
include(../homed-common/homed-endpoint.pri)
include(../homed-common/homed-parser.pri)

HEADERS += \
    controller.h \
    display.h

SOURCES += \
    controller.cpp \
    display.cpp

QMAKE_CXXFLAGS += -fno-strict-aliasing
