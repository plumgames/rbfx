//
// Copyright (c) 2017-2020 the rbfx project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include <stdint.h>

#if URHO3D_PROFILING
#include <TracyClient.cpp>
#if _WIN32
#   include <windows.h>
#else
#   include "pthread.h"
#endif
#endif
#include "Profiler.h"
#if URHO3D_PROFILING_BASIC
#include <Urho3D/Core/Timer.h>
#include <Urho3D/IO/Log.h>
#include <EASTL/map.h>
#include <EASTL/string.h>
#endif

namespace Urho3D
{

void SetProfilerThreadName(const char* name)
{
#if URHO3D_PROFILING
    tracy::SetThreadName(name);
#endif
}
    
#ifdef URHO3D_PROFILING_BASIC
struct Entry
{
    float ms_ = 0;
    unsigned count_ = 0;
    bool render = false;
    bool update = false;
};

ea::stack<ProfilerBasicSample*> samples_;
ea::map<ea::string, Entry> framePrev_;
ea::map<ea::string, Entry> frameCurr_;
unsigned frameCount_ = 0;
bool updateSeen_ = false;
bool renderSeen_ = false;

struct ProfilerBasicSample::PIMPL
{
    HiresTimer timer_{};
    ea::string name_{};
    bool ended_ = false;
    bool update_ = false;
    bool render_ = false;

    void End()
    {
        if (ended_)
        {
            return;
        }

        Entry& entry = frameCurr_[name_];
        entry.ms_ += timer_.GetUSec() / 1000.0f;
        entry.update = update_;
        entry.render = render_;
        ++entry.count_;
        ended_ = true;
    }
};

ProfilerBasicSample::ProfilerBasicSample(const char* file, int line, const char* func, const char* name)
{
    samples_.push(this);
    pimpl_ = new PIMPL();
    pimpl_->name_ = fmt::format("{}_{}:{}", func, name, line).c_str();
    // pimpl_->name_ = fmt::format("{}:{}_{}_{}", file, line, func, name).c_str();

    if (!updateSeen_ && strcmp(name, "Update") == 0)
    {
        pimpl_->update_ = true;
        updateSeen_ = true;
    }

    if (!renderSeen_ && strcmp(name, "Render") == 0)
    {
        pimpl_->render_ = true;
        renderSeen_ = true;
    }
}

ProfilerBasicSample::~ProfilerBasicSample()
{
    samples_.pop();
    pimpl_->End();
    delete pimpl_;
}

void ProfilerBasicSample::EndFrame()
{
    URHO3D_ASSERT(samples_.size() == 1);
    samples_.top()->pimpl_->End();
    ++frameCount_;
    framePrev_ = frameCurr_;
    frameCurr_.clear();
    updateSeen_ = false;
    renderSeen_ = false;
}

void ProfilerBasicSample::PrintFrame()
{
    ea::vector<ea::pair<ea::string, Entry>> ranked;
    for (auto& pair : framePrev_)
    {
        ranked.push_back(pair);
    }

    float update = 0;
    float render = 0;

    ea::sort(ranked.begin(), ranked.end(),
        [&update, &render](const ea::pair<ea::string, Entry>& a, const ea::pair<ea::string, Entry>& b)
    {
        const Entry& entryA = a.second;
        const Entry& entryB = b.second;
        if (a.second.update)
        {
            update = entryA.ms_;
        }
        if (a.second.render)
        {
            render = entryA.ms_;
        }
        return entryA.ms_ > entryB.ms_;
    });

    ea::string msg = "\n";
    for (const auto& pair : ranked)
    {
        const Entry& entry = pair.second;
        const ea::string& name = pair.first;
        msg += fmt::format("{:.3f}({}) {}\n", entry.ms_, entry.count_, name).c_str();
    }

    URHO3D_LOGDEBUG("***PROFILER FRAME START*** ({}) U:{:.3f} R:{:.3f}", frameCount_, update, render);
    URHO3D_LOGDEBUG(msg);
    URHO3D_LOGDEBUG("***PROFILER FRAME END*** ({})", frameCount_);
}
#endif

}
