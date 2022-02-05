#pragma once

#include "utils/math.h"

enum class AnimationChannelMode
{
    Translation,
    Rotation,
    Scale,
};

struct AnimationChannel
{
    uint32_t targetNode;
    AnimationChannelMode mode;
    std::vector<std::pair<float, float4>> keyframes;
};

class SkeletalMesh;

class Animation
{
    friend class GLTFLoader;

public:
    Animation(const std::string& name);

    void Update(SkeletalMesh* mesh, float delta_time);

private:
    void UpdateChannel(SkeletalMesh* mesh, const AnimationChannel& channel);

private:
    std::string m_name;
    std::vector<AnimationChannel> m_channels;
    float m_timeDuration = 0.0f;
    float m_currentAnimTime = 0.0f;
};