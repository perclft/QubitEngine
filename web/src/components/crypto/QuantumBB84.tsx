import React, { useState, useEffect } from 'react';
import { QuantumCryptoClient } from '../../generated/crypto/crypto.client';
import { BB84AliceRequest, BB84BobRequest, BB84AliceState, BB84BobState, Basis, ReconcileRequest } from '../../generated/crypto/crypto';
import { GrpcWebFetchTransport } from '@protobuf-ts/grpcweb-transport';
import './QuantumBB84.css';

const ENVOY_URL = 'http://localhost:8080';
const transport = new GrpcWebFetchTransport({ baseUrl: ENVOY_URL });
const client = new QuantumCryptoClient(transport);

export function QuantumBB84({ onBack }: { onBack: () => void }) {
    const [numBits, setNumBits] = useState(10);
    const [eveProb, setEveProb] = useState(0.0);
    const [sessionId, setSessionId] = useState('');

    const [aliceState, setAliceState] = useState<BB84AliceState | null>(null);
    const [bobState, setBobState] = useState<BB84BobState | null>(null);
    const [reconciled, setReconciled] = useState<any>(null);

    const [status, setStatus] = useState('Idle');
    const [error, setError] = useState('');

    const startAlice = async () => {
        try {
            setStatus('Alice Preparing Qubits...');
            const sid = `session_${Date.now()}`;
            setSessionId(sid);
            setBobState(null);
            setReconciled(null);
            setError('');

            const req = BB84AliceRequest.create({
                numBits,
                sessionId: sid,
                eavesdropProbability: eveProb
            });

            const { response } = await client.startBB84Alice(req);
            setAliceState(response);
            setStatus('Alice Sent Qubits. Waiting for Bob...');
        } catch (err: any) {
            setError('Alice Error: ' + err.message);
        }
    };

    const startBob = async () => {
        if (!aliceState) return;
        try {
            setStatus('Bob Measuring...');
            const req = BB84BobRequest.create({
                sessionId: sessionId,
                // In simulation, "sending" is implicit via session store
            });

            const { response } = await client.startBB84Bob(req);
            setBobState(response);
            setStatus('Bob Finished. Ready to Reconcile.');
        } catch (err: any) {
            setError('Bob Error: ' + err.message);
        }
    };

    const reconcile = async () => {
        if (!aliceState || !bobState) return;
        try {
            setStatus('Reconciling Bases...');
            const req = ReconcileRequest.create({
                sessionId: sessionId,
                // Pass simulated data as proof (in real QKD this is done via classical channel)
                // Here we just pass session ID, server has the truth. 
                // Wait, proto ReconcileRequest expects full arrays?
                // Let's check proto.
                // ReconcileRequest: session_id, repeated bases...
                // Wait, server has them in memory!
                // Sending them again mimics the "public discussion" phase.
            });
            // Actually, my server implementation uses sessionID to look up everything. 
            // I can pass empty arrays if the server doesn't enforce it?
            // Server implementation:
            // func (s *CryptoServer) ReconcileBB84(ctx context.Context, req *pb.ReconcileRequest)
            // It iterates session.AliceBases.
            // It IGNORES request content other than SessionId!
            // Great, simplified simulation.

            const { response } = await client.reconcileBB84(req);
            setReconciled(response);
            setStatus('Reconciliation Complete.');
        } catch (err: any) {
            setError('Reconcile Error: ' + err.message);
        }
    };

    const renderBits = (bits: number[], bases: Basis[]) => {
        return (
            <div className="bit-stream">
                {bits.map((b, i) => (
                    <div key={i} className={`bit-box basis-${bases[i]}`}>
                        <span className="bit-val">{b}</span>
                        <span className="basis-val">{bases[i] === Basis.RECTILINEAR ? '+' : '×'}</span>
                    </div>
                ))}
            </div>
        );
    };

    const renderBobBits = (measures: number[], bases: Basis[]) => {
        // Bob doesn't know values until he measures.
        return renderBits(measures, bases);
    };

    return (
        <div className="bb84-container">
            <div className="header">
                <button onClick={onBack}>&larr; Back</button>
                <h2>Quantum Cryptography: BB84 Protocol</h2>
            </div>

            <div className="controls">
                <div className="control-group">
                    <label>Qubits: {numBits}</label>
                    <input type="range" min="5" max="50" value={numBits} onChange={e => setNumBits(Number(e.target.value))} />
                </div>
                <div className="control-group">
                    <label style={{ color: eveProb > 0 ? 'red' : 'inherit' }}>
                        Eve Probability: {(eveProb * 100).toFixed(0)}%
                    </label>
                    <input type="range" min="0" max="1" step="0.1" value={eveProb} onChange={e => setEveProb(parseFloat(e.target.value))} />
                </div>
            </div>

            <div className="status-bar">{status} {error && <span className="error">| {error}</span>}</div>

            <div className="actors">
                <div className="actor alice">
                    <h3>Alice (Sender)</h3>
                    <button onClick={startAlice} disabled={!!aliceState && !reconciled}>
                        {aliceState ? 'Restart' : '1. Prepare & Send'}
                    </button>

                    {aliceState && (
                        <div className="state-display">
                            <h4>prepared_qubits[]</h4>
                            {renderBits(aliceState.bits, aliceState.bases)}
                        </div>
                    )}
                </div>

                <div className="channel">
                    <div className="quantum-pipe">
                        {aliceState && !bobState && <div className="photon-stream">⚛️ ⚛️ ⚛️</div>}
                        {eveProb > 0 && <div className="eve-indicator">🕵️‍♀️ Eve is Listening!</div>}
                    </div>
                </div>

                <div className="actor bob">
                    <h3>Bob (Receiver)</h3>
                    <button onClick={startBob} disabled={!aliceState || !!bobState}>
                        2. Measure
                    </button>

                    {bobState && (
                        <div className="state-display">
                            <h4>measured_qubits[]</h4>
                            {renderBobBits(bobState.measurements, bobState.bases)}
                        </div>
                    )}
                </div>
            </div>

            {bobState && (
                <div className="reconciliation">
                    <button className="primary-btn" onClick={reconcile} disabled={!!reconciled}>
                        3. Reconcile Bases (Public Discussion)
                    </button>

                    {reconciled && (
                        <div className="results-panel">
                            <h3>Shared Secret Key</h3>
                            <div className="key-display">
                                <code className="hex-key">
                                    {reconciled.sharedKey ? Array.from(reconciled.sharedKey).map((b: any) => b.toString(16).padStart(2, '0')).join('') : 'Processing...'}
                                </code>
                            </div>

                            <div className="stats">
                                <div className="stat">
                                    <label>Sifted Bits</label>
                                    <span>{reconciled.siftedBits} / {reconciled.originalBits}</span>
                                </div>
                                <div className="stat">
                                    <label>Error Rate</label>
                                    <span className={reconciled.errorRate > 0.1 ? 'danger' : 'success'}>
                                        {(reconciled.errorRate * 100).toFixed(2)}%
                                    </span>
                                </div>
                                <div className="stat">
                                    <label>Security</label>
                                    <span className={reconciled.secure ? 'success' : 'danger'}>
                                        {reconciled.secure ? 'SECURE ✅' : 'COMPROMISED ❌'}
                                    </span>
                                </div>
                            </div>

                            {reconciled.secure ? (
                                <p className="success-msg">Channel is secure. Key established.</p>
                            ) : (
                                <p className="danger-msg">High error rate detected! Eve intercepted the key.</p>
                            )}
                        </div>
                    )}
                </div>
            )}
        </div>
    );
}
