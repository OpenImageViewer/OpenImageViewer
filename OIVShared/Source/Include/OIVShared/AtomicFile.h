#pragma once
#include <filesystem>
#include <span>

namespace OIV
{
    // Publish a complete file by replacing a unique temporary sibling. Failed writes leave the old file intact.
    void WriteFileAtomically(const std::filesystem::path& path, std::span<const std::byte> bytes);
}  // namespace OIV