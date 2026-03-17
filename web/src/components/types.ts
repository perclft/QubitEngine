export interface Dimension {
  width: number;
  height: number;
}

export interface ComplexNumber {
  real: number;
  imag: number;
}

export interface ResultItem {
  index: number;
  probability: number;
}

export interface ExecutionResult {
  serverId: string;
  results: ResultItem[];
  stateVector?: ComplexNumber[];
  error?: string;
}

export interface NodeItem {
  id: string;
  x: number;
  y: number;
}

export interface EdgeItem {
  node1: string;
  node2: string;
}

export interface TopologyData {
  nodes: NodeItem[];
  edges: EdgeItem[];
  error?: string;
}
