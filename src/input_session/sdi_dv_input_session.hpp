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
#include "input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterHD_Core.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>

namespace Deltacast::VideoMonitor::Session
{
    struct SdiDvInputSessionConfiguration : public InputSessionConfig
    {
    };

    template <typename TStream>
    class SdiDvInputSession : public InputSession<TStream>
    {
     public:
        explicit SdiDvInputSession(const SdiDvInputSessionConfiguration&     config,
                                   Deltacast::VideoMonitor::SharedResources& shared_resources)
            : InputSession<TStream>(config, shared_resources)
        {
        }

        virtual ~SdiDvInputSession() = default;

        void open_board() override
        {
            spdlog::trace("Opening board {} for RX{}", this->device_id(), this->stream_id());
            this->m_board = std::make_unique<Deltacast::Wrapper::Board>(
                Deltacast::Wrapper::Board::open(
                    this->device_id(),
                    [this](Deltacast::Wrapper::Board& board) -> void
                    {
                        if (board.has_firmware_loopback(this->stream_id()))
                        {
                            spdlog::trace("Enabling firmware loopback on RX{} for cleanup path",
                                          this->stream_id());
                            board.firmware_loopback(this->stream_id()).enable();
                        }
                        else if (board.has_active_loopback(this->stream_id()))
                        {
                            spdlog::trace("Enabling active loopback on RX{} for cleanup path",
                                          this->stream_id());
                            board.active_loopback(this->stream_id()).enable();
                        }
                        else if (board.has_passive_loopback(this->stream_id()))
                        {
                            spdlog::trace("Enabling passive loopback on RX{} for cleanup path",
                                          this->stream_id());
                            board.passive_loopback(this->stream_id()).enable();
                        }
                    }));

            spdlog::trace("Board {} opened for RX{}", this->device_id(), this->stream_id());
        }

        auto video_input_has_changed() -> bool override
        {
            auto& board = this->board();
            this->ensure_stream_is_prepared();

            if (!Deltacast::VideoMonitor::Helper::wait_for_input(
                    board.rx(this->stream_id()), this->shared_resources().stop_is_requested))
            {
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

        auto get_video_buffer() -> std::pair<UBYTE*, ULONG> override
        {
            this->ensure_board_is_opened();
            auto slot = this->stream().pop_slot();
            return slot->video().buffer();
        }

     protected:
        void disable_loopback()
        {
            auto& board = this->board();

            if (board.has_firmware_loopback(this->stream_id()))
            {
                spdlog::trace("Disabling firmware loopback on RX{}", this->stream_id());
                board.firmware_loopback(this->stream_id()).disable();
            }
            else if (board.has_active_loopback(this->stream_id()))
            {
                spdlog::trace("Disabling active loopback on RX{}", this->stream_id());
                board.active_loopback(this->stream_id()).disable();
            }
            else if (board.has_passive_loopback(this->stream_id()))
            {
                spdlog::trace("Disabling passive loopback on RX{}", this->stream_id());
                board.passive_loopback(this->stream_id()).disable();
            }
            else
            {
                spdlog::trace("No loopback to disable on RX{}", this->stream_id());
            }
        }

     private:
        auto has_video_input_changed() -> bool override = 0;
    };
}  // namespace Deltacast::VideoMonitor::Session