cbuffer CB : register(b0)
{
    uint c_resourceUAV;
}

cbuffer FloatValueCB : register(b1)
{
    float4 c_clearValueFloat;
}

cbuffer UintValueCB : register(b1)
{
    uint4 c_clearValueUint;
}

#if defined(UAV_TYPE_FLOAT)
    #define UAV_TYPE float
    #define CLEAR_VALUE c_clearValueFloat.x
#elif defined(UAV_TYPE_FLOAT2)
    #define UAV_TYPE float2
    #define CLEAR_VALUE c_clearValueFloat.xy
#elif defined(UAV_TYPE_FLOAT3)
    #define UAV_TYPE float3
    #define CLEAR_VALUE c_clearValueFloat.xyz
#elif defined(UAV_TYPE_FLOAT4)
    #define UAV_TYPE float4
    #define CLEAR_VALUE c_clearValueFloat.xyzw
#elif defined(UAV_TYPE_UINT)
    #define UAV_TYPE uint
    #define CLEAR_VALUE c_clearValueUint.x
#elif defined(UAV_TYPE_UINT2)
    #define UAV_TYPE uint2
    #define CLEAR_VALUE c_clearValueUint.xy
#elif defined(UAV_TYPE_UINT3)
    #define UAV_TYPE uint3
    #define CLEAR_VALUE c_clearValueUint.xyz
#elif defined(UAV_TYPE_UINT4)
    #define UAV_TYPE uint4
    #define CLEAR_VALUE c_clearValueUint.xyzw
#else
    #error "unsupported uav type"
#endif

[numthreads(8, 8, 1)]
void clear_texture2d(uint2 dispatchThreadID : SV_DispatchThreadID)
{
    RWTexture2D<UAV_TYPE> uav = ResourceDescriptorHeap[c_resourceUAV];
    uav[dispatchThreadID] = CLEAR_VALUE;
}

[numthreads(8, 8, 1)]
void clear_texture2d_array(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWTexture2DArray<UAV_TYPE> uav = ResourceDescriptorHeap[c_resourceUAV];
    uav[dispatchThreadID] = CLEAR_VALUE;    
}

[numthreads(8, 8, 8)]
void clear_texture3d(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    RWTexture3D<UAV_TYPE> uav = ResourceDescriptorHeap[c_resourceUAV];
    uav[dispatchThreadID] = CLEAR_VALUE;
}

[numthreads(64, 1, 1)]
void clear_typed_buffer(uint dispatchThreadID : SV_DispatchThreadID)
{
    RWBuffer<UAV_TYPE> uav = ResourceDescriptorHeap[c_resourceUAV];
    uav[dispatchThreadID] = CLEAR_VALUE;
}

[numthreads(64, 1, 1)]
void clear_raw_buffer(uint dispatchThreadID : SV_DispatchThreadID)
{
    RWByteAddressBuffer uav = ResourceDescriptorHeap[c_resourceUAV];
    uav.Store<UAV_TYPE>(sizeof(UAV_TYPE) * dispatchThreadID, CLEAR_VALUE);
}