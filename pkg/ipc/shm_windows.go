//go:build windows

package ipc

import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/windows"
)

var (
	modkernel32          = windows.NewLazySystemDLL("kernel32.dll")
	procOpenFileMappingW = modkernel32.NewProc("OpenFileMappingW")
)

// ProbeSharedMemoryAccess tests active write/mapping capability in Windows named shared memory namespace.
func ProbeSharedMemoryAccess() bool {
	probeName, err := windows.UTF16PtrFromString("Local\\qe_shm_probe_access")
	if err != nil {
		return false
	}

	hMapFile, err := windows.CreateFileMapping(
		windows.InvalidHandle,
		nil,
		windows.PAGE_READWRITE,
		0,
		4096,
		probeName,
	)
	if err != nil {
		return false
	}
	defer windows.CloseHandle(hMapFile)

	pBuf, err := windows.MapViewOfFile(hMapFile, windows.FILE_MAP_WRITE, 0, 0, 4096)
	if err != nil {
		return false
	}
	defer windows.UnmapViewOfFile(pBuf)

	slice := unsafe.Slice((*byte)(unsafe.Pointer(pBuf)), 4096)
	slice[0] = 0x42
	return slice[0] == 0x42
}

func openFileMapping(desiredAccess uint32, inheritHandle bool, name *uint16) (windows.Handle, error) {
	var inherit uintptr
	if inheritHandle {
		inherit = 1
	}
	r1, _, err := procOpenFileMappingW.Call(uintptr(desiredAccess), inherit, uintptr(unsafe.Pointer(name)))
	if r1 == 0 {
		return 0, err
	}
	return windows.Handle(r1), nil
}

func readSegmentBytes(segmentName string, sizeBytes uint64, numQubits uint32) ([]complex128, error) {
	uName, err := windows.UTF16PtrFromString(segmentName)
	if err != nil {
		return nil, fmt.Errorf("invalid segment name string: %w", err)
	}

	hMapFile, err := openFileMapping(windows.FILE_MAP_READ, false, uName)
	if err != nil {
		return nil, fmt.Errorf("OpenFileMapping failed for %s: %w", segmentName, err)
	}
	defer windows.CloseHandle(hMapFile)

	pBuf, err := windows.MapViewOfFile(hMapFile, windows.FILE_MAP_READ, 0, 0, uintptr(sizeBytes))
	if err != nil {
		return nil, fmt.Errorf("MapViewOfFile failed: %w", err)
	}
	defer windows.UnmapViewOfFile(pBuf)

	elementCount := 1 << numQubits
	rawBytes := unsafe.Slice((*byte)(unsafe.Pointer(pBuf)), int(sizeBytes))

	if len(rawBytes) < elementCount*16 {
		return nil, fmt.Errorf("SHM segment size mismatch: expected at least %d bytes, got %d", elementCount*16, len(rawBytes))
	}

	result := make([]complex128, elementCount)
	dstBytes := unsafe.Slice((*byte)(unsafe.Pointer(&result[0])), elementCount*16)
	copy(dstBytes, rawBytes[:elementCount*16])

	return result, nil
}
