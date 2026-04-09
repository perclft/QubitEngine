import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { CircuitDiagram } from './CircuitDiagram';

describe('CircuitDiagram', () => {
  it('renders correctly with given quantum circuit JSON', () => {
    render(<CircuitDiagram numQubits={2} gates={[]} />);
    
    // Test base layout rendered
    expect(screen.getByText('Quantum Circuit')).toBeInTheDocument();
  });
});
