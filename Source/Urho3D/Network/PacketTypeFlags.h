//
// Copyright (c) 2017-2022 the rbfx project.
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
#pragma once

#include <Urho3D/Container/FlagSet.h>

namespace Urho3D
{

/// Packet types for outgoing buffers. Outgoing messages are grouped by their type
enum PacketType : uint16_t
{
    Reliable            = 1u << 0,
    Ordered             = 1u << 1,
    UnreliableUnordered = 0,
    UnreliableOrdered   = Ordered,
    ReliableUnordered   = Reliable,
    ReliableOrdered     = Reliable | Ordered,
};
URHO3D_FLAGSET(PacketType, PacketTypeFlags);
//static_assert(sizeof(PacketType) == sizeof(NetworkMessageId));

enum PacketTargetType : uint8_t
{
    Default,
    RelaySelf,
    RelayOthers,
    RelayAll
};

}
