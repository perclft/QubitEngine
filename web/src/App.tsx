import { useState, useRef } from 'react';
import { Canvas } from '@react-three/fiber';
import { OrbitControls } from '@react-three/drei';
import { BlochSphere } from './components/BlochSphere';
import { GenerativeCanvas } from './components/GenerativeCanvas';
import { QuantumAudio } from './components/QuantumAudio';
import { ArtStudio } from './components/ArtStudio';
import { QuantumBB84 } from './components/crypto/QuantumBB84';
import { QuantumChemistry } from './components/chemistry/QuantumChemistry';
import { CircuitLibrary } from './components/education/CircuitLibrary';
import { QuizMode } from './components/education/QuizMode';
import { TutorialOverlay } from './components/education/TutorialOverlay';
import { CircuitComposer } from './components/composer/CircuitComposer';
import { QuantumComputeClient } from './generated/quantum.client';
import { CircuitRequest, GateOperation, GateOperation_GateType, CircuitRequest_ExecutionBackend } from './generated/quantum';
import { GrpcWebFetchTransport } from '@protobuf-ts/grpcweb-transport';
import './App.css';

// Envoy Proxy Address
const ENVOY_URL = 'http://localhost:8080';

// Transport setup
const transport = new GrpcWebFetchTransport({
  baseUrl: ENVOY_URL,
});

const client = new QuantumComputeClient(transport);

// View types
type ViewMode = 'dashboard' | 'artStudio' | 'crypto' | 'chemistry' | 'composer';

