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

#include <cstdint>
#include <fmt/format.h>
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

    namespace Exceptions
    {

        // Custom exceptions
        class VideoMonitorException : public std::runtime_error
        {
         public:
            explicit VideoMonitorException(const std::string& message) : std::runtime_error(message)
            {
            }
        };

        class SignalDetectionException : public VideoMonitorException
        {
         public:
            explicit SignalDetectionException(const std::string& message)
                : VideoMonitorException(fmt::format("Signal Detection Error: {}", message))
            {
            }
        };

        class SignalConfigurationException : public VideoMonitorException
        {
         public:
            explicit SignalConfigurationException(const std::string& message)
                : VideoMonitorException(fmt::format("Signal Configuration Error: {}", message))
            {
            }
        };

        class RendererException : public VideoMonitorException
        {
         public:
            explicit RendererException(const std::string& message)
                : VideoMonitorException(fmt::format("Renderer Error: {}", message))
            {
            }
        };

        class RendererInitializationException : public RendererException
        {
         public:
            explicit RendererInitializationException(const std::string& message)
                : RendererException(fmt::format("Initialization failed: {}", message))
            {
            }
        };

        class DeviceException : public VideoMonitorException
        {
         public:
            explicit DeviceException(const std::string& message)
                : VideoMonitorException(fmt::format("Device Error: {}", message))
            {
            }
        };

        class StreamException : public VideoMonitorException
        {
         public:
            explicit StreamException(const std::string& message)
                : VideoMonitorException(fmt::format("Stream Error: {}", message))
            {
            }
        };

        class StreamNotPreparedException : public StreamException
        {
         public:
            explicit StreamNotPreparedException(uint32_t stream_id)
                : StreamException(
                      fmt::format("Stream must be prepared before configuring it (Stream ID: {})",
                                  stream_id))
            {
            }
        };

        class BoardNotOpenedException : public DeviceException
        {
         public:
            explicit BoardNotOpenedException(uint32_t device_id)
                : DeviceException(fmt::format(
                      "Board must be opened before preparing the stream (Device ID: {})",
                      device_id))
            {
            }
        };

        class UnsupportedChannelTypeException : public DeviceException
        {
         public:
            explicit UnsupportedChannelTypeException(const std::string& channel_type)
                : DeviceException(fmt::format("Unsupported channel type: {}", channel_type))
            {
            }
        };
    }  // namespace Exceptions
}  // namespace Deltacast::VideoMonitor
