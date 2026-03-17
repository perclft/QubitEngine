package models

type JobPriority int32

const (
	PriorityLow      JobPriority = 0
	PriorityNormal   JobPriority = 1
	PriorityHigh     JobPriority = 2
	PriorityRealtime JobPriority = 3
)

type JobState int32

const (
	StateUnknown   JobState = 0
	StateQueued    JobState = 1
	StateRunning   JobState = 2
	StateCompleted JobState = 3
	StateFailed    JobState = 4
	StateCancelled JobState = 5
)

type Job struct {
	ID           string            `json:"id"`
	UserID       string            `json:"user_id"`
	Priority     JobPriority       `json:"priority"`
	State        JobState          `json:"state"`
	NumQubits    int32             `json:"num_qubits"`
	NumOps       int32             `json:"num_ops"`
	Shots        int32             `json:"shots"`
	CallbackURL  string            `json:"callback_url"`
	Metadata     map[string]string `json:"metadata"`
	CircuitJSON  string            `json:"circuit_json"`
	WorkerID     string            `json:"worker_id"`
	SubmittedAt  int64             `json:"submitted_at"`
	StartedAt    int64             `json:"started_at"`
	CompletedAt  int64             `json:"completed_at"`
	ErrorMessage string            `json:"error_message"`
	Position     int32             `json:"position"`
}

type ComplexNumber struct {
	Real float64 `json:"real"`
	Imag float64 `json:"imag"`
}
