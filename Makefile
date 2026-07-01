CXX = $(shell command -v clang++ 2>/dev/null || command -v g++)
PKG_CONFIG ?= pkg-config
LDFLAGS ?= $(shell command -v ld.lld >/dev/null 2>&1 && echo -fuse-ld=lld)

CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O0 -g0
TEST_CXXFLAGS := $(filter-out -O%,$(CXXFLAGS)) -O0 -g0
CPPFLAGS += -Iinclude
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtkmm-3.0 libcurl 2>/dev/null)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtkmm-3.0 libcurl 2>/dev/null)
PQ_CFLAGS := $(shell $(PKG_CONFIG) --cflags libpq 2>/dev/null)
PQ_LIBS := $(shell $(PKG_CONFIG) --libs libpq 2>/dev/null)

TARGET := bin/ScanGUI
SERVER_TARGET := bin/ScanGUIServer
TEST_TARGET := bin/ScanGUITests
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
DEP_DIR := $(BUILD_DIR)/dep
SERVER_OBJ_DIR := $(BUILD_DIR)/server-obj
SERVER_DEP_DIR := $(BUILD_DIR)/server-dep
TEST_OBJ_DIR := $(BUILD_DIR)/test-obj
TEST_DEP_DIR := $(BUILD_DIR)/test-dep

SRCS := $(shell find src -name '*.cpp' | sort)
OBJS := $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
DEPS := $(patsubst src/%.cpp,$(DEP_DIR)/%.d,$(SRCS))

SERVER_SRCS := $(shell find server_src -name '*.cpp' | sort) src/JsonScanRepository.cpp
SERVER_OBJS := $(patsubst server_src/%.cpp,$(SERVER_OBJ_DIR)/%.o,$(filter server_src/%.cpp,$(SERVER_SRCS))) $(SERVER_OBJ_DIR)/shared/JsonScanRepository.o
SERVER_DEPS := $(patsubst server_src/%.cpp,$(SERVER_DEP_DIR)/%.d,$(filter server_src/%.cpp,$(SERVER_SRCS))) $(SERVER_DEP_DIR)/shared/JsonScanRepository.d

TEST_SRCS := tests/run_unit_tests.cpp src/ScanSession.cpp src/JsonScanRepository.cpp src/application/FileSystemScanDataSource.cpp src/application/OfflineLibrarySync.cpp src/application/DownloadQueue.cpp server_src/HttpTypes.cpp
TEST_OBJS := $(patsubst %.cpp,$(TEST_OBJ_DIR)/%.o,$(TEST_SRCS))
TEST_DEPS := $(patsubst %.cpp,$(TEST_DEP_DIR)/%.d,$(TEST_SRCS))

all: init $(TARGET)

init:
	@mkdir -p bin scan cache/api $(OBJ_DIR) $(DEP_DIR) $(SERVER_OBJ_DIR) $(SERVER_DEP_DIR) $(SERVER_OBJ_DIR)/shared $(SERVER_DEP_DIR)/shared $(TEST_OBJ_DIR) $(TEST_DEP_DIR)

$(TARGET): $(OBJS) | init
	$(CXX) $(LDFLAGS) -o $@ $^ $(GTK_LIBS) -pthread

$(OBJ_DIR)/%.o: src/%.cpp | init
	@mkdir -p $(dir $@) $(dir $(patsubst $(OBJ_DIR)/%.o,$(DEP_DIR)/%.d,$@))
	@tmp_o=$$(mktemp /tmp/scangui_obj_XXXXXX.o); tmp_d=$$(mktemp /tmp/scangui_dep_XXXXXX.d); \
	dep_path="$(patsubst $(OBJ_DIR)/%.o,$(DEP_DIR)/%.d,$@)"; \
	$(CXX) $(CPPFLAGS) $(GTK_CFLAGS) $(CXXFLAGS) -MMD -MP -MT "$@" -MF $$tmp_d -c $< -o $$tmp_o; status=$$?; \
	if [ $$status -eq 0 ]; then cp $$tmp_o "$@" && cp $$tmp_d "$$dep_path"; status=$$?; fi; \
	rm -f $$tmp_o $$tmp_d; exit $$status

