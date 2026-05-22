import { NextResponse } from 'next/server';
import * as grpc from '@grpc/grpc-js';
import { QuantumSchedulerClient, ClusterMetricsResponse } from '../../../api/scheduler';

const SCHEDULER_ADDR = process.env.SCHEDULER_GRPC_ADDR || process.env.SCHEDULER_ADDR || '127.0.0.1:50053';

export async function GET() {
  const encoder = new TextEncoder();
  const client = new QuantumSchedulerClient(SCHEDULER_ADDR, grpc.credentials.createInsecure());
  const meta = new grpc.Metadata();
  const token = process.env.QUBIT_ENGINE_AUTH_TOKEN || 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJ0ZXN0LXVzZXIiLCJpYXQiOjE3NzQ3MzYwNzcsImV4cCI6MjA5MDA5NjA3N30.1nKrhtvdTUoaAL8wzWGNHQhk40cHRpbxWjbAbS1lNSA';
  meta.add('authorization', `Bearer ${token}`);

  let isClosed = false;
  let stream: grpc.ClientReadableStream<ClusterMetricsResponse> | null = null;

  const customReadable = new ReadableStream({
    start(controller) {
      stream = client.streamClusterMetrics({}, meta);
      
      stream.on('data', (response: ClusterMetricsResponse) => {
        if (isClosed) return;
        try {
          const metrics = {
            activeWorkers: response.activeWorkers,
            queueDepth: response.queueDepth,
            memoryUsagePercent: response.memoryUsagePercent,
            jobsByState: response.jobsByState || {},
          };
          controller.enqueue(encoder.encode(`data: ${JSON.stringify(metrics)}\n\n`));
        } catch (e) {
          console.error('SSE Enqueue Error:', e);
        }
      });

      stream.on('error', (err: grpc.ServiceError) => {
        console.error('SSE gRPC Error:', err);
        if (!isClosed) {
          try { controller.error(err); } catch { /* ignore */ }
          isClosed = true;
        }
      });

      stream.on('end', () => {
        if (!isClosed) {
          try { controller.close(); } catch { /* ignore */ }
          isClosed = true;
        }
      });
    },
    cancel() {
      isClosed = true;
      if (stream) {
        stream.cancel();
      }
    }
  });

  return new NextResponse(customReadable, {
    headers: {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache, no-transform',
      'Connection': 'keep-alive',
    },
  });
}
