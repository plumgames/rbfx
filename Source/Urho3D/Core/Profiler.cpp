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
#if URHO3D_PROFILING_DEVICE
#include <Urho3D/Core/Timer.h>
#include <Urho3D/Core/Thread.h>
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

#ifdef URHO3D_PROFILING_DEVICE
struct Entry
{
    float ms_ = 0;
    unsigned count_ = 0;
    bool renderScope_ = false;
    bool updateScope_ = false;
};

ea::stack<ProfilerDeviceSample*> samples_;
ea::map<ea::string, Entry> framePrev_;
ea::map<ea::string, Entry> frameCurr_;
unsigned frameCount_ = 0;
bool updateScopeSeen_ = false;
bool renderScopeSeen_ = false;
float updateScopeMs_ = 0;
float renderScopeMs_ = 0;

struct ProfilerDeviceSample::PIMPL
{
    HiresTimer timer_{};
    ea::string name_{};
    float durationMs = 0;
    bool ended_ = false;
    bool updateScope_ = false;
    bool renderScope_ = false;

    void End()
    {
        URHO3D_ASSERT(Thread::IsMainThread());

        if (ended_)
        {
            return;
        }

        Entry& entry = frameCurr_[name_];
        entry.ms_ += timer_.GetUSec() / 1000.0f;
        entry.updateScope_ = updateScope_;
        entry.renderScope_ = renderScope_;
        ++entry.count_;
        ended_ = true;
        durationMs = entry.ms_;
    }
};

ProfilerDeviceSample::ProfilerDeviceSample(const char* file, int line, const char* func, const char* name)
{
    URHO3D_ASSERT(Thread::IsMainThread());

    samples_.push(this);
    pimpl_ = new PIMPL();
    pimpl_->name_ = fmt::format("{}_{}:{}", func, name, line).c_str();
    // pimpl_->name_ = fmt::format("{}:{}_{}_{}", file, line, func, name).c_str();

    if (!updateScopeSeen_ && strcmp(name, "Update") == 0)
    {
        pimpl_->updateScope_ = true;
        updateScopeSeen_ = true;
    }

    if (!renderScopeSeen_ && strcmp(name, "Render") == 0)
    {
        pimpl_->renderScope_ = true;
        renderScopeSeen_ = true;
    }
}

ProfilerDeviceSample::~ProfilerDeviceSample()
{
    samples_.pop();
    pimpl_->End();
    delete pimpl_;
}

void ProfilerDeviceSample::EndFrame()
{
    URHO3D_ASSERT(Thread::IsMainThread());

    float frameMs = 0;
    if (samples_.size() == 1)
    {
        auto& firstFrame = samples_.top();
        firstFrame->pimpl_->End();
        frameMs = firstFrame->pimpl_->durationMs;
    }
    ++frameCount_;
    framePrev_ = frameCurr_;
    frameCurr_.clear();
    updateScopeSeen_ = false;
    renderScopeSeen_ = false;

    for (auto& pair : framePrev_)
    {
        const Entry& e = pair.second;
        if (e.updateScope_)
        {
            updateScopeMs_ = e.ms_;
        }
        if (e.renderScope_)
        {
            renderScopeMs_ = e.ms_;
        }
    }

    const float stallFrameMs = 500;
    if (frameMs >= stallFrameMs)
    {
        URHO3D_LOGWARNING("FRAME SPIKE! ({}ms)", frameMs);
        PrintFrame();
    }
}

void ProfilerDeviceSample::PrintFrame()
{
    URHO3D_ASSERT(Thread::IsMainThread());

    ea::vector<ea::pair<ea::string, Entry>> ranked;
    for (auto& pair : framePrev_)
    {
        ranked.push_back(pair);
    }

    ea::sort(ranked.begin(), ranked.end(),
        [=](const ea::pair<ea::string, Entry>& a, const ea::pair<ea::string, Entry>& b)
    {
        const Entry& entryA = a.second;
        const Entry& entryB = b.second;
        return entryA.ms_ > entryB.ms_;
    });

    ea::string msg = "\n";
    for (const auto& pair : ranked)
    {
        const Entry& entry = pair.second;
        const ea::string& name = pair.first;
        msg += fmt::format("{:.3f}({}) {}\n", entry.ms_, entry.count_, name).c_str();
    }

    URHO3D_LOGDEBUG("***PROFILER FRAME START*** ({}) U:{:.3f} R:{:.3f}", frameCount_, updateScopeMs_, renderScopeMs_);
    URHO3D_LOGDEBUG(msg);
    URHO3D_LOGDEBUG("***PROFILER FRAME END*** ({})", frameCount_);
}

float ProfilerDeviceSample::GetFrameUpdateMs()
{
    return updateScopeMs_;
}

float ProfilerDeviceSample::GetFrameRenderMs()
{
    return renderScopeMs_;
}
#endif
} // namespace Urho3D
