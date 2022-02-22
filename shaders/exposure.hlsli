#pragma once

//reference : https://placeholderart.wordpress.com/2014/12/15/implementing-a-physically-based-camera-automatic-exposure/


float ComputeEV100(float aperture, float shutterSpeed, float ISO)
{
    // https://en.wikipedia.org/wiki/Exposure_value
    // EV = log2(N^2 / t) = EV_100 + log2 (S / 100)
    // EV_100 = log2(N^2 / t) - log2(S / 100)
    //        = log2((N^2 / t) * (100 / S))

    return log2((aperture * aperture / shutterSpeed) * (100.0 / ISO));
}

float ComputeEV100(float avgLuminance)
{
    // https://en.wikipedia.org/wiki/Light_meter#Calibration_constants
    const float K = 12.5;

    return log2(avgLuminance * 100.0 / K);
}

float ComputeExposure(float EV100)
{
    // with saturation-based speed
    // https://en.wikipedia.org/wiki/Film_speed
    // maxLum = 78 / ( S * q ) * N^2 / t
    //        = 78 / ( S * q ) * 2^ EV_100

    const float lensAttenuation = 0.65;
    const float lensImperfectionExposureScale = 78.0 / (100.0 * lensAttenuation); // 78 / ( S * q )

    float maxLuminance = lensImperfectionExposureScale * pow(2.0, EV100);
    return 1.0 / maxLuminance;
}

float ComputeExposure(float aperture, float shutterSpeed, float ISO)
{
    //combination of ComputeEV100 and ComputeExposure
    float maxLuminance = (7800.0 / 65.0) * (aperture * aperture) / (shutterSpeed * ISO);
    return 1.0 / maxLuminance;
}

float GetMeteringWeight(float2 screenPos, float2 screenSize)
{
#if METERING_MODE_SPOT
    const float screenDiagonal = 0.5f * (screenSize.x + screenSize.y);
    const float radius = 0.075 * screenDiagonal;
    const float2 center = screenSize * 0.5f;
    float d = length(center - screenPos) - radius;
    return 1.0 - saturate(d);
#elif METERING_MODE_CENTER_WEIGHTED
    const float screenDiagonal = 0.5f * (screenSize.x + screenSize.y);
    const float2 center = screenSize * 0.5f;
    //return 1.0 - saturate(pow(length(center - screenPos) * 2 / screenDiagonal, 0.5));
    return smoothstep(1.0, 0.0, length(center - screenPos) / (max(screenSize.x, screenSize.y) * 0.5));
#else //METERING_MODE_AVERAGE
    return 1.0;
#endif
}