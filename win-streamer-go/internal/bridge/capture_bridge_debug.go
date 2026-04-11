//go:build debug
// +build debug

package bridge

/*
#cgo CFLAGS: -I${SRCDIR}/../../../win-capture-native/include
#cgo LDFLAGS: -L${SRCDIR}/../../../win-capture-native/build/bin/Debug -lwin-capture-native
#include <Windows.h>
#include "capture.h"
*/
import "C"
