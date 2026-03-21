import * as grpc from '@grpc/grpc-js';
import { QuantumComputeClient } from '../api/quantum';

const getClient = () => {
    const addr = process.env.ENGINE_GRPC_ADDR || process.env.ENGINE_ADDR || '127.0.0.1:50051';
    return new QuantumComputeClient(addr, grpc.credentials.createInsecure());
};

const getMetadata = () => {
  const meta = new grpc.Metadata();
  const token = process.env.QUBIT_ENGINE_AUTH_TOKEN || 'default-secret-token';
  meta.add('authorization', `Bearer ${token}`);
  return meta;
};

export async function runVQEStream(
  molecule: number, 
  maxIterations: number, 
  learningRate: number, 
  optimizerType: number,
  onData: (data: any) => void
): Promise<void> {
  return new Promise((resolve, reject) => {
    const client = getClient();
    const req = { molecule, maxIterations, learningRate, optimizerType, observables: [] };
    const stream = client.runVqe(req, getMetadata());
    
    stream.on('data', (response: any) => {
      onData({
        iteration: response.iteration,
        energy: response.energy,
        parameters: response.parameters,
        converged: response.converged,
      });
    });

    stream.on('error', (err: any) => {
      reject(err);
    });

    stream.on('end', () => {
      resolve();
    });
  });
}
