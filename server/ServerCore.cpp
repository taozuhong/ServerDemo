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
#include "TimeSchema.pb.h"

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

        // create a protobuf message for client
        TimeStamp timeStamp;
        timeStamp.set_time(GetCurrentTime("%Y%m%d%H%M%S"));

        struct timespec ts = {0};
        clock_gettime(CLOCK_REALTIME, &ts);
        timeStamp.set_tick(ts.tv_nsec);

        //format data to string
        std::cout << "Send message to client: \n" << FormatObject(timeStamp) << std::endl;

        //send a protobuf message to client
        stlstring timeTextBuf;
        if (timeStamp.SerializeToString(&timeTextBuf)) {
            bufferevent_write(bev_new_conn, timeTextBuf.data(), timeTextBuf.size());
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
    }

    char *heart_buff = (char *) malloc(evbuff_size + 1);
    if (NULL != heart_buff) {
        memset(heart_buff, 0, evbuff_size + 1);
        if (0 < bufferevent_read(bev, heart_buff, evbuff_size)) {
            if (pSvrCore->m_debugMode) {
                std::cout << "Recv data from client: " << evbuff_size << "[done]" << std::endl;
            }

            HeartMsg heart_msg;
            if (heart_msg.ParseFromString(heart_buff)) {
                std::cout << "Read message from client: \n" << FormatObject(heart_msg) << std::endl;
            } else if (pSvrCore->m_debugMode) {
                std::cout << "Recv data from client: parse fail." << std::endl;
            }

            LOG(INFO) << "Receive data from client: "  << evbuff_size;
        }

        free(heart_buff);
    } else if (pSvrCore->m_debugMode) {
        std::cout << "Recv data from client: alloc memory fail." << std::endl;
    }


    // create a protobuf message for client
    TimeStamp time_stamp;
    time_stamp.set_time(GetCurrentTime("%Y%m%d%H%M%S"));

    struct timespec ts = {0};
    clock_gettime(CLOCK_REALTIME, &ts);
    time_stamp.set_tick(ts.tv_nsec);

    //send a protobuf message to client
    stlstring timeTextBuf;
    if (time_stamp.SerializeToString(&timeTextBuf)) {
        bufferevent_write(bev, timeTextBuf.data(), timeTextBuf.size());
        std::cout << "Send message to client: \n" << FormatObject(time_stamp) << std::endl;

        LOG(INFO) << "Send data to client: "  << evbuff_size;
    } else if (pSvrCore->m_debugMode) {
        std::cout << "Send message to client: serialize failed." << std::endl;
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
    } else if (events & BEV_EVENT_EOF) {
        std::cout << "Connection closed." << std::endl;

        // exit forked process when client close connection
        if (pSvrCore->m_forkMode)
        {
            event_base_loopbreak(pSvrCore->m_EventBase);
        }
    } else if (events & BEV_EVENT_ERROR) {
        std::cerr << "Got an error on the connection: " << strerror(errno) << std::endl;/*XXX win32*/
        bufferevent_free(bev);
        LOG(ERROR) << "Found error, close connection: " << strerror(errno);
    }

    /* None of the other events can happen here, since we haven't enabled
     * timeouts */
    // bufferevent_free(bev);
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
            LOG(INFO) << "SIGINT activated, exit server";

            struct timeval delay = {2, 0};
            event_base_loopexit(pSvrCore->m_EventBase, &delay);

            break;
        }

        case SIGCHLD:
        {
            pid_t pid;
            int status;

            while ((pid = waitpid(-1, &status, WNOHANG)) != -1) {
                /* Handle the death of pid p */
                std::cout << "Child process closed: " << pid << std::endl;
                LOG(INFO) << "Child process closed: " << pid;
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
