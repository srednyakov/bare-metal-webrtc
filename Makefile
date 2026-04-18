.PHONY: all gen build build-debug test clean

BIN_DIR := bin
BIN_DIR_RELEASE := ${BIN_DIR}/Release
BIN_DIR_DEBUG := ${BIN_DIR}/Debug

NATIVE_CAPTURE_DIR := win-capture-native
NATIVE_CAPTURE_BUILD_DIR := $(NATIVE_CAPTURE_DIR)/build

STREAMER_DIR := win-streamer-go

all: gen build

gen:
	go run codegen/errors.go \
		$(NATIVE_CAPTURE_DIR)/errors.json \
		$(NATIVE_CAPTURE_DIR)/include/errors.h \
		$(NATIVE_CAPTURE_DIR)/src/errors.cpp \
		${STREAMER_DIR}/internal/bridge/capture_error.go

build: $(BIN_DIR_RELEASE)
	cd $(NATIVE_CAPTURE_DIR) && cmake --preset clang64-release
	cmake --build $(NATIVE_CAPTURE_BUILD_DIR) --config Release
	cp $(NATIVE_CAPTURE_BUILD_DIR)/bin/Release/libwin-capture-native.dll $(BIN_DIR_RELEASE)/

	cd ${STREAMER_DIR} && go build -o ../${BIN_DIR_RELEASE}/capture-test.exe ./cmd/capture-test
	cp $(STREAMER_DIR)/configs/config.example.yaml $(BIN_DIR_RELEASE)/capture_test_config.yaml

build-debug: $(BIN_DIR_DEBUG)
	cd $(NATIVE_CAPTURE_DIR) && cmake --preset clang64-debug
	cmake --build $(NATIVE_CAPTURE_BUILD_DIR) --config Debug
	cp $(NATIVE_CAPTURE_BUILD_DIR)/bin/Debug/libwin-capture-native.dll $(BIN_DIR_DEBUG)/

	cd ${STREAMER_DIR} && go build -tags debug -o ../${BIN_DIR_DEBUG}/capture-test.exe ./cmd/capture-test
	cp $(STREAMER_DIR)/configs/config.example.yaml $(BIN_DIR_DEBUG)/capture_test_config.yaml
	
$(BIN_DIR_RELEASE):
	mkdir -p $(BIN_DIR_RELEASE)

$(BIN_DIR_DEBUG):
	mkdir -p $(BIN_DIR_DEBUG)

test:
	cd $(STREAMER_DIR) && go test ./...

clean:
	cmake -E rm -rf $(NATIVE_CAPTURE_BUILD_DIR)
	cmake -E rm -rf $(BIN_DIR)
