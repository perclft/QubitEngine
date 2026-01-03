import { GrpcWebFetchTransport } from "@protobuf-ts/grpcweb-transport";
import { QuantumComputeClient } from "../generated/quantum.client";
import { CircuitRequest, GateOperation_GateType, CircuitRequest_ExecutionBackend, StateResponse } from "../generated/quantum";

// Standard Envoy Proxy port for gRPC-Web
const BACKEND_URL = "http://localhost:8080";

export interface QuantumStateData {
    entropy: number;
    phase: number;
    amplitudes: { real: number; imag: number }[];
}

export class BackendBridge {
    private client: QuantumComputeClient;
    private transport: GrpcWebFetchTransport;
    private isConnected: boolean = false;

    constructor() {
        this.transport = new GrpcWebFetchTransport({
            baseUrl: BACKEND_URL,
        });
        this.client = new QuantumComputeClient(this.transport);
    }

    /**
     * Connects to the backend and returns a generator/iterator of quantum states.
     * If connection fails, it falls back to a simulated stream.
     */
    async *streamQuantumState(): AsyncGenerator<QuantumStateData> {
        try {
            // Define a simple test circuit: 3 Qubits, Hadamard on all to create high entropy
            const request: CircuitRequest = {
                numQubits: 3,
                operations: [
                    { type: GateOperation_GateType.HADAMARD, targetQubit: 0, controlQubit: 0, secondControlQubit: 0, angle: 0, classicalRegister: 0 },
                    { type: GateOperation_GateType.HADAMARD, targetQubit: 1, controlQubit: 0, secondControlQubit: 0, angle: 0, classicalRegister: 0 },
                    { type: GateOperation_GateType.PHASE_S, targetQubit: 2, controlQubit: 0, secondControlQubit: 0, angle: 0, classicalRegister: 0 }
                ],
                executionBackend: CircuitRequest_ExecutionBackend.SIMULATOR,
                noiseProbability: 0.01 // Add variance
            };

            const stream = this.client.visualizeCircuit(request);
            
            this.isConnected = true;
            console.log("Connected to Quantum Backend via gRPC-Web");

            for await (const response of stream.responses) {
                yield this.parseResponse(response);
            }
        } catch (error) {
            console.warn("Backend unavailable, falling back to Simulation Mode:", error);
            this.isConnected = false;
            // Fallback to simulation
            yield* this.simulateStream();
        }
    }

    private parseResponse(res: StateResponse): QuantumStateData {
        // Calculate Entropy and Phase from the State Vector
        // Entropy S = -sum(p * log(p)) where p = |amplitude|^2
        let entropy = 0;
        let phaseSum = 0;
        // let totalProb = 0; // Unused but part of standard calc

        const amplitudes = res.stateVector.map(c => ({ real: c.real, imag: c.imag }));

        for (const amp of amplitudes) {
            const prob = amp.real ** 2 + amp.imag ** 2;
            if (prob > 1e-9) {
                entropy -= prob * Math.log(prob);
                phaseSum += Math.atan2(amp.imag, amp.real) * prob; // Weighted phase
            }
            // totalProb += prob;
        }

        return {
            entropy: Math.abs(entropy), // Simple Von Neumann proxy
            phase: phaseSum,
            amplitudes
        };
    }

    // Mock Generator for "Demo Mode"
    private async *simulateStream(): AsyncGenerator<QuantumStateData> {
        let t = 0;
        while (true) {
            t += 0.05;
            // Simulate oscillating entropy (breathing effect)
            const simulatedEntropy = 0.3 + Math.sin(t * 0.5) * 0.2 + (Math.random() * 0.05);
            const simulatedPhase = t; // Continual rotation
            
            yield {
                entropy: simulatedEntropy,
                phase: simulatedPhase,
                amplitudes: [] // Simulation logic in shader uses phase/entropy, doesn't need raw amps
            };

            // 60 FPS Emulation
            await new Promise(resolve => setTimeout(resolve, 16));
        }
    }
}

export const backend = new BackendBridge();
