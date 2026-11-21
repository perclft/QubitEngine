//go:build linux

package ipc

import "golang.org/x/sys/unix"

func shmOpen(name string, flag int, mode uint32) (int, error) {
	return unix.ShmOpen(name, flag, mode)
}

func shmUnlink(name string) error {
	return unix.ShmUnlink(name)
}
