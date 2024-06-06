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
#include <Urho3D/Network/Transport/NetworkServer.h>

namespace Urho3D
{
class AppConnection;

class URHO3D_API AppServer : public NetworkServer
{
    URHO3D_OBJECT(AppServer, NetworkServer);
public:
    explicit AppServer(Context* context);
    static void RegisterObject(Context* context);
    bool Listen(const URL&) override;
    void Stop() override;

private:
    void HandleConnected(StringHash eventType, VariantMap& eventData);
    void HandleDisconnected(StringHash eventType, VariantMap& eventData);
    void HandleMessage(StringHash eventType, VariantMap& eventData);

    void OnConnected(AppConnection* connection);
    void OnDisconnected(AppConnection* connection);
    void OnMessage(AppConnection* connection, const ea::string& data);

    struct ConnectionLink
    {
        SharedPtr<AppConnection> client_;
        SharedPtr<AppConnection> server_;
    };
    ea::map<SharedPtr<AppConnection>, ea::shared_ptr<ConnectionLink>> connections_;
};

}   // namespace Urho3D
