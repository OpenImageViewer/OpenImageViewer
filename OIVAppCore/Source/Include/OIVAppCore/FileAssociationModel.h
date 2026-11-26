#pragma once
#include <OIVAppCore/FileAssociationPolicy.h>
#include <algorithm>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

namespace OIV
{
    struct FileAssociationEntry
    {
        std::string extension, group;
        FileAssociationStatus observed = FileAssociationStatus::Unknown;
        std::optional<bool> desired;
        bool Associated() const { return desired.value_or(observed == FileAssociationStatus::CurrentViewer); }
        bool Pending() const { return desired.has_value(); }
    };
    // The grids project this state; neither grid owns association intentions.
    // Shared MIME groups move together, while unknown results never become requests.
    class FileAssociationModel
    {
      public:

        const std::vector<FileAssociationEntry>& Entries() const { return fEntries; }
        const FileAssociationEntry* Find(std::string_view extension) const
        {
            const auto found = std::ranges::lower_bound(fEntries, extension, {}, &FileAssociationEntry::extension);
            return found != fEntries.end() && found->extension == extension ? &*found : nullptr;
        }
        void Refresh(std::vector<FileAssociationEntry> entries)
        {
            for (auto& entry : entries)
            {
                const auto* previous = Find(entry.extension);
                if (previous != nullptr && previous->Pending() &&
                    (entry.observed == FileAssociationStatus::Unknown ||
                     (entry.observed == FileAssociationStatus::CurrentViewer) != *previous->desired))
                    entry.desired = previous->desired;
            }
            std::ranges::sort(entries, {}, &FileAssociationEntry::extension);
            fEntries = std::move(entries);
        }
        void Select(const std::string& extension, bool associated)
        {
            const auto* selected = Find(extension);
            if (selected != nullptr && selected->observed != FileAssociationStatus::Unknown)
                for (auto& entry : fEntries)
                    if (entry.group == selected->group && entry.observed != FileAssociationStatus::Unknown)
                        entry.desired = associated == (entry.observed == FileAssociationStatus::CurrentViewer)
                                            ? std::nullopt
                                            : std::optional{associated};
        }
        void Discard()
        {
            for (auto& entry : fEntries)
                entry.desired.reset();
        }
        size_t PendingCount() const { return std::ranges::count_if(fEntries, &FileAssociationEntry::Pending); }

      private:

        std::vector<FileAssociationEntry> fEntries;
    };
}  // namespace OIV
