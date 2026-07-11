const full = Deno.args.includes("--full");
for (const arg of Deno.args) {
  if (arg !== "--full" && arg !== "--") {
    throw new Error(`Unknown argument: ${arg}`);
  }
}

const gpu = navigator.gpu;

console.log("WebGPU device information\n");

const adapter = await gpu.requestAdapter({
  powerPreference: "high-performance",
});
if (!adapter) throw new Error("No WebGPU adapter found");

const info = adapter.info;
const features = [...adapter.features].sort();
const languageFeatures = [
  ...(gpu as GPU & { wgslLanguageFeatures: ReadonlySet<string> })
    .wgslLanguageFeatures,
].sort();

console.log(`Description:       ${info.description || "unknown"}`);
console.log(`Vendor:            ${info.vendor || "unknown"}`);
console.log(`Architecture:      ${info.architecture || "unknown"}`);
console.log(`Device:            ${info.device || "unknown"}`);
console.log(`Fallback adapter:  ${info.isFallbackAdapter}`);
console.log(
  `Subgroup size:     ${info.subgroupMinSize}–${info.subgroupMaxSize}`,
);

console.log("\nCompute limits");
const limits = adapter.limits;
console.log(
  `Max invocations/workgroup:  ${limits.maxComputeInvocationsPerWorkgroup}`,
);
console.log(
  `Max workgroup size:         ${limits.maxComputeWorkgroupSizeX} × ${limits.maxComputeWorkgroupSizeY} × ${limits.maxComputeWorkgroupSizeZ}`,
);
console.log(
  `Max workgroup storage:      ${limits.maxComputeWorkgroupStorageSize} bytes`,
);
console.log(
  `Max storage buffer binding: ${limits.maxStorageBufferBindingSize} bytes`,
);
console.log(`Max buffer size:            ${limits.maxBufferSize} bytes`);
console.log(
  `Max workgroups/dimension:   ${limits.maxComputeWorkgroupsPerDimension}`,
);

if (full) {
  const additionalLimits: Array<[string, number]> = [
    ["Max texture dimension 1D", limits.maxTextureDimension1D],
    ["Max texture dimension 2D", limits.maxTextureDimension2D],
    ["Max texture dimension 3D", limits.maxTextureDimension3D],
    ["Max texture array layers", limits.maxTextureArrayLayers],
    ["Max bind groups", limits.maxBindGroups],
    ["Max bindings/bind group", limits.maxBindingsPerBindGroup],
    [
      "Max dynamic uniform buffers/pipeline layout",
      limits.maxDynamicUniformBuffersPerPipelineLayout,
    ],
    [
      "Max dynamic storage buffers/pipeline layout",
      limits.maxDynamicStorageBuffersPerPipelineLayout,
    ],
    [
      "Max sampled textures/shader stage",
      limits.maxSampledTexturesPerShaderStage,
    ],
    ["Max samplers/shader stage", limits.maxSamplersPerShaderStage],
    [
      "Max storage buffers/shader stage",
      limits.maxStorageBuffersPerShaderStage,
    ],
    [
      "Max storage textures/shader stage",
      limits.maxStorageTexturesPerShaderStage,
    ],
    [
      "Max uniform buffers/shader stage",
      limits.maxUniformBuffersPerShaderStage,
    ],
    ["Max uniform buffer binding size", limits.maxUniformBufferBindingSize],
    [
      "Min uniform buffer offset alignment",
      limits.minUniformBufferOffsetAlignment,
    ],
    [
      "Min storage buffer offset alignment",
      limits.minStorageBufferOffsetAlignment,
    ],
    ["Max vertex buffers", limits.maxVertexBuffers],
    ["Max vertex attributes", limits.maxVertexAttributes],
    ["Max vertex buffer array stride", limits.maxVertexBufferArrayStride],
    ["Max inter-stage shader variables", limits.maxInterStageShaderVariables],
    ["Max color attachments", limits.maxColorAttachments],
    [
      "Max color attachment bytes/sample",
      limits.maxColorAttachmentBytesPerSample,
    ],
  ];

  console.log("\nAdditional limits");
  const labelWidth = Math.max(
    ...additionalLimits.map(([label]) => label.length),
  );
  for (const [label, value] of additionalLimits) {
    console.log(`${label.padEnd(labelWidth)}  ${value}`);
  }
  console.log(`\nPreferred canvas format: ${gpu.getPreferredCanvasFormat()}`);
}

console.log("\nAdapter features");
console.log(features.length === 0 ? "(none)" : features.join("\n"));

console.log("\nWGSL language features");
console.log(
  languageFeatures.length === 0 ? "(none)" : languageFeatures.join("\n"),
);
