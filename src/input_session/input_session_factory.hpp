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

#include "input_session_base.hpp"
#include "ip_input_configuration.hpp"
#include "shared_resources.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace Deltacast::VideoMonitor::Session
{
    class InputSessionFactory
    {
     public:
        static auto
        create_input_session(uint32_t device_id, uint32_t stream_id,
                             std::optional<std::filesystem::path> sdp_file_path,
                             std::optional<Deltacast::VideoMonitor::Session::IpNetworkConfiguration>
                                 ip_network_configuration,
                             std::optional<Deltacast::VideoMonitor::Session::IpInputConfiguration>
                                                                       ip_media_configuration,
                             Deltacast::VideoMonitor::SharedResources& shared_resources)
            -> std::unique_ptr<Deltacast::VideoMonitor::Session::InputSessionBase>;
    };
}  // namespace Deltacast::VideoMonitor::Session