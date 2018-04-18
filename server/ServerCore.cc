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

#include "ServerCore.h"

#include <signal.h>
#include <arpa/inet.h>
#include <ctime>
#include <iostream>
#include <cstring>
#include <google/protobuf/text_format.h>
#include <event.h>
#include "glog/logging.h"
#include <sys/wait.h>

using std::fstream;

void ServerCore::EventListenerCallback(struct evconnlistener *listener,
                                       evutil_socket_t fd,
                                       struct sockaddr *sa,
                                       int socklen,
                                       void *user_data) {
    ServerCore * pSvrCore = (ServerCore*)user_data;
    struct bufferevent *bev_new_conn = nullptr;

    struct sockaddr_in *addr_in = (struct sockaddr_in *) sa;
    char *ip_address = inet_ntoa(addr_in->sin_addr);
    if (pSvrCore->m_debugMode) {
        std::cout << "Client: " << ip_address << std::endl;
    }
    LOG(INFO) << "Client connecting: " << ip_address;

    bev_new_conn = bufferevent_socket_new(pSvrCore->m_EventBase, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev_new_conn) {
        if (pSvrCore->m_debugMode) {
            std::cerr << "Error constructing bufferevent!" << std::endl;
        }
        event_base_loopbreak(pSvrCore->m_EventBase);
        return;
    }

    // Generate user id and store it
    uint64_t user_id = random();
    pSvrCore->m_UserID = user_id;
    pSvrCore->m_BuffEventHandle = bev_new_conn;
    pSvrCore->m_UserConnectionMap[user_id] = bev_new_conn;
    std::cout << "Client user id: " << user_id << std::endl;

    if (fork()) {
        /* In parent */
        bufferevent_free(bev_new_conn);
    } else {
        pSvrCore->m_forkMode = true;

        /* child process need to reinit event base object  */
        event_reinit(pSvrCore->m_EventBase);


        // bind read and write function for new connection
        bufferevent_setcb(bev_new_conn, EventConnReadCallback, EventConnWriteCallback, EventConnEventCallback,
                          user_data);
        bufferevent_enable(bev_new_conn, EV_READ | EV_WRITE);
        LOG(INFO) << "Register callback for connection";

        // create a protobuf message for client (create user id)
        CltSvrPkg clientPkg;
        BuildPackage(clientPkg, CmdActions::CMD_TIME);
        clientPkg.mutable_head()->set_uid(user_id);
        clientPkg.mutable_data()->mutable_time()->set_time(GetCurrentTime("%Y%m%d%H%M%S"));

        struct timespec ts = {0};
        clock_gettime(CLOCK_REALTIME, &ts);
        clientPkg.mutable_data()->mutable_time()->set_tick(ts.tv_nsec);

        //send a protobuf message to client
        stlstring timeTextBuf;
        if (clientPkg.SerializeToString(&timeTextBuf)) {
            bufferevent_write(bev_new_conn, timeTextBuf.data(), timeTextBuf.size());

            //format data to string
            std::cout << "Send message to client: " << timeTextBuf.size() << std::endl
                      << FormatObject(clientPkg) << std::endl;

            LOG(INFO) << "Send message to client: " << user_id << "(" << timeTextBuf.size() << ")";
        }
    }
}

void ServerCore::EventConnReadCallback(struct bufferevent *bev, void *user_data) {
    ServerCore * pSvrCore = (ServerCore*)user_data;

    struct evbuffer *evbuf_input = bufferevent_get_input(bev);
    size_t evbuff_size = evbuffer_get_length(evbuf_input);
    if (0 >= evbuff_size) {
        return;
    }

    // read client data from socket
    if (pSvrCore->m_debugMode) {
        std::cout << "Recv data from client: " << evbuff_size << std::endl;
        LOG(INFO) << "Receive data from client: "  << evbuff_size;
    }

    char *heart_buff = (char *) malloc(evbuff_size + 1);
    if (NULL != heart_buff) {
        memset(heart_buff, 0, evbuff_size + 1);
        if (0 < bufferevent_read(bev, heart_buff, evbuff_size)) {
            if (pSvrCore->m_debugMode) {
                std::cout << "Recv data from client: " << evbuff_size << "[done]" << std::endl;
            }

            CltSvrPkg clientPkg;
            clientPkg.default_instance();
            clientPkg.mutable_data()->mutable_heart()->set_tick(0);
            if (clientPkg.ParseFromArray(heart_buff, evbuff_size)) {
                std::cout << "Read message from client: \n" << FormatObject(clientPkg) << std::endl;

                // Make response for client
                pSvrCore->HandleMessageAndResponse(clientPkg);

                LOG(INFO) << "Receive data from client: "  << clientPkg.head().uid() << "-->" << clientPkg.head().cmd();
            } else if (pSvrCore->m_debugMode) {
                std::cout << "Recv data from client: parse fail." << std::endl;
                LOG(INFO) << "Recv data from client: parse fail.";
            }
        }

        free(heart_buff);
    } else if (pSvrCore->m_debugMode) {
        std::cout << "Recv data from client: alloc memory fail." << std::endl;
    }
}


