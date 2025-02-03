//
// Copyright (c) 2017-2024 the rbfx project.
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

#include <Urho3D/Core/Object.h>
#include <Urho3D/Network/AbstractConnection.h>
#include <Urho3D/Core/URL.h>
#include <Urho3D/Network/Transport/NetworkConnection.h>

namespace Urho3D
{

URHO3D_EVENT(E_EVENTCONNCONNECTED, EventConnectionConnected)
{
    URHO3D_PARAM(P_EVENTCONNECTION, EventConnection); // EventConnection pointer
}

URHO3D_EVENT(E_EVENTCONNDISCONNECTED, EventConnectionDisconnected)
{
    URHO3D_PARAM(P_EVENTCONNECTION, EventConnection); // EventConnection pointer
}

URHO3D_EVENT(E_EVENTCONNMESSAGE, EventConnectionMessage)
{
    URHO3D_PARAM(P_EVENTCONNECTION, EventConnection); // EventConnection pointer
    URHO3D_PARAM(P_DATA, Data); // String
}

class URHO3D_API EventConnection : public NetworkConnection
{
    URHO3D_OBJECT(EventConnection, NetworkConnection);
public:
    explicit EventConnection(Context* context);
    ~EventConnection();
    static void RegisterObject(Context* context);
    bool Connect(const URL&) override;
    void Disconnect() override;
    void SendMessage(ea::string_view data, PacketTypeFlags) override;
    unsigned GetMaxMessageSize() const override { return 1024; }

    void OnConnected();
    void OnDisconnected();
    void OnMessage(const ea::string_view& data);
};

}   // namespace Urho3D
