#pragma once

struct Sample
{
    //Visible point and surface normal
    float3 x_v;
    float3 n_v;
    //Sample point and surface normal
    float3 x_s;
    float3 n_s;
    //Outgoing radiance at sample point in RGB
    float3 Lo;
};

// Weighted Reservoir Sampling
struct Reservoir
{
    Sample z;
    float w_sum;
    float M;
    float W;
    
    bool Update(Sample s_new, float w_new, float random)
    {
        w_sum += w_new;
        M += 1;
        
        if (random < w_new / max(w_sum, 0.00001)) //avoid divide by 0
        {
            z = s_new;
            return true;
        }
        
        return false;
    }
    
    bool Merge(Reservoir r, float target_pdf, float random)
    {
        float M0 = M;
        bool updated = Update(r.z, target_pdf * r.W * r.M, random);
        M = M0 + r.M;
        
        return updated;
    }
};