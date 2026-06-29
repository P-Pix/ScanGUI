CXX ?= g++
PKG_CONFIG ?= pkg-config

CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtkmm-3.0 libcurl 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtkmm-3.0 libcurl 2>/dev/null)
PQ_CFLAGS := $(shell $(PKG_CONFIG) --cflags libpq 2>/dev/null)
PQ_LIBS := $(shell $(PKG_CONFIG) --libs libpq 2>/dev/null)

TARGET := bin/ScanGUI
SERVER_TARGET := bin/ScanGUIServer
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
DEP_DIR := $(BUILD_DIR)/dep
SERVER_OBJ_DIR := $(BUILD_DIR)/server-obj
SERVER_DEP_DIR := $(BUILD_DIR)/server-dep

SRCS := $(shell find src -name '*.cpp' | sort)
OBJS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(patsubst src/%.cpp,$(DEP_DIR)/%.d,$(SRCS))

SERVER_SRCS := $(shell find server_src -name '*.cpp' | sort) src/JsonScanRepository.cpp
SERVER_OBJS := $(patsubst server_src/%.cpp,$(SERVER_OBJ_DIR)/%.o,$(filter server_src/%.cpp,$(SERVER_SRCS))) $(SERVER_OBJ_DIR)/shared/JsonScanRepository.o
SERVER_DEPS := $(patsubst server_src/%.cpp,$(SERVER_DEP_DIR)/%.d,$(filter server_src/%.cpp,$(SERVER_SRCS))) $(SERVER_DEP_DIR)/shared/JsonScanRepository.d

all: init $(TARGET)

init:
	@mkdir -p bin scan cache/api $(OBJ_DIR) $(DEP_DIR) $(SERVER_OBJ_DIR) $(SERVER_DEP_DIR)

$(TARGET): $(OBJS) | init
	$(CXX) -o $@ $^ $(GTK_LIBS)

$(OBJ_DIR)/%.o: src/%.cpp | init
	@mkdir -p $(dir $@) $(dir $(patsubst $(OBJ_DIR)/%.o,$(DEP_DIR)/%.d,$@))
	$(CXX) $(CPPFLAGS) $(GTK_CFLAGS) $(CXXFLAGS) -MMD -MP -MF $(patsubst $(OBJ_DIR)/%.o,$(DEP_DIR)/%.d,$@) -c $< -o $@

server: init $(SERVER_TARGET)

$(SERVER_TARGET): $(SERVER_OBJS) | init
	$(CXX) -o $@ $^ $(PQ_LIBS) -pthread

$(SERVER_OBJ_DIR)/%.o: server_src/%.cpp | init
	@mkdir -p $(dir $@) $(dir $(patsubst $(SERVER_OBJ_DIR)/%.o,$(SERVER_DEP_DIR)/%.d,$@))
	$(CXX) $(CPPFLAGS) $(PQ_CFLAGS) $(CXXFLAGS) -MMD -MP -MF $(patsubst $(SERVER_OBJ_DIR)/%.o,$(SERVER_DEP_DIR)/%.d,$@) -c $< -o $@

$(SERVER_OBJ_DIR)/shared/JsonScanRepository.o: src/JsonScanRepository.cpp | init
	@mkdir -p $(dir $@) $(SERVER_DEP_DIR)/shared
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $(SERVER_DEP_DIR)/shared/JsonScanRepository.d -c $< -o $@

run: all
	./$(TARGET)

run-server: server
	./$(SERVER_TARGET)

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET) $(SERVER_TARGET)

mrproper: clean
	rm -rf bin scan cache

-include $(DEPS)
-include $(SERVER_DEPS)

.PHONY: all init run server run-server clean mrproper
