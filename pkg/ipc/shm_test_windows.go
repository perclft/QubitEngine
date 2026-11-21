//go:build windows

package ipc

import (
	"unsafe"

	"golang.org/x/sys/windows"
)

func writeTestSegment(segmentName string, sizeBytes uint64, data []complex128) (func(), error) {
	uName, err := windows.UTF16PtrFromString(segmentName)
	if err != nil {
		return nil, err
	}
	hMapFile, err := windows.CreateFileMapping(
		windows.InvalidHandle,
		nil,
		windows.PAGE_READWRITE,
		uint32(sizeBytes>>32),
		uint32(sizeBytes&0xFFFFFFFF),
		uName,
	)
	if err != nil {
		return nil, err
	}
	pBuf, err := windows.MapViewOfFile(hMapFile, windows.FILE_MAP_WRITE, 0, 0, uintptr(sizeBytes))
	if err != nil {
		windows.CloseHandle(hMapFile)
		return nil, err
	}

	dstBytes := unsafe.Slice((*byte)(unsafe.Pointer(pBuf)), int(sizeBytes))
	srcBytes := unsafe.Slice((*byte)(unsafe.Pointer(&data[0])), len(data)*16)
	copy(dstBytes, srcBytes)

	cleanup := func() {
		windows.UnmapViewOfFile(pBuf)
		windows.CloseHandle(hMapFile)
	}
	return cleanup, nil
}
