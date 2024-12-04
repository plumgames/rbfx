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
struct ProfilerBasicSample::PIMPL
{
    HiresTimer timer_{};
    ea::string name_{};
};

ea::map<ea::string, unsigned> framePrev_;
ea::map<ea::string, unsigned> frameCurr_;
unsigned FrameCount = 0;

ProfilerBasicSample::ProfilerBasicSample(const char* name)
{
    pimpl_ = new PIMPL();
    pimpl_->name_ = name;
}

ProfilerBasicSample::~ProfilerBasicSample()
{
    frameCurr_[pimpl_->name_] = pimpl_->timer_.GetUSec();
    delete pimpl_;
}

void ProfilerBasicSample::EndFrame()
{
    ++FrameCount;
    framePrev_ = frameCurr_;
    frameCurr_.clear();
}

void ProfilerBasicSample::PrintFrame()
{
    ea::vector<ea::pair<ea::string, unsigned>> sortedSamples;
    for (auto& pair : framePrev_)
    {
        sortedSamples.push_back(pair);
    }

    ea::sort(sortedSamples.begin(), sortedSamples.end(),
        [](const ea::pair<ea::string, unsigned>& a, const ea::pair<ea::string, unsigned>& b)
    {
        return a.second > b.second;
    });

    ea::string msg;
    for (const auto& pair : sortedSamples)
    {
        msg += pair.first.c_str() + ea::string(": ") + ea::to_string(pair.second / 1000.0f) + '\n';
    }

    URHO3D_LOGDEBUG("***FRAME START*** ({})", FrameCount);
    URHO3D_LOGDEBUG(msg);
    URHO3D_LOGDEBUG("***FRAME END*** ({})", FrameCount);
}
#endif

}
