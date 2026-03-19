"use server";

import * as grpc from '@grpc/grpc-js';
import { QuantumComputeClient } from '../api/quantum';
import { QuantumSchedulerClient } from '../api/scheduler';
import { CircuitRequest, GateOperation_GateType } from '../api/quantum';
import { ExecutionResult, TopologyData, ComplexNumber, ClusterMetricsData } from '../components/types';

// --- Types ---

export interface VQEResult {
  iteration: number;
  energy: number;
  parameters: number[];
  converged: boolean;
}

export interface JobStatusResult {
  jobId: string;
  state: number;
  positionInQueue: number;
  progressPercent: number;
  workerId: string;
  errorMessage: string;
}

export interface JobListResult {
  jobs: JobStatusResult[];
  totalCount: number;
}

// --- Singleton Clients ---

let _computeClient: QuantumComputeClient | null = null;
let _schedulerClient: QuantumSchedulerClient | null = null;

const ENGINE_ADDR = process.env.ENGINE_ADDR || '127.0.0.1:50051';
const SCHEDULER_ADDR = process.env.SCHEDULER_ADDR || '127.0.0.1:50053';

const getClient = () => {
  if (!_computeClient) {
    _computeClient = new QuantumComputeClient(ENGINE_ADDR, grpc.credentials.createInsecure());
  }
  return _computeClient;
};

const getSchedulerClient = () => {
  if (!_schedulerClient) {
    _schedulerClient = new QuantumSchedulerClient(SCHEDULER_ADDR, grpc.credentials.createInsecure());
  }
  return _schedulerClient;
};

const getMetadata = () => {
  const meta = new grpc.Metadata();
  const token = process.env.QUBIT_ENGINE_AUTH_TOKEN || 'default-secret-token';
  meta.add('authorization', `Bearer ${token}`);
  return meta;
};

// Gate type string -> enum mapping
const GATE_MAP: Record<string, GateOperation_GateType> = {
  HADAMARD: GateOperation_GateType.HADAMARD,
  PAULI_X: GateOperation_GateType.PAULI_X,
  PAULI_Y: GateOperation_GateType.PAULI_Y,
  PAULI_Z: GateOperation_GateType.PAULI_Z,
  CNOT: GateOperation_GateType.CNOT,
  SWAP: GateOperation_GateType.SWAP,
  PHASE_S: GateOperation_GateType.PHASE_S,
  PHASE_T: GateOperation_GateType.PHASE_T,
  ROTATION_X: GateOperation_GateType.ROTATION_X,
  ROTATION_Y: GateOperation_GateType.ROTATION_Y,
  ROTATION_Z: GateOperation_GateType.ROTATION_Z,
  TOFFOLI: GateOperation_GateType.TOFFOLI,
  MEASURE: GateOperation_GateType.MEASURE,
  CZ: GateOperation_GateType.CZ,
};

// ─── Dashboard: GHZ Demo ───────────────────────────────────────────────
export async function runDemoCircuit(numQubits: number): Promise<ExecutionResult | { error: string }> {
  return new Promise((resolve) => {
    const client = getClient();
    const req: CircuitRequest = {
      numQubits,
      operations: [
        { type: GateOperation_GateType.HADAMARD, targetQubit: 0, controlQubit: 0, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0 },
        { type: GateOperation_GateType.CNOT, targetQubit: 1, controlQubit: 0, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0 },
        { type: GateOperation_GateType.CNOT, targetQubit: 2, controlQubit: 1, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0 },
      ],
      noiseProbability: 0,
      executionBackend: 0,
      measurementStrategy: 1,
      useShm: false,
    };
    client.runCircuit(req, getMetadata(), (err, response) => {
      if (err) { resolve({ error: err.message }); return; }
      resolve({
        serverId: response.serverId,
        results: response.sparseStates.map((st: any) => ({ index: st.qubitIndex, probability: st.probability })),
      });
    });
  });
}

// ─── Dashboard: Topology ────────────────────────────────────────────────
export async function getTopology(): Promise<TopologyData | { error: string }> {
  return new Promise((resolve) => {
    const client = getClient();
    client.getHardwareTopology({}, getMetadata(), (err, response) => {
      if (err) { resolve({ error: err.message }); return; }
      resolve({ 
        nodes: response.nodes.map((n: any) => ({ id: n.id.toString(), x: n.x, y: n.y })), 
        edges: response.edges.map((e: any) => ({ node1: e.node1.toString(), node2: e.node2.toString() })) 
      });
    });
  });
}

