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

#include <Urho3D/Core/Context.h>
#include <Urho3D/Network/Transport/Event/EventServer.h>
#include <Urho3D/Network/Transport/Event/EventConnection.h>

namespace Urho3D
{

EventServer::EventServer(Context* context)
    : NetworkServer(context)
{
    SubscribeToEvent(E_EVENTCONNCONNECTED, URHO3D_HANDLER(EventServer, HandleConnected));
    SubscribeToEvent(E_EVENTCONNDISCONNECTED, URHO3D_HANDLER(EventServer, HandleDisconnected));
    SubscribeToEvent(E_EVENTCONNMESSAGE, URHO3D_HANDLER(EventServer, HandleMessage));
}

void EventServer::RegisterObject(Context* context)
{
    context->AddAbstractReflection<EventServer>(Category_Network);
}

bool EventServer::Listen(const URL&)
{
    return true;
}

void EventServer::Stop()
{
    while (!connections_.empty())
    {
        OnDisconnected(connections_.begin()->first);
    }
}

void EventServer::HandleConnected(StringHash eventType, VariantMap& eventData)
{
    using namespace EventConnectionConnected;
    auto* connection = static_cast<EventConnection*>(eventData[P_EVENTCONNECTION].GetPtr());
    OnConnected(connection);
}

void EventServer::HandleDisconnected(StringHash eventType, VariantMap& eventData)
{
    using namespace EventConnectionDisconnected;
    auto* connection = static_cast<EventConnection*>(eventData[P_EVENTCONNECTION].GetPtr());
    OnDisconnected(connection);
}

void EventServer::HandleMessage(StringHash eventType, VariantMap& eventData)
{
    using namespace EventConnectionMessage;
    auto* connection = static_cast<EventConnection*>(eventData[P_EVENTCONNECTION].GetPtr());
    auto message = eventData[P_DATA].GetString();
    OnMessage(connection, message);
}

void EventServer::OnConnected(EventConnection* connection)
{
    auto clientConnection = SharedPtr<EventConnection>(connection);
    const auto itr = connections_.find(clientConnection);
    if (itr != connections_.end())
    {
        return;
    }

    auto link = ea::make_shared<ConnectionLink>();
    link->client_ = clientConnection;
    link->server_ = MakeShared<EventConnection>(context_);

    connections_.insert(ea::make_pair(link->client_, link));
    connections_.insert(ea::make_pair(link->server_, link));

    if (onConnected_)
    {
        onConnected_(link->server_);
    }

    link->server_->OnConnected();
    link->client_->OnConnected();
}

void EventServer::OnDisconnected(EventConnection* c)
{
    auto connection = SharedPtr<EventConnection>(c);
    const auto itr = connections_.find(connection);
    if (itr == connections_.end())
    {
        return;
    }

    auto link = itr->second;

    if (onDisconnected_)
    {
        onDisconnected_(link->server_);
    }

    link->server_->OnDisconnected();
    link->client_->OnDisconnected();

    connections_.erase(link->server_);
    connections_.erase(link->client_);
}

void EventServer::OnMessage(EventConnection* c, const ea::string& data)
{
    auto connection = SharedPtr<EventConnection>(c);
    const auto itr = connections_.find(connection);
    if (itr == connections_.end())
    {
        return;
    }

    auto link = itr->second;
    auto linkedConnection = connection == link->client_ ? link->server_ : link->client_;
    linkedConnection->OnMessage(data);
}

}
