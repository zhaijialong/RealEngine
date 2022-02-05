#include "animation.h"
#include "skeletal_mesh.h"

Animation::Animation(const std::string& name)
{
    m_name = name;
}

void Animation::Update(SkeletalMesh* mesh, float delta_time)
{
    m_currentAnimTime += delta_time;
    if (m_currentAnimTime > m_timeDuration)
    {
        m_currentAnimTime = m_currentAnimTime - m_timeDuration;
    }

    for (size_t i = 0; i < m_channels.size(); ++i)
    {
        const AnimationChannel& channel = m_channels[i];
        UpdateChannel(mesh, channel);
    }
}

void Animation::UpdateChannel(SkeletalMesh* mesh, const AnimationChannel& channel)
{
    std::pair<float, float4> lower_frame;
    std::pair<float, float4> upper_frame;

    bool found = false;
    for (size_t frame = 0; frame < channel.keyframes.size() - 1; ++frame)
    {
        if (channel.keyframes[frame].first <= m_currentAnimTime &&
            channel.keyframes[frame + 1].first >= m_currentAnimTime)
        {
            lower_frame = channel.keyframes[frame];
            upper_frame = channel.keyframes[frame + 1];
            found = true;
            break;
        }
    }

    float interpolation_value;
    if (found)
    {
        interpolation_value = (m_currentAnimTime - lower_frame.first) / (upper_frame.first - lower_frame.first);
    }
    else
    {
        lower_frame = upper_frame = channel.keyframes[0];
        interpolation_value = 0.0f;
    }

    SkeletalMeshNode* node = mesh->GetNode(channel.targetNode);

    switch (channel.mode)
    {
    case AnimationChannelMode::Translation:
    {
        float3 lower_value = float3(lower_frame.second.x, lower_frame.second.y, -lower_frame.second.z);
        float3 upper_value = float3(upper_frame.second.x, upper_frame.second.y, -upper_frame.second.z);

        node->translation = lerp(lower_value, upper_value, interpolation_value);
        break;
    }
    case AnimationChannelMode::Rotation:
    {
        float4 lower_value = float4(lower_frame.second.x, lower_frame.second.y, -lower_frame.second.z, -lower_frame.second.w);
        float4 upper_value = float4(upper_frame.second.x, upper_frame.second.y, -upper_frame.second.z, -upper_frame.second.w);

        node->rotation = slerp(lower_value, upper_value, interpolation_value);
        break;
    }
    case AnimationChannelMode::Scale:
    {
        node->scale = lerp(lower_frame.second.xyz(), upper_frame.second.xyz(), interpolation_value);
        break;
    }
    default:
        break;
    }
}
