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
#include "input_session_base.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterHD_Core.h>
#include <cstdint>
#include <memory>
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

        auto get_slots_statistics() -> std::pair<ULONG, ULONG> override
        {
            auto& stream = this->stream();
            return { stream.buffer_queue().slots_count(), stream.buffer_queue().slots_dropped() };
        }

     protected:
        std::unique_ptr<TStream> m_stream;

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
                throw Deltacast::VideoMonitor::Exceptions::StreamNotPreparedException(stream_id());
            }
        }
    };
}  // namespace Deltacast::VideoMonitor::Session