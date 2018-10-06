namespace LLUtils
{
    class NoCopyable
    {
    public:
        NoCopyable() = default;
        NoCopyable(const NoCopyable&) = delete;
        NoCopyable& operator=(const NoCopyable&) = delete;
    };
}