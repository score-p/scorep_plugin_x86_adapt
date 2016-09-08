/*
 * Copyright (c) 2016, Technische Universität Dresden, Germany
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <x86_adapt_cxx/x86_adapt.hpp>

#include <scorep/plugin/plugin.hpp>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <sched.h>
}

class recorder_thread
{
public:
    recorder_thread(x86_adapt::device&& device, std::chrono::nanoseconds interval)
    : device_(device), interval_(interval)
    {
    }

    void loop(const std::vector<x86_adapt::configuration_item>& cis,
              std::map<x86_adapt::configuration_item,
                       std::vector<std::pair<scorep::chrono::ticks, std::uint64_t>>>& timelines,
              cpu_set_t cpumask)
    {
        sched_setaffinity(0, sizeof(cpu_set_t), &cpumask);

        scorep::plugin::log::logging::debug() << "Entered measurement loop on CPU #"
                                              << device_.id();
        while (looping.load())
        {
            for (const auto& ci : cis)
            {
                timelines[ci].emplace_back(scorep::chrono::measurement_clock::now(), device_(ci));
            }

            std::this_thread::sleep_for(interval_);
        }
        scorep::plugin::log::logging::debug() << "Leaving measurement loop on CPU #"
                                              << device_.id();
    }

    void start(const std::vector<x86_adapt::configuration_item>& cis,
               std::map<x86_adapt::configuration_item,
                        std::vector<std::pair<scorep::chrono::ticks, std::uint64_t>>>& timelines)
    {
        looping.store(true);

        cpu_set_t cpumask;
        sched_getaffinity(0, sizeof(cpu_set_t), &cpumask);

        thread_ = std::make_unique<std::thread>(&recorder_thread::loop, this, std::ref(cis),
                                                std::ref(timelines), cpumask);
    }

    void stop()
    {
        looping.store(false);

        thread_->join();

        thread_.reset(nullptr);
    }

private:
    x86_adapt::device device_;
    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> looping;
    std::chrono::nanoseconds interval_;
};

using namespace scorep::plugin::policy;

template <typename T, typename Policies>
using handle_oid_policy = object_id<x86_adapt::configuration_item, T, Policies>;

class x86_adapt_plugin : public scorep::plugin::base<x86_adapt_plugin, async, post_mortem,
                                                     scorep_clock, per_thread, handle_oid_policy>
{
    static bool is_pinned()
    {
        cpu_set_t cpumask;
        sched_getaffinity(0, sizeof(cpu_set_t), &cpumask);

        return CPU_COUNT(&cpumask) == 1;
    }

    static int get_current_cpu()
    {
        int res = sched_getcpu();

        if (res < 0)
        {
            scorep::exception::raise("Failed to get current cpu for current thread");
        }

        return res;
    }

public:
    x86_adapt_plugin()
    {
        scorep::plugin::log::logging::info() << "Plugin loaded.";
    }

    std::vector<scorep::plugin::metric_property> get_metric_properties(const std::string& knob_name)
    {
        auto configuration_item = x86_adapt_.cpu_configuration_items().lookup(knob_name);

        make_handle(knob_name, configuration_item);

        scorep::plugin::log::logging::debug() << "Added new metric for Knob: '" << knob_name << "'";

        return { scorep::plugin::metric_property(knob_name, configuration_item.description(), "#")
                     .absolute_point()
                     .value_uint() };
    }

    void add_metric(x86_adapt::configuration_item& ci)
    {
        if (!is_pinned())
        {
            scorep::plugin::log::logging::fatal()
                << "This plugin requires a immutable one to one mapping between threads and cpus.";

            scorep::exception::raise("Thread is not pinned to one specific CPU. Cannot continue.");
        }

        int cpu = get_current_cpu();

        auto device = x86_adapt_.cpu(cpu);

        std::lock_guard<std::mutex> lock(init_mutex);

        scorep::plugin::log::logging::debug()
            << "Create data structures for recorder threads on CPU #" << cpu;

        recorders_.emplace(cpu, std::make_unique<recorder_thread>(std::move(device),
                                                                  std::chrono::milliseconds(50)));
        (void)values_[cpu];
    }

    void start()
    {
        int cpu = get_current_cpu();
        scorep::plugin::log::logging::debug() << "Starting measurement thread for CPU #" << cpu;
        recorders_[cpu]->start(get_handles(), values_[cpu]);
    }

    void stop()
    {
        int cpu = get_current_cpu();
        scorep::plugin::log::logging::debug() << "Stopped measurement thread for CPU #" << cpu;
        recorders_[cpu]->stop();
    }

    template <typename Cursor>
    void get_all_values(x86_adapt::configuration_item& knob, Cursor& c)
    {
        scorep::plugin::log::logging::debug() << "Get values called on thread for CPU #"
                                              << get_current_cpu();
        auto timeline = values_[get_current_cpu()][knob];

        for (const auto& entry : timeline)
        {
            c << entry;
        }
    }

public:
    x86_adapt::x86_adapt x86_adapt_;
    std::mutex init_mutex;
    std::map<int, std::unique_ptr<recorder_thread>> recorders_;
    std::map<int, std::map<x86_adapt::configuration_item,
                           std::vector<std::pair<scorep::chrono::ticks, std::uint64_t>>>>
        values_;
};
