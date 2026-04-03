/*
 * SPDX-FileCopyrightText: Copyright (c) DELTACAST.TV. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at * * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdexcept>
#include <string>

namespace Deltacast::VideoMonitor
{
    // Exit codes
    enum class ExitCode
    {
        Success = 0,
        StopRequestedBeforeSignal = -1,
        FailureUnexpected = 1
    };

    // Custom exceptions
    class ApplicationException : public std::runtime_error
    {
     public:
        explicit ApplicationException(const std::string& message) : std::runtime_error(message) {}
    };

    class SignalDetectionException : public ApplicationException
    {
     public:
        explicit SignalDetectionException(const std::string& message)
            : ApplicationException("Signal Detection Error: " + message)
        {
        }
    };

    class SignalConfigurationException : public ApplicationException
    {
     public:
        explicit SignalConfigurationException(const std::string& message)
            : ApplicationException("Signal Configuration Error: " + message)
        {
        }
    };

    class RendererException : public ApplicationException
    {
     public:
        explicit RendererException(const std::string& message)
            : ApplicationException("Renderer Error: " + message)
        {
        }
    };

    class RendererInitializationException : public RendererException
    {
     public:
        explicit RendererInitializationException(const std::string& message)
            : RendererException("Initialization failed: " + message)
        {
        }
    };

    class DeviceException : public ApplicationException
    {
     public:
        explicit DeviceException(const std::string& message)
            : ApplicationException("Device Error: " + message)
        {
        }
    };

    class StreamException : public ApplicationException
    {
     public:
        explicit StreamException(const std::string& message)
            : ApplicationException("Stream Error: " + message)
        {
        }
    };
}  // namespace Deltacast::VideoMonitor
