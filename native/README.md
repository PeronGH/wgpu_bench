# Native Vulkan bandwidth benchmark

This is the native Vulkan equivalent of the WebGPU compute-copy benchmark. It
uses GPU timestamp queries, so command submission and CPU waiting are excluded
from the measured time.

## Build and run

```sh
cmake -S native -B native/build -G Ninja
cmake --build native/build
./native/build/vulkan_bandwidth
```

Arguments are buffer size in MiB, iterations per burst, and burst count:

```sh
./native/build/vulkan_bandwidth 128 20 10
```

The reported bandwidth counts one source read and one destination write. `Peak`
is the fastest burst; `Median` is included to show typical performance.
