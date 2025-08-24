# Platform-independent Makefile for GPUDbug2 (Qt + C++14)

CXX      = g++
MOC      = moc
UIC      = uic

SRC_DIR  = src
BUILD_DIR= build
BUILD_BIN= $(BUILD_DIR)/bin
MOC_DIR  = $(BUILD_DIR)/moc
OBJ_DIR  = $(BUILD_DIR)/obj

TARGET   = $(BUILD_BIN)/GPUDbug2

# Use environment variables for Qt paths, or fallback to defaults
QT_INC ?= -I$(shell pkg-config --cflags Qt5Widgets)
QT_LIB ?= $(shell pkg-config --libs Qt5Widgets)

CXXFLAGS = -std=c++14 -Wall -O2 $(QT_INC) -I$(BUILD_DIR)
LDFLAGS  = $(QT_LIB)

VERSION_MAJOR_MINOR := $(shell cat VERSION)
VERSION := $(VERSION_MAJOR_MINOR).$(shell git rev-list --count HEAD 2>/dev/null || echo 0)
BUILD_DATE := $(shell date "+%Y-%m-%d %H:%M:%S")

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
HDRS = $(wildcard $(SRC_DIR)/*.h)

MOC_HDRS = $(filter %mainwindow.h %debugger.h,$(HDRS))
MOC_SRCS = $(patsubst $(SRC_DIR)/%.h,$(MOC_DIR)/moc_%.cpp,$(MOC_HDRS))

OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o) $(MOC_SRCS:$(MOC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

all: $(BUILD_DIR) $(MOC_DIR) $(BUILD_BIN) $(OBJ_DIR) $(BUILD_DIR)/version.h $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/version.h: version.h.in VERSION
	sed -e 's/@APP_VERSION@/$(VERSION)/' -e 's/@APP_BUILD_DATE@/$(BUILD_DATE)/' $< > $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(BUILD_DIR)/version.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(MOC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(MOC_DIR)/moc_%.cpp: $(SRC_DIR)/%.h
	$(MOC) $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BUILD_BIN):
	mkdir -p $(BUILD_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(MOC_DIR):
	mkdir -p $(MOC_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean