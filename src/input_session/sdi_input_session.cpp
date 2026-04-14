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

#include "sdi_input_session.hpp"
#include "helper.hpp"
#include "sdi_dv_input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/helper/sdi.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterCppApi/stream/sdi/sdi_stream.hpp>
#include <VideoMasterHD_Core.h>
#include <VideoMasterHD_Sdi.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace Deltacast::VideoMonitor::Session
{
    SdiInputSession::SdiInputSession(const SdiInputSessionConfiguration&       config,
                                     Deltacast::VideoMonitor::SharedResources& shared_resources)
        : SdiDvInputSession(config, shared_resources)
    {
    }

    void SdiInputSession::prepare_stream()
    {
        auto& board = this->board();
        this->disable_loopback();

        m_stream = std::make_unique<Deltacast::Wrapper::SdiStream>(board.sdi().open_stream(
            Deltacast::VideoMonitor::Helper::rx_index_to_streamtype(this->stream_id()),
            VHD_SDI_STPROC_DISJOINED_VIDEO));

        spdlog::info("Waiting for signal...");
        if (!Deltacast::VideoMonitor::Helper::wait_for_input(
                board.rx(this->stream_id()), this->shared_resources().stop_is_requested))
        {
            throw std::runtime_error("No input signal detected on the specified stream");
        }

        m_video_standard = m_stream->video_standard();
        m_clock_divisor = m_stream->clock_divisor();
        m_interface = m_stream->interface();
    }

    void SdiInputSession::configure_stream()
    {
        auto& stream = this->stream();

        stream.buffer_queue().set_depth(buffer_queue_size);
        stream.set_buffer_packing(VHD_BUFPACK_VIDEO_YUV422_8);
        stream.set_video_standard(m_video_standard);
        stream.set_clock_divisor(m_clock_divisor);
        stream.set_interface(m_interface);
    }

    auto SdiInputSession::has_input_changed() -> bool
    {
        auto& stream = this->stream();
        return (stream.video_standard() != m_video_standard ||
                stream.clock_divisor() != m_clock_divisor || stream.interface() != m_interface);
    }

    auto SdiInputSession::get_video_characteristics()
        -> Deltacast::Wrapper::Helper::VideoCharacteristics
    {
        this->ensure_stream_is_prepared();
        return Deltacast::Wrapper::Helper::Sdi::video_standard_to_characteristics(m_video_standard);
    }
}  // namespace Deltacast::VideoMonitor::Session