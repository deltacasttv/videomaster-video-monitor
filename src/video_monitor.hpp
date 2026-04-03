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

#include "shared_resources.hpp"
#include "exceptions.hpp"

#include <CLI/CLI.hpp>
#include <optional>

namespace Deltacast::VideoMonitor
{
    class VideoMonitorApp
    {
     public:
        VideoMonitorApp(SharedResources& shared_resources);
        int run(int argc, char** argv);

     private:
        CLI::App                                  m_app;
        Deltacast::VideoMonitor::SharedResources& m_shared_resources;
        uint32_t                                  m_device_id;
        uint32_t                                  m_stream_id;

        void init_cli();
        void init_log();
    };
}  // namespace Deltacast::VideoMonitor