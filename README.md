# WebGPU and Vulkan Bandwidth Benchmark

A compute-shader benchmark for measuring GPU memory-copy bandwidth through
WebGPU and native Vulkan. Each shader invocation copies 16 bytes from a source
storage buffer to a destination storage buffer.

The reported bandwidth counts both sides of the copy: one read plus one write.
Results are shown in decimal GB/s.

## Implementations

- `webgpu_bandwidth.ts`: WebGPU benchmark for Deno
- `native/`: native Vulkan benchmark using GPU timestamp queries

Both implementations run multiple bursts and report the fastest observed result
along with the median. The Vulkan implementation excludes CPU submission and
waiting time by measuring directly on the GPU.

## Results

Test device:

| Component      | Value                                |
| -------------- | ------------------------------------ |
| GPU            | Qualcomm Adreno 840                  |
| Vulkan API     | 1.4.295                              |
| Vulkan driver  | Qualcomm proprietary 0.842.38        |
| Buffer size    | 128 MiB source + 128 MiB destination |
| Work per burst | 20 full-buffer copies                |
| Bursts         | 10                                   |

### WebGPU

```text
Adapter:      Adreno (TM) 840
Buffer size:  128.0 MiB × 2
Iterations:   20 per burst
Bursts:       10
Best elapsed: 86.63 ms
Median:       61.21 GB/s
Peak:         61.98 GB/s (read + write)
```

### Native Vulkan

```text
Adapter:      Adreno (TM) 840
Buffer size:  128.00 MiB x 2
Iterations:   20 per burst
Bursts:       10
Best elapsed: 73.63 ms
Median:       71.79 GB/s
Peak:         72.91 GB/s (read + write)
```

Native Vulkan reached approximately 18% higher peak bandwidth in these runs.
Results vary with GPU clocks, temperature, power mode, background activity, and
device memory configuration.

## Run the WebGPU benchmark

Requires Deno with WebGPU support:

```sh
deno task bench
```

Optional arguments specify buffer size in MiB, iterations per burst, and burst
count:

```sh
deno task bench -- 128 20 10
```

## Run the native Vulkan benchmark

Requires a C++20 compiler, CMake, Ninja, Vulkan headers and loader, and `glslc`:

```sh
cmake -S native -B native/build -G Ninja
cmake --build native/build
./native/build/vulkan_bandwidth
```

Optional arguments use the same order as the WebGPU benchmark:

```sh
./native/build/vulkan_bandwidth 128 20 10
```

See [`native/README.md`](native/README.md) for native-specific details.
