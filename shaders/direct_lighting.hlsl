#include "common.hlsli"
#include "local_light.hlsli"

cbuffer CB : register(b1)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_customDataRT;
    uint c_depthRT;
    uint c_shadowRT;
    uint c_outputRT;
};

static Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthRT];
static Texture2D diffuseRT = ResourceDescriptorHeap[c_diffuseRT];
static Texture2D specularRT = ResourceDescriptorHeap[c_specularRT];
static Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
static Texture2D customDataRT = ResourceDescriptorHeap[c_customDataRT];
static Texture2D<float> shadowRT = ResourceDescriptorHeap[c_shadowRT];
static RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_outputRT];

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 pos = dispatchThreadID.xy;
    
    float depth = depthRT[pos];
    if (depth == 0.0)
    {
        return;
    }
    
    float3 worldPos = GetWorldPosition(pos.xy, depth);
    float3 V = normalize(GetCameraCB().cameraPos - worldPos);
    
    float3 diffuse = diffuseRT[pos].xyz;
    float3 specular = specularRT[pos].xyz;
    ShadingModel shadingModel = DecodeShadingModel(specularRT[pos].w);
    float3 N = DecodeNormal(normalRT[pos].xyz);
    float roughness = normalRT[pos].w;
    float4 customData = customDataRT[pos];
    float visibility = shadowRT[pos.xy];

    float3 lighting = EvaluateBRDF(shadingModel, SceneCB.lightDir, V, N, diffuse, specular, roughness, customData, SceneCB.lightColor * visibility);
    
    //TODO : clustered light culling
    
    for (uint i = 0; i < SceneCB.localLightCount; ++i)
    {
        LocalLightData light = GetLocalLightData(i);
        
        lighting += CalculateLocalLight(light, worldPos, shadingModel, V, N, diffuse, specular, roughness, customData);
    }

    outTexture[pos] = float4(lighting, 1.0);
}