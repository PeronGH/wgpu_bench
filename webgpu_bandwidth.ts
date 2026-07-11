const MIB = 1024 * 1024;

function positiveInteger(
  value: string | undefined,
  fallback: number,
  name: string,
): number {
  if (value === undefined) return fallback;
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed) || parsed <= 0) {
    throw new Error(`${name} must be a positive integer`);
  }
  return parsed;
}

const requestedSize = positiveInteger(Deno.args[0], 128, "sizeMiB") * MIB;
const iterations = positiveInteger(Deno.args[1], 20, "iterations");
const bursts = positiveInteger(Deno.args[2], 10, "bursts");
const adapter = await navigator.gpu.requestAdapter({
  powerPreference: "high-performance",
});
if (!adapter) throw new Error("No WebGPU adapter found");

const device = await adapter.requestDevice();
device.addEventListener("uncapturederror", (event) => {
  console.error(
    "WebGPU error:",
    (event as GPUUncapturedErrorEvent).error.message,
  );
});

const maxBindingSize = Number(adapter.limits.maxStorageBufferBindingSize);
const size = Math.min(requestedSize, maxBindingSize);
const bytesPerInvocation = 16;
const workgroupSize = 256;
const bytesPerWorkgroup = bytesPerInvocation * workgroupSize;
const alignedSize = Math.floor(size / bytesPerWorkgroup) * bytesPerWorkgroup;
if (alignedSize === 0) throw new Error("Requested buffer is too small");

const shader = device.createShaderModule({
  code: `
    @group(0) @binding(0) var<storage, read> source: array<vec4<u32>>;
    @group(0) @binding(1) var<storage, read_write> destination: array<vec4<u32>>;

    @compute @workgroup_size(${workgroupSize})
    fn copy(@builtin(global_invocation_id) id: vec3<u32>) {
      destination[id.x] = source[id.x];
    }
  `,
});

const compilation = await shader.getCompilationInfo();
for (const message of compilation.messages) {
  if (message.type === "error") throw new Error(`WGSL: ${message.message}`);
}

const pipeline = device.createComputePipeline({
  layout: "auto",
  compute: { module: shader, entryPoint: "copy" },
});
const source = device.createBuffer({
  size: alignedSize,
  usage: GPUBufferUsage.STORAGE,
});
const destination = device.createBuffer({
  size: alignedSize,
  usage: GPUBufferUsage.STORAGE,
});
const bindGroup = device.createBindGroup({
  layout: pipeline.getBindGroupLayout(0),
  entries: [
    { binding: 0, resource: { buffer: source } },
    { binding: 1, resource: { buffer: destination } },
  ],
});
const workgroups = alignedSize / bytesPerWorkgroup;

function encodeCopies(count: number): GPUCommandBuffer {
  const encoder = device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bindGroup);
  for (let i = 0; i < count; i++) pass.dispatchWorkgroups(workgroups);
  pass.end();
  return encoder.finish();
}

// Warm up pipeline creation, shader execution, and GPU clocks before measuring.
device.queue.submit([encodeCopies(3)]);
await device.queue.onSubmittedWorkDone();

const commands = Array.from({ length: bursts }, () => encodeCopies(iterations));
const transferredBytes = alignedSize * iterations * 2; // one read plus one write
const results: { elapsedMs: number; bandwidthGBs: number }[] = [];

for (const command of commands) {
  const started = performance.now();
  device.queue.submit([command]);
  await device.queue.onSubmittedWorkDone();
  const elapsedMs = performance.now() - started;
  results.push({
    elapsedMs,
    bandwidthGBs: transferredBytes / (elapsedMs * 1e6),
  });
}

const fastest = results.reduce((best, result) =>
  result.bandwidthGBs > best.bandwidthGBs ? result : best
);
const sortedBandwidths = results.map((result) => result.bandwidthGBs).sort((
  a,
  b,
) => a - b);
const medianBandwidth =
  sortedBandwidths[Math.floor(sortedBandwidths.length / 2)];

console.log(`Adapter:      ${adapter.info.description}`);
console.log(`Buffer size:  ${(alignedSize / MIB).toFixed(1)} MiB × 2`);
console.log(`Iterations:   ${iterations} per burst`);
console.log(`Bursts:       ${bursts}`);
console.log(`Best elapsed: ${fastest.elapsedMs.toFixed(2)} ms`);
console.log(`Median:       ${medianBandwidth.toFixed(2)} GB/s`);
console.log(
  `Peak:         ${fastest.bandwidthGBs.toFixed(2)} GB/s (read + write)`,
);

source.destroy();
destination.destroy();
device.destroy();