void ServerCore::EventConnWriteCallback(struct bufferevent *bev, void *user_data) {
    ServerCore * pSvrCore = (ServerCore*)user_data;

    struct evbuffer *output = bufferevent_get_output(bev);
    if (evbuffer_get_length(output) == 0) {
        if (pSvrCore->m_debugMode) {
            std::cout << "flushed answer===================================" << std::endl << std::endl;
        }

        //bufferevent_free(bev);
    }
}

void ServerCore::EventConnEventCallback(struct bufferevent *bev, short events, void *user_data) {
    ServerCore * pSvrCore = (ServerCore*)user_data;

    if (events & BEV_EVENT_CONNECTED) {
        std::cout << "Connection created." << std::endl;
        LOG(INFO) << "Connection created.";
    } else if (events & BEV_EVENT_EOF) {
        std::cout << "Connection closed." << std::endl;
        LOG(INFO) << "Connection closed.";

        // exit forked process when client close connection
        if (pSvrCore->m_forkMode)
        {
            bufferevent_free(bev);
            event_base_loopbreak(pSvrCore->m_EventBase);

            LOG(INFO) << "Close connection and exit event loop.";
        }
    } else if (events & BEV_EVENT_ERROR) {
        std::cerr << "Got an error on the connection: " << strerror(errno) << std::endl;/*XXX win32*/
        bufferevent_free(bev);
        LOG(ERROR) << "Found error, close connection: " << strerror(errno);
    }
}

void ServerCore::EventSignalCallback(evutil_socket_t sig, short events, void *user_data) {
    ServerCore * pSvrCore = (ServerCore*)user_data;

    switch (sig)
    {
        case SIGINT:
        {
            if (pSvrCore->m_debugMode) {
                std::cout << std::endl << "Caught an interrupt signal; exiting cleanly in two seconds." << std::endl;
            }
            LOG(INFO) << "SIGINT activated, exit server process......";

            struct timeval delay = {2, 0};
            event_base_loopexit(pSvrCore->m_EventBase, &delay);

            LOG(INFO) << "SIGINT activated, exit server process......[done]";
            break;
        }

        case SIGCHLD:
        {
            pid_t pid;
            int status;

            // kill zombie process
            while ((pid = waitpid(-1, &status, WNOHANG)) != -1) {
                /* Handle the dead process pid*/
                if ((-1 != pid) && WIFSIGNALED(status) && WTERMSIG(status)) {
                    std::cout << "Child process closed: " << pid << std::endl;
                    LOG(INFO) << "Child process closed: " << pid;
                } else {
                    break;
                }
            }
            break;
        }

        default:
            // do nonthing
            break;
    }

}

ServerCore::ServerCore(bool debug) {
    m_debugMode = debug;

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    LOG(INFO) << "Verify google protobuf version and initialize";
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // create libevent object
    LOG(INFO) << "Create event base object";
    m_EventBase = event_base_new();
}

ServerCore::~ServerCore() {
    // free event handle and resource
    LOG(INFO) << "Destroy event object";
    evconnlistener_free(m_EventListener);
    event_free(m_EventSignal);
    event_base_free(m_EventBase);

    // Delete all global objects allocated by libprotobuf.
    LOG(INFO) << "Shutdown google protobuf library";
    google::protobuf::ShutdownProtobufLibrary();
}

bool ServerCore::Initialize(const stlstring ip, uint32_t port) {
    if (nullptr == m_EventBase) {
        return false;
    }

    struct sockaddr_in sin = {0};
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &sin.sin_addr);
    m_EventListener = evconnlistener_new_bind(m_EventBase,
                                       ServerCore::EventListenerCallback,
                                       this,
                                       LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
                                       (struct sockaddr *) &sin,
                                       sizeof(sin));
    if (!m_EventListener) {
        return false;
    }
    LOG(INFO) << "Bind ip/port and listen: " << ip << " " << port;

    // register signal handler
    m_EventSignal = evsignal_new(m_EventBase, SIGINT, ServerCore::EventSignalCallback, this);
    if (!m_EventSignal || evsignal_add(m_EventSignal, NULL) < 0) {
        return false;
    }
    LOG(INFO) << "Register signal SIGINT";

    m_EventSignalChild = evsignal_new(m_EventBase, SIGCHLD, ServerCore::EventSignalCallback, this);
    if (!m_EventSignalChild || evsignal_add(m_EventSignalChild, NULL) < 0) {
        return false;
    }
    LOG(INFO) << "Register signal SIGCHLD";

    return  true;
}

