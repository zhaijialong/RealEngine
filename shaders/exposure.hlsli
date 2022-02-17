#pragma once

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

float ConvertEV100ToExposure(float EV100)
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
    //combination of ComputeEV100 and ConvertEV100ToExposure
    float maxLuminance = (7800.0 / 65.0) * (aperture * aperture) / (shutterSpeed * ISO);
    return 1.0 / maxLuminance;
}