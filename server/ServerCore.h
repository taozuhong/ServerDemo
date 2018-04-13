/***********************************************************************/
/*
** 2018 April 10
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
**    Author: taozuhong@gmail.com (Andy Tao)
**
*************************************************************************/

#ifndef TIMESVR_SERVERCORE_H
#define TIMESVR_SERVERCORE_H

#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <google/protobuf/message.h>
#include <string>
#include <unordered_map>
#include "TimeSchema.pb.h"

using google::protobuf::Message;
typedef std::unordered_map<uint64_t, struct bufferevent *> UserConnMap;
typedef std::string stlstring;


class ServerCore {
private:
    bool m_debugMode = false;
    bool m_forkMode = false;

    // Libevent data struct declare
    struct event_base *     m_EventBase = nullptr;
    struct evconnlistener * m_EventListener = nullptr;
    struct event *          m_EventSignal = nullptr;
    struct event *          m_EventSignalChild = nullptr;
    UserConnMap             m_UserConnectionMap;

    static CltSvrPkg & BuildPackage(CltSvrPkg & pkg, CmdActions actions = CmdActions::CMD_HEART);

    static void EventListenerCallback(struct evconnlistener *listener,
                                      evutil_socket_t fd,
                                      struct sockaddr *sa,
                                      int socklen,
                                      void *user_data);

    static void EventConnReadCallback(struct bufferevent *bev, void *user_data);

    static void EventConnWriteCallback(struct bufferevent *bev, void *user_data);

    static void EventConnEventCallback(struct bufferevent *bev, short events, void *user_data);

    static void EventSignalCallback(evutil_socket_t sig, short events, void *user_data);

    static stlstring GetCurrentTime(const stlstring &fmt = "%Y%m%d%H%M%S");

    static stlstring FormatObject(const Message & message);

public:
    ServerCore(bool debug = false);
    bool Initialize(const stlstring ip = "127.0.0.1", uint32_t port = 9999);
    void Run();
    ~ServerCore();

};


#endif //TIMESVR_SERVERCORE_H
