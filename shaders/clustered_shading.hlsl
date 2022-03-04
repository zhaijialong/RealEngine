#include "common.hlsli"

cbuffer CB : register(b1)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_depthRT;
    uint c_shadowRT;
    
    uint c_outputRT;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 pos = dispatchThreadID.xy;
    
    Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthRT];
    float depth = depthRT[pos];
    if (depth == 0.0)
    {
        return;
    }
    
    Texture2D diffuseRT = ResourceDescriptorHeap[c_diffuseRT];
    Texture2D specularRT = ResourceDescriptorHeap[c_specularRT];
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    
    float3 diffuse = diffuseRT[pos].xyz;
    float3 specular = specularRT[pos].xyz;    
    float3 N = OctNormalDecode(normalRT[pos].xyz);
    float roughness = normalRT[pos].w;

    float3 worldPos = GetWorldPosition(pos.xy, depth);
    float3 V = normalize(CameraCB.cameraPos - worldPos);

    //TODO : clustered light culling & shading
    
    Texture2D<float> shadowRT = ResourceDescriptorHeap[c_shadowRT];
    float visibility = shadowRT[pos.xy];
    float NdotL = saturate(dot(N, SceneCB.lightDir));
    float3 direct_light = BRDF(SceneCB.lightDir, V, N, diffuse, specular, roughness) * visibility * SceneCB.lightColor * NdotL;

    RWTexture2D<float4> outTexture = ResourceDescriptorHeap[c_outputRT];
    outTexture[pos] = float4(direct_light, 1.0);
}