TARGETS := alarm_header

alarm_header_SOURCES := alarm_header.cpp \
                        json_alarms.cpp \
                        alarmdefinition.cpp

alarm_header_CPPFLAGS := -I../modules/cpp-common/include \
                         -I../modules/rapidjson/include

# Add cpp-common/src as VPATH so build will find modules there.
VPATH = ../modules/cpp-common/src

include ${ROOT}/build-infra/cpp.mk
