# Utility function for the alarm header creation. The list of files here
# should be kept in sync with the main alarm-header makefile
${BUILD_DIR}/bin/alarm_header : ../modules/cpp-common/src/json_alarms.cpp \
                                ../modules/cpp-common/src/alarm_header.cpp \
                                ../modules/cpp-common/src/alarmdefinition.cpp \
                                ../modules/cpp-common/makefiles/alarms-header.mk
	ROOT=${ROOT} ${MAKE} -f ../modules/cpp-common/makefiles/alarms-header.mk
