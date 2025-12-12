import { useState, useRef } from 'react';
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
  const [stepCount, setStepCount] = useState(0);
  const [serverId, setServerId] = useState<string>('');
  const [noiseProb, setNoiseProb] = useState(0.0);
  const [audioActive, setAudioActive] = useState(false);
  const [enableEffects, setEnableEffects] = useState(false);
  const [isRunning, setIsRunning] = useState(false);
  const abortControllerRef = useRef<AbortController | null>(null);

  const runSimulation = async () => {
    // Create abort controller for this run
    abortControllerRef.current = new AbortController();
    const signal = abortControllerRef.current.signal;

    setStatus('Connecting...');
    setServerId('');
    setIsRunning(true);
    setStepCount(0);

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
      let step = 0;
      for await (const response of streamCall.responses) {
        step++;
        setStepCount(step);
        // Artificial delay for visualization/audio playback (2000ms = 2 Seconds)
        console.log(`Step ${step} - Delaying 2 seconds...`);
        await new Promise(resolve => setTimeout(resolve, 2000));

        // Check for abort before updating state
        if (signal.aborted) {
          setStatus('Stopped');
          setIsRunning(false);
          return;
        }

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
      setIsRunning(false);
    } catch (err: any) {
      if (err.name === 'AbortError' || signal.aborted) {
        setStatus('Stopped');
      } else {
        console.error(err);
        setStatus('Error: ' + err.message);
      }
      setIsRunning(false);
    }
  };

  const stopSimulation = () => {
    if (abortControllerRef.current) {
      abortControllerRef.current.abort();
      setStatus('Stopping...');
    }
  };

  return (
    <div className="App">
      <div className="ui-overlay">
        <h1>Qubit Engine Live <span style={{ fontSize: '0.4em', color: 'lime' }}>v3.0-SLOWMO</span></h1>
        <p>Status: {status} {stepCount > 0 && <span style={{ color: 'yellow' }}>| Step {stepCount}/51</span>}</p>

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
        <div style={{ display: 'flex', gap: '10px' }}>
          <button onClick={runSimulation} disabled={isRunning}>
            {isRunning ? 'Running...' : 'Run 1-Qubit Simulation'}
          </button>
          <button
            onClick={stopSimulation}
            disabled={!isRunning}
            style={{ background: '#ff4444', color: 'white' }}
          >
            Stop
          </button>
        </div>
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
