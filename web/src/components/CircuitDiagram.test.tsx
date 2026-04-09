import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import CircuitDiagram from './CircuitDiagram';

describe('CircuitDiagram', () => {
  it('renders correctly with given quantum circuit JSON', () => {
    const circuitData = {
      num_qubits: 2,
      operations: [],
    };
    render(<CircuitDiagram circuit={circuitData} />);
    
    // Test base layout rendered
    expect(screen.getByText('Quantum Circuit')).toBeInTheDocument();
  });
});
