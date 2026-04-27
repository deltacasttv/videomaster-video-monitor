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

    void SdiInputSession::prepare_video_stream()
    {
        auto& board = this->board();
        spdlog::trace("Preparing SDI stream on RX{}", this->stream_id());
        this->disable_loopback();

        m_stream = std::make_unique<Deltacast::Wrapper::SdiStream>(board.sdi().open_stream(
            Deltacast::VideoMonitor::Helper::rx_index_to_streamtype(this->stream_id()),
            VHD_SDI_STPROC_DISJOINED_VIDEO));

        spdlog::debug("Opened SDI stream on RX{}", this->stream_id());

        spdlog::trace("Waiting for signal...");
        if (!Deltacast::VideoMonitor::Helper::wait_for_input(
                board.rx(this->stream_id()), this->shared_resources().stop_is_requested))
        {
            throw std::runtime_error("No input signal detected on the specified stream");
        }

        m_video_standard = m_stream->video_standard();
        m_clock_divisor = m_stream->clock_divisor();
        m_interface = m_stream->interface();

        spdlog::info("Detected SDI signal on RX{}: standard={}, clock_divisor={}, interface={}",
                     this->stream_id(), static_cast<int>(m_video_standard),
                     static_cast<int>(m_clock_divisor), static_cast<int>(m_interface));
    }

    void SdiInputSession::configure_video_stream()
    {
        auto& stream = this->stream();

        stream.buffer_queue().set_depth(buffer_queue_size);
        stream.set_buffer_packing(VHD_BUFPACK_VIDEO_YUV422_8);
        stream.set_video_standard(m_video_standard);
        stream.set_interface(m_interface);

        spdlog::debug("Configured SDI stream on RX{} with queue depth {}", this->stream_id(),
                      buffer_queue_size);
    }

    auto SdiInputSession::has_video_input_changed() -> bool
    {
        auto&      stream = this->stream();
        const auto video_standard_changed = stream.video_standard() != m_video_standard;
        const auto clock_divisor_changed = stream.clock_divisor() != m_clock_divisor;
        const auto interface_changed = stream.interface() != m_interface;

        if (video_standard_changed || clock_divisor_changed || interface_changed)
        {
            spdlog::debug("SDI signal change details on RX{}: standard {}->{}, clock_divisor "
                          "{}->{}, interface {}->{}",
                          this->stream_id(), static_cast<int>(m_video_standard),
                          static_cast<int>(stream.video_standard()),
                          static_cast<int>(m_clock_divisor),
                          static_cast<int>(stream.clock_divisor()), static_cast<int>(m_interface),
                          static_cast<int>(stream.interface()));
        }

        return video_standard_changed || clock_divisor_changed || interface_changed;
    }

    auto SdiInputSession::get_video_characteristics()
        -> Deltacast::Wrapper::Helper::VideoCharacteristics
    {
        this->ensure_stream_is_prepared();
        return Deltacast::Wrapper::Helper::Sdi::video_standard_to_characteristics(m_video_standard);
    }
}  // namespace Deltacast::VideoMonitor::Session