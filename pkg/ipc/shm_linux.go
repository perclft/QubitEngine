//go:build linux

package ipc

import "golang.org/x/sys/unix"

func shmPath(name string) string {
	if len(name) > 0 && name[0] == '/' {
		name = name[1:]
	}
	return "/dev/shm/" + name
}

func shmOpen(name string, flag int, mode uint32) (int, error) {
	return unix.Open(shmPath(name), flag|unix.O_CLOEXEC, mode)
}

func shmUnlink(name string) error {
	return unix.Unlink(shmPath(name))
}
