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
#include "helper.hpp"
#include "sdi_dv_input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/board/board.hpp>
#include <VideoMasterCppApi/helper/dv.hpp>
#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterCppApi/stream/dv/dv_stream.hpp>
#include <VideoMasterHD_Core.h>
#include <VideoMasterHD_Dv.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace Deltacast::VideoMonitor::Session
{
    DvInputSession::DvInputSession(const DvInputSessionConfiguration&        config,
                                   Deltacast::VideoMonitor::SharedResources& shared_resources)
        : SdiDvInputSession(config, shared_resources)
    {
    }

    void DvInputSession::prepare_stream()
    {
        auto& board = this->board();
        disable_loopback();

        m_stream = std::make_unique<Deltacast::Wrapper::DvStream>(board.dv().open_stream(
            Deltacast::VideoMonitor::Helper::rx_index_to_streamtype(stream_id()),
            VHD_DV_STPROC_DISJOINED_VIDEO));

        spdlog::info("Waiting for signal...");
        if (!Deltacast::VideoMonitor::Helper::wait_for_input(board.rx(stream_id()),
                                                             shared_resources().stop_is_requested))
        {
            throw std::runtime_error("No input signal detected on the specified stream");
        }

        m_active_width = stream().active_width();
        m_active_height = stream().active_height();
        m_interlaced = stream().interlaced();
        m_framerate = stream().frame_rate();
        m_cable_color_space = stream().cable_color_space();
        m_cable_sampling = stream().cable_sampling();

        m_signal_characteristics = { m_active_width, m_active_height,
                                     static_cast<BOOL32>(m_interlaced), m_framerate };
    }

    void DvInputSession::configure_stream()
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
    }

    auto DvInputSession::has_input_changed() -> bool
    {
        auto& stream = this->stream();
        return (stream.active_width() != m_active_width ||
                stream.active_height() != m_active_height || stream.interlaced() != m_interlaced ||
                stream.frame_rate() != m_framerate ||
                stream.cable_color_space() != m_cable_color_space ||
                stream.cable_sampling() != m_cable_sampling);
    }

    auto DvInputSession::get_video_characteristics()
        -> Deltacast::Wrapper::Helper::VideoCharacteristics
    {
        return m_signal_characteristics;
    }
}  // namespace Deltacast::VideoMonitor::Session