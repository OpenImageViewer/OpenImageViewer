#include "RendererSelection.h"
#include <LLUtils/StringUtility.h>
#include <array>
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace OIV
{
    IRendererSharedPtr SelectRenderer(std::span<const RendererBackend> backends, const RendererOptions& options,
                                      const OIV_RendererInitializationParams& params)
    {
        struct Candidate
        {
            RendererAdapter adapter;
            bool needsClassification = false;
            bool exhausted           = false;
        };
        struct BackendState
        {
            bool discovered = false;
            std::vector<Candidate> candidates;
        };
        // There are three real implementations; candidate counts are driver-reported and remain dynamic.
        static_assert(Acceleration::Hardware < Acceleration::Unknown && Acceleration::Unknown < Acceleration::Software);
        std::array<BackendState, MaxRendererCount> states;
        assert(backends.size() <= states.size());
        const auto api  = options.renderer       ? options.renderer
                          : options.adapterIndex ? std::optional{GetDefaultRenderer()}
                                                 : std::nullopt;
        const auto name = !options.adapterIndex && options.adapterName ? detail::TrimAdapterName(*options.adapterName)
                                                                       : std::string_view{};
        const bool constrained = options.adapterIndex || options.adapterName;
        IRendererSharedPtr selected;
        std::string failures;
        const auto record = [&](std::string message)
        {
            std::cerr << "[Renderer] " << message << '\n';
            failures += "\n  " + message;
        };
        for (const auto tier : AccelerationOrder)
        {
            for (size_t b = 0; b < backends.size() && !selected; ++b)
            {
                const auto& backend = backends[b];
                auto& state         = states[b];
                if ((api && backend.info.type != *api) || (constrained && !backend.info.supportsAdapterSelection))
                    continue;
                if (!state.discovered)
                {
                    state.discovered = true;
                    try
                    {
                        if (backend.info.supportsAdapterSelection)
                        {
                            auto probe = backend.create();
                            for (auto& adapter : probe->EnumerateAdapters())
                                if ((!options.adapterIndex || adapter.index == *options.adapterIndex) &&
                                    (name.empty() || detail::AdapterNameMatches(name, adapter.name, adapter.vendorId)))
                                    state.candidates.push_back({std::move(adapter)});
                        }
                        else
                            state.candidates.push_back({{}, true});
                        if (state.candidates.empty())
                            record(std::string(backend.info.name) + ": no adapters satisfy the requested index/name");
                    }
                    catch (const std::exception& error)
                    {
                        record(std::string(backend.info.name) + ": discovery failed: " + error.what());
                    }
                }
                // With a name, enumeration order wins. Otherwise preserve each API's automatic
                // preferred GPU, then exhaust the other devices in the current acceleration tier.
                for (int preference = 0; preference < 2 && !selected; ++preference)
                {
                    for (auto& candidate : state.candidates)
                    {
                        const bool preferred = !constrained && candidate.adapter.preferred;
                        if (selected || preferred != (preference == 0) || candidate.exhausted ||
                            (!candidate.needsClassification && candidate.adapter.acceleration != tier))
                            continue;
                        const auto description = std::string(backend.info.name) + " adapter " +
                                                 std::to_string(candidate.adapter.index) + " (" +
                                                 candidate.adapter.name + ")";
                        try
                        {
                            auto renderer            = backend.create();
                            auto initialization      = params;
                            const OIVString dataPath = OIVString(params.dataPath ? params.dataPath : OIV_TEXT(".")) +
                                                       OIV_TEXT("/") +
                                                       LLUtils::StringUtility::ConvertString<OIVString>(
                                                           std::string(backend.info.name));
                            initialization.dataPath  = dataPath.c_str();
                            initialization.gpuIndex  = candidate.adapter.index;
                            // Index precedence and name filtering were resolved before entering the backend.
                            initialization.adapterName  = nullptr;
                            initialization.acceleration = candidate.needsClassification
                                                              ? std::nullopt
                                                              : std::optional{candidate.adapter.acceleration};
                            if (renderer->Init(initialization) != 0)
                                throw std::runtime_error("Initialization returned a failure status");
                            const auto actual              = renderer->GetAcceleration();
                            candidate.adapter.acceleration = actual;
                            candidate.needsClassification  = false;
                            const bool soleContext         = !backend.info.supportsAdapterSelection &&
                                                             (api || backends.size() == 1);
                            if (static_cast<int>(actual) > static_cast<int>(tier) && !soleContext)
                            {
                                // GL can be classified only with a context. Release it before trying
                                // another candidate, and recreate it only if its actual tier is reached.
                                record(description + ": deferred to " + std::string(GetAccelerationName(actual)));
                            }
                            else
                            {
                                std::cerr << "[Renderer] Selected " << description << " ["
                                          << GetAccelerationName(actual) << "]\n";
                                selected = std::move(renderer);
                            }
                        }
                        catch (const std::exception& error)
                        {
                            candidate.exhausted = true;
                            record(description + ": " + error.what());
                        }
                    }
                }
            }
            if (selected)
                break;
        }
        if (!selected)
            throw std::runtime_error("No eligible renderer could initialize." + failures);
        return selected;
    }
}  // namespace OIV
