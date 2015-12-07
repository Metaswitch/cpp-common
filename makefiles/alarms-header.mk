TARGETS := alarm_header

alarm_header_SOURCES := alarm_header.cpp \
                        json_alarms.cpp \
                        alarmdefinition.cpp

alarm_header_CPPFLAGS := -I${MODULE_DIR}/cpp-common/include \
                         -I${MODULE_DIR}/rapidjson/include

# Add cpp-common/src as VPATH so build will find modules there.
VPATH = ${MODULE_DIR}/cpp-common/src

include ${ROOT}/build-infra/cpp.mk