server: init $(SERVER_TARGET)

$(SERVER_TARGET): $(SERVER_OBJS) | init
	$(CXX) $(LDFLAGS) -o $@ $^ $(PQ_LIBS) -pthread

$(SERVER_OBJ_DIR)/%.o: server_src/%.cpp | init
	@mkdir -p $(dir $@) $(dir $(patsubst $(SERVER_OBJ_DIR)/%.o,$(SERVER_DEP_DIR)/%.d,$@))
	@tmp_o=$$(mktemp /tmp/scangui_obj_XXXXXX.o); tmp_d=$$(mktemp /tmp/scangui_dep_XXXXXX.d); \
	dep_path="$(patsubst $(SERVER_OBJ_DIR)/%.o,$(SERVER_DEP_DIR)/%.d,$@)"; \
	$(CXX) $(CPPFLAGS) $(PQ_CFLAGS) $(CXXFLAGS) -MMD -MP -MT "$@" -MF $$tmp_d -c $< -o $$tmp_o; status=$$?; \
	if [ $$status -eq 0 ]; then cp $$tmp_o "$@" && cp $$tmp_d "$$dep_path"; status=$$?; fi; \
	rm -f $$tmp_o $$tmp_d; exit $$status

$(SERVER_OBJ_DIR)/shared/JsonScanRepository.o: src/JsonScanRepository.cpp | init
	@mkdir -p $(dir $@) $(SERVER_DEP_DIR)/shared
	@tmp_o=$$(mktemp /tmp/scangui_obj_XXXXXX.o); tmp_d=$$(mktemp /tmp/scangui_dep_XXXXXX.d); \
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MT "$@" -MF $$tmp_d -c $< -o $$tmp_o; status=$$?; \
	if [ $$status -eq 0 ]; then cp $$tmp_o "$@" && cp $$tmp_d "$(SERVER_DEP_DIR)/shared/JsonScanRepository.d"; status=$$?; fi; \
	rm -f $$tmp_o $$tmp_d; exit $$status

run: all
	./$(TARGET)

run-server: server
	./$(SERVER_TARGET)

tests: init $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS) | init
	$(CXX) $(LDFLAGS) -o $@ $^ -pthread

$(TEST_OBJ_DIR)/%.o: %.cpp | init
	@mkdir -p $(dir $@) $(dir $(patsubst $(TEST_OBJ_DIR)/%.o,$(TEST_DEP_DIR)/%.d,$@))
	@tmp_o=$$(mktemp /tmp/scangui_obj_XXXXXX.o); tmp_d=$$(mktemp /tmp/scangui_dep_XXXXXX.d); \
	dep_path="$(patsubst $(TEST_OBJ_DIR)/%.o,$(TEST_DEP_DIR)/%.d,$@)"; \
	$(CXX) $(CPPFLAGS) $(TEST_CXXFLAGS) -MMD -MP -MT "$@" -MF $$tmp_d -c $< -o $$tmp_o; status=$$?; \
	if [ $$status -eq 0 ]; then cp $$tmp_o "$@" && cp $$tmp_d "$$dep_path"; status=$$?; fi; \
	rm -f $$tmp_o $$tmp_d; exit $$status

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET) $(SERVER_TARGET) $(TEST_TARGET)

mrproper: clean
	rm -rf bin scan cache

-include $(DEPS)
-include $(SERVER_DEPS)
-include $(TEST_DEPS)

.PHONY: all init run server run-server tests clean mrproper
.NOTPARALLEL:
.SECONDARY: $(OBJS) $(SERVER_OBJS) $(TEST_OBJS)

.PRECIOUS: $(OBJS) $(SERVER_OBJS) $(TEST_OBJS)
