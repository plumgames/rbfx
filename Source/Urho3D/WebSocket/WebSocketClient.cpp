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
    URHO3D_LOGDEBUG("WebSocketClient::Ctor");

    workQueue_ = GetSubsystem<WorkQueue>();

    WeakPtr<WebSocketClient> self(this);

    ws_ = ea::make_unique<WebSocket>();
    ws_->onOpen(
        [=]()
    {
        workQueue_->PostTaskForMainThread(
            [=]()
        {
            if (!self.Expired())
            {
                URHO3D_LOGDEBUG("<- OPENED {}", url_);
                onOpen_();
            }
        });
    });
    ws_->onClosed(
        [=]()
    {
        workQueue_->PostTaskForMainThread(
            [=]()
        {
            if (!self.Expired())
            {
                URHO3D_LOGDEBUG("<- CLOSED {}", url_);
                onClosed_();
            }
        });
    });
    ws_->onError(
        [=](std::string error)
    {
        workQueue_->PostTaskForMainThread(
            [=]()
        {
            if (!self.Expired())
            {
                URHO3D_LOGDEBUG("<- ERROR {} {}", url_, error);
                onError_(error.c_str());
            }
        });
    });
    ws_->onMessage(
        [=](binary msg)
    {
        workQueue_->PostTaskForMainThread(
            [=]()
        {
            if (!self.Expired())
            {
                VectorBuffer buffer(msg.data(), msg.size());
                onMessageBinary_(buffer);
            }
        });
    },
        [=](std::string msg)
    {
        workQueue_->PostTaskForMainThread(
            [=]()
        {
            if (!self.Expired())
            {
                onMessageString_(msg.c_str());
            }
        });
    });
    ws_->onBufferedAmountLow(
        [=]() { workQueue_->PostTaskForMainThread([=]() { URHO3D_LOGWARNING("OnWSBufferedAmountLow"); }); });
}

WebSocketClient::~WebSocketClient()
{
    URHO3D_LOGDEBUG("WebSocketClient::Dtor");

    ws_->onOpen([](){});
    ws_->onClosed([](){});
    ws_->onError([](std::string error){});
    ws_->onMessage([](binary msg){}, [](std::string msg){});
    ws_->onBufferedAmountLow([]() { });
    ws_ = nullptr;
}

void WebSocketClient::Open(const ea::string& url)
{
    url_ = url;
    URHO3D_LOGDEBUG("=> {}", url_);
    ws_->open(url_.c_str());
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
