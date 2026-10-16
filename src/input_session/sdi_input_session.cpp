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
#include "exceptions.hpp"
#include "helper.hpp"
#include "loopback_input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/helper/sdi.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterCppApi/stream/sdi/sdi_stream.hpp>
#include <VideoMasterCppApi/to_string.hpp>
#if defined(__APPLE__)
#include <VideoMasterHD/VideoMasterHD_Core.h>
#include <VideoMasterHD/VideoMasterHD_Sdi.h>
#else
#include <VideoMasterHD_Core.h>
#include <VideoMasterHD_Sdi.h>
#endif
#include <memory>
#include <spdlog/spdlog.h>

namespace Deltacast::VideoMonitor::Session
{
    SdiInputSession::SdiInputSession(const SdiInputSessionConfiguration&       config,
                                     Deltacast::VideoMonitor::SharedResources& shared_resources)
        : LoopbackInputSession(config, shared_resources)
    {
    }

    void SdiInputSession::prepare_video_stream()
    {
        auto& board = this->board();
        spdlog::trace("Preparing SDI stream on RX{}", this->stream_id());
        this->release_stream();
        this->disable_loopback();

        m_stream = std::make_unique<Deltacast::Wrapper::SdiStream>(board.sdi().open_stream(
            Deltacast::VideoMonitor::Helper::rx_index_to_streamtype(this->stream_id()),
            VHD_SDI_STPROC_DISJOINED_VIDEO));

        spdlog::debug("Opened SDI stream on RX{}", this->stream_id());

        spdlog::trace("Waiting for signal...");
        if (!Deltacast::VideoMonitor::Helper::wait_for_input(
                board.rx(this->stream_id()), this->shared_resources().stop_is_requested))
        {
            if (this->shared_resources().stop_is_requested)
            {
                throw Deltacast::VideoMonitor::Exceptions::StopRequestedException(
                    "Stop requested while waiting for initial signal");
            }

            throw Deltacast::VideoMonitor::Exceptions::SignalDetectionException(
                "No input signal detected on the specified stream");
        }

        m_video_standard = m_stream->video_standard();
        m_clock_divisor = m_stream->clock_divisor();
        m_interface = m_stream->interface();

        spdlog::info("Detected SDI signal on RX{}: standard={}, clock_divisor={}, interface={}",
                     this->stream_id(), Deltacast::Wrapper::to_pretty_string(m_video_standard),
                     Deltacast::Wrapper::to_pretty_string(m_clock_divisor),
                     Deltacast::Wrapper::to_pretty_string(m_interface));
    }

    void SdiInputSession::configure_video_stream()
    {
        auto& stream = this->stream();

        set_sdi_mode_on_asi_channel();

        stream.buffer_queue().set_depth(buffer_queue_size);
        stream.set_buffer_packing(VHD_BUFPACK_VIDEO_YUV422_8);
        stream.set_video_standard(m_video_standard);
        stream.set_interface(m_interface);

        set_field_merging_mode();

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
                          this->stream_id(), Deltacast::Wrapper::to_pretty_string(m_video_standard),
                          Deltacast::Wrapper::to_pretty_string(stream.video_standard()),
                          Deltacast::Wrapper::to_pretty_string(m_clock_divisor),
                          Deltacast::Wrapper::to_pretty_string(stream.clock_divisor()),
                          Deltacast::Wrapper::to_pretty_string(m_interface),
                          Deltacast::Wrapper::to_pretty_string(stream.interface()));
        }

        return video_standard_changed || clock_divisor_changed || interface_changed;
    }

    void SdiInputSession::set_sdi_mode_on_asi_channel()
    {
        auto&      rx = this->board().rx(this->stream_id());
        const auto channel_type = rx.type();
        if (channel_type == VHD_CHNTYPE_3GSDI_ASI || channel_type == VHD_CHNTYPE_12GSDI_ASI)
        {
            rx.set_mode(VHD_CHANNEL_MODE_SDI);
            spdlog::debug("Set {} RX{} to SDI mode",
                          Deltacast::Wrapper::to_pretty_string(channel_type), this->stream_id());
        }
    }

    auto SdiInputSession::get_video_characteristics()
        -> Deltacast::Wrapper::Helper::VideoCharacteristics
    {
        this->ensure_stream_is_prepared();
        return Deltacast::Wrapper::Helper::Sdi::video_standard_to_characteristics(m_video_standard);
    }
}  // namespace Deltacast::VideoMonitor::Session