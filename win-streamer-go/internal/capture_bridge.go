//go:build windows
// +build windows

package internal

/*
#cgo CFLAGS: -I${SRCDIR}/../../win-capture-native/include
#cgo LDFLAGS: -L${SRCDIR}/../../win-capture-native/build/bin/Release -lwin-capture-native
#include <Windows.h>
#include "capture.h"
*/
import "C"

import (
	"errors"
	"unsafe"
)

// EncodedFrame represents an encoded H.264 frame
type EncodedFrame struct {
	Data     []byte
	Size     uint32
	Pts      uint64
	Keyframe bool
}

// Capturer wraps the native capturer handle
type Capturer struct {
	handle unsafe.Pointer
}

// NewCapturerX264 creates a new capturer with x264 encoder (configPath can be empty to use defaults)
func NewCapturerX264(configPath string) (*Capturer, error) {
	var cConfigPath *C.char
	if configPath != "" {
		cConfigPath = C.CString(configPath)
		defer C.free(unsafe.Pointer(cConfigPath))
	}

	var handle unsafe.Pointer
	errCode := C.capture_native_create_capturer_x264(cConfigPath, &handle)
	if CaptureError(errCode) != CaptureErrorOK {
		return nil, errors.New("failed to create capturer: " + CaptureError(errCode).Error())
	}

	return &Capturer{handle: handle}, nil
}

// Start starts the capture process
func (c *Capturer) Start() error {
	errCode := C.capture_native_start_capturer(c.handle)
	if CaptureError(errCode) != CaptureErrorOK {
		return errors.New("failed to start capturer: " + CaptureError(errCode).Error())
	}
	return nil
}

// Stop stops the capture process
func (c *Capturer) Stop() {
	C.capture_native_stop_capturer(c.handle)
}

// Close releases all resources
func (c *Capturer) Close() {
	C.capture_native_delete_capturer(c.handle)
}

// PopFrame retrieves the front encoded frame and removes it from the queue
func (c *Capturer) GetFrame() (*EncodedFrame, error) {
	frame := C.capture_native_get_frame(c.handle)
	if frame.data == nil {
		return nil, errors.New("no frame available")
	}

	data := C.GoBytes(unsafe.Pointer(frame.data), C.int(frame.size))
	C.capture_native_pop_frame(c.handle) // remove from the queue

	return &EncodedFrame{
		Data:     data,
		Size:     uint32(frame.size),
		Pts:      uint64(frame.pts),
		Keyframe: frame.keyframe != 0,
	}, nil
}

// GetLastCapturerError returns the last capturer worker error
func (c *Capturer) GetLastCapturerError() CaptureError {
	errCode := C.capture_native_get_last_capturer_error(c.handle)
	return CaptureError(errCode)
}

// GetLastEncoderError returns the last encoder worker error
func (c *Capturer) GetLastEncoderError() CaptureError {
	errCode := C.capture_native_get_last_encoder_error(c.handle)
	return CaptureError(errCode)
}
