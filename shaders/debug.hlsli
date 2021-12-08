#pragma once

#include "common.hlsli"

namespace debug
{
    struct LineVertex
    {
        float3 position;
        uint color;
    };

    void DrawLine(float3 start, float3 end, float3 color)
    {
        RWByteAddressBuffer argumentsBuffer = ResourceDescriptorHeap[SceneCB.debugLineDrawCommandUAV];
        RWStructuredBuffer<LineVertex> vertexBuffer = ResourceDescriptorHeap[SceneCB.debugLineVertexBufferUAV];

        uint vertex_count;
        argumentsBuffer.InterlockedAdd(0, 2, vertex_count); //increment vertex_count by 2
    
        LineVertex p;
        p.color = Float4ToRGBA8Unorm(float4(color, 1.0));
    
        p.position = start;
        vertexBuffer[vertex_count] = p;
    
        p.position = end;
        vertexBuffer[vertex_count + 1] = p;
    }

    void DrawTriangle(float3 p0, float3 p1, float3 p2, float3 color)
    {
        DrawLine(p0, p1, color);
        DrawLine(p1, p2, color);
        DrawLine(p2, p0, color);
    }

    void DrawBox(float3 min, float3 max, float3 color)
    {
        float3 p0 = float3(min.x, min.y, min.z);
        float3 p1 = float3(max.x, min.y, min.z);
        float3 p2 = float3(max.x, max.y, min.z);
        float3 p3 = float3(min.x, max.y, min.z);
        float3 p4 = float3(min.x, min.y, max.z);
        float3 p5 = float3(max.x, min.y, max.z);
        float3 p6 = float3(max.x, max.y, max.z);
        float3 p7 = float3(min.x, max.y, max.z);

        DrawLine(p0, p1, color);
        DrawLine(p1, p2, color);
        DrawLine(p2, p3, color);
        DrawLine(p3, p0, color);

        DrawLine(p4, p5, color);
        DrawLine(p5, p6, color);
        DrawLine(p6, p7, color);
        DrawLine(p7, p4, color);
    
        DrawLine(p0, p4, color);
        DrawLine(p1, p5, color);
        DrawLine(p2, p6, color);
        DrawLine(p3, p7, color);
    }

    static const uint DEBUG_SPHERE_M = 10; //latitude (horizontal)
    static const uint DEBUG_SPHERE_N = 20; //longitude (vertical)

    float3 SpherePosition(uint m, uint n, float3 center, float radius)
    {
        float x = sin(M_PI * m / DEBUG_SPHERE_M) * cos(2 * M_PI * n / DEBUG_SPHERE_N);
        float z = sin(M_PI * m / DEBUG_SPHERE_M) * sin(2 * M_PI * n / DEBUG_SPHERE_N);
        float y = cos(M_PI * m / DEBUG_SPHERE_M);
        return center + float3(x, y, z) * radius;
    }

    void DrawSphere(float3 center, float radius, float3 color)
    {
        for (uint m = 0; m < DEBUG_SPHERE_M; ++m)
        {
            for (uint n = 0; n < DEBUG_SPHERE_N; ++n)
            {
                float3 p0 = SpherePosition(m, n, center, radius);
                float3 p1 = SpherePosition(min(m + 1, DEBUG_SPHERE_M), n, center, radius);
                float3 p2 = SpherePosition(m, min(n + 1, DEBUG_SPHERE_N), center, radius);
            
                DrawTriangle(p0, p1, p2, color);
            }
        }
    }
    
    struct Text
    {
        float2 screenPosition;
        uint text;
    };
    
    //packed version of stbtt_bakedchar
    struct BakedChar
    {
        uint bbox; //uint8_t x0, y0, x1, y1; // coordinates of bbox in bitmap
        float xoff;
        float yoff;
        float xadvance;
    };
    
    void PrintChar(inout float2 screenPos, uint text)
    {
        RWByteAddressBuffer counterBuffer = ResourceDescriptorHeap[SceneCB.debugTextCounterBufferUAV];
        RWStructuredBuffer<Text> textBuffer = ResourceDescriptorHeap[SceneCB.debugTextBufferUAV];
        StructuredBuffer<BakedChar> bakedCharBuffer = ResourceDescriptorHeap[SceneCB.debugFontCharBufferSRV];

        uint text_count;
        counterBuffer.InterlockedAdd(0, 1, text_count);
        
        textBuffer[text_count].screenPosition = screenPos;
        textBuffer[text_count].text = text;
        
        screenPos.x += bakedCharBuffer[text].xadvance;
    }
    
    void PrintString(inout float2 screenPos, uint c1, uint c2)
    {
        PrintChar(screenPos, c1);PrintChar(screenPos, c2);
    }
    
    void PrintString(inout float2 screenPos, uint c1, uint c2, uint c3)
    {
        PrintChar(screenPos, c1);PrintChar(screenPos, c2);PrintChar(screenPos, c3);
    }
    
