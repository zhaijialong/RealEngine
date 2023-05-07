#pragma once

#include "Jolt/Jolt.h"
#include "Jolt/Core/JobSystemWithBarrier.h"
#include "Jolt/Core/FixedSizeFreeList.h"
#include "enkiTS/TaskScheduler.h"

class JoltJobSystem : public JPH::JobSystemWithBarrier
{
    struct CompletionAction : enki::ICompletable
    {
        enki::Dependency dependency;

        virtual void OnDependenciesComplete(enki::TaskScheduler* pTaskScheduler_, uint32_t threadNum_) override;
    };

    class JoltJob : public JobSystem::Job, public enki::ITaskSet
    {
    public:
        JoltJob(const char* inJobName, JPH::ColorArg inColor, JobSystem* inJobSystem, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies) :
            JobSystem::Job(inJobName, inColor, inJobSystem, inJobFunction, inNumDependencies),
            ITaskSet(1)
        {
            m_completionAction.SetDependency(m_completionAction.dependency, this);
        }

        virtual void ExecuteRange(enki::TaskSetPartition range_, uint32_t threadnum_) override;

    private:
        CompletionAction m_completionAction;
    };

public:
    JoltJobSystem(JPH::uint inMaxJobs, JPH::uint inMaxBarriers);
    ~JoltJobSystem();

    virtual int GetMaxConcurrency() const override;
    virtual JobHandle CreateJob(const char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction, JPH::uint32 inNumDependencies = 0) override;

private:
    virtual void QueueJob(Job* inJob) override;
    virtual void QueueJobs(Job** inJobs, JPH::uint inNumJobs) override;
    virtual void FreeJob(Job* inJob) override;

private:
    enki::TaskScheduler* m_pTaskScheduler;
    JPH::FixedSizeFreeList<JoltJob> m_jobs;
};