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

#include "input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#if defined(__APPLE__)
#include <VideoMasterHD/VideoMasterHD_Core.h>
#else
#include <VideoMasterHD_Core.h>
#endif
#include <memory>
#include <spdlog/spdlog.h>

namespace Deltacast::VideoMonitor::Session
{
    struct LoopbackInputSessionConfiguration : public InputSessionConfig
    {
    };

    template <typename TStream, typename TSlot>
    class LoopbackInputSession : public InputSession<TStream>
    {
     public:
        explicit LoopbackInputSession(const LoopbackInputSessionConfiguration&  config,
                                      Deltacast::VideoMonitor::SharedResources& shared_resources)
            : InputSession<TStream>(config, shared_resources)
        {
        }

        auto get_video_buffer() -> std::pair<UBYTE*, ULONG> override
        {
            this->ensure_board_is_opened();
            this->m_current_slot = this->stream().pop_slot();
            return (static_cast<TSlot&>(*this->m_current_slot)).video().buffer();
        }

        virtual ~LoopbackInputSession() = default;

        void open_board() override
        {
            spdlog::trace("Opening board {} for RX{}", this->device_id(), this->stream_id());
            // Shared with the close callback, which runs after this class's members are destroyed.
            auto       loopback_was_enabled = std::make_shared<bool>(false);
            const auto stream_id = this->stream_id();
            this->m_board = std::make_unique<Deltacast::Wrapper::Board>(
                Deltacast::Wrapper::Board::open(
                    this->device_id(),
                    [loopback_was_enabled, stream_id](Deltacast::Wrapper::Board& board) -> void
                    {
                        auto* loopback = find_loopback(board, stream_id);
                        if (loopback != nullptr && *loopback_was_enabled)
                        {
                            spdlog::trace("Restoring the loopback of RX{}", stream_id);
                            loopback->enable();
                        }
                    }));

            const auto* loopback = find_loopback(*this->m_board, stream_id);
            *loopback_was_enabled = loopback != nullptr && loopback->is_enabled();
            spdlog::trace("Board {} opened for RX{}, loopback initially {}", this->device_id(),
                          stream_id, *loopback_was_enabled ? "enabled" : "disabled or absent");
        }

     protected:
        void disable_loopback()
        {
            auto* loopback = find_loopback(this->board(), this->stream_id());
            if (loopback == nullptr)
            {
                spdlog::trace("No loopback to disable on RX{}", this->stream_id());
                return;
            }
            spdlog::trace("Disabling loopback on RX{}", this->stream_id());
            loopback->disable();
        }

     private:
        static auto find_loopback(Deltacast::Wrapper::Board& board, uint32_t stream_id)
            -> Deltacast::Wrapper::BoardComponents::Loopback*
        {
            if (board.has_firmware_loopback(stream_id))
            {
                return &board.firmware_loopback(stream_id);
            }
            if (board.has_active_loopback(stream_id))
            {
                return &board.active_loopback(stream_id);
            }
            if (board.has_passive_loopback(stream_id))
            {
                return &board.passive_loopback(stream_id);
            }
            return nullptr;
        }
    };
}  // namespace Deltacast::VideoMonitor::Session