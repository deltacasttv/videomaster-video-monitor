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
#include <VideoMasterCppApi/stream/sdi/sdi_stream.hpp>
#include <VideoMasterHD_Core.h>
#include <VideoMasterHD_Sdi.h>

namespace Deltacast::VideoMonitor::Session
{
    struct SdiInputSessionConfiguration : public SdiDvInputSessionConfiguration
    {
    };

    class SdiInputSession : public SdiDvInputSession<Deltacast::Wrapper::SdiStream>
    {
     public:
        explicit SdiInputSession(const SdiInputSessionConfiguration&       config,
                                 Deltacast::VideoMonitor::SharedResources& shared_resources);
        virtual ~SdiInputSession() = default;
        void prepare_video_stream() override;
        void configure_video_stream() override;
        auto get_video_characteristics()
            -> Deltacast::Wrapper::Helper::VideoCharacteristics override;

     private:
        auto has_video_input_changed() -> bool override;

        VHD_VIDEOSTANDARD m_video_standard{};
        VHD_CLOCKDIVISOR  m_clock_divisor{};
        VHD_INTERFACE     m_interface{};
    };
}  // namespace Deltacast::VideoMonitor::Session