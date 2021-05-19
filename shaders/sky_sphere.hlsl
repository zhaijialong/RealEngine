struct SkySphereConstant
{
    float4x4 mtxWVP;
    float4x4 mtxWorld;
    float3 cameraPos;
    uint posBuffer;
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
    float3 V = normalize(SkySphereCB.cameraPos.xyz - input.worldPos);

    return float4(V, 1.0);
}