import { render, screen, waitFor } from '@testing-library/react';
import { ClusterMetrics } from './ClusterMetrics';
import { vi, describe, it, expect, beforeEach } from 'vitest';
import * as actions from '../app/actions';

// Mock the actions module
vi.mock('../app/actions', () => ({
  getClusterMetrics: vi.fn(),
}));

describe('ClusterMetrics', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('renders loading state initially', () => {
    render(<ClusterMetrics />);
    // Pulse div is rendered when metrics are null
    const pulse = document.querySelector('.animate-pulse');
    expect(pulse).toBeInTheDocument();
  });

  it('renders metrics after successful fetch', async () => {
    const mockMetrics = {
      activeWorkers: 5,
      queueDepth: 12,
      memoryUsagePercent: 45.5,
    };
    (actions.getClusterMetrics as any).mockResolvedValue(mockMetrics);

    render(<ClusterMetrics />);

    await waitFor(() => {
      expect(screen.getByText('5')).toBeInTheDocument();
      expect(screen.getByText('12')).toBeInTheDocument();
      expect(screen.getByText('45.5%')).toBeInTheDocument();
    });
  });

  it('renders error state on failure', async () => {
    (actions.getClusterMetrics as any).mockRejectedValue(new Error('Network error'));

    render(<ClusterMetrics />);

    await waitFor(() => {
      expect(screen.getByText(/Failed to load metrics:.*Network error/s)).toBeInTheDocument();
    });
  });
});
