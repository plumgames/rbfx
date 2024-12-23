#include "../Precompiled.h"

#include <Urho3D/WebSocket/WebSocketClient.h>
#include <Urho3D/IO/VectorBuffer.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Core/WorkQueue.h>

#include <rtc/websocket.hpp>

namespace Urho3D
{
WebSocketClient::WebSocketClient(Context* context)
    : BaseClassName(context)
{
    workQueue_ = GetSubsystem<WorkQueue>();

    ws_ = ea::make_unique<WebSocket>();
    ws_->onOpen([=]() { workQueue_->PostTaskForMainThread([=]() { onOpen_(); }); });
    ws_->onClosed([=]() { workQueue_->PostTaskForMainThread([=]() { onClosed_(); }); });
    ws_->onError([=](std::string error) { workQueue_->PostTaskForMainThread([=]() { onError_(error.c_str()); }); });
    ws_->onMessage(
        [=](binary msg) 
        {
        workQueue_->PostTaskForMainThread(
            [=]()
        {
            VectorBuffer buffer(msg.data(), msg.size());
            onMessageBinary_(buffer);
        });
        },
        [=](std::string msg) { workQueue_->PostTaskForMainThread([=]() { onMessageString_(msg.c_str()); }); }
    );
    ws_->onBufferedAmountLow(
        [=]()
    {
        workQueue_->PostTaskForMainThread([=]() { URHO3D_LOGWARNING("OnWSBufferedAmountLow"); });
    });
}

WebSocketClient::~WebSocketClient()
{
}

void WebSocketClient::Open(const ea::string& url)
{
    ws_->open(url.c_str());
}

void WebSocketClient::Close()
{
    ws_->close();
}

bool WebSocketClient::Send(const VectorBuffer& msg)
{
    return ws_->send(reinterpret_cast<const std::byte*>(msg.GetData()), msg.GetSize());
}

bool WebSocketClient::IsOpen() const
{
    return ws_->isOpen();
}

bool WebSocketClient::IsClosed() const
{
    return ws_->isClosed();
}

} // namespace Urho3D
