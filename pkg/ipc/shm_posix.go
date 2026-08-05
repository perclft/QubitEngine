//go:build !windows

package ipc

import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/unix"
)

// ProbeSharedMemoryAccess tests active write/mmap/stat capability in /dev/shm.
func ProbeSharedMemoryAccess() bool {
	probeName := "/.qe_shm_probe_access"
	fd, err := unix.ShmOpen(probeName, unix.O_CREAT|unix.O_RDWR, 0600)
	if err != nil {
		return false
	}
	defer unix.ShmUnlink(probeName)
	defer unix.Close(fd)

	if err := unix.Ftruncate(fd, 4096); err != nil {
		return false
	}

	data, err := unix.Mmap(fd, 0, 4096, unix.PROT_READ|unix.PROT_WRITE, unix.MAP_SHARED)
	if err != nil {
		return false
	}
	defer unix.Munmap(data)

	data[0] = 0x42
	return data[0] == 0x42
}

func readSegmentBytes(segmentName string, sizeBytes uint64, numQubits uint32) ([]complex128, error) {
	fd, err := unix.ShmOpen(segmentName, unix.O_RDWR, 0600)
	if err != nil {
		if len(segmentName) > 0 && segmentName[0] == '/' {
			fd, err = unix.ShmOpen(segmentName[1:], unix.O_RDWR, 0600)
		}
		if err != nil {
			return nil, fmt.Errorf("ShmOpen failed for %s: %w", segmentName, err)
		}
	}
	defer unix.Close(fd)

	b, err := unix.Mmap(fd, 0, int(sizeBytes), unix.PROT_READ, unix.MAP_SHARED)
	if err != nil {
		return nil, fmt.Errorf("Mmap failed: %w", err)
	}
	defer unix.Munmap(b)

	elementCount := 1 << numQubits
	if len(b) < elementCount*16 {
		return nil, fmt.Errorf("SHM segment size mismatch: expected at least %d bytes, got %d", elementCount*16, len(b))
	}

	result := make([]complex128, elementCount)
	copy(unsafeSliceToBytes(result), b[:elementCount*16])
	return result, nil
}

func unsafeSliceToBytes(slice []complex128) []byte {
	if len(slice) == 0 {
		return nil
	}
	return unsafe.Slice((*byte)(unsafe.Pointer(&slice[0])), len(slice)*16)
}
