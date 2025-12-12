// Hardware Backend Abstraction Layer
// Unified interface for IBM Quantum, Rigetti, IonQ, and local simulators

package backends

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"time"
)

// ------------------------------------------------------------------
// Unified Backend Interface
// ------------------------------------------------------------------

type QuantumBackend interface {
	Name() string
	Provider() string
	MaxQubits() int
	IsSimulator() bool
	
	// Submit a circuit and get a job ID
	Submit(ctx context.Context, circuit *Circuit) (string, error)
	
	// Get job status
	Status(ctx context.Context, jobID string) (*JobStatus, error)
	
	// Get results (blocks until complete or timeout)
	Results(ctx context.Context, jobID string) (*ExecutionResult, error)
	
	// Cancel a running job
	Cancel(ctx context.Context, jobID string) error
	
	// Get backend calibration data
	Calibration(ctx context.Context) (*CalibrationData, error)
}

type Circuit struct {
	NumQubits  int            `json:"num_qubits"`
	Gates      []GateOp       `json:"gates"`
	Shots      int            `json:"shots"`
	Metadata   map[string]any `json:"metadata"`
}

type GateOp struct {
	Name   string    `json:"name"`
	Qubits []int     `json:"qubits"`
	Params []float64 `json:"params,omitempty"`
}

type JobStatus struct {
	ID          string    `json:"id"`
	Status      string    `json:"status"` // "queued", "running", "completed", "failed", "cancelled"
	Position    int       `json:"position,omitempty"`
	CreatedAt   time.Time `json:"created_at"`
	StartedAt   time.Time `json:"started_at,omitempty"`
	CompletedAt time.Time `json:"completed_at,omitempty"`
	Error       string    `json:"error,omitempty"`
}

type ExecutionResult struct {
	JobID       string            `json:"job_id"`
	Counts      map[string]int    `json:"counts"` // Measurement outcomes
	Memory      []string          `json:"memory,omitempty"` // Per-shot results
	TimeUsed    time.Duration     `json:"time_used"`
	BackendName string            `json:"backend_name"`
}

type CalibrationData struct {
	LastUpdate     time.Time           `json:"last_update"`
	T1             map[int]float64     `json:"t1"`             // T1 times per qubit (μs)
	T2             map[int]float64     `json:"t2"`             // T2 times per qubit (μs)
	ReadoutError   map[int]float64     `json:"readout_error"`  // Per-qubit readout error
	GateErrors     map[string]float64  `json:"gate_errors"`    // Per-gate error rates
	Connectivity   [][2]int            `json:"connectivity"`   // Qubit connectivity graph
}

// ------------------------------------------------------------------
// IBM Quantum Backend
// ------------------------------------------------------------------

type IBMQuantumBackend struct {
	apiKey    string
	hub       string
	group     string
	project   string
	backend   string
	baseURL   string
	client    *http.Client
}

type IBMConfig struct {
	APIKey   string
	Hub      string
	Group    string
	Project  string
	Backend  string // e.g., "ibmq_manila", "ibm_osaka"
}

func NewIBMQuantumBackend(config IBMConfig) *IBMQuantumBackend {
	return &IBMQuantumBackend{
		apiKey:  config.APIKey,
		hub:     config.Hub,
		group:   config.Group,
		project: config.Project,
		backend: config.Backend,
		baseURL: "https://api.quantum-computing.ibm.com/runtime",
		client:  &http.Client{Timeout: 30 * time.Second},
	}
}

func (b *IBMQuantumBackend) Name() string      { return b.backend }
func (b *IBMQuantumBackend) Provider() string  { return "IBM Quantum" }
func (b *IBMQuantumBackend) MaxQubits() int    { return 127 } // Varies by backend
func (b *IBMQuantumBackend) IsSimulator() bool { return false }

