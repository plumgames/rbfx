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

#include <Urho3D/Network/Transport/Event/EventConnection.h>
#include <Urho3D/Core/Context.h>

namespace Urho3D
{

EventConnection::EventConnection(Context* context)
    : NetworkConnection(context)
{
}

EventConnection::~EventConnection()
{
    URHO3D_ASSERT(state_ == NetworkConnection::State::Disconnected);
}

void EventConnection::RegisterObject(Context* context)
{
    context->AddAbstractReflection<EventConnection>(Category_Network);
}

bool EventConnection::Connect(const URL&)
{
    using namespace EventConnectionConnected;
    VariantMap& eventData = GetEventDataMap();
    eventData[P_EVENTCONNECTION] = this;
    SendEvent(E_EVENTCONNCONNECTED, eventData);
    return true;
}

void EventConnection::Disconnect()
{
    using namespace EventConnectionDisconnected;
    VariantMap& eventData = GetEventDataMap();
    eventData[P_EVENTCONNECTION] = this;
    SendEvent(E_EVENTCONNDISCONNECTED, eventData);
}

void EventConnection::SendMessage(ea::string_view data, PacketTypeFlags)
{
    using namespace EventConnectionMessage;
    VariantMap& eventData = GetEventDataMap();
    eventData[P_EVENTCONNECTION] = this;
    eventData[P_DATA] = ea::string(data);
    SendEvent(E_EVENTCONNMESSAGE, eventData);
}

void EventConnection::OnMessage(const ea::string_view& data)
{
    if (!onMessage_)
    {
        return;
    }

    onMessage_(data);
}

void EventConnection::OnConnected()
{
    if (state_ == NetworkConnection::State::Connected)
    {
        return;
    }

    state_ = NetworkConnection::State::Connected;

    if (onConnected_)
    {
        onConnected_();
    }
}

void EventConnection::OnDisconnected()
{
    if (state_ == NetworkConnection::State::Disconnected)
    {
        return;
    }

    state_ = NetworkConnection::State::Disconnected;

    if (onDisconnected_)
    {
        onDisconnected_();
    }
}

}   // namespace Urho3D
