//go:build windows
// +build windows

package main

import (
	"fmt"
	"strings"
	"syscall"
	"unsafe"

	pb "github.com/perclft/QubitEngine/api/generated"
)

var (
	modkernel32          = syscall.NewLazyDLL("kernel32.dll")
	procOpenFileMappingA = modkernel32.NewProc("OpenFileMappingA")
	procMapViewOfFile    = modkernel32.NewProc("MapViewOfFile")
	procUnmapViewOfFile  = modkernel32.NewProc("UnmapViewOfFile")
)

const FILE_MAP_READ = 4

func readSharedMemory(descriptor string, numQubits int32) ([]*pb.StateResponse_ComplexNumber, error) {
	descName := strings.TrimPrefix(descriptor, "/")
	if strings.Contains(descName, "..") || strings.Contains(descName, "/") || strings.Contains(descName, "\\") {
		return nil, fmt.Errorf("invalid shm descriptor: path traversal detected")
	}
	descPtr, err := syscall.BytePtrFromString(descriptor)
	if err != nil {
		return nil, err
	}
	hMem, _, errStr := procOpenFileMappingA.Call(uintptr(FILE_MAP_READ), 0, uintptr(unsafe.Pointer(descPtr)))
	if hMem == 0 {
		return nil, fmt.Errorf("OpenFileMappingA failed: %v", errStr)
	}
	defer syscall.CloseHandle(syscall.Handle(hMem))

	addr, _, errStr := procMapViewOfFile.Call(hMem, uintptr(FILE_MAP_READ), 0, 0, 0)
	if addr == 0 {
		return nil, fmt.Errorf("MapViewOfFile failed: %v", errStr)
	}
	defer procUnmapViewOfFile.Call(addr)

	numElements := 1 << numQubits

	result := make([]*pb.StateResponse_ComplexNumber, numElements)
	floats := (*[1 << 30]float64)(unsafe.Pointer(addr))[: numElements*2 : numElements*2]

	for i := 0; i < numElements; i++ {
		result[i] = &pb.StateResponse_ComplexNumber{
			Real: floats[i*2],
			Imag: floats[i*2+1],
		}
	}
	return result, nil
}
