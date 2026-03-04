//go:build !windows
// +build !windows

package main

import (
	"os"
	"strings"
	"syscall"
	"unsafe"

	pb "github.com/perclft/QubitEngine/api/generated"
)

func readSharedMemory(descriptor string, numQubits int32) ([]*pb.StateResponse_ComplexNumber, error) {
	path := "/dev/shm/" + strings.TrimPrefix(descriptor, "/")
	f, err := os.OpenFile(path, os.O_RDONLY, 0666)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	numElements := 1 << numQubits
	sizeBytes := numElements * 16

	data, err := syscall.Mmap(int(f.Fd()), 0, sizeBytes, syscall.PROT_READ, syscall.MAP_SHARED)
	if err != nil {
		return nil, err
	}
	defer syscall.Munmap(data)

	result := make([]*pb.StateResponse_ComplexNumber, numElements)
	floats := (*[1 << 30]float64)(unsafe.Pointer(&data[0]))[: numElements*2 : numElements*2]
	for i := 0; i < numElements; i++ {
		result[i] = &pb.StateResponse_ComplexNumber{
			Real: floats[i*2],
			Imag: floats[i*2+1],
		}
	}
	return result, nil
}
