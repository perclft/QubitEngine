import { GrpcWebFetchTransport } from "@protobuf-ts/grpcweb-transport";
import { VQESolverClient } from "./src/generated/api/proto/physics/vqe.client";
import { VQERequest, AnsatzType, OptimizerType } from "./src/generated/api/proto/physics/vqe";

async function run() {
    const transport = new GrpcWebFetchTransport({ baseUrl: "http://localhost:8080" });
    const client = new VQESolverClient(transport);

    const request = VQERequest.create({
        target: {
            oneofKind: "molecule",
            molecule: {
                name: "H2",
                atoms: [],
                charge: 0,
                multiplicity: 1,
                basisSet: "sto-3g"
            }
        },
        ansatz: AnsatzType.ANSATZ_UCCSD,
        optimizer: OptimizerType.OPTIMIZER_COBYLA,
        maxIterations: 10,
        convergenceThreshold: 1e-6
    });

    try {
        console.log("Starting gRPC-Web request to localhost:8080...");
        const stream = client.findGroundState(request);

        for await (const response of stream.responses) {
            console.log(`Iteration: ${response.iteration}, Energy: ${response.energy}`);
        }
        console.log("Stream finished.");
        
        const status = await stream.status;
        console.log("Status:", status);
        
        const trailers = await stream.trailers;
        console.log("Trailers:", trailers);
        
    } catch (err: any) {
        console.error("Caught error:");
        console.error(err);
    }
}

run();
