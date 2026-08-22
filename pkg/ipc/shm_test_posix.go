//go:build !windows

package ipc

import (
	"fmt"

	"golang.org/x/sys/unix"
)

func writeTestSegment(segmentName string, sizeBytes uint64, data []complex128) (func(), error) {
	name := segmentName
	if len(name) > 0 && name[0] != '/' {
		name = "/" + name
	}
	fd, err := shmOpen(name, unix.O_CREAT|unix.O_RDWR|unix.O_TRUNC, 0600)
	if err != nil {
		return nil, fmt.Errorf("shmOpen failed: %w", err)
	}
	if err := unix.Ftruncate(fd, int64(sizeBytes)); err != nil {
		unix.Close(fd)
		shmUnlink(name)
		return nil, fmt.Errorf("Ftruncate failed: %w", err)
	}
	dataBytes, err := unix.Mmap(fd, 0, int(sizeBytes), unix.PROT_READ|unix.PROT_WRITE, unix.MAP_SHARED)
	if err != nil {
		unix.Close(fd)
		shmUnlink(name)
		return nil, fmt.Errorf("Mmap failed: %w", err)
	}
	copy(dataBytes, unsafeSliceToBytes(data))
	cleanup := func() {
		unix.Munmap(dataBytes)
		unix.Close(fd)
		shmUnlink(name)
	}
	return cleanup, nil
}