function App() {
  const [viewMode, setViewMode] = useState<ViewMode>('dashboard');
  const [backendType, setBackendType] = useState<CircuitRequest_ExecutionBackend>(CircuitRequest_ExecutionBackend.SIMULATOR);
  const [q0State, setQ0State] = useState({ real: 1, imag: 0 }); // |0>
  const [amplitude1, setAmplitude1] = useState({ real: 0, imag: 0 }); // Coeff of |1> for Qubit 0
  const [status, setStatus] = useState('Idle');
  const [stepCount, setStepCount] = useState(0);
  const [serverId, setServerId] = useState<string>('');
  const [noiseProb, setNoiseProb] = useState(0.0);
  const [audioActive, setAudioActive] = useState(false);
  const [enableEffects, setEnableEffects] = useState(false);
  const [isRunning, setIsRunning] = useState(false);
  const [showLibrary, setShowLibrary] = useState(false);
  const [showQuiz, setShowQuiz] = useState(false);
  const [showTutorial, setShowTutorial] = useState(false);
  const abortControllerRef = useRef<AbortController | null>(null);

  const runSimulation = async (customReq?: CircuitRequest) => {
    // Create abort controller for this run
    abortControllerRef.current = new AbortController();
    const signal = abortControllerRef.current.signal;

    setStatus('Connecting...');
    setServerId('');
    setIsRunning(true);
    setStepCount(0);

    try {
      let req = customReq;

      if (!req) {
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

        req = CircuitRequest.create({
          numQubits: 1,
          operations: ops,
          noiseProbability: noiseProb,
          executionBackend: backendType
        });
      }

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

  // If Art Studio mode, render it instead
  if (viewMode === 'artStudio') {
    return <ArtStudio onBack={() => setViewMode('dashboard')} />;
  }

  // If Crypto mode, render it
  if (viewMode === 'crypto') {
    return <QuantumBB84 onBack={() => setViewMode('dashboard')} />;
  }

  // If Chemistry mode, render it
  if (viewMode === 'chemistry') {
    return (
      <div style={{ width: '100vw', height: '100vh', position: 'relative' }}>
        <button
          onClick={() => setViewMode('dashboard')}
          style={{
            position: 'absolute',
            top: '20px',
            right: '20px',
            zIndex: 1000,
            padding: '10px',
            background: '#333',
            color: 'white',
            border: '1px solid #555'
          }}
        >
          Exit Chemistry Sim
        </button>
        <QuantumChemistry />
      </div>
    );
  }

  // If Composer mode, render it
  if (viewMode === 'composer') {
    return (
      <CircuitComposer
        onBack={() => setViewMode('dashboard')}
        onRun={(req) => {
          setViewMode('dashboard'); // Switch back to see viz
          // Inject backend preference from Main UI into the request (if not set)
          // Actually the composer creates a new request. We should merge or ensure backend is set.
          // Composer logic sets numQubits and Ops.
          // We should add backendType and noiseProb from App state to it!
          req.executionBackend = backendType;
          req.noiseProbability = noiseProb;

          // Small timeout to allow render to switch back to dashboard before running
          setTimeout(() => runSimulation(req), 100);
        }}
      />
    );
  }

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

        {/* Backend Selector */}
        <div style={{ margin: '10px 0', borderTop: '1px solid #333', paddingTop: '10px' }}>
          <label style={{ marginRight: '10px', color: '#aaa' }}>Execution Target:</label>
          <select
            value={backendType}
            onChange={(e) => setBackendType(parseInt(e.target.value))}
            style={{
              background: '#1f2937', color: 'white', border: '1px solid #4b5563',
              padding: '5px', borderRadius: '4px'
            }}
          >
            <option value={CircuitRequest_ExecutionBackend.SIMULATOR}>Local Simulator (Instant)</option>
            <option value={CircuitRequest_ExecutionBackend.MOCK_HARDWARE}>Mock QPU (Queued)</option>
            <option value={CircuitRequest_ExecutionBackend.REAL_IBM_Q}>IBM Quantum (Disabled)</option>
          </select>
        </div>

        {serverId && <p style={{ color: '#00ffff' }}>Computed By: {serverId}</p>}
        <div style={{ display: 'flex', gap: '10px', flexWrap: 'wrap' }}>
          <button onClick={() => runSimulation()} disabled={isRunning}>
            {isRunning ? 'Running...' : 'Run 1-Qubit Simulation'}
          </button>
          <button
            onClick={stopSimulation}
            disabled={!isRunning}
            style={{ background: '#ff4444', color: 'white' }}
          >
            Stop
          </button>
          <button
            onClick={() => setViewMode('artStudio')}
            style={{
              background: 'linear-gradient(135deg, #6366f1, #8b5cf6)',
              color: 'white',
              border: 'none',
              padding: '10px 20px',
              borderRadius: '8px',
              cursor: 'pointer',
              fontWeight: '600'
            }}
          >
            🎨 Art Studio
          </button>
          <button
            onClick={() => setViewMode('crypto')}
            style={{
              background: 'linear-gradient(135deg, #10b981, #34d399)',
              color: 'white',
              border: 'none',
              padding: '10px 20px',
              borderRadius: '8px',
              cursor: 'pointer',
              fontWeight: '600'
            }}
          >
            🔐 Crypto
          </button>
          <button
            onClick={() => setViewMode('chemistry')}
            style={{
              background: 'linear-gradient(135deg, #f59e0b, #d97706)',
              color: 'white',
              border: 'none',
              padding: '10px 20px',
              borderRadius: '8px',
              cursor: 'pointer',
              fontWeight: '600'
            }}
          >
            🧪 VQE Lab
          </button>
          <button
            onClick={() => setViewMode('composer')}
            style={{
              background: 'linear-gradient(135deg, #ec4899, #db2777)',
              color: 'white',
              border: 'none',
              padding: '10px 20px',
              borderRadius: '8px',
              cursor: 'pointer',
              fontWeight: '600'
            }}
          >
            🎹 Composer
          </button>
        </div>

        {/* Education Toolbar */}
        <div style={{ marginTop: '15px', display: 'flex', gap: '8px' }}>
          <button onClick={() => setShowLibrary(true)} style={eduBtnStyle}>📚 Library</button>
          <button onClick={() => setShowQuiz(true)} style={eduBtnStyle}>🧠 Quiz</button>
          <button onClick={() => setShowTutorial(true)} style={eduBtnStyle}>🎓 Tour</button>
        </div>
      </div>

      {/* Education Overlays */}
      {showLibrary && (
        <CircuitLibrary
          onLoadCircuit={(req) => {
            // In a real app we'd load this into state. 
            // For now, let's just run it immediately as a demo!
            console.log("Loaded Circuit:", req);
            client.visualizeCircuit(req); // Fire and forget for demo
            setStatus("Loaded Preset Circuit");
          }}
          onClose={() => setShowLibrary(false)}
        />
      )}
      {showQuiz && <QuizMode onClose={() => setShowQuiz(false)} />}
      {showTutorial && <TutorialOverlay onClose={() => setShowTutorial(false)} />}

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
    </div >
  );
}

const eduBtnStyle = {
  background: 'rgba(255, 255, 255, 0.1)',
  border: '1px solid rgba(255, 255, 255, 0.2)',
  color: '#ddd',
  padding: '5px 12px',
  borderRadius: '15px',
  cursor: 'pointer',
  fontSize: '0.85rem'
};

export default App;

