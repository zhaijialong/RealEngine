#include "common.hlsli"
#include "gpu_scene.hlsli"

cbuffer CB : register(b0)
{
    uint c_spriteCount;
    uint c_spriteBufferAddress;
}

struct Sprite
{
    float3 position;
    float size;
    
    uint color;
    uint texture;
    uint objectID;
    uint _pad;
};

struct VertexOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
#if OBJECT_ID_PASS
    nointerpolation uint objectID : OBJECT_ID;
#else
    float4 color : COLOR;
#endif
    nointerpolation uint textureIndex : TEXTURE_INDEX;
};

#define GROUP_SIZE (32)
#define MAX_VERTEX_COUNT (GROUP_SIZE * 4)
#define MAX_PRIMITIVE_COUNT (GROUP_SIZE * 2)

[numthreads(GROUP_SIZE, 1, 1)]
[outputtopology("triangle")]
void ms_main(uint3 dispatchThreadID : SV_DispatchThreadID,
    uint groupIndex : SV_GroupIndex,
    out indices uint3 indices[MAX_PRIMITIVE_COUNT],
    out vertices VertexOut vertices[MAX_VERTEX_COUNT])
{
    uint spriteIndex = dispatchThreadID.x;
    bool validSprite = spriteIndex < c_spriteCount;
    
    uint validSpriteCount = WaveActiveCountBits(validSprite);
    SetMeshOutputCounts(validSpriteCount * 4, validSpriteCount * 2);
    
    if (groupIndex < validSpriteCount)
    {
        uint v0 = groupIndex * 4 + 0;
        uint v1 = groupIndex * 4 + 1;
        uint v2 = groupIndex * 4 + 2;
        uint v3 = groupIndex * 4 + 3;
        
        uint primitive0 = groupIndex * 2;
        uint primitive1 = groupIndex * 2 + 1;
        
        Sprite sprite = LoadSceneConstantBuffer<Sprite>(c_spriteBufferAddress + sizeof(Sprite) * spriteIndex);
        
        float4 clipPos = mul(GetCameraCB().mtxViewProjectionNoJitter, float4(sprite.position, 1.0));
        
        float x0 = clipPos.x - SceneCB.rcpDisplaySize.x * clipPos.w * sprite.size;
        float y0 = clipPos.y + SceneCB.rcpDisplaySize.y * clipPos.w * sprite.size;
        float x1 = clipPos.x + SceneCB.rcpDisplaySize.x * clipPos.w * sprite.size;
        float y1 = clipPos.y - SceneCB.rcpDisplaySize.y * clipPos.w * sprite.size;
        
        vertices[v0].pos = float4(x0, y0, clipPos.zw);
        vertices[v1].pos = float4(x0, y1, clipPos.zw);
        vertices[v2].pos = float4(x1, y0, clipPos.zw);
        vertices[v3].pos = float4(x1, y1, clipPos.zw);
        
        vertices[v0].uv = float2(0, 0);
        vertices[v1].uv = float2(0, 1);
        vertices[v2].uv = float2(1, 0);
        vertices[v3].uv = float2(1, 1);
        
#if OBJECT_ID_PASS
        vertices[v0].objectID = sprite.objectID;
        vertices[v1].objectID = sprite.objectID;
        vertices[v2].objectID = sprite.objectID;
        vertices[v3].objectID = sprite.objectID;
#else
        float4 color = UnpackRGBA8Unorm(sprite.color);
        vertices[v0].color = color;
        vertices[v1].color = color;
        vertices[v2].color = color;
        vertices[v3].color = color;
#endif
        
        vertices[v0].textureIndex = sprite.texture;
        vertices[v1].textureIndex = sprite.texture;
        vertices[v2].textureIndex = sprite.texture;
        vertices[v3].textureIndex = sprite.texture;
        
        indices[primitive0] = uint3(groupIndex * 4, groupIndex * 4 + 1, groupIndex * 4 + 2);
        indices[primitive1] = uint3(groupIndex * 4 + 1, groupIndex * 4 + 2, groupIndex * 4 + 3);
    }
}

#if OBJECT_ID_PASS
uint ps_main(VertexOut input) : SV_Target
{
    Texture2D texture = ResourceDescriptorHeap[NonUniformResourceIndex(input.textureIndex)];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

    float4 color = texture.SampleLevel(linearSampler, input.uv, 0);
    return color.a > 0 ? input.objectID : 0;
}
#else
float4 ps_main(VertexOut input) : SV_Target
{
    Texture2D texture = ResourceDescriptorHeap[NonUniformResourceIndex(input.textureIndex)];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    
    float4 color = texture.SampleLevel(linearSampler, input.uv, 0);
    color *= input.color;
    
    return color;
}
#endif