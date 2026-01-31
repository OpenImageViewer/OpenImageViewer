#pragma once

#include <condition_variable>
#include <functional>
#include <set>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <mutex>

#include "Task.h"
#include <oivshared/ThreadSafeUniqueIdProvider.h>

template <typename Request, typename Response>
class PriorityTaskExecutor
{
  public:
    using TaskId = uint64_t;
    using ProcessFn = std::function<Response(const Request&)>;
    using TaskType = Task<Response>;

    explicit PriorityTaskExecutor(
        size_t threadCount,
        ProcessFn process)
        : fProcess(std::move(process))
    {
        for (size_t i = 0; i < threadCount; ++i)
        {
            fWorkers.emplace_back([this] { WorkerLoop(); });
        }
    }

    ~PriorityTaskExecutor()
    {
        std::vector<QueueItem> cancelledItems;
        {
            std::lock_guard lock(fMutex);
            fStopping = true;

            for (const auto& item : fQueue)
                cancelledItems.push_back(item);

            fQueue.clear();
            fLookup.clear();
        }

        for (auto& item : cancelledItems)
        {
            item.Result.SetError(TaskError::ExecutorStopped);
            fIdProvider.Release(item.Id);
        }

        fCv.notify_all();

        for (auto& t : fWorkers)
            t.join();
    }

    TaskType Submit(Request request, int priority)
    {
        TaskType task = TaskType::CreatePending();

        {
            std::lock_guard lock(fMutex);
            if (fStopping)
            {
                task.SetError(TaskError::ExecutorStopped);
                return task;
            }

            TaskId id = fIdProvider.Acquire();
            auto it = fQueue.emplace(
                QueueItem{
                    priority,
                    fSequence++,
                    id,
                    std::move(request),
                    task
                });
            fLookup[id] = it;
        }

        fCv.notify_one();
        return task;
    }

    bool Cancel(TaskId id)
    {
        std::lock_guard lock(fMutex);
        auto it = fLookup.find(id);
        if (it == fLookup.end())
            return false;

        it->second->Result.SetError(TaskError::Cancelled);
        fQueue.erase(it->second);
        fLookup.erase(it);
        fIdProvider.Release(id);
        return true;
    }

private:
    struct QueueItem
    {
        int Priority;
        uint64_t Sequence;
        TaskId Id;
        Request Req;
        TaskType Result;
    };

    struct Compare
    {
        bool operator()(const QueueItem& a, const QueueItem& b) const
        {
            if (a.Priority != b.Priority)
                return a.Priority > b.Priority;
            return a.Sequence < b.Sequence;
        }
    };

    void WorkerLoop()
    {
        while (true)
        {
            QueueItem item;

            {
                std::unique_lock lock(fMutex);
                fCv.wait(lock, [&] {
                    return fStopping || !fQueue.empty();
                });

                if (fStopping && fQueue.empty())
                    return;

                auto it = fQueue.begin();
                item = std::move(*it);
                fLookup.erase(item.Id);
                fQueue.erase(it);
            }

            try
            {
                Response result = fProcess(item.Req);
                item.Result.SetValue(std::move(result));
            }
            catch (...)
            {
                item.Result.SetError(TaskError::ExecutorStopped);
            }

            fIdProvider.Release(item.Id);
        }
    }

  private:
    std::multiset<QueueItem, Compare> fQueue;
    std::unordered_map<TaskId, decltype(fQueue.begin())> fLookup;

    ThreadSafeUniqueIdProvider<TaskId> fIdProvider;
    uint64_t fSequence = 0;

    std::vector<std::thread> fWorkers;
    ProcessFn fProcess;

    std::mutex fMutex;
    std::condition_variable fCv;
    bool fStopping = false;
};
