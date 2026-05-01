#pragma once
#include <functional>
namespace OIV
{
    class RecursiveDelayedOp
    {
        using Callback = std::function< void(void)>;
    public:

        RecursiveDelayedOp(Callback callback) : fCallback(callback) {}

        void Begin()
        {
            fCounter++;
        }
        void End(bool commit = true)
        {
            fCounter--;
            if (fCounter == 0 && commit == true)
                fCallback();
        }

        void Queue()
        {
            Begin();
            End();
        }
    private:
        int fCounter = 0;
        Callback fCallback;
    };

    using RecrusiveDelayedOp = RecursiveDelayedOp;
}
