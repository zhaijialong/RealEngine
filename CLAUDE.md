# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
cmake -B build
cmake --build build --config Release
```

Requires Windows with a DX12 Ultimate GPU (or macOS for Metal). No test or lint infrastructure exists.

## Architecture

The engine has three main layers:

**GFX Layer** (`source/gfx/`) — Graphics API abstraction. All objects are created through `IGfxDevice` and accessed via interfaces (`IGfxBuffer`, `IGfxTexture`, `IGfxCommandList`, etc.). Backend implementations live in `d3d12/`, `vulkan/`, `metal/`, and `mock/` subdirectories. Resources are fully bindless via SM6.6 — accessed by GPU address, not descriptor tables.

**Renderer** (`source/renderer/`) — Built on top of the GFX layer. The central class is `Renderer`, which owns the `RenderGraph` and all render passes.

- `RenderGraph` — DAG-based frame graph. Passes are added with `AddPass<Data>()` taking a setup lambda (declares resource reads/writes via `RGBuilder`) and an execute lambda (records GPU commands). Handles automatic barrier insertion, transient resource aliasing, and async compute scheduling.
- `GpuScene` — Manages GPU-side scene data: instance buffer, material buffer, animation buffer, TLAS for ray tracing.
- `BasePass` — GPU-driven two-phase occlusion culling (instance cull → meshlet cull), always 2 indirect dispatches per PSO regardless of object count.
- Lighting passes in `lighting/`: direct lighting, GTAO, diffuse/specular GI, ReSTIR.
- Post-processing in `post_processing/`: TAA, bloom, DOF, motion blur, upscaling (FSR2/DLSS/XeSS).

**World** (`source/world/`) — Scene objects: camera, lights, static/skeletal meshes, physics (Jolt). World objects upload their data to `GpuScene` each frame.

## Key Patterns

**Adding a render pass:** Create a struct for pass data, call `m_pRenderGraph->AddPass<Data>()` in the renderer, declare resource usage in the setup lambda, record commands in the execute lambda.

**GFX resource creation:** All resources created via `IGfxDevice` methods (e.g., `CreateBuffer`, `CreateTexture`, `CreateShader`). Descriptors created separately via `CreateShaderResourceView` etc.

**Shader compilation:** Shaders are HLSL, compiled via DXC. Shader objects created with `IGfxDevice::CreateShader()` specifying entry point and profile.
