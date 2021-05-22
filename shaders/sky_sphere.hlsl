#include "atmosphere.hlsli"
#include "global_constants.hlsli"

struct SkySphereConstant
{
    float4x4 mtxWVP;
    float4x4 mtxWorld;
    uint posBuffer;
    float3 cameraPos;
};

ConstantBuffer<SkySphereConstant> SkySphereCB : register(b1);

struct VSOutput
{
    float4 pos : SV_POSITION;
    float3 worldPos : TEXCOORD;
};

VSOutput vs_main(uint vertex_id : SV_VertexID)
{
    StructuredBuffer<float3> posBuffer = ResourceDescriptorHeap[SkySphereCB.posBuffer];
    float4 position = float4(posBuffer[vertex_id], 1.0);
    
    VSOutput output;
    output.pos = mul(SkySphereCB.mtxWVP, position);
    output.worldPos = mul(SkySphereCB.mtxWorld, position).xyz;

    output.pos.z = 0.000001;

    return output;
}

float4 ps_main(VSOutput input) : SV_TARGET
{
    float3 rayStart = SkySphereCB.cameraPos;
    float3 rayDir = normalize(input.worldPos - SkySphereCB.cameraPos);
    float rayLength = INFINITY;

    bool PlanetShadow = false;
    if (PlanetShadow)
    {
        float2 planetIntersection = PlanetIntersection(rayStart, rayDir);
        if (planetIntersection.x > 0)
        {
            rayLength = min(rayLength, planetIntersection.x);
        }
    }

    float3 lightDir = SceneCB.lightDir;
    float3 lightColor = SceneCB.lightColor;

    float3 transmittance;
    float3 color = IntegrateScattering(rayStart, rayDir, rayLength, lightDir, lightColor, transmittance);

    return float4(color, 1);
}