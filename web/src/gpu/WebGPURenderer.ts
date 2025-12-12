// WebGPU Quantum State Visualization
// GPU-accelerated wavefunction and probability rendering

export interface WebGPUQuantumRenderer {
    initWebGPU(): Promise<boolean>;
    renderStateVector(amplitudes: Complex[]): void;
    renderProbabilities(probabilities: number[]): void;
    renderBlochSphere(theta: number, phi: number): void;
    destroy(): void;
}

interface Complex {
    real: number;
    imag: number;
}

// Shader code for probability histogram rendering
const PROBABILITY_SHADER = `
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>,
};

struct Uniforms {
    viewProjection: mat4x4<f32>,
    time: f32,
    numBars: u32,
    maxProb: f32,
    padding: f32,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var<storage, read> probabilities: array<f32>;

@vertex
fn vertexMain(
    @builtin(vertex_index) vertexIndex: u32,
    @builtin(instance_index) instanceIndex: u32
) -> VertexOutput {
    var output: VertexOutput;
    
    let barWidth = 2.0 / f32(uniforms.numBars);
    let barHeight = probabilities[instanceIndex] / uniforms.maxProb;
    
    // Generate bar vertices (4 corners per bar, 6 indices for 2 triangles)
    let vertexId = vertexIndex % 6u;
    var localPos: vec2<f32>;
    
    switch (vertexId) {
        case 0u: { localPos = vec2<f32>(0.0, 0.0); }
        case 1u, 4u: { localPos = vec2<f32>(1.0, 0.0); }
        case 2u, 3u: { localPos = vec2<f32>(0.0, 1.0); }
        case 5u: { localPos = vec2<f32>(1.0, 1.0); }
        default: { localPos = vec2<f32>(0.0, 0.0); }
    }
    
    let x = -1.0 + f32(instanceIndex) * barWidth + localPos.x * barWidth * 0.9;
    let y = -1.0 + localPos.y * barHeight * 1.8;
    
    output.position = uniforms.viewProjection * vec4<f32>(x, y, 0.0, 1.0);
    
    // Color based on probability (quantum gradient)
    let hue = f32(instanceIndex) / f32(uniforms.numBars) * 360.0;
    let saturation = 0.8;
    let lightness = 0.5 + barHeight * 0.3;
    output.color = hslToRgb(hue, saturation, lightness);
    
    return output;
}

fn hslToRgb(h: f32, s: f32, l: f32) -> vec4<f32> {
    let c = (1.0 - abs(2.0 * l - 1.0)) * s;
    let x = c * (1.0 - abs(fract(h / 60.0) * 2.0 - 1.0));
    let m = l - c / 2.0;
    
    var rgb: vec3<f32>;
    let hi = u32(h / 60.0) % 6u;
    
    switch (hi) {
        case 0u: { rgb = vec3<f32>(c, x, 0.0); }
        case 1u: { rgb = vec3<f32>(x, c, 0.0); }
        case 2u: { rgb = vec3<f32>(0.0, c, x); }
        case 3u: { rgb = vec3<f32>(0.0, x, c); }
        case 4u: { rgb = vec3<f32>(x, 0.0, c); }
        case 5u: { rgb = vec3<f32>(c, 0.0, x); }
        default: { rgb = vec3<f32>(0.0, 0.0, 0.0); }
    }
    
    return vec4<f32>(rgb + m, 1.0);
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4<f32> {
    return input.color;
}
`;

// Shader for Bloch sphere rendering
const BLOCH_SPHERE_SHADER = `
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) normal: vec3<f32>,
    @location(1) worldPos: vec3<f32>,
};

struct BlochUniforms {
    viewProjection: mat4x4<f32>,
    model: mat4x4<f32>,
    theta: f32,
    phi: f32,
    time: f32,
    padding: f32,
};

@group(0) @binding(0) var<uniform> uniforms: BlochUniforms;

@vertex
fn vertexMain(
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>
) -> VertexOutput {
    var output: VertexOutput;
    
    let worldPos = uniforms.model * vec4<f32>(position, 1.0);
    output.position = uniforms.viewProjection * worldPos;
    output.normal = normalize((uniforms.model * vec4<f32>(normal, 0.0)).xyz);
    output.worldPos = worldPos.xyz;
    
    return output;
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4<f32> {
    let lightDir = normalize(vec3<f32>(1.0, 1.0, 1.0));
    let ambient = 0.2;
    let diffuse = max(dot(input.normal, lightDir), 0.0) * 0.6;
    let specular = pow(max(dot(reflect(-lightDir, input.normal), normalize(-input.worldPos)), 0.0), 32.0) * 0.4;
    
    let baseColor = vec3<f32>(0.3, 0.6, 1.0);
    let finalColor = baseColor * (ambient + diffuse) + vec3<f32>(1.0) * specular;
    
    return vec4<f32>(finalColor, 0.7);
}
`;