    void PrintString(inout float2 screenPos, uint c1, uint c2, uint c3, uint c4)
    {
        PrintChar(screenPos, c1);PrintChar(screenPos, c2);PrintChar(screenPos, c3);PrintChar(screenPos, c4);
    }
    
    void PrintString(inout float2 screenPos, uint c1, uint c2, uint c3, uint c4, uint c5)
    {
        PrintChar(screenPos, c1);PrintChar(screenPos, c2);PrintChar(screenPos, c3);PrintChar(screenPos, c4);PrintChar(screenPos, c5);
    }
    
    void PrintString(inout float2 screenPos, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6)
    {
        PrintChar(screenPos, c1);PrintChar(screenPos, c2);PrintChar(screenPos, c3);PrintChar(screenPos, c4);PrintChar(screenPos, c5);
        PrintChar(screenPos, c6);
    }
    
    void PrintString(inout float2 screenPos, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7)
    {
        PrintChar(screenPos, c1);PrintChar(screenPos, c2);PrintChar(screenPos, c3);PrintChar(screenPos, c4);PrintChar(screenPos, c5);
        PrintChar(screenPos, c6);PrintChar(screenPos, c7);
    }
    
    void PrintString(inout float2 screenPos, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7, uint c8)
    {
        PrintChar(screenPos, c1);PrintChar(screenPos, c2);PrintChar(screenPos, c3);PrintChar(screenPos, c4);PrintChar(screenPos, c5);
        PrintChar(screenPos, c6);PrintChar(screenPos, c7);PrintChar(screenPos, c8);
    }
    
    void PrintString(inout float2 screenPos, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7, uint c8, uint c9)
    {
        PrintChar(screenPos, c1);PrintChar(screenPos, c2);PrintChar(screenPos, c3);PrintChar(screenPos, c4);PrintChar(screenPos, c5);
        PrintChar(screenPos, c6);PrintChar(screenPos, c7);PrintChar(screenPos, c8);PrintChar(screenPos, c9);
    }
    
    void PrintString(inout float2 screenPos, uint c1, uint c2, uint c3, uint c4, uint c5, uint c6, uint c7, uint c8, uint c9, uint c10)
    {
        PrintChar(screenPos, c1);PrintChar(screenPos, c2);PrintChar(screenPos, c3);PrintChar(screenPos, c4);PrintChar(screenPos, c5);
        PrintChar(screenPos, c6);PrintChar(screenPos, c7);PrintChar(screenPos, c8);PrintChar(screenPos, c9);PrintChar(screenPos, c10);
    }
    
    uint GetDigitChar(uint number) //0-9
    {
        return clamp(number, 0, 9) + '0';
    }
    
    void PrintInt(inout float2 screenPos, int number)
    {
        if (number < 0)
        {
            PrintChar(screenPos, '-');
            number = -number;
        }
        
        uint length = number == 0 ? 1 : log10(number) + 1;        
        uint divisor = round(pow(10, length - 1));
     
        for (uint i = 0; i < length; ++i)
        {
            uint digit = number / divisor;
            PrintChar(screenPos, GetDigitChar(digit));

            number = number - digit * divisor;
            divisor /= 10;
        }
    }
    
    void PrintFloat(inout float2 screenPos, float number)
    {
        if(isinf(number))
        {
            PrintString(screenPos, 'I', 'N', 'F');
        }
        else if(isnan(number))
        {
            PrintString(screenPos, 'N', 'A', 'N');
        }
        else
        {            
            PrintInt(screenPos, (int)number);
            PrintChar(screenPos, '.');
            
            uint frac_value = frac(number) * 100000;
            PrintInt(screenPos, frac_value);
        }
    }
    
    void PrintInt(inout float2 screenPos, int2 value)
    {
        PrintInt(screenPos, value.x);
        PrintChar(screenPos, ',');
        PrintInt(screenPos, value.y);
    }
    
    void PrintInt(inout float2 screenPos, int3 value)
    {
        PrintInt(screenPos, value.xy);
        PrintChar(screenPos, ',');
        PrintInt(screenPos, value.z);
    }
    
    void PrintInt(inout float2 screenPos, int4 value)
    {
        PrintInt(screenPos, value.xyz);
        PrintChar(screenPos, ',');
        PrintInt(screenPos, value.w);
    }
    
    void PrintFloat(inout float2 screenPos, float2 value)
    {
        PrintFloat(screenPos, value.x);
        PrintChar(screenPos, ',');
        PrintFloat(screenPos, value.y);
    }
    
    void PrintFloat(inout float2 screenPos, float3 value)
    {
        PrintFloat(screenPos, value.xy);
        PrintChar(screenPos, ',');
        PrintFloat(screenPos, value.z);
    }
    
    void PrintFloat(inout float2 screenPos, float4 value)
    {
        PrintFloat(screenPos, value.xyz);
        PrintChar(screenPos, ',');
        PrintFloat(screenPos, value.w);
    }
}