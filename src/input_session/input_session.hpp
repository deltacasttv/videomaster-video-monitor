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

#include "exceptions.hpp"
#include "helper.hpp"
#include "input_session_base.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#if defined(__APPLE__)
#include <VideoMasterHD/VideoMasterHD_Core.h>
#else
#include <VideoMasterHD_Core.h>
#endif
#include <cstdint>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>

namespace Deltacast::VideoMonitor::Session
{
    struct InputSessionConfig : InputSessionBaseConfig
    {
    };

    template <typename TStream>
    class InputSession : public InputSessionBase
    {
     public:
        constexpr static uint32_t buffer_queue_size = 8;
        explicit InputSession(const InputSessionConfig&                 config,
                              Deltacast::VideoMonitor::SharedResources& shared_resources)
            : InputSessionBase(config, shared_resources)
        {
        }
        virtual ~InputSession() = default;

        void start_video_stream() override { this->stream().start(); }

        auto get_video_slots_statistics() -> std::pair<ULONG, ULONG> override
        {
            auto& stream = this->stream();
            return { stream.buffer_queue().slots_count(), stream.buffer_queue().slots_dropped() };
        }

        auto video_input_has_changed() -> bool override
        {
            auto& board = this->board();
            this->ensure_stream_is_prepared();

            if (!Deltacast::VideoMonitor::Helper::wait_for_input(
                    board.rx(this->stream_id()), this->shared_resources().stop_is_requested))
            {
                if (this->shared_resources().stop_is_requested)
                {
                    throw Deltacast::VideoMonitor::Exceptions::StopRequestedException(
                        "Stop requested while waiting for input signal change");
                }

                throw Deltacast::VideoMonitor::Exceptions::SignalDetectionException(
                    "Failed to wait for input signal change");
            }

            const auto input_has_changed = has_video_input_changed();
            if (input_has_changed)
            {
                spdlog::warn("Video input characteristics changed on RX{}", this->stream_id());
            }

            return input_has_changed;
        }



        virtual void set_field_merging_mode()
        {
            ensure_stream_is_prepared();
            auto video_characteristics = get_video_characteristics();

            if (video_characteristics.interlaced)
            {
                if (board().supports_field_merging())
                {
                    stream().enable_field_merge();
                }
                else
                {
                    spdlog::warn("Field merging mode is not supported on this board. Field merging "
                                 "will not be enabled for interlaced video ({}x{} {}, "
                                 "framerate={}) and will result in a two-plane output.",
                                 video_characteristics.width, video_characteristics.height,
                                 video_characteristics.interlaced ? "i" : "p",
                                 video_characteristics.framerate);
                }
            }
            else
            {
                spdlog::trace("Field merging mode is not enabled for progressive video ({}x{} {}, "
                              "framerate={})",
                              video_characteristics.width, video_characteristics.height,
                              video_characteristics.interlaced ? "i" : "p",
                              video_characteristics.framerate);
                return;
            }
        }

     protected:
        std::unique_ptr<TStream>                      m_stream;
        decltype(std::declval<TStream&>().pop_slot()) m_current_slot;

        auto stream() -> TStream&
        {
            ensure_stream_is_prepared();
            return *m_stream;
        }

        auto stream() const -> const TStream&
        {
            ensure_stream_is_prepared();
            return *m_stream;
        }

        void ensure_stream_is_prepared() const
        {
            if (!m_stream)
            {
                throw Deltacast::VideoMonitor::Exceptions::StreamException(
                    fmt::format("Stream must be prepared before configuring it (Stream ID: {})",
                                stream_id()));
            }
        }
    };
}  // namespace Deltacast::VideoMonitor::Session