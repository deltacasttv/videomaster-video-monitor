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

#include "dv_input_session.hpp"
#include "exceptions.hpp"
#include "helper.hpp"
#include "loopback_input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/helper/dv.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterCppApi/stream/dv/dv_stream.hpp>
#if defined(__APPLE__)
#include <VideoMasterHD/VideoMasterHD_Core.h>
#include <VideoMasterHD/VideoMasterHD_Dv.h>
#else
#include <VideoMasterHD_Core.h>
#include <VideoMasterHD_Dv.h>
#endif
#include <memory>
#include <spdlog/spdlog.h>

namespace Deltacast::VideoMonitor::Session
{
    DvInputSession::DvInputSession(const DvInputSessionConfiguration&        config,
                                   Deltacast::VideoMonitor::SharedResources& shared_resources)
        : LoopbackInputSession(config, shared_resources)
    {
    }

    void DvInputSession::prepare_video_stream()
    {
        auto& board = this->board();
        spdlog::trace("Preparing DV stream on RX{}", this->stream_id());
        disable_loopback();

        m_stream = std::make_unique<Deltacast::Wrapper::DvStream>(board.dv().open_stream(
            Deltacast::VideoMonitor::Helper::rx_index_to_streamtype(stream_id()),
            VHD_DV_STPROC_DISJOINED_VIDEO));

        spdlog::debug("Opened DV stream on RX{}", this->stream_id());

        spdlog::trace("Waiting for signal...");
        if (!Deltacast::VideoMonitor::Helper::wait_for_input(board.rx(stream_id()),
                                                             shared_resources().stop_is_requested))
        {
            if (shared_resources().stop_is_requested)
            {
                throw Deltacast::VideoMonitor::Exceptions::StopRequestedException(
                    "Stop requested while waiting for initial signal");
            }

            throw Deltacast::VideoMonitor::Exceptions::SignalDetectionException(
                "No input signal detected on the specified stream");
        }

        m_active_width = stream().active_width();
        m_active_height = stream().active_height();
        m_interlaced = stream().interlaced();
        m_framerate = stream().frame_rate();
        m_cable_color_space = stream().cable_color_space();
        m_cable_sampling = stream().cable_sampling();

        m_signal_characteristics = { .width = m_active_width,
                                     .height = m_active_height,
                                     .interlaced = static_cast<BOOL32>(m_interlaced),
                                     .framerate = m_framerate };

        spdlog::info("Detected DV signal on RX{}: {}x{}, interlaced={}, framerate={}, "
                     "colorspace={}, sampling={}",
                     this->stream_id(), m_active_width, m_active_height, m_interlaced,
                     static_cast<int>(m_framerate), static_cast<int>(m_cable_color_space),
                     static_cast<int>(m_cable_sampling));
    }

    void DvInputSession::configure_video_stream()
    {
        ensure_stream_is_prepared();

        auto& stream = this->stream();
        stream.buffer_queue().set_depth(buffer_queue_size);
        stream.set_buffer_packing(VHD_BUFPACK_VIDEO_YUV422_8);

        stream.set_active_width(m_active_width);
        stream.set_active_height(m_active_height);
        m_interlaced ? stream.set_interlaced() : stream.set_progressive();
        stream.set_frame_rate(m_framerate);
        stream.set_cable_color_space(m_cable_color_space);
        stream.set_cable_sampling(m_cable_sampling);

        set_field_merging_mode();

        spdlog::debug("Configured DV stream on RX{} with queue depth {}", this->stream_id(),
                      buffer_queue_size);
    }

    auto DvInputSession::has_video_input_changed() -> bool
    {
        auto&      stream = this->stream();
        const auto active_width_changed = stream.active_width() != m_active_width;
        const auto active_height_changed = stream.active_height() != m_active_height;
        const auto interlaced_changed = stream.interlaced() != m_interlaced;
        const auto framerate_changed = stream.frame_rate() != m_framerate;
        const auto cable_color_space_changed = stream.cable_color_space() != m_cable_color_space;
        const auto cable_sampling_changed = stream.cable_sampling() != m_cable_sampling;

        if (active_width_changed || active_height_changed || interlaced_changed ||
            framerate_changed || cable_color_space_changed || cable_sampling_changed)
        {
            spdlog::debug("DV signal change details on RX{}: {}x{}->{}x{}, interlaced {}->{}, "
                          "framerate {}->{}, colorspace {}->{}, sampling {}->{}",
                          this->stream_id(), m_active_width, m_active_height, stream.active_width(),
                          stream.active_height(), m_interlaced, stream.interlaced(),
                          static_cast<int>(m_framerate), static_cast<int>(stream.frame_rate()),
                          static_cast<int>(m_cable_color_space),
                          static_cast<int>(stream.cable_color_space()),
                          static_cast<int>(m_cable_sampling),
                          static_cast<int>(stream.cable_sampling()));
        }

        return active_width_changed || active_height_changed || interlaced_changed ||
               framerate_changed || cable_color_space_changed || cable_sampling_changed;
    }

    auto DvInputSession::get_video_characteristics()
        -> Deltacast::Wrapper::Helper::VideoCharacteristics
    {
        return m_signal_characteristics;
    }
}  // namespace Deltacast::VideoMonitor::Session