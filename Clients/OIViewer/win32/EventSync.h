#pragma once

#include <Windows.h>
#include <iostream>
#include <vector>
#include <memory>
#include <mutex>
#include <any>
#include <LLUtils/Exception.h>

struct EventData
{
    uint16_t id;
    std::any data;
};

using OnMessageCallback = std::function<void(const EventData&)>;
using EventDataList = std::vector<EventData>;

class SystemEvent final
{
  public:

    SystemEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCTSTR lpName)
        : fEvent(CreateEvent(lpEventAttributes, bManualReset, bInitialState, lpName))
    {
        if (!fEvent)
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Failed to create event");
        }
    }

    ~SystemEvent()
    {
        if (fEvent)
        {
            ::CloseHandle(fEvent);
        }
    }

    const HANDLE& get() const noexcept { return fEvent; }

    void Set()
    {
        if (!::SetEvent(get()))
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Failed to set event");
        }
    }

    void Reset()
    {
        if (!::ResetEvent(get()))
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Failed to reset event");
        }
    }

    SystemEvent(const SystemEvent&) = delete;
    SystemEvent& operator=(const SystemEvent&) = delete;

    SystemEvent(SystemEvent&&) = delete;
    SystemEvent& operator=(SystemEvent&&) = delete;

  private:

    HANDLE fEvent;
};

class EventSync
{
  public:

    // Constructor: Creates the event with the callback function
    EventSync(OnMessageCallback callback) : event(nullptr, TRUE, FALSE, nullptr), fCallback(std::move(callback)) {}

    // Get the event handle for use in MsgWaitForMultipleObjects
    const HANDLE& GetEventHandle() const noexcept { return event.get(); }

    // Add shared data to the list (perfect forwarding)
    template <typename T>
    void AddData(uint16_t id, T&& anyVar)
    {
        // Lock the mutex for thread safety
        std::lock_guard<std::mutex> lock(mtx);

        // Add data to the list
        sharedDataList.emplace_back(EventData{id, std::forward<T>(anyVar)});

        // Set the event signaling that data is available
        event.Set();
    }

    // Process the data and call the callback function
    void ProcessData()
    {
        EventDataList sharedData;
        {
            std::lock_guard<std::mutex> lock(mtx);
            sharedData = std::move(sharedDataList);  // Move data to local variable
            event.Reset();                           // Reset the event after processi
        }

        for (const auto& data : sharedData)
        {
            fCallback(data);  // Callback invocation
        }
    }

  private:

    SystemEvent event;             // Event handle wrapper with unique_ptr for automatic cleanup
    std::mutex mtx;                // Mutex for thread synchronization
    EventDataList sharedDataList;  // List of shared data objects
    OnMessageCallback fCallback;   // Callback function to process shared data
};
