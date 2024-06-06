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
#include <Urho3D/Network/Transport/App/AppServer.h>
#include <Urho3D/Network/Transport/App/AppConnection.h>

namespace Urho3D
{

AppServer::AppServer(Context* context)
    : NetworkServer(context)
{
    SubscribeToEvent(E_APPCONNCONNECTED, URHO3D_HANDLER(AppServer, HandleConnected));
    SubscribeToEvent(E_APPCONNDISCONNECTED, URHO3D_HANDLER(AppServer, HandleDisconnected));
    SubscribeToEvent(E_APPCONNMESSAGE, URHO3D_HANDLER(AppServer, HandleMessage));
}

void AppServer::RegisterObject(Context* context)
{
    context->AddAbstractReflection<AppServer>(Category_Network);
}

bool AppServer::Listen(const URL&)
{
    return true;
}

void AppServer::Stop()
{
    while (!connections_.empty())
    {
        OnDisconnected(connections_.begin()->first);
    }
}

void AppServer::HandleConnected(StringHash eventType, VariantMap& eventData)
{
    using namespace AppConnectionConnected;
    auto* connection = static_cast<AppConnection*>(eventData[P_APPCONNECTION].GetPtr());
    OnConnected(connection);
}

void AppServer::HandleDisconnected(StringHash eventType, VariantMap& eventData)
{
    using namespace AppConnectionDisconnected;
    auto* connection = static_cast<AppConnection*>(eventData[P_APPCONNECTION].GetPtr());
    OnDisconnected(connection);
}

void AppServer::HandleMessage(StringHash eventType, VariantMap& eventData)
{
    using namespace AppConnectionMessage;
    auto* connection = static_cast<AppConnection*>(eventData[P_APPCONNECTION].GetPtr());
    auto message = eventData[P_DATA].GetString();
    OnMessage(connection, message);
}

void AppServer::OnConnected(AppConnection* connection)
{
    auto clientConnection = SharedPtr<AppConnection>(connection);
    const auto itr = connections_.find(clientConnection);
    if (itr != connections_.end())
    {
        return;
    }

    auto link = ea::make_shared<ConnectionLink>();
    link->client_ = clientConnection;
    link->server_ = MakeShared<AppConnection>(context_);

    connections_.insert(ea::make_pair(link->client_, link));
    connections_.insert(ea::make_pair(link->server_, link));

    if (onConnected_)
    {
        onConnected_(link->server_);
    }

    link->server_->OnConnected();
    link->client_->OnConnected();
}

void AppServer::OnDisconnected(AppConnection* c)
{
    auto connection = SharedPtr<AppConnection>(c);
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

void AppServer::OnMessage(AppConnection* c, const ea::string& data)
{
    auto connection = SharedPtr<AppConnection>(c);
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
