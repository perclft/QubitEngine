"use server";

import * as grpc from '@grpc/grpc-js';
import { cookies } from 'next/headers';
import { QuantumComputeClient } from '../api/quantum';
import { QuantumSchedulerClient } from '../api/scheduler';
import { CircuitRegistryClient, CircuitMetadata } from '../api/registry';
import { 
  CircuitRequest, 
  GateOperation_GateType, 
  Measurement, 
  StateResponse, 
  StateResponse_ComplexNumber, 
  VQEResponse, 
  QubitNode, 
  CouplerEdge,
  HardwareTopologyResponse
} from '../api/quantum';
import { JobStatus, JobList } from '../api/scheduler';
import { ExecutionResult, TopologyData, ClusterMetricsData, ComplexNumber } from '../components/types';

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

// --- Dynamic Client Resolution ---

const computeClients = new Map<string, QuantumComputeClient>();
const schedulerClients = new Map<string, QuantumSchedulerClient>();
const registryClients = new Map<string, CircuitRegistryClient>();

const getClient = async () => {
  const cookieStore = await cookies();
  const addr = cookieStore.get('engine_addr')?.value || process.env.ENGINE_GRPC_ADDR || process.env.ENGINE_ADDR || '127.0.0.1:50051';
  let client = computeClients.get(addr);
  if (!client) {
    client = new QuantumComputeClient(addr, grpc.credentials.createInsecure());
    computeClients.set(addr, client);
  }
  return client;
};

const getSchedulerClient = async () => {
  const cookieStore = await cookies();
  const addr = cookieStore.get('scheduler_addr')?.value || process.env.SCHEDULER_GRPC_ADDR || process.env.SCHEDULER_ADDR || '127.0.0.1:50053';
  let client = schedulerClients.get(addr);
  if (!client) {
    client = new QuantumSchedulerClient(addr, grpc.credentials.createInsecure());
    schedulerClients.set(addr, client);
  }
  return client;
};

const getRegistryClient = async () => {
  const cookieStore = await cookies();
  const addr = cookieStore.get('registry_addr')?.value || process.env.REGISTRY_GRPC_ADDR || process.env.REGISTRY_ADDR || '127.0.0.1:50052';
  let client = registryClients.get(addr);
  if (!client) {
    client = new CircuitRegistryClient(addr, grpc.credentials.createInsecure());
    registryClients.set(addr, client);
  }
  return client;
};

const getMetadata = async () => {
  const meta = new grpc.Metadata();
  const cookieStore = await cookies();
  const token = process.env.QUBIT_ENGINE_AUTH_TOKEN || cookieStore.get('auth_token')?.value || "default-secret-token";
  meta.add('authorization', `Bearer ${token}`);
  return meta;
};

export async function saveSettingsAction(engineAddr: string, schedulerAddr: string, authToken: string): Promise<{ success: boolean }> {
  const cookieStore = await cookies();
  cookieStore.set('engine_addr', engineAddr, { path: '/', maxAge: 60 * 60 * 24 * 365 });
  cookieStore.set('scheduler_addr', schedulerAddr, { path: '/', maxAge: 60 * 60 * 24 * 365 });
  cookieStore.set('auth_token', authToken, { path: '/', maxAge: 60 * 60 * 24 * 365 });
  return { success: true };
}

export async function getSettingsAction(): Promise<{ engineAddr: string; schedulerAddr: string; authToken: string }> {
  const cookieStore = await cookies();
  return {
    engineAddr: cookieStore.get('engine_addr')?.value || process.env.ENGINE_GRPC_ADDR || process.env.ENGINE_ADDR || '127.0.0.1:50051',
    schedulerAddr: cookieStore.get('scheduler_addr')?.value || process.env.SCHEDULER_GRPC_ADDR || process.env.SCHEDULER_ADDR || '127.0.0.1:50053',
    authToken: cookieStore.get('auth_token')?.value || process.env.QUBIT_ENGINE_AUTH_TOKEN || "default-secret-token",
  };
}

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
  return new Promise(async (resolve) => {
    const client = await getClient();
    const req: CircuitRequest = {
      numQubits,
      operations: [
        { type: GateOperation_GateType.HADAMARD, targetQubit: 0, controlQubit: 0, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0, noiseGamma: 0 },
        { type: GateOperation_GateType.CNOT, targetQubit: 1, controlQubit: 0, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0, noiseGamma: 0 },
        { type: GateOperation_GateType.CNOT, targetQubit: 2, controlQubit: 1, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0, noiseGamma: 0 },
      ],
      noiseProbability: 0,
      noiseConfig: undefined,
      executionBackend: 0,
      measurementStrategy: 1,
      useShm: false,
    };
    client.runCircuit(req, await getMetadata(), (err: grpc.ServiceError | null, response: StateResponse) => {
      if (err) { resolve({ error: err.message }); return; }
      resolve({
        serverId: response.serverId,
        results: response.sparseStates.map((st: Measurement) => ({ index: st.qubitIndex, probability: st.probability })),
      });
    });
  });
}

