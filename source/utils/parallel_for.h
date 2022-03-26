#pragma once

#include "core/engine.h"
#include "enkiTS/TaskScheduler.h"

template <typename F>
inline void ParallelFor(uint32_t begin, uint32_t end, F fun)
{
    enki::TaskScheduler* ts = Engine::GetInstance()->GetTaskScheduler();
    enki::TaskSet taskSet(end - begin + 1,
        [&](enki::TaskSetPartition range, uint32_t threadnum)
        {
            for (uint32_t i = range.start; i != range.end; ++i)
            {
                fun(i + begin);
            }
        });
    ts->AddTaskSetToPipe(&taskSet);
    ts->WaitforTask(&taskSet);
}

template <typename F>
inline void ParallelFor(uint32_t num, F fun)
{
    ParallelFor<F>(0, num - 1, fun);
}