void ServerCore::Run() {
    LOG(INFO) << "Enter event loop";

    // enter event loop
    event_base_dispatch(m_EventBase);
}



stlstring ServerCore::GetCurrentTime(const stlstring &fmt) {
    time_t now = time(0);
    struct tm *pstruct = std::localtime(&now);

    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    char szBuf[128] = {0};
    strftime(szBuf, sizeof(szBuf), fmt.c_str(), pstruct);

    return szBuf;
}

stlstring ServerCore::FormatObject(const Message &message) {
    using google::protobuf::TextFormat;

    stlstring formatedText;
    TextFormat::PrintToString(message, &formatedText);

    return formatedText;
}

CltSvrPkg &ServerCore::BuildPackage(CltSvrPkg &pkg, CmdActions actions) {
    pkg.default_instance();
    PkgHead *pHead = pkg.mutable_head();
    PkgBody *pBody = pkg.mutable_data();
    pHead->set_cmd(CmdActions::CMD_HEART);
    pHead->set_cmdseq(0);
    pHead->set_cmdtype(0);
    pHead->set_srcid(1);
    pHead->set_dstid(2);
    pHead->set_uid(0);


    HeartMsg * pHeartMsg = nullptr;
    TimeStamp * pTime = nullptr;
    BroadMsg * pBroad = nullptr;
    switch (actions) {
        case CmdActions::CMD_HEART :
        {
            pHeartMsg = pBody->mutable_heart();
            pHead->set_cmd(CmdActions::CMD_HEART);
            pHeartMsg->set_tick(0);
            break;
        }
        case CmdActions::CMD_TIME: {
            pTime = pBody->mutable_time();
            pHead->set_cmd(CmdActions::CMD_TIME);
            pTime->set_tick(0);
            break;
        }
        case CmdActions::CMD_BROAD: {
            pBroad = pBody->mutable_info();
            pHead->set_cmd(CmdActions::CMD_BROAD);
            pBroad->set_timestamp(0);
            break;
        }
        default:
            break;
    }

    return  pkg;
}

void ServerCore::HandleMessageAndResponse(CltSvrPkg & pkg) {
    if ((0 == m_UserID) || (nullptr == m_BuffEventHandle))
    {
        LOG(INFO) << "User id and connection is null";
        return;
    }

    struct timespec ts = {0};
    CltSvrPkg server_response;
    switch (pkg.head().cmd())
    {
        case CmdActions::CMD_TIME: {
            BuildPackage(server_response, CmdActions::CMD_TIME);
            server_response.mutable_head()->set_uid(m_UserID);
            server_response.mutable_data()->mutable_time()->set_time(GetCurrentTime("%Y%m%d%H%M%S"));
            clock_gettime(CLOCK_REALTIME, &ts);
            server_response.mutable_data()->mutable_time()->set_tick(ts.tv_nsec);
            BufferEventSendResponse(m_BuffEventHandle, server_response);
            break;
        }
        case CmdActions::CMD_HEART: {
            BuildPackage(server_response, CmdActions::CMD_HEART);
            server_response.mutable_head()->set_uid(m_UserID);
            clock_gettime(CLOCK_REALTIME, &ts);
            server_response.mutable_data()->mutable_heart()->set_tick(ts.tv_nsec);
            BufferEventSendResponse(m_BuffEventHandle, server_response);
            break;
        }
        case CmdActions::CMD_BROAD: {
            BuildPackage(server_response, CmdActions::CMD_BROAD);
            server_response.mutable_head()->set_uid(m_UserID);
            clock_gettime(CLOCK_REALTIME, &ts);
            server_response.mutable_data()->mutable_info()->set_timestamp(ts.tv_nsec);
            server_response.mutable_data()->mutable_info()->set_title(pkg.data().info().title());
            server_response.mutable_data()->mutable_info()->set_message(pkg.data().info().message());
            for (auto it : m_UserConnectionMap)
            {
                if (it.first != m_UserID) {
                    BufferEventSendResponse(m_BuffEventHandle, server_response);
                }
            }
            break;
        }
        case CmdActions::CMD_UNKNOWN:
            break;
        default:
            break;
    }
}

void ServerCore::BufferEventSendResponse(struct bufferevent *buffevent, CltSvrPkg & response) {
    stlstring timeTextBuf;
    if (response.SerializeToString(&timeTextBuf)) {
        bufferevent_write(buffevent, timeTextBuf.data(), timeTextBuf.size());
        std::cout << "Send message to client: " << timeTextBuf.size() << std::endl
                  << FormatObject(response) << std::endl;

        LOG(INFO) << "Send data to client: " << timeTextBuf.size();
    } else if (m_debugMode) {
        std::cout << "Send message to client: serialize failed." << std::endl;
    }
}