func (b *IBMQuantumBackend) Submit(ctx context.Context, circuit *Circuit) (string, error) {
	// Convert to IBM Qiskit format
	qasm := b.circuitToQASM(circuit)
	
	// Submit via Runtime API
	payload := map[string]any{
		"program_id": "sampler",
		"hub":        b.hub,
		"group":      b.group,
		"project":    b.project,
		"backend":    b.backend,
		"params": map[string]any{
			"circuits": []string{qasm},
			"shots":    circuit.Shots,
		},
	}
	
	// Make API request
	body, _ := json.Marshal(payload)
	req, _ := http.NewRequestWithContext(ctx, "POST", b.baseURL+"/jobs", 
		bytes.NewReader(body))
	req.Header.Set("Authorization", "Bearer "+b.apiKey)
	req.Header.Set("Content-Type", "application/json")
	
	resp, err := b.client.Do(req)
	if err != nil {
		return "", fmt.Errorf("IBM submit failed: %w", err)
	}
	defer resp.Body.Close()
	
	var result struct {
		ID string `json:"id"`
	}
	json.NewDecoder(resp.Body).Decode(&result)
	
	return result.ID, nil
}

func (b *IBMQuantumBackend) circuitToQASM(circuit *Circuit) string {
	// Convert internal circuit format to OpenQASM 3.0
	qasm := fmt.Sprintf("OPENQASM 3.0;\ninclude \"stdgates.inc\";\nqubit[%d] q;\nbit[%d] c;\n\n",
		circuit.NumQubits, circuit.NumQubits)
	
	for _, gate := range circuit.Gates {
		gateName := b.gateNameToQASM(gate.Name)
		if len(gate.Params) > 0 {
			qasm += fmt.Sprintf("%s(", gateName)
			for i, p := range gate.Params {
				if i > 0 { qasm += ", " }
				qasm += fmt.Sprintf("%f", p)
			}
			qasm += ") "
		} else {
			qasm += gateName + " "
		}
		for i, q := range gate.Qubits {
			if i > 0 { qasm += ", " }
			qasm += fmt.Sprintf("q[%d]", q)
		}
		qasm += ";\n"
	}
	
	qasm += "\nc = measure q;\n"
	return qasm
}

func (b *IBMQuantumBackend) gateNameToQASM(name string) string {
	mapping := map[string]string{
		"H": "h", "X": "x", "Y": "y", "Z": "z",
		"CNOT": "cx", "CZ": "cz", "SWAP": "swap",
		"RX": "rx", "RY": "ry", "RZ": "rz",
		"S": "s", "T": "t", "Sdg": "sdg", "Tdg": "tdg",
	}
	if mapped, ok := mapping[name]; ok {
		return mapped
	}
	return name
}

// ... Status, Results, Cancel, Calibration implementations ...

func (b *IBMQuantumBackend) Status(ctx context.Context, jobID string) (*JobStatus, error) {
	// Implementation
	return &JobStatus{ID: jobID, Status: "running"}, nil
}

func (b *IBMQuantumBackend) Results(ctx context.Context, jobID string) (*ExecutionResult, error) {
	// Implementation
	return &ExecutionResult{JobID: jobID, BackendName: b.backend}, nil
}

func (b *IBMQuantumBackend) Cancel(ctx context.Context, jobID string) error {
	return nil // Implementation
}

func (b *IBMQuantumBackend) Calibration(ctx context.Context) (*CalibrationData, error) {
	return &CalibrationData{LastUpdate: time.Now()}, nil
}

// ------------------------------------------------------------------
// Rigetti Backend (via Quil)
// ------------------------------------------------------------------

type RigettiBackend struct {
	apiKey  string
	qpu     string
	baseURL string
	client  *http.Client
}

type RigettiConfig struct {
	APIKey string
	QPU    string // e.g., "Aspen-M-3"
}

