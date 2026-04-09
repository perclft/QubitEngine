import { describe, it, expect } from 'vitest';
import { render, screen } from '@testing-library/react';
import TopologyGraph from './TopologyGraph';

describe('TopologyGraph', () => {
  it('displays the topology placeholder empty state or frame', () => {
    // For a d3 or cytoscape graph, usually we just check the container renders
    render(<TopologyGraph data={{ nodes: [], edges: [] }} />);
    expect(screen.getByText('Hardware Topology')).toBeInTheDocument();
  });
});
