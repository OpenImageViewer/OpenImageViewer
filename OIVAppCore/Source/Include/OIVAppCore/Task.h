#pragma once

#include <condition_variable>
#include <coroutine>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

enum class TaskError
{
    None,
    Cancelled,
    ExecutorStopped
};

template <typename T>
class Task
{
  public:

    struct State
    {
        std::mutex fMutex;
        std::condition_variable fCv;
        bool fReady = false;

        std::optional<T> fValue;
        TaskError fError = TaskError::None;
        std::vector<std::coroutine_handle<>> fContinuations;
    };

    struct promise_type
    {
        std::shared_ptr<State> fState = std::make_shared<State>();

        Task get_return_object() { return Task(std::coroutine_handle<promise_type>::from_promise(*this), fState); }

        std::suspend_never initial_suspend() noexcept { return {}; }

        struct FinalAwaiter
        {
            bool await_ready() const noexcept { return false; }

            // The frame is suspended before publishing readiness, so waiters may now release it.
            static void await_suspend(std::coroutine_handle<promise_type> coroutine) noexcept
            {
                auto state = coroutine.promise().fState;
                Task::FinishCoroutine(state);
            }

            void await_resume() const noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }

        void return_value(T value)
        {
            // Only the producer writes the unpublished result. Final suspension publishes it.
            fState->fValue = std::move(value);
        }

        void unhandled_exception() { std::terminate(); }
    };

    Task() = default;

    static Task CreatePending() { return Task(std::make_shared<State>()); }

    static Task FromValue(T value)
    {
        Task task = CreatePending();
        task.SetValue(std::move(value));
        return task;
    }

    bool await_ready() const noexcept
    {
        if (fState == nullptr)
            return true;

        std::lock_guard lock(fState->fMutex);
        return fState->fReady;
    }

    bool await_suspend(std::coroutine_handle<> continuation)
    {
        if (fState == nullptr)
            return false;

        std::lock_guard lock(fState->fMutex);
        if (fState->fReady)
            return false;

        fState->fContinuations.push_back(continuation);
        return true;
    }

    T await_resume() { return GetResult(); }

    T get()
    {
        if (fState == nullptr)
            return T{};

        std::unique_lock lock(fState->fMutex);
        fState->fCv.wait(lock, [&] { return fState->fReady; });

        if (fState->fError != TaskError::None)
            throw fState->fError;

        return fState->fValue.value_or(T{});
    }

    void SetValue(T value) { CompleteWithValue(fState, std::move(value)); }

    void SetError(TaskError err) { CompleteWithError(fState, err); }

  private:

    struct CoroutineHolder
    {
        explicit CoroutineHolder(std::coroutine_handle<promise_type> handle) : fHandle(handle) {}

        ~CoroutineHolder()
        {
            if (fHandle)
                fHandle.destroy();
        }

        std::coroutine_handle<promise_type> fHandle;
    };

    explicit Task(std::shared_ptr<State> state) : fState(std::move(state)) {}

    Task(std::coroutine_handle<promise_type> handle, std::shared_ptr<State> state)
        : fState(std::move(state)), fCoroutine(std::make_shared<CoroutineHolder>(handle))
    {
    }

    T GetResult() const
    {
        if (fState == nullptr)
            return T{};

        std::lock_guard lock(fState->fMutex);
        if (fState->fError != TaskError::None)
            throw fState->fError;

        return fState->fValue.value_or(T{});
    }

    static void FinishCoroutine(const std::shared_ptr<State>& state) noexcept
    {
        std::vector<std::coroutine_handle<>> continuations;
        {
            std::lock_guard lock(state->fMutex);
            state->fReady = true;
            continuations = std::move(state->fContinuations);
        }

        state->fCv.notify_all();
        ResumeContinuations(continuations);
    }

    static void CompleteWithValue(const std::shared_ptr<State>& state, T value)
    {
        if (state == nullptr)
            return;

        std::vector<std::coroutine_handle<>> continuations;
        {
            std::lock_guard lock(state->fMutex);
            if (state->fReady)
                return;

            state->fValue = std::move(value);
            state->fError = TaskError::None;
            state->fReady = true;
            continuations = std::move(state->fContinuations);
        }

        state->fCv.notify_all();
        ResumeContinuations(continuations);
    }

    static void CompleteWithError(const std::shared_ptr<State>& state, TaskError err)
    {
        if (state == nullptr)
            return;

        std::vector<std::coroutine_handle<>> continuations;
        {
            std::lock_guard lock(state->fMutex);
            if (state->fReady)
                return;

            state->fError = err;
            state->fReady = true;
            continuations = std::move(state->fContinuations);
        }

        state->fCv.notify_all();
        ResumeContinuations(continuations);
    }

    static void ResumeContinuations(const std::vector<std::coroutine_handle<>>& continuations)
    {
        for (const auto continuation : continuations)
        {
            if (continuation)
                continuation.resume();
        }
    }

  private:

    std::shared_ptr<State> fState;
    std::shared_ptr<CoroutineHolder> fCoroutine;
};