func NewRigettiBackend(config RigettiConfig) *RigettiBackend {
	return &RigettiBackend{
		apiKey:  config.APIKey,
		qpu:     config.QPU,
		baseURL: "https://api.qcs.rigetti.com",
		client:  &http.Client{Timeout: 30 * time.Second},
	}
}

func (b *RigettiBackend) Name() string      { return b.qpu }
func (b *RigettiBackend) Provider() string  { return "Rigetti" }
func (b *RigettiBackend) MaxQubits() int    { return 80 }
func (b *RigettiBackend) IsSimulator() bool { return false }

func (b *RigettiBackend) circuitToQuil(circuit *Circuit) string {
	quil := ""
	for _, gate := range circuit.Gates {
		gateName := b.gateNameToQuil(gate.Name)
		if len(gate.Params) > 0 {
			quil += fmt.Sprintf("%s(", gateName)
			for i, p := range gate.Params {
				if i > 0 { quil += ", " }
				quil += fmt.Sprintf("%f", p)
			}
			quil += ") "
		} else {
			quil += gateName + " "
		}
		for _, q := range gate.Qubits {
			quil += fmt.Sprintf("%d ", q)
		}
		quil += "\n"
	}
	
	for i := 0; i < circuit.NumQubits; i++ {
		quil += fmt.Sprintf("MEASURE %d ro[%d]\n", i, i)
	}
	
	return quil
}

func (b *RigettiBackend) gateNameToQuil(name string) string {
	mapping := map[string]string{
		"H": "H", "X": "X", "Y": "Y", "Z": "Z",
		"CNOT": "CNOT", "CZ": "CZ", "SWAP": "SWAP",
		"RX": "RX", "RY": "RY", "RZ": "RZ",
	}
	if mapped, ok := mapping[name]; ok {
		return mapped
	}
	return name
}

func (b *RigettiBackend) Submit(ctx context.Context, circuit *Circuit) (string, error) {
	return "rigetti-job-" + fmt.Sprint(time.Now().UnixNano()), nil
}

func (b *RigettiBackend) Status(ctx context.Context, jobID string) (*JobStatus, error) {
	return &JobStatus{ID: jobID, Status: "running"}, nil
}

func (b *RigettiBackend) Results(ctx context.Context, jobID string) (*ExecutionResult, error) {
	return &ExecutionResult{JobID: jobID, BackendName: b.qpu}, nil
}

func (b *RigettiBackend) Cancel(ctx context.Context, jobID string) error { return nil }

func (b *RigettiBackend) Calibration(ctx context.Context) (*CalibrationData, error) {
	return &CalibrationData{LastUpdate: time.Now()}, nil
}

// ------------------------------------------------------------------
// IonQ Backend
// ------------------------------------------------------------------

type IonQBackend struct {
	apiKey  string
	target  string // "qpu" or "simulator"
	baseURL string
	client  *http.Client
}

type IonQConfig struct {
	APIKey string
	Target string // "qpu.harmony", "qpu.aria-1", "simulator"
}

func NewIonQBackend(config IonQConfig) *IonQBackend {
	return &IonQBackend{
		apiKey:  config.APIKey,
		target:  config.Target,
		baseURL: "https://api.ionq.co/v0.3",
		client:  &http.Client{Timeout: 30 * time.Second},
	}
}

func (b *IonQBackend) Name() string      { return b.target }
func (b *IonQBackend) Provider() string  { return "IonQ" }
func (b *IonQBackend) MaxQubits() int    { return 32 }
func (b *IonQBackend) IsSimulator() bool { return b.target == "simulator" }

func (b *IonQBackend) circuitToIonQ(circuit *Circuit) map[string]any {
	gates := make([]map[string]any, 0, len(circuit.Gates))
	
	for _, gate := range circuit.Gates {
		g := map[string]any{
			"gate":    b.gateNameToIonQ(gate.Name),
			"targets": gate.Qubits,
		}
		if len(gate.Params) > 0 {
			g["rotation"] = gate.Params[0]
		}
		gates = append(gates, g)
	}
	
	return map[string]any{
		"qubits": circuit.NumQubits,
		"circuit": gates,
	}
}

