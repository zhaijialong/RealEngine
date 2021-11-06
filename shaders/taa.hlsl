#include "taa_constants.hlsli"
#include "common.hlsli"

cbuffer genrateVelocityCB : register(b0)
{
    uint c_depthRT;
    uint c_velocityRT;
    uint c_outputVelocityRT;
};

[numthreads(8, 8, 1)]
void generate_velocity_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 pos = dispatchThreadID.xy;
    
    Texture2D velocityRT = ResourceDescriptorHeap[c_velocityRT];
    RWTexture2D<float4> outputRT = ResourceDescriptorHeap[c_outputVelocityRT];
    
    float4 velocity = velocityRT[pos];
    if (abs(velocity.x) > 0.001 || abs(velocity.y) > 0.001)
    {
        outputRT[pos] = velocity;
    }
    else
    {
        Texture2D depthRT = ResourceDescriptorHeap[c_depthRT];
        float depth = depthRT[pos].x;
    
        float2 screenPos = ((float2)pos + 0.5);
        float4 clipPos = float4(GetNdcPosition(screenPos), depth, 1.0);
    
        float4 prevClipPos = mul(CameraCB.mtxClipToPrevClipNoJitter, clipPos);
        float3 prevNdcPos = GetNdcPos(prevClipPos);
        float2 prevScreenPos = GetScreenPosition(prevNdcPos.xy);
    
        float2 motion = prevScreenPos.xy - screenPos.xy;
        
        float linearZ = GetLinearDepth(depth);
        float prevLinearZ = GetLinearDepth(prevNdcPos.z);
        
        // reduce 16bit precision issues - push the older frame ever so slightly into foreground
        prevLinearZ *= 0.999;
        
        float deltaZ = prevLinearZ - linearZ;
        
        outputRT[pos] = float4(motion, deltaZ, 0);
    }
}

float3 Tonemap_ACES(float3 x)
{
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

float3 InverseTonemap_ACES(float3 x)
{
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (-d * x + b - sqrt(-1.0127 * x * x + 1.3702 * x + 0.0009)) / (2.0 * (c * x - a));
}

#include "IntelTAA.hlsli"

[numthreads(INTEL_TAA_NUM_THREADS_X, INTEL_TAA_NUM_THREADS_Y, 1)]
void main(uint3 inDispatchIdx : SV_DispatchThreadID, uint3 inGroupID : SV_GroupID, uint3 inGroupThreadID : SV_GroupThreadID)
{
    TAAParams params;
    params.CBData = taaCB.consts;
    params.VelocityBuffer = ResourceDescriptorHeap[taaCB.velocityRT];
    params.ColourTexture = ResourceDescriptorHeap[taaCB.colorRT];
    params.HistoryTexture = ResourceDescriptorHeap[taaCB.historyRT];
    params.DepthBuffer = ResourceDescriptorHeap[taaCB.depthRT];
    params.PrvDepthBuffer = ResourceDescriptorHeap[taaCB.prevDepthRT];
    params.OutTexture = ResourceDescriptorHeap[taaCB.outTexture];
    params.MinMagLinearMipPointClamp = SamplerDescriptorHeap[SceneCB.linearClampSampler];
    params.MinMagMipPointClamp = SamplerDescriptorHeap[SceneCB.pointClampSampler];

    IntelTAA(inDispatchIdx, inGroupID, inGroupThreadID, params);
}

cbuffer applyCB : register(b0)
{
    uint c_inputRT;
    uint c_outputRT;
    uint c_historyRT;
};

[numthreads(8, 8, 1)]
void apply_main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D inputTexture = ResourceDescriptorHeap[c_inputRT];
    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_outputRT];
    RWTexture2D<float4> historyTexture = ResourceDescriptorHeap[c_historyRT];
    
    float4 taaColor = inputTexture[dispatchThreadID.xy];
    
    outTexture[dispatchThreadID.xy] = float4(TAAInverseTonemap(taaColor.rgb), 1);
    historyTexture[dispatchThreadID.xy] = taaColor;
}