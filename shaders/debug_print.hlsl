#include "common.hlsli"
#include "debug.hlsli"

#define GROUP_SIZE (32)

cbuffer RootConstants : register(b0)
{
    uint c_drawCommandBufferUAV;
    uint c_debugTextCounterBufferSRV;
    uint c_debugTextBufferSRV;
    uint c_fontTexture;
};

[numthreads(1, 1, 1)]
void build_command()
{
    ByteAddressBuffer debugTextCounterBuffer = ResourceDescriptorHeap[c_debugTextCounterBufferSRV];
    RWByteAddressBuffer drawCommandBuffer = ResourceDescriptorHeap[c_drawCommandBufferUAV];

    uint text_count = debugTextCounterBuffer.Load(0);
    uint group_count = (text_count + GROUP_SIZE - 1) / GROUP_SIZE;
    
    drawCommandBuffer.Store3(0, uint3(group_count, 1, 1));
}



struct VertexOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 color : COLOR;
};

#define MAX_VERTEX_COUNT (GROUP_SIZE * 4)
#define MAX_PRIMITIVE_COUNT (GROUP_SIZE * 2)

[numthreads(GROUP_SIZE, 1, 1)]
[outputtopology("triangle")]
void main_ms(
    uint3 dispatchThreadID : SV_DispatchThreadID,
    uint groupID : SV_GroupID,
    uint groupIndex : SV_GroupIndex,
    out indices uint3 indices[MAX_PRIMITIVE_COUNT],
    out vertices VertexOut vertices[MAX_VERTEX_COUNT])
{
    ByteAddressBuffer debugTextCounterBuffer = ResourceDescriptorHeap[c_debugTextCounterBufferSRV];
    uint text_count = debugTextCounterBuffer.Load(0);
 
#if 0
    uint text_index = dispatchThreadID.x; //amd crashes
#else
    uint text_index = groupID * GROUP_SIZE + groupIndex;
#endif
    bool valid_text = text_index < text_count;
    
    uint valid_text_count = WaveActiveCountBits(valid_text);
    SetMeshOutputCounts(valid_text_count * 4, valid_text_count * 2);
    
    if (groupIndex < valid_text_count)
    {
        StructuredBuffer<debug::Text> textBuffer = ResourceDescriptorHeap[c_debugTextBufferSRV];
        StructuredBuffer<debug::BakedChar> bakedCharBuffer = ResourceDescriptorHeap[SceneCB.debugFontCharBufferSRV];
        
        debug::Text text = textBuffer[text_index];
        debug::BakedChar backedChar = bakedCharBuffer[text.text];
        
        uint v0 = groupIndex * 4 + 0;
        uint v1 = groupIndex * 4 + 1;
        uint v2 = groupIndex * 4 + 2;
        uint v3 = groupIndex * 4 + 3;
        
        uint primitive0 = groupIndex * 2;
        uint primitive1 = groupIndex * 2 + 1;
        
        float2 pos = text.screenPosition;
        uint16_t4 charBox = unpack_u8u16((uint8_t4_packed)backedChar.bbox);
        
        //basically gpu version of stbtt_GetBakedQuad
        int round_x = floor((pos.x + backedChar.xoff) + 0.5f);
        int round_y = floor((pos.y + backedChar.yoff) + 0.5f);
        
        float x0 = round_x;
        float y0 = round_y;
        float x1 = round_x + charBox.z - charBox.x;
        float y1 = round_y + charBox.w - charBox.y;
    
        float width = 0;
        float height = 0;
        Texture2D<float> fontTexture = ResourceDescriptorHeap[c_fontTexture];
        fontTexture.GetDimensions(width, height);
    
        float s0 = charBox.x / width;
        float t0 = charBox.y / height;
        float s1 = charBox.z / width;
        float t1 = charBox.w / height;

        vertices[v0].pos = float4(GetNdcPosition(float2(x0, y0), SceneCB.rcpDisplaySize), 0.0, 1.0);
        vertices[v1].pos = float4(GetNdcPosition(float2(x0, y1), SceneCB.rcpDisplaySize), 0.0, 1.0);
        vertices[v2].pos = float4(GetNdcPosition(float2(x1, y0), SceneCB.rcpDisplaySize), 0.0, 1.0);
        vertices[v3].pos = float4(GetNdcPosition(float2(x1, y1), SceneCB.rcpDisplaySize), 0.0, 1.0);
        
        vertices[v0].uv = float2(s0, t0);
        vertices[v1].uv = float2(s0, t1);
        vertices[v2].uv = float2(s1, t0);
        vertices[v3].uv = float2(s1, t1);
    
        float3 color = UnpackRGBA8Unorm(text.color).xyz;
    
        vertices[v0].color = color;
        vertices[v1].color = color;
        vertices[v2].color = color;
        vertices[v3].color = color;
    
        indices[primitive0] = uint3(groupIndex * 4, groupIndex * 4 + 1, groupIndex * 4 + 2);
        indices[primitive1] = uint3(groupIndex * 4 + 1, groupIndex * 4 + 2, groupIndex * 4 + 3);
    }
}

float4 main_ps(VertexOut input) : SV_Target
{
    Texture2D<float> fontTexture = ResourceDescriptorHeap[c_fontTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    
    return float4(input.color, fontTexture.SampleLevel(linearSampler, input.uv, 0).x);
}