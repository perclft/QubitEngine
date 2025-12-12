import { useState } from 'react';
import { Canvas } from '@react-three/fiber';
import { OrbitControls } from '@react-three/drei';
import { BlochSphere } from './components/BlochSphere';
import { GenerativeCanvas } from './components/GenerativeCanvas';
import { QuantumAudio } from './components/QuantumAudio';
import { QuantumComputeClient } from './generated/quantum.client';
import { CircuitRequest, GateOperation, GateOperation_GateType } from './generated/quantum';
import { GrpcWebFetchTransport } from '@protobuf-ts/grpcweb-transport';
import './App.css';

// Envoy Proxy Address
const ENVOY_URL = 'http://localhost:8080';

// Transport setup
const transport = new GrpcWebFetchTransport({
  baseUrl: ENVOY_URL,
});

const client = new QuantumComputeClient(transport);

function App() {
  const [q0State, setQ0State] = useState({ real: 1, imag: 0 }); // |0>
  const [amplitude1, setAmplitude1] = useState({ real: 0, imag: 0 }); // Coeff of |1> for Qubit 0
  const [status, setStatus] = useState('Idle');
  const [serverId, setServerId] = useState<string>('');
  const [noiseProb, setNoiseProb] = useState(0.0);
  const [audioActive, setAudioActive] = useState(false);
  const [enableEffects, setEnableEffects] = useState(false);

  const runSimulation = async () => {
    setStatus('Connecting...');
    setServerId('');

    try {
      // 1. Build Circuit Request (Standard Test: Hadamard + Phase)
      const op1 = GateOperation.create({
        type: GateOperation_GateType.HADAMARD,
        targetQubit: 0
      });

      const op2 = GateOperation.create({
        type: GateOperation_GateType.PHASE_S,
        targetQubit: 0
      });

      const ops = [op1];
      for (let i = 0; i < 50; i++) ops.push(op2); // Spin it longer for audio effect

      const req = CircuitRequest.create({
        numQubits: 1,
        operations: ops,
        noiseProbability: noiseProb
      });

      // 2. Start Stream
      const streamCall = client.visualizeCircuit(req);

      setStatus('Streaming...');

      // 3. Iterate Responses (Async Iterable)
      for await (const response of streamCall.responses) {
        // Artificial delay for visualization/audio playback (1000ms = 1 Second)
        console.log("Tick - Delaying...");
        await new Promise(resolve => setTimeout(resolve, 1000));

        console.log('Received response:', response);
        // Capture Pod ID
        if (response.serverId) {
          console.log('ServerID:', response.serverId);
          setServerId(response.serverId);
        }

        const vec = response.stateVector;
        if (vec.length >= 2) {
          // Qubit 0 State: c0|0> + c1|1>
          const c0 = vec[0];
          const c1 = vec[1];

          setQ0State({ real: c0.real, imag: c0.imag });
          setAmplitude1({ real: c1.real, imag: c1.imag });
        }
      }

      setStatus('Completed');
    } catch (err: any) {
      console.error(err);
      setStatus('Error: ' + err.message);
    }
  };

  return (
    <div className="App">
      <div className="ui-overlay">
        <h1>Qubit Engine Live</h1>
        <p>Status: {status}</p>

        <div style={{ margin: '10px 0', border: '1px solid #333', padding: '10px', borderRadius: '5px' }}>
          <label style={{ display: 'block', marginBottom: '5px' }}>Entropy (Noise): {noiseProb.toFixed(2)}</label>
          <input
            type="range"
            min="0"
            max="0.5"
            step="0.01"
            value={noiseProb}
            onChange={e => setNoiseProb(parseFloat(e.target.value))}
            style={{ width: '100%' }}
          />
        </div>

        <div style={{ margin: '10px 0' }}>
          <label style={{ display: 'block', marginBottom: '5px' }}>
            <input
              type="checkbox"
              checked={audioActive}
              onChange={e => setAudioActive(e.target.checked)}
              style={{ marginRight: '8px' }}
            />
            Enable Quantum Audio
          </label>
          <label style={{ display: 'block' }}>
            <input
              type="checkbox"
              checked={enableEffects}
              onChange={e => setEnableEffects(e.target.checked)}
              style={{ marginRight: '8px' }}
            />
            Enable Visual Overdrive ⚠️
          </label>
        </div>

        {serverId && <p style={{ color: '#00ffff' }}>Computed By: {serverId}</p>}
        <button onClick={runSimulation}>Run 1-Qubit Simulation</button>
      </div>

      <div className="canvas-container">
        <Canvas camera={{ position: [3, 2, 5] }}>
          <ambientLight />
          <pointLight position={[10, 10, 10]} />
          <BlochSphere amplitude0={q0State} amplitude1={amplitude1} enableEffects={enableEffects} />
          <OrbitControls />
          <gridHelper />
        </Canvas>
      </div>

      <GenerativeCanvas amplitude0={q0State} amplitude1={amplitude1} />
      <QuantumAudio amplitude0={q0State} amplitude1={amplitude1} isActive={audioActive} />
    </div>
  );
}

export default App;
