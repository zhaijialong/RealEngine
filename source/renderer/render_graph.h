#pragma once
#pragma once

#include "render_pass.h"

class RenderGraph
{
public:
    void AddPass(RenderPass* pass);

    template<class ParamT, class... ArgT>
    void AddPass();

    void Compile();
    void Execute();
    void Clear();
};

template<class ParamT, class ...ArgT>
inline void RenderGraph::AddPass()
{
}
