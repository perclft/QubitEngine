import { NextResponse } from 'next/server';
import * as grpc from '@grpc/grpc-js';
import { QuantumSchedulerClient } from '../../../api/scheduler';

const SCHEDULER_ADDR = process.env.SCHEDULER_GRPC_ADDR || process.env.SCHEDULER_ADDR || '127.0.0.1:50053';

export async function GET() {
  const encoder = new TextEncoder();
  const client = new QuantumSchedulerClient(SCHEDULER_ADDR, grpc.credentials.createInsecure());
  const meta = new grpc.Metadata();
  const token = process.env.QUBIT_ENGINE_AUTH_TOKEN || 'default-secret-token';
  meta.add('authorization', `Bearer ${token}`);
  const customReadable = new ReadableStream({
    start(controller) {
      const stream = client.streamClusterMetrics({}, meta);
      let isClosed = false;
      
      stream.on('data', (response: any) => {
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

      stream.on('error', (err: any) => {
        console.error('SSE gRPC Error:', err);
        if (!isClosed) {
          try { controller.error(err); } catch (e) {}
          isClosed = true;
        }
      });

      stream.on('end', () => {
        if (!isClosed) {
          try { controller.close(); } catch (e) {}
          isClosed = true;
        }
      });

      // Handle client disconnect
      return () => {
        isClosed = true;
        stream.cancel();
      };
    },
    cancel() {
      // This is called when the consumer cancels the stream
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
