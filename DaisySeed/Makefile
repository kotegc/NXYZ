# Project Name
TARGET = DaisySeed

# Sources
CPP_SOURCES = DaisySeed.cpp

# Library Locations
# Overridable defaults — override with:
#   make LIBDAISY_DIR=/path/to/DaisyExamples/libDaisy DAISYSP_DIR=/path/to/DaisyExamples/DaisySP
LIBDAISY_DIR ?= ../../../../../Desktop/DaisyExamples/libDaisy/
DAISYSP_DIR ?= ../../../../../Desktop/DaisyExamples/DaisySP/

# Shared UART protocol definition (../shared) — must be set before the
# core Makefile appends its own include paths to this variable.
C_INCLUDES = -I../shared

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
