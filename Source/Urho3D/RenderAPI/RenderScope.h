// Copyright (c) 2023-2023 the rbfx project.
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT> or the accompanying LICENSE file.

#pragma once

#include "Urho3D/Core/Format.h"
#include "Urho3D/Core/NonCopyable.h"
#include "Urho3D/Urho3D.h"

namespace Urho3D
{

class RenderContext;

/// Utility class to add debug scope markers.
class URHO3D_API RenderScope : public NonCopyable
{
public:
    explicit RenderScope(RenderContext* renderContext, ea::string_view name)
#ifdef URHO3D_DEBUG_GRAPHICS_SCOPES
        : renderContext_(renderContext)
#endif
    {
#ifdef URHO3D_DEBUG_GRAPHICS_SCOPES
        if (renderContext_)
            BeginGroup(name);
#endif
    }

    template <class ... T>
    explicit RenderScope(RenderContext* renderContext, ea::string_view format, T&& ... args)
#ifdef URHO3D_DEBUG_GRAPHICS_SCOPES
        : renderContext_(renderContext)
#endif
    {
#ifdef URHO3D_DEBUG_GRAPHICS_SCOPES
        if (renderContext_)
            BeginGroup(Format(format, args...));
#endif
    }

    ~RenderScope()
    {
#ifdef URHO3D_DEBUG_GRAPHICS_SCOPES
        if (renderContext_)
            EndGroup();
#endif
    }

#ifdef URHO3D_DEBUG_GRAPHICS_SCOPES
private:
    void BeginGroup(ea::string_view name);
    void EndGroup();
#endif

#ifdef URHO3D_DEBUG_GRAPHICS_SCOPES
    RenderContext* renderContext_{};
#endif
};

} // namespace Urho3D