// ─── Circuit Lab: Custom Circuit ────────────────────────────────────────
export async function runCustomCircuit(
  numQubits: number,
  ops: { type: string; targetQubit: number; controlQubit: number; angle: number }[],
  noiseProbability: number,
  backend: number,
): Promise<ExecutionResult | { error: string }> {
  return new Promise((resolve) => {
    const client = getClient();
    const req: CircuitRequest = {
      numQubits,
      operations: ops.map(op => ({
        type: GATE_MAP[op.type] ?? GateOperation_GateType.HADAMARD,
        targetQubit: op.targetQubit,
        controlQubit: op.controlQubit,
        classicalRegister: 0,
        angle: op.angle || 0,
        secondControlQubit: 0,
        secondTargetQubit: 0,
        noiseProbability: 0,
      })),
      noiseProbability,
      executionBackend: backend,
      measurementStrategy: 0, // FULL_STATE
      useShm: false,
    };
    client.runCircuit(req, getMetadata(), (err, response) => {
      if (err) { resolve({ error: err.message }); return; }
      resolve({
        serverId: response.serverId,
        stateVector: response.stateVector.map((sv: any) => ({ real: sv.real, imag: sv.imag })),
        results: response.sparseStates.map((st: any) => ({ index: st.qubitIndex, probability: st.probability })),
      });
    });
  });
}

// ─── VQE Explorer: Run VQE (streaming) ─────────────────────────────────
export async function runVQE(molecule: number, maxIterations: number, learningRate: number, optimizerType: number): Promise<{ iterations: VQEResult[] } | { error: string }> {
  return new Promise((resolve) => {
    const client = getClient();
    const req = { molecule, maxIterations, learningRate, optimizerType, observables: [] };
    const iterations: VQEResult[] = [];
    const stream = client.runVqe(req, getMetadata());
    stream.on('data', (response: any) => {
      iterations.push({
        iteration: response.iteration,
        energy: response.energy,
        parameters: response.parameters,
        converged: response.converged,
      });
    });
    stream.on('error', (err: any) => {
      if (iterations.length > 0) {
        resolve({ iterations });
      } else {
        resolve({ error: err.message });
      }
    });
    stream.on('end', () => {
      resolve({ iterations });
    });
  });
}

// ─── Visualizer: Step-by-step Circuit (streaming) ───────────────────────
export async function visualizeCircuit(numQubits: number): Promise<{ steps: any[] } | { error: string }> {
  return new Promise((resolve) => {
    const client = getClient();
    const req: CircuitRequest = {
      numQubits,
      operations: [
        { type: GateOperation_GateType.HADAMARD, targetQubit: 0, controlQubit: 0, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0 },
        { type: GateOperation_GateType.CNOT, targetQubit: 1, controlQubit: 0, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0 },
        { type: GateOperation_GateType.CNOT, targetQubit: 2, controlQubit: 1, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0 },
      ],
      noiseProbability: 0,
      executionBackend: 0,
      measurementStrategy: 0,
      useShm: false,
    };
    const steps: any[] = [];
    const stream = client.visualizeCircuit(req, getMetadata());
    stream.on('data', (response: any) => {
      const probabilities = response.stateVector.map((sv: any) => sv.real * sv.real + sv.imag * sv.imag);
      steps.push({ probabilities, serverId: response.serverId });
    });
    stream.on('error', (err: any) => {
      if (steps.length > 0) {
        resolve({ steps });
      } else {
        resolve({ error: err.message });
      }
    });
    stream.on('end', () => {
      resolve({ steps });
    });
  });
}

// ─── Job Queue: List Jobs ───────────────────────────────────────────────
export async function listJobs(): Promise<JobListResult | { error: string }> {
  return new Promise((resolve) => {
    const client = getSchedulerClient();
    client.listJobs({ userId: "", stateFilter: 0, limit: 50, offset: 0 }, getMetadata(), (err, response) => {
      if (err) { resolve({ error: err.message }); return; }
      resolve({
        jobs: response.jobs.map((j: any) => ({
          jobId: j.jobId,
          state: j.state,
          positionInQueue: j.positionInQueue,
          progressPercent: j.progressPercent,
          workerId: j.workerId,
          errorMessage: j.errorMessage,
        })),
        totalCount: response.totalCount,
      });
    });
  });
}

// ─── Job Queue: Get Job Status ──────────────────────────────────────────
export async function getJobStatus(jobId: string): Promise<JobStatusResult | { error: string }> {
  return new Promise((resolve) => {
    const client = getSchedulerClient();
    client.getJobStatus({ jobId, submittedAt: 0, estimatedWaitSeconds: 0 }, getMetadata(), (err, response) => {
      if (err) { resolve({ error: err.message }); return; }
      resolve({
        jobId: response.jobId,
        state: response.state,
        positionInQueue: response.positionInQueue,
        progressPercent: response.progressPercent,
        workerId: response.workerId,
        errorMessage: response.errorMessage,
      });
    });
  });
}
// --- Cluster Metrics ---------------------------------------------------
export async function getClusterMetrics(): Promise<ClusterMetricsData | { error: string }> {
  return new Promise((resolve) => {
    const client = getSchedulerClient();
    (client as any).getClusterMetrics({}, getMetadata(), (err: any, response: any) => {
      if (err) { resolve({ error: err.message }); return; }
      resolve({
        activeWorkers: response.activeWorkers,
        queueDepth: response.queueDepth,
        memoryUsagePercent: response.memoryUsagePercent,
        jobsByState: response.jobsByState || {},
      });
    });
  });
}
