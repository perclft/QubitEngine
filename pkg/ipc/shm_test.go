package ipc

import (
	"context"
	"encoding/json"
	"fmt"
	"math/cmplx"
	"runtime"
	"testing"
	"unsafe"

	"golang.org/x/sys/windows"
)

func isWindowsOS() bool {
	return runtime.GOOS == "windows"
}

func writeTestSegment(segmentName string, sizeBytes uint64, data []complex128) (func(), error) {
	if isWindowsOS() {
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
	return func() {}, fmt.Errorf("non-windows test segment helper implemented per OS")
}

func TestProbeSharedMemoryAccess(t *testing.T) {
	ok := ProbeSharedMemoryAccess()
	if !ok {
		t.Log("ProbeSharedMemoryAccess returned false (SHM access disabled or unavailable on host/container)")
	} else {
		t.Log("ProbeSharedMemoryAccess returned true (SHM access verified)")
	}
}

func TestReadAndAcknowledgeStateVector_BitIdentical(t *testing.T) {
	qubitCounts := []uint32{10, 18, 22, 25}

	for _, numQubits := range qubitCounts {
		elementCount := 1 << numQubits
		sizeBytes := uint64(elementCount * 16)
		segmentName := fmt.Sprintf("Local\\qe_shm_test_bit_identical_%d", numQubits)

		expected := make([]complex128, elementCount)
		for i := 0; i < elementCount; i++ {
			expected[i] = complex(float64(i)*0.5, float64(i)*-0.25)
		}

		cleanup, err := writeTestSegment(segmentName, sizeBytes, expected)
		if err != nil {
			t.Fatalf("Failed to write test segment for N=%d: %v", numQubits, err)
		}

		desc := ShmDescriptor{
			SegmentName: segmentName,
			SizeBytes:   sizeBytes,
			NumQubits:   numQubits,
			DataType:    "complex128",
			CreatedAtMs: 1785800000000,
			AckToken:    "test_token_bit_identical",
		}
		descBytes, _ := json.Marshal(desc)

		actual, err := ReadAndAcknowledgeStateVector(context.Background(), string(descBytes), nil)
		if err != nil {
			cleanup()
			t.Fatalf("ReadAndAcknowledgeStateVector failed for N=%d: %v", numQubits, err)
		}

		if len(actual) != elementCount {
			cleanup()
			t.Fatalf("Element count mismatch for N=%d: expected %d, got %d", numQubits, elementCount, len(actual))
		}

		for i := 0; i < elementCount; i++ {
			if actual[i] != expected[i] {
				diff := cmplx.Abs(actual[i] - expected[i])
				cleanup()
				t.Fatalf("Mismatch at index %d for N=%d: expected %v, got %v (diff=%g)", i, numQubits, expected[i], actual[i], diff)
			}
		}
		cleanup()
		t.Logf("N=%d (%d bytes): 100%% bit-identical state vector verified", numQubits, sizeBytes)
	}
}
