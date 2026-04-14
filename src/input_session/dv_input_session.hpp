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

#include "sdi_dv_input_session.hpp"
#include "shared_resources.hpp"

#include <VideoMasterCppApi/helper/video.hpp>
#include <VideoMasterCppApi/stream/dv/dv_stream.hpp>
#include <VideoMasterHD_Core.h>
#include <VideoMasterHD_Dv.h>

namespace Deltacast::VideoMonitor::Session
{
    struct DvInputSessionConfiguration : public SdiDvInputSessionConfiguration
    {
    };

    class DvInputSession : public SdiDvInputSession<Deltacast::Wrapper::DvStream>
    {
     public:
        explicit DvInputSession(const DvInputSessionConfiguration&        config,
                                Deltacast::VideoMonitor::SharedResources& shared_resources);
        virtual ~DvInputSession() = default;
        void prepare_stream() override;
        void configure_stream() override;
        auto get_video_characteristics()
            -> Deltacast::Wrapper::Helper::VideoCharacteristics override;

     private:
        auto            has_input_changed() -> bool override;
        unsigned int    m_active_width{};
        unsigned int    m_active_height{};
        bool            m_interlaced{};
        unsigned int    m_framerate{};
        VHD_DV_CS       m_cable_color_space{};
        VHD_DV_SAMPLING m_cable_sampling{};

        Deltacast::Wrapper::Helper::VideoCharacteristics m_signal_characteristics{};
    };
}  // namespace Deltacast::VideoMonitor::Session