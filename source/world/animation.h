#pragma once

#include "utils/math.h"
#include "EASTL/string.h"
#include "EASTL/vector.h"

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
    eastl::vector<eastl::pair<float, float4>> keyframes;
};

class SkeletalMesh;

class Animation
{
    friend class GLTFLoader;

public:
    Animation(const eastl::string& name);

    void Update(SkeletalMesh* mesh, float delta_time);

private:
    void UpdateChannel(SkeletalMesh* mesh, const AnimationChannel& channel);

private:
    eastl::string m_name;
    eastl::vector<AnimationChannel> m_channels;
    float m_timeDuration = 0.0f;
    float m_currentAnimTime = 0.0f;
};