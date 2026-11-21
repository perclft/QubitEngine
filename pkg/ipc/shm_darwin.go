//go:build darwin

package ipc

/*
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

static int c_shm_open(const char *name, int flag, int mode) {
    return shm_open(name, flag, (mode_t)mode);
}

static int c_shm_unlink(const char *name) {
    return shm_unlink(name);
}
*/
import "C"
import (
	"unsafe"
)

func shmOpen(name string, flag int, mode uint32) (int, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	fd, err := C.c_shm_open(cName, C.int(flag), C.int(mode))
	if fd < 0 {
		return -1, err
	}
	return int(fd), nil
}

func shmUnlink(name string) error {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))
	res, err := C.c_shm_unlink(cName)
	if res < 0 {
		return err
	}
	return nil
}
