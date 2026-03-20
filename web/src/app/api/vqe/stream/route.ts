import { NextRequest } from "next/server";
import { runVQEStream } from "../../../actions_streaming";

export async function GET(req: NextRequest) {
  const searchParams = req.nextUrl.searchParams;
  const molecule = parseInt(searchParams.get("molecule") || "0");
  const maxIterations = parseInt(searchParams.get("maxIterations") || "50");
  const learningRate = parseFloat(searchParams.get("learningRate") || "0.1");
  const optimizer = parseInt(searchParams.get("optimizer") || "0");

  const stream = new ReadableStream({
    async start(controller) {
      const encoder = new TextEncoder();
      
      try {
        await runVQEStream(molecule, maxIterations, learningRate, optimizer, (data) => {
          const chunk = encoder.encode(JSON.stringify(data) + "\n");
          controller.enqueue(chunk);
        });
        controller.close();
      } catch (err: any) {
        controller.error(err);
      }
    },
  });

  return new Response(stream, {
    headers: {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      "Connection": "keep-alive",
    },
  });
}
