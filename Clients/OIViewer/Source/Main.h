#pragma once
#include "CommandLine.h"

using ForwardFileCallback = bool (*)(const LLUtils::native_string_type&);
OIV::CommandLineExit RunViewer(const OIV::CommandLineParameters& parameters, ForwardFileCallback forwardFile = nullptr);
