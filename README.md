# RealEngine

My toy engine, mainly for rendering experiment and prototyping. 
It requires a GPU which supports DX12 Ultimate with latest driver.
It is not supposed to be used in production, and prefers clean design and code over being heavily optimized.
![screenshot](bin/screenshot.jpg)

## some highlights

* utilize latest DX12 features, such as raytracing, mesh shaders, HLSL 2021, etc.
* render graph based architecture, with automatic barriers and transient resources management
  ![rendergraph](bin/rendergraph.svg)
* fully bindless resources with SM6.6
* two-phase occlusion culling (like Ubi's GPU driven pipeline). no matter how many different meshes and textures, always two drawcalls(indirect DispatchMesh) per PSO.
* hybrid rendering with raytracing
* `Print`, `DrawLine` functions in shaders which can be very useful for debugging

## planned features

- [x] render graph(barriers, resource aliasing, async compute)
- [x] GTAO
- [x] specular GI
- [x] diffuse GI
- [x] clustered shading
- [x] Bloom
- [x] Auto Exposure
- [x] TAA
- [x] CAS
- [x] meshlet
- [x] RTX
- [x] a reference pathtracer
- [x] FSR2/DLSS/XeSS
- [x] DOF
- [x] motion blur
- [ ] visibility buffer
- [ ] Direct Storage
- [ ] maybe  volumetric cloud/fog, water/ocean simulation, large-scale landscape, or any random things

## third party licenses

* [DLSS](https://github.com/NVIDIA/DLSS/blob/main/LICENSE.txt)
* [XeSS](https://github.com/intel/xess/blob/main/licenses/LICENSE.pdf)
* [NRD](https://github.com/NVIDIAGameWorks/RayTracingDenoiser/blob/master/LICENSE.txt)