// ─── Dashboard: Topology ────────────────────────────────────────────────
export async function getTopology(): Promise<TopologyData | { error: string }> {
  return new Promise(async (resolve) => {
    const client = await getClient();
    client.getHardwareTopology({}, await getMetadata(), (err: grpc.ServiceError | null, response: HardwareTopologyResponse) => {
      if (err) { resolve({ error: err.message }); return; }
      resolve({ 
        nodes: response.nodes.map((n: QubitNode) => ({ id: n.id.toString(), x: n.x, y: n.y })), 
        edges: response.edges.map((e: CouplerEdge) => ({ node1: e.node1.toString(), node2: e.node2.toString() })) 
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
  return new Promise(async (resolve) => {
    const client = await getClient();
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
        noiseGamma: 0,
      })),
      noiseProbability,
      noiseConfig: undefined,
      executionBackend: backend,
      measurementStrategy: 0, // FULL_STATE
      useShm: false,
    };
    client.runCircuit(req, await getMetadata(), (err: grpc.ServiceError | null, response: StateResponse) => {
      if (err) { resolve({ error: err.message }); return; }
      resolve({
        serverId: response.serverId,
        stateVector: response.stateVector.map((sv: StateResponse_ComplexNumber) => ({ real: sv.real, imag: sv.imag })),
        results: response.sparseStates.map((st: Measurement) => ({ index: st.qubitIndex, probability: st.probability })),
      });
    });
  });
}

// ─── VQE Explorer: Run VQE (streaming) ─────────────────────────────────
export async function* runVQE(molecule: number, maxIterations: number, learningRate: number, optimizerType: number): AsyncGenerator<VQEResult, void, unknown> {
  const client = await getClient();
  const req = { molecule, maxIterations, learningRate, optimizerType, observables: [] };
  const stream = client.runVqe(req, await getMetadata());
  
  const queue: VQEResult[] = [];
  let error: grpc.ServiceError | null = null;
  let done = false;
  let resolveWait: (() => void) | null = null;

  stream.on('data', (response: VQEResponse) => {
    queue.push({
      iteration: response.iteration,
      energy: response.energy,
      parameters: response.parameters,
      converged: response.converged,
    });
    if (resolveWait) {
      resolveWait();
      resolveWait = null;
    }
  });

  stream.on('error', (err: grpc.ServiceError) => {
    error = err;
    if (resolveWait) {
      resolveWait();
      resolveWait = null;
    }
  });

  stream.on('end', () => {
    done = true;
    if (resolveWait) {
      resolveWait();
      resolveWait = null;
    }
  });

  while (true) {
    const currentError = error as grpc.ServiceError | null;
    if (queue.length > 0) {
      yield queue.shift()!;
    } else if (currentError) {
      throw new Error(currentError.message || "Stream error");
    } else if (done) {
      break;
    } else {
      await new Promise<void>((r) => { resolveWait = r; });
    }
  }
}

// ─── Visualizer: Step-by-step Circuit (streaming) ───────────────────────
export async function visualizeCircuit(numQubits: number): Promise<{ steps: { probabilities: number[]; serverId: string }[] } | { error: string }> {
  return new Promise(async (resolve) => {
    const client = await getClient();
    const req: CircuitRequest = {
      numQubits,
      operations: [
        { type: GateOperation_GateType.HADAMARD, targetQubit: 0, controlQubit: 0, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0, noiseGamma: 0 },
        { type: GateOperation_GateType.CNOT, targetQubit: 1, controlQubit: 0, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0, noiseGamma: 0 },
        { type: GateOperation_GateType.CNOT, targetQubit: 2, controlQubit: 1, classicalRegister: 0, angle: 0, secondControlQubit: 0, secondTargetQubit: 0, noiseProbability: 0, noiseGamma: 0 },
      ],
      noiseProbability: 0,
      noiseConfig: undefined,
      executionBackend: 0,
      measurementStrategy: 0,
      useShm: false,
    };
    const steps: { probabilities: number[]; serverId: string }[] = [];
    const stream = client.visualizeCircuit(req, await getMetadata());
    stream.on('data', (response: StateResponse) => {
      const probabilities = response.stateVector.map((sv: StateResponse_ComplexNumber) => sv.real * sv.real + sv.imag * sv.imag);
      steps.push({ probabilities, serverId: response.serverId });
    });
    stream.on('error', (err: grpc.ServiceError) => {
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

// ─── Visualizer: Step-by-step Custom Circuit (streaming) ─────────────────
export async function visualizeCustomCircuit(
  numQubits: number,
  ops: { type: string; targetQubit: number; controlQubit: number; angle: number }[]
): Promise<{ steps: { stateVector: ComplexNumber[]; probabilities: number[]; serverId: string }[] } | { error: string }> {
  return new Promise(async (resolve) => {
    const client = await getClient();
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
        noiseGamma: 0,
      })),
      noiseProbability: 0,
      noiseConfig: undefined,
      executionBackend: 0,
      measurementStrategy: 0, // FULL_STATE
      useShm: false,
    };
    const steps: { stateVector: ComplexNumber[]; probabilities: number[]; serverId: string }[] = [];
    const stream = client.visualizeCircuit(req, await getMetadata());
    stream.on('data', (response: StateResponse) => {
      const stateVector = response.stateVector.map((sv: StateResponse_ComplexNumber) => ({ real: sv.real, imag: sv.imag }));
      const probabilities = response.stateVector.map((sv: StateResponse_ComplexNumber) => sv.real * sv.real + sv.imag * sv.imag);
      steps.push({ stateVector, probabilities, serverId: response.serverId });
    });
    stream.on('error', (err: grpc.ServiceError) => {
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
  return new Promise(async (resolve) => {
    const client = await getSchedulerClient();
    client.listJobs({ userId: "", stateFilter: 0, limit: 50, offset: 0 }, await getMetadata(), (err: grpc.ServiceError | null, response: JobList) => {
      if (err) { resolve({ error: err.message }); return; }
      resolve({
        jobs: response.jobs.map((j: JobStatus) => ({
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
  return new Promise(async (resolve) => {
    const client = await getSchedulerClient();
    client.getJobStatus({ jobId, submittedAt: 0, estimatedWaitSeconds: 0 }, await getMetadata(), (err, response) => {
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
  return new Promise(async (resolve) => {
    const client = await getSchedulerClient();
    client.getClusterMetrics({}, await getMetadata(), (err, response) => {
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
// --- Circuit Registry --------------------------------------------------
export async function saveCircuit(name: string, description: string, numQubits: number, ops: { type: string; targetQubit: number; controlQubit: number; angle: number }[]): Promise<CircuitMetadata | { error: string }> {
  return new Promise(async (resolve) => {
    const client = await getRegistryClient();
    const circuit: CircuitRequest = {
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
        noiseGamma: 0,
      })),
      noiseProbability: 0,
      noiseConfig: undefined,
      executionBackend: 0,
      measurementStrategy: 0,
      useShm: false,
    };

    client.saveCircuit({ name, description, circuit, domain: 'general', tags: [], isPublic: true }, await getMetadata(), (err, response) => {
      if (err) { resolve({ error: err.message }); return; }
      if (!response) { resolve({ error: "Empty response from registry" }); return; }
      resolve(response);
    });
  });
}

export async function listSavedCircuits(): Promise<CircuitMetadata[] | { error: string }> {
  return new Promise(async (resolve) => {
    const client = await getRegistryClient();
    client.listCircuits({ domain: '', tags: [], author: '', publicOnly: false, page: 1, pageSize: 50 }, await getMetadata(), (err, response) => {
      if (err) { resolve({ error: err.message }); return; }
      if (!response) { resolve({ error: "Empty response from registry" }); return; }
      resolve(response.circuits);
    });
  });
}

export async function loadCircuit(circuitId: string): Promise<CircuitRequest | { error: string }> {
  return new Promise(async (resolve) => {
    const client = await getRegistryClient();
    client.loadCircuit({ circuitId, version: 0 }, await getMetadata(), (err, response) => {
      if (err) { resolve({ error: err.message }); return; }
      if (!response) { resolve({ error: "Empty response from registry" }); return; }
      resolve(response);
    });
  });
}