// State vector amplitude shader (complex plane visualization)
const AMPLITUDE_SHADER = `
struct AmplitudeUniforms {
    viewProjection: mat4x4<f32>,
    time: f32,
    numStates: u32,
    padding: vec2<f32>,
};

struct ComplexAmplitude {
    real: f32,
    imag: f32,
};

@group(0) @binding(0) var<uniform> uniforms: AmplitudeUniforms;
@group(0) @binding(1) var<storage, read> amplitudes: array<ComplexAmplitude>;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>,
};

@vertex
fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var output: VertexOutput;
    
    let stateIndex = vertexIndex / 2u;
    let isEnd = (vertexIndex % 2u) == 1u;
    
    let amp = amplitudes[stateIndex];
    let magnitude = sqrt(amp.real * amp.real + amp.imag * amp.imag);
    let phase = atan2(amp.imag, amp.real);
    
    // Map phase to hue
    let hue = (phase + 3.14159) / (2.0 * 3.14159) * 360.0;
    
    var pos: vec2<f32>;
    if (isEnd) {
        pos = vec2<f32>(amp.real, amp.imag);
    } else {
        pos = vec2<f32>(0.0, 0.0);
    }
    
    // Offset by state index
    let offset = vec2<f32>(
        -1.0 + (f32(stateIndex) + 0.5) * 2.0 / f32(uniforms.numStates),
        0.0
    );
    
    output.position = uniforms.viewProjection * vec4<f32>(pos * 0.8 + offset, 0.0, 1.0);
    output.color = vec4<f32>(
        cos(phase) * 0.5 + 0.5,
        magnitude,
        sin(phase) * 0.5 + 0.5,
        1.0
    );
    
    return output;
}

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4<f32> {
    return input.color;
}
`;

export class WebGPUQuantumRendererImpl implements WebGPUQuantumRenderer {
    private device: GPUDevice | null = null;
    private context: GPUCanvasContext | null = null;
    private canvas: HTMLCanvasElement | null = null;

    private probabilityPipeline: GPURenderPipeline | null = null;
    private amplitudePipeline: GPURenderPipeline | null = null;
    private blochPipeline: GPURenderPipeline | null = null;

    private probabilityBuffer: GPUBuffer | null = null;
    private amplitudeBuffer: GPUBuffer | null = null;
    private uniformBuffer: GPUBuffer | null = null;

    constructor(canvas: HTMLCanvasElement) {
        this.canvas = canvas;
    }

    async initWebGPU(): Promise<boolean> {
        if (!navigator.gpu) {
            console.error('WebGPU not supported');
            return false;
        }

        const adapter = await navigator.gpu.requestAdapter();
        if (!adapter) {
            console.error('Failed to get GPU adapter');
            return false;
        }

        this.device = await adapter.requestDevice();
        this.context = this.canvas!.getContext('webgpu');

        if (!this.context) {
            console.error('Failed to get WebGPU context');
            return false;
        }

        const format = navigator.gpu.getPreferredCanvasFormat();
        this.context.configure({
            device: this.device,
            format: format,
            alphaMode: 'premultiplied',
        });

        // Create pipelines
        await this.createPipelines(format);

        console.log('🚀 WebGPU initialized for quantum visualization');
        return true;
    }

    private async createPipelines(format: GPUTextureFormat): Promise<void> {
        // Create probability pipeline
        const probabilityModule = this.device!.createShaderModule({
            code: PROBABILITY_SHADER
        });

        this.probabilityPipeline = this.device!.createRenderPipeline({
            layout: 'auto',
            vertex: {
                module: probabilityModule,
                entryPoint: 'vertexMain',
            },
            fragment: {
                module: probabilityModule,
                entryPoint: 'fragmentMain',
                targets: [{ format }],
            },
            primitive: {
                topology: 'triangle-list',
            },
        });

        // Create buffers
        this.uniformBuffer = this.device!.createBuffer({
            size: 256,
            usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
        });

        this.probabilityBuffer = this.device!.createBuffer({
            size: 1024 * 4, // Up to 1024 states
            usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
        });

        this.amplitudeBuffer = this.device!.createBuffer({
            size: 1024 * 8, // Complex numbers (2 floats each)
            usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
        });
    }

    renderStateVector(amplitudes: Complex[]): void {
        if (!this.device || !this.context || !this.amplitudePipeline) return;

        // Update amplitude buffer
        const data = new Float32Array(amplitudes.length * 2);
        amplitudes.forEach((a, i) => {
            data[i * 2] = a.real;
            data[i * 2 + 1] = a.imag;
        });
        this.device.queue.writeBuffer(this.amplitudeBuffer!, 0, data);

        // Render...
    }

    renderProbabilities(probabilities: number[]): void {
        if (!this.device || !this.context || !this.probabilityPipeline) return;

        // Update probability buffer
        const data = new Float32Array(probabilities);
        this.device.queue.writeBuffer(this.probabilityBuffer!, 0, data);

        // Update uniforms
        const maxProb = Math.max(...probabilities);
        const uniformData = new Float32Array([
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1,
            performance.now() / 1000,
            probabilities.length,
            maxProb,
            0
        ]);
        this.device.queue.writeBuffer(this.uniformBuffer!, 0, uniformData);

        // Render
        const encoder = this.device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
            colorAttachments: [{
                view: this.context.getCurrentTexture().createView(),
                loadOp: 'clear',
                storeOp: 'store',
                clearValue: { r: 0.05, g: 0.05, b: 0.1, a: 1 },
            }],
        });

        pass.setPipeline(this.probabilityPipeline);
        pass.draw(6, probabilities.length);
        pass.end();

        this.device.queue.submit([encoder.finish()]);
    }

    renderBlochSphere(theta: number, phi: number): void {
        // Render Bloch sphere with state vector
        console.log(`Rendering Bloch sphere: θ=${theta.toFixed(3)}, φ=${phi.toFixed(3)}`);
    }

    destroy(): void {
        this.probabilityBuffer?.destroy();
        this.amplitudeBuffer?.destroy();
        this.uniformBuffer?.destroy();
        this.device?.destroy();
    }
}
