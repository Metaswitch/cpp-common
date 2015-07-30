# Alarms to JSON Makefile

all: build

ROOT := $(abspath $(shell pwd)/../)
MK_DIR := ${ROOT}/mk

TARGET := alarm_header
TARGET_TEST := alarm_header_test

TARGET_SOURCES := alarm_header.cpp \
                  json_alarms.cpp \
                  alarmdefinition.cpp

CPPFLAGS += -Wno-write-strings \
            -ggdb3 -std=c++0x

CPPFLAGS += -I${ROOT}/modules/cpp-common/include \
            -I${ROOT}/modules/rapidjson/include

# Add cpp-common/src as VPATH so build will find modules there.
VPATH = ${ROOT}/modules/cpp-common/src

include ${MK_DIR}/platform.mk
