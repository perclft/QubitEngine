"use server";

import * as grpc from '@grpc/grpc-js';
import { QuantumComputeClient } from '../api/quantum';
import { CircuitRequest, GateOperation_GateType } from '../api/quantum';

const ENGINE_ADDR = process.env.ENGINE_ADDR || '127.0.0.1:50051';

const getClient = () => {
  return new QuantumComputeClient(
    ENGINE_ADDR,
    grpc.credentials.createInsecure()
  );
};

// Add Auth Metadata Helper
const getMetadata = () => {
  const meta = new grpc.Metadata();
  const token = process.env.QUBIT_ENGINE_AUTH_TOKEN || 'default-secret-token';
  meta.add('authorization', `Bearer ${token}`);
  return meta;
};

export async function runDemoCircuit(numQubits: number) {
  return new Promise((resolve, reject) => {
    const client = getClient();
    
    // Create a 3-qubit GHZ state circuit as demo
    const req: CircuitRequest = {
      numQubits: numQubits,
      operations: [
        {
          type: GateOperation_GateType.HADAMARD,
          targetQubit: 0,
          controlQubit: 0,
          classicalRegister: 0,
          angle: 0,
          secondControlQubit: 0,
          secondTargetQubit: 0,
          noiseProbability: 0
        },
        {
          type: GateOperation_GateType.CNOT,
          targetQubit: 1,
          controlQubit: 0,
          classicalRegister: 0,
          angle: 0,
          secondControlQubit: 0,
          secondTargetQubit: 0,
          noiseProbability: 0
        },
        {
          type: GateOperation_GateType.CNOT,
          targetQubit: 2,
          controlQubit: 1,
          classicalRegister: 0,
          angle: 0,
          secondControlQubit: 0,
          secondTargetQubit: 0,
          noiseProbability: 0
        }
      ],
      noiseProbability: 0,
      executionBackend: 0, // SIMULATOR
      measurementStrategy: 1, // SPARSE_STATE (only probabilities > 1e-6)
      useShm: false
    };

    client.runCircuit(req, getMetadata(), (err, response) => {
      if (err) {
        console.error("gRPC Error:", err);
        resolve({ error: err.message });
        return;
      }
      
      const results = response.sparseStates.map(st => ({
        index: st.qubitIndex,
        probability: st.probability
      }));
      
      resolve({
        serverId: response.serverId,
        results
      });
    });
  });
}

export async function getTopology() {
  return new Promise((resolve, reject) => {
    const client = getClient();
    client.getHardwareTopology({}, getMetadata(), (err, response) => {
      if (err) {
        console.error("gRPC Error:", err);
        resolve({ error: err.message });
        return;
      }
      resolve({
        nodes: response.nodes,
        edges: response.edges
      });
    });
  });
}
