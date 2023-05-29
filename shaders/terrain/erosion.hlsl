#include "../common.hlsli"
#include "../random.hlsli"
#include "constants.hlsli"
#include "noise.hlsli"

static RWTexture2D<float> heightmapUAV = ResourceDescriptorHeap[c_heightmapUAV];

// https://github.com/weigert/SimpleErosion/blob/master/source/include/world/world.cpp

float3 surfaceNormal(int i, int j)
{
  /*
    Note: Surface normal is computed in this way, because the square-grid surface is meshed using triangles.
    To avoid spatial artifacts, you need to weight properly with all neighbors.
  */

    float3 n = 0.15 * normalize(float3(terrainHeightScale * (heightmapUAV[int2(i, j)] - heightmapUAV[int2(i + 1, j)]), 1.0, 0.0)); //Positive X
    n += 0.15 * normalize(float3(terrainHeightScale * (heightmapUAV[int2(i - 1, j)] - heightmapUAV[int2(i, j)]), 1.0, 0.0)); //Negative X
    n += 0.15 * normalize(float3(0.0, 1.0, terrainHeightScale * (heightmapUAV[int2(i, j)] - heightmapUAV[int2(i, j + 1)]))); //Positive Y
    n += 0.15 * normalize(float3(0.0, 1.0, terrainHeightScale * (heightmapUAV[int2(i, j - 1)] - heightmapUAV[int2(i, j)]))); //Negative Y

    //Diagonals! (This removes the last spatial artifacts)
    n += 0.1 * normalize(float3(terrainHeightScale * (heightmapUAV[int2(i, j)] - heightmapUAV[int2(i + 1, j + 1)]) / sqrt(2), sqrt(2), terrainHeightScale * (heightmapUAV[int2(i, j)] - heightmapUAV[int2(i + 1, j + 1)]) / sqrt(2))); //Positive Y
    n += 0.1 * normalize(float3(terrainHeightScale * (heightmapUAV[int2(i, j)] - heightmapUAV[int2(i + 1, j - 1)]) / sqrt(2), sqrt(2), terrainHeightScale * (heightmapUAV[int2(i, j)] - heightmapUAV[int2(i + 1, j - 1)]) / sqrt(2))); //Positive Y
    n += 0.1 * normalize(float3(terrainHeightScale * (heightmapUAV[int2(i, j)] - heightmapUAV[int2(i - 1, j + 1)]) / sqrt(2), sqrt(2), terrainHeightScale * (heightmapUAV[int2(i, j)] - heightmapUAV[int2(i - 1, j + 1)]) / sqrt(2))); //Positive Y
    n += 0.1 * normalize(float3(terrainHeightScale * (heightmapUAV[int2(i, j)] - heightmapUAV[int2(i - 1, j - 1)]) / sqrt(2), sqrt(2), terrainHeightScale * (heightmapUAV[int2(i, j)] - heightmapUAV[int2(i - 1, j - 1)]) / sqrt(2))); //Positive Y

    return n;
}

struct Particle
{
    float2 pos;
    float2 speed;

    float volume; //This will vary in time
    float sediment; //Fraction of Volume that is Sediment!
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
     //Particle Properties
    const float dt = 1.2;
    const float density = 1.0; //This gives varying amounts of inertia and stuff...
    const float evapRate = 0.001;
    const float depositionRate = 0.1;
    const float minVol = 0.01;
    const float friction = 0.05;
    
    uint width, height;
    heightmapUAV.GetDimensions(width, height);
    
    PRNG rng = PRNG::Create(dispatchThreadID.xy, uint2(width, height));
    
    Particle drop;
    drop.pos = float2(rng.RandomFloat() * width, rng.RandomFloat() * height);
    drop.speed = 0.0;
    drop.volume = 1.0;
    drop.sediment = 0.0;

    //As long as the droplet exists...
    while (drop.volume > minVol)
    {

        int2 ipos = drop.pos; //Floored Droplet Initial Position
        float3 n = surfaceNormal(ipos.x, ipos.y); //Surface Normal at Position

        //Accelerate particle using newtonian mechanics using the surface normal.
        drop.speed += dt * float2(n.x, n.z) / (drop.volume * density); //F = ma, so a = F/m
        drop.pos += dt * drop.speed;
        drop.speed *= (1.0 - dt * friction); //Friction Factor

        /*
        Note: For multiplied factors (e.g. friction, evaporation)
        time-scaling is correctly implemented like above.
        */

        //Check if Particle is still in-bounds
        if (!all(drop.pos >=  0) ||
            !all(drop.pos < int2(width, height)))
        {
            break;
        }

        //Compute sediment capacity difference
        float maxsediment = drop.volume * length(drop.speed) * (heightmapUAV[int2(ipos.x, ipos.y)] - heightmapUAV[int2(drop.pos.x, drop.pos.y)]);
        if (maxsediment < 0.0)
            maxsediment = 0.0;
        float sdiff = maxsediment - drop.sediment;

        //Act on the Heightmap and Droplet!
        drop.sediment += dt * depositionRate * sdiff;
        heightmapUAV[int2(ipos.x, ipos.y)] -= dt * drop.volume * depositionRate * sdiff;

        //Evaporate the Droplet (Note: Proportional to Volume! Better: Use shape factor to make proportional to the area instead.)
        drop.volume *= (1.0 - dt * evapRate);
    }
}
