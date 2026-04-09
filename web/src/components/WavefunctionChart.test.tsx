import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { WavefunctionChart } from './WavefunctionChart';

describe('WavefunctionChart', () => {
  it('renders a default state correctly', () => {
    
    // We mock ResizeObserver which Recharts requires
    global.ResizeObserver = class ResizeObserver {
      observe() {}
      unobserve() {}
      disconnect() {}
    };

    render(<WavefunctionChart result={null} error={null} />);
    expect(screen.getByText('Quantum State Distribution')).toBeInTheDocument();
  });
});
