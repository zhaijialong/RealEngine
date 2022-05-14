cbuffer CB : register(b0)
{
    uint c_rayCounterBufferSRV;
    uint c_indirectArgsBufferUAV;
    uint c_denoiserArgsBufferUAV;
};

[numthreads(1, 1, 1)]
void main()
{
    Buffer<uint> rayCounterBufferSRV = ResourceDescriptorHeap[c_rayCounterBufferSRV];
    RWBuffer<uint> indirectArgsBufferUAV = ResourceDescriptorHeap[c_indirectArgsBufferUAV];
    RWBuffer<uint> denoiserArgsBufferUAV = ResourceDescriptorHeap[c_denoiserArgsBufferUAV];
    
    uint ray_count = rayCounterBufferSRV[0];
    uint tile_count = rayCounterBufferSRV[1];

    indirectArgsBufferUAV[0] = (ray_count + 63) / 64;
    indirectArgsBufferUAV[1] = 1;
    indirectArgsBufferUAV[2] = 1;
    
    denoiserArgsBufferUAV[0] = tile_count;
    denoiserArgsBufferUAV[1] = 1;
    denoiserArgsBufferUAV[2] = 1;
}