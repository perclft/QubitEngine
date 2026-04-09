import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import { TopologyGraph } from './TopologyGraph';

describe('TopologyGraph', () => {
  it('displays the topology placeholder empty state or frame', () => {
    render(<TopologyGraph topology={{ nodes: [], edges: [] }} />);
    expect(screen.getByText('Hardware Topology')).toBeInTheDocument();
  });
});
