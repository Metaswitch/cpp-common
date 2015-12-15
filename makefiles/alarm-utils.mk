# Utility function for the alarm header creation. The list of files here
# should be kept in sync with the main alarm-header makefile
${BUILD_DIR}/bin/alarm_header : ${MODULE_DIR}/cpp-common/src/json_alarms.cpp \
                                ${MODULE_DIR}/cpp-common/src/alarm_header.cpp \
                                ${MODULE_DIR}/cpp-common/src/alarmdefinition.cpp \
                                ${MODULE_DIR}/cpp-common/makefiles/alarms-header.mk
	ROOT=${ROOT} MODULE_DIR=${MODULE_DIR} BUILD_DIR=${BUILD_DIR} ${MAKE} -f ${MODULE_DIR}/cpp-common/makefiles/alarms-header.mk
