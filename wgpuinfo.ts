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

console.log("\nAdapter features");
console.log(features.length === 0 ? "(none)" : features.join("\n"));

console.log("\nWGSL language features");
console.log(
  languageFeatures.length === 0 ? "(none)" : languageFeatures.join("\n"),
);
