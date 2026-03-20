import { describe, it, expect, vi, beforeEach } from 'vitest';
import { runVQEStream } from './actions_streaming';
import * as grpc from '@grpc/grpc-js';

// Mock gRPC
vi.mock('@grpc/grpc-js', () => {
  return {
    credentials: { createInsecure: vi.fn() },
    Metadata: vi.fn().mockImplementation(function() {
      return { add: vi.fn() };
    }),
  };
});

vi.mock('../api/quantum', () => {
    const EventEmitter = require('events');
    const mockStream = new EventEmitter();
    return {
        QuantumComputeClient: vi.fn().mockImplementation(function() {
            return {
                runVqe: vi.fn().mockReturnValue(mockStream)
            };
        })
    };
});

describe('runVQEStream', () => {
    it('calls onData for each chunk in the gRPC stream', async () => {
        const { QuantumComputeClient } = await import('../api/quantum');
        const mockStream = (new (QuantumComputeClient as any)()).runVqe();
        
        const onData = vi.fn();
        const promise = runVQEStream(0, 10, 0.1, 0, onData);

        // Simulate data arriving
        mockStream.emit('data', { iteration: 1, energy: -1.1, converged: false });
        mockStream.emit('data', { iteration: 2, energy: -1.2, converged: true });
        mockStream.emit('end');

        await promise;

        expect(onData).toHaveBeenCalledTimes(2);
        expect(onData).toHaveBeenCalledWith(expect.objectContaining({ iteration: 1, energy: -1.1 }));
        expect(onData).toHaveBeenCalledWith(expect.objectContaining({ iteration: 2, energy: -1.2, converged: true }));
    });

    it('rejects on gRPC error', async () => {
        const { QuantumComputeClient } = await import('../api/quantum');
        const mockStream = (new (QuantumComputeClient as any)()).runVqe();
        
        const onData = vi.fn();
        const promise = runVQEStream(0, 10, 0.1, 0, onData);

        mockStream.emit('error', new Error('gRPC Fail'));

        await expect(promise).rejects.toThrow('gRPC Fail');
    });
});
