#pragma once

#include <cstdint>
#include <memory>

class GLContext
{
  public:

    GLContext();
    GLContext(const GLContext&)            = delete;
    GLContext& operator=(const GLContext&) = delete;
    ~GLContext();

    void Init(std::uintptr_t window, void* nativeDisplay);
    void Purge();
    void Resize(int width, int height);
    void SetSwapInterval(int interval);
    void SwapBuffers() const;

  private:

    class NativeState;
    std::unique_ptr<NativeState> fNativeState;
};
