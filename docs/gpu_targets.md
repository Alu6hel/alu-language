# Alu Enterprise: GPU Compiler Targets

The Alu Enterprise Edition features advanced LLVM backend integration for emitting native shader binaries. This allows you to leverage Alu's Hoare-logic proofs and mathematically verified state machine directly on the GPU for massive parallel processing.

## Supported Targets

Alu currently supports the two most prominent modern graphics and compute APIs:
- **Vulkan (SPIR-V)**: Industry standard cross-platform compute.
- **Apple Metal (AIR)**: High-performance compute on macOS and iOS devices.

## Building Shaders

You can instruct the Alu compiler to target a specific GPU architecture by passing the target flag during the build step:

### Building for Vulkan
```bash
alu_cxx --target-vulkan src/compute.alu
```
This generates a mathematically verified SPIR-V shader file (`compute.spv`), which you can load directly into a Vulkan `VkShaderModule`.

### Building for Apple Metal
```bash
alu_cxx --target-metal src/compute.alu
```
This leverages the macOS `xcrun metal` tools to generate an optimized Metal Library (`compute.metallib`). You can load this into an `MTLLibrary` to dispatch compute pipelines on iOS and macOS.

## Enterprise Architecture Note
Because Alu enforces strong pre/post-conditions, compiling for the GPU guarantees that there are no race conditions or out-of-bounds buffer reads in your compute kernel—before it ever touches the physical silicon. This translates to absolute stability in high-ticket render pipelines and data processing workloads.
