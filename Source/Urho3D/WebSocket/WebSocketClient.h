#pragma once

#include <Urho3D/Core/Object.h>

#include <rtc/common.hpp>

namespace rtc
{
class WebSocket;
}

using namespace rtc;

namespace Urho3D
{
class WorkQueue;

class URHO3D_API WebSocketClient : public Object
{
    URHO3D_OBJECT(WebSocketClient, Object);

public:
    WebSocketClient(Context* context);
    ~WebSocketClient() override;

    void Open(const ea::string& url);
    void Close();
    bool Send(const VectorBuffer& msg);

    bool IsOpen() const;
    bool IsClosed() const;

    ea::function<void()> onOpen_;
    ea::function<void()> onClosed_;
    ea::function<void(ea::string)> onError_;
    ea::function<void(const ea::string&)> onMessageString_;
    ea::function<void(VectorBuffer&)> onMessageBinary_;
private:
    ea::unique_ptr<WebSocket> ws_;
    SharedPtr<WorkQueue> workQueue_;
};

}
