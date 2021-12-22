#pragma once

#include "core/engine.h"
#include "enkiTS/TaskScheduler.h"

template <typename I, typename F>
inline void ParallelFor(I begin, I end, F fun)
{
	enki::TaskScheduler* ts = Engine::GetInstance()->GetTaskScheduler();
	enki::TaskSet taskSet(end - begin + 1,
		[&](enki::TaskSetPartition range, I threadnum)
		{
			for (I i = range.start; i != range.end; ++i)
			{
				fun(i + begin);
			}
		});
	ts->AddTaskSetToPipe(&taskSet);
	ts->WaitforTask(&taskSet);
}

template <typename I, typename F>
inline void ParallelFor(I num, F fun)
{
	ParallelFor(0, num - 1, fun);
}