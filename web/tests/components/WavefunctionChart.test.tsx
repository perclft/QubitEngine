import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import WavefunctionChart from './WavefunctionChart';

describe('WavefunctionChart', () => {
  it('renders a default state correctly', () => {
    const defaultData = [
      { state: '|0>', probability: 1.0 },
      { state: '|1>', probability: 0.0 }
    ];
    
    // We mock ResizeObserver which Recharts requires
    global.ResizeObserver = class ResizeObserver {
      observe() {}
      unobserve() {}
      disconnect() {}
    };

    render(<WavefunctionChart data={defaultData} />);
    expect(screen.getByText('Probability Distribution')).toBeInTheDocument();
  });
});