func (b *IonQBackend) gateNameToIonQ(name string) string {
	mapping := map[string]string{
		"H": "h", "X": "x", "Y": "y", "Z": "z",
		"CNOT": "cnot", "CZ": "zz", "SWAP": "swap",
		"RX": "rx", "RY": "ry", "RZ": "rz",
	}
	if mapped, ok := mapping[name]; ok {
		return mapped
	}
	return name
}

func (b *IonQBackend) Submit(ctx context.Context, circuit *Circuit) (string, error) {
	return "ionq-job-" + fmt.Sprint(time.Now().UnixNano()), nil
}

func (b *IonQBackend) Status(ctx context.Context, jobID string) (*JobStatus, error) {
	return &JobStatus{ID: jobID, Status: "running"}, nil
}

func (b *IonQBackend) Results(ctx context.Context, jobID string) (*ExecutionResult, error) {
	return &ExecutionResult{JobID: jobID, BackendName: b.target}, nil
}

func (b *IonQBackend) Cancel(ctx context.Context, jobID string) error { return nil }

func (b *IonQBackend) Calibration(ctx context.Context) (*CalibrationData, error) {
	return &CalibrationData{LastUpdate: time.Now()}, nil
}

// ------------------------------------------------------------------
// Local Simulator Backend
// ------------------------------------------------------------------

type LocalSimulatorBackend struct {
	engineAddr string
	maxQubits  int
}

func NewLocalSimulatorBackend(engineAddr string) *LocalSimulatorBackend {
	return &LocalSimulatorBackend{
		engineAddr: engineAddr,
		maxQubits:  30, // Limited by memory
	}
}

func (b *LocalSimulatorBackend) Name() string      { return "qubit-engine-sim" }
func (b *LocalSimulatorBackend) Provider() string  { return "QubitEngine" }
func (b *LocalSimulatorBackend) MaxQubits() int    { return b.maxQubits }
func (b *LocalSimulatorBackend) IsSimulator() bool { return true }

func (b *LocalSimulatorBackend) Submit(ctx context.Context, circuit *Circuit) (string, error) {
	return "local-" + fmt.Sprint(time.Now().UnixNano()), nil
}

func (b *LocalSimulatorBackend) Status(ctx context.Context, jobID string) (*JobStatus, error) {
	return &JobStatus{ID: jobID, Status: "completed"}, nil
}

func (b *LocalSimulatorBackend) Results(ctx context.Context, jobID string) (*ExecutionResult, error) {
	return &ExecutionResult{
		JobID:       jobID,
		Counts:      map[string]int{"0000": 512, "1111": 512},
		BackendName: b.Name(),
	}, nil
}

func (b *LocalSimulatorBackend) Cancel(ctx context.Context, jobID string) error { return nil }

func (b *LocalSimulatorBackend) Calibration(ctx context.Context) (*CalibrationData, error) {
	// Perfect simulator - no errors
	return &CalibrationData{LastUpdate: time.Now()}, nil
}

// ------------------------------------------------------------------
// Backend Registry
// ------------------------------------------------------------------

type BackendRegistry struct {
	backends map[string]QuantumBackend
}

func NewBackendRegistry() *BackendRegistry {
	return &BackendRegistry{
		backends: make(map[string]QuantumBackend),
	}
}

func (r *BackendRegistry) Register(name string, backend QuantumBackend) {
	r.backends[name] = backend
}

func (r *BackendRegistry) Get(name string) (QuantumBackend, bool) {
	b, ok := r.backends[name]
	return b, ok
}

func (r *BackendRegistry) List() []string {
	names := make([]string, 0, len(r.backends))
	for name := range r.backends {
		names = append(names, name)
	}
	return names
}

// Import bytes package
import "bytes"
