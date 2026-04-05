.PHONY: gen

gen:
	go run codegen/errors.go \
		win-capture-native/errors.json \
		win-capture-native/include/errors.h \
		win-capture-native/src/errors.cpp \
		win-streamer-go/internal/capture_error.go
