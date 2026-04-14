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
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterHD_Core.h>
#include <cstdint>
#include <memory>
#include <utility>

namespace Deltacast::VideoMonitor::Session
{
    struct InputSessionBaseConfig
    {
        uint32_t device_id;
        uint32_t stream_id;
    };

    class InputSessionBase
    {
     public:
        constexpr static uint32_t buffer_queue_size = 8;
        explicit InputSessionBase(const InputSessionBaseConfig&             config,
                                  Deltacast::VideoMonitor::SharedResources& shared_resources)
            : m_device_id(config.device_id), m_stream_id(config.stream_id),
              m_shared_resources(shared_resources)
        {
        }
        virtual ~InputSessionBase() = default;
        virtual void open_board() = 0;
        virtual void prepare_stream() = 0;
        virtual void configure_stream() = 0;
        virtual void start_stream() = 0;
        virtual auto input_has_changed() -> bool = 0;
        virtual auto get_video_buffer() -> std::pair<UBYTE*, ULONG> = 0;
        virtual auto get_video_characteristics()
            -> Deltacast::Wrapper::Helper::VideoCharacteristics = 0;
        virtual auto get_slots_statistics() -> std::pair<ULONG, ULONG> = 0;

     protected:
        std::unique_ptr<Deltacast::Wrapper::Board> m_board;

        auto device_id() const -> uint32_t { return m_device_id; }

        auto stream_id() const -> uint32_t { return m_stream_id; }

        auto shared_resources() -> Deltacast::VideoMonitor::SharedResources&
        {
            return m_shared_resources;
        }

        auto board() -> Deltacast::Wrapper::Board&
        {
            ensure_board_is_opened();
            return *m_board;
        }

        void ensure_board_is_opened() const
        {
            if (!m_board)
            {
                throw Deltacast::VideoMonitor::Exceptions::BoardNotOpenedException(m_device_id);
            }
        }

     private:
        virtual auto has_input_changed() -> bool = 0;

        uint32_t m_device_id;
        uint32_t m_stream_id;

        Deltacast::VideoMonitor::SharedResources& m_shared_resources;
    };
}  // namespace Deltacast::VideoMonitor::Session