#include "jolt_job_system.h"
#include "core/engine.h"
#include "utils/profiler.h"
#include "Jolt/Core/Profiler.h"

using namespace JPH;

JoltJobSystem::JoltJobSystem(JPH::uint inMaxJobs, JPH::uint inMaxBarriers)
{
    JobSystemWithBarrier::Init(inMaxBarriers);

    m_pTaskScheduler = Engine::GetInstance()->GetTaskScheduler();
    m_jobs.Init(inMaxJobs, inMaxJobs);
}

JoltJobSystem::~JoltJobSystem()
{
    m_pTaskScheduler->WaitforAll();
}

int JoltJobSystem::GetMaxConcurrency() const
{
    return m_pTaskScheduler->GetNumTaskThreads();
}

JobSystem::JobHandle JoltJobSystem::CreateJob(const char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies)
{
    JPH_PROFILE_FUNCTION();

    uint32 index;
    while (true)
    {
        index = m_jobs.ConstructObject(inName, inColor, this, inJobFunction, inNumDependencies);
        if (index != FixedSizeFreeList<Job>::cInvalidObjectIndex)
        {
            break;
        }

        JPH_ASSERT(false, "No jobs available!");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    Job* job = &m_jobs.Get(index);

    JobHandle handle(job);

    if (inNumDependencies == 0)
    {
        QueueJob(job);
    }

    return handle;
}

void JoltJobSystem::QueueJob(Job* inJob)
{
    JPH_PROFILE_FUNCTION();

    inJob->AddRef();
#if 1
    m_pTaskScheduler->AddTaskSetToPipe((JoltJob*)inJob);
#else
    inJob->Execute();
    inJob->Release();
#endif
}

void JoltJobSystem::QueueJobs(Job** inJobs, JPH::uint inNumJobs)
{
    JPH_PROFILE_FUNCTION();

    for (uint i = 0; i < inNumJobs; ++i)
    {
        QueueJob(inJobs[i]);
    }
}

void JoltJobSystem::FreeJob(Job* inJob)
{
    m_jobs.DestructObject((JoltJob*)inJob);
}

void JoltJobSystem::JoltJob::ExecuteRange(enki::TaskSetPartition range_, uint32_t threadnum_)
{
    CPU_EVENT("Physics", "Physics Task");

    Execute();
}

void JoltJobSystem::CompletionAction::OnDependenciesComplete(enki::TaskScheduler* pTaskScheduler_, uint32_t threadNum_)
{
    ICompletable::OnDependenciesComplete(pTaskScheduler_, threadNum_);

    JoltJob* job = (JoltJob*)dependency.GetDependencyTask();
    job->Release();
}
