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

#include "shared_resources.hpp"

#include <CLI/CLI.hpp>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace Deltacast::VideoMonitor
{
    class VideoMonitorApp
    {
     public:
        VideoMonitorApp(SharedResources& shared_resources);
        auto run(int argc, char** argv) -> int;

     private:
        CLI::App                                  m_app;
        Deltacast::VideoMonitor::SharedResources& m_shared_resources;
        uint32_t                                  m_device_id = 0;
        uint32_t                                  m_stream_id = 0;
        std::optional<std::filesystem::path>      m_sdp_file_path;
        std::optional<std::string>                m_gateway;
        std::optional<bool>                       m_ip_dhcp;
        std::optional<std::string>                m_ip_address;
        std::optional<std::string>                m_ip_subnet;
        std::optional<bool>                       m_ip_sps_dhcp;
        std::optional<std::string>                m_ip_sps_address;
        std::optional<std::string>                m_ip_sps_subnet;
        std::string                               m_log_level = "info";
        std::filesystem::path                     m_log_directory = ".";

        void init_cli();
        void init_log();

        [[nodiscard]] auto check_device_id() const -> bool;
        [[nodiscard]] auto check_stream_id() const -> bool;
    };
}  // namespace Deltacast::VideoMonitor