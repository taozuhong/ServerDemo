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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <unistd.h>
#include <iostream>
#include <string>
#include <ctime>
#include "TimeSchema.pb.h"
#include <google/protobuf/text_format.h>


static const int PORT = 9999;
static const char IP[] = "127.0.0.1";

using std::string;
using google::protobuf::TextFormat;

char GetOneChar();
string GetCurrentTime(const string &fmt = "%Y%m%d%H%M%S");
bool SendToServer(int fd, CltSvrPkg & response);
CltSvrPkg & BuildPackage(CltSvrPkg & pkg, CmdActions actions = CmdActions::CMD_HEART);


int main(int argc, char* argv[])
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == fd)
    {
        int errsv = errno;
        std::cerr << strerror(errsv) << std::endl;
        return  -1;
    }


    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    inet_pton( AF_INET, IP, &sin.sin_addr );
    if (-1 == connect(fd, (struct sockaddr *)&sin, sizeof(sin)))
    {
        int errsv = errno;
        std::cerr << strerror(errsv) << "(" << __LINE__ << ")" << std::endl;
        return  -2;
    }


    // create and initialize HeartMsg
    CltSvrPkg client_heart_pkg, client_time_pkg, server_response_pkg;
    BuildPackage(client_heart_pkg, CmdActions::CMD_HEART);
    BuildPackage(client_time_pkg, CmdActions::CMD_TIME);
    BuildPackage(server_response_pkg, CmdActions::CMD_TIME);


    // read user input and receive server message
    struct timespec ts = {0};
    string str_text_format;
    ssize_t recv_size = 0;
    ssize_t user_input = 0;
    uint64_t server_user_id = 0;
    char client_buffer[1024];
    while (true)
    {
        memset(client_buffer, 0, sizeof(client_buffer));
        recv_size = recv(fd, client_buffer, sizeof(client_buffer), 0);
        if (0 < recv_size)
        {
            std::cout << "Recv server data: " << recv_size << std::endl;
            // server_response_pkg.mutable_data()->mutable_time()->set_tick(0);
            if (server_response_pkg.ParseFromArray(client_buffer, recv_size))
            {
                if (0 == server_user_id)
                {
                    server_user_id = server_response_pkg.head().uid();
                    client_heart_pkg.mutable_head()->set_uid(server_user_id);
                    client_time_pkg.mutable_head()->set_uid(server_user_id);
                }

                str_text_format.clear();
                TextFormat::PrintToString(server_response_pkg, &str_text_format);
                std::cout << str_text_format << std::endl;
            } else {
                std::cout << "Recv server data: parser data fail" << std::endl;
            }

            user_input = GetOneChar();
            switch (user_input)
            {
                case 'q':
                case 'Q':
                {
                    std::cout << "safe exit client." << std::endl;
                    goto END_WHILE;
                }
                case 't':
                case 'T':
                {
                    clock_gettime(CLOCK_REALTIME, &ts);
                    client_time_pkg.mutable_data()->mutable_time()->set_tick(ts.tv_nsec);
                    client_time_pkg.mutable_data()->mutable_time()->set_time(GetCurrentTime("%Y%m%d%H%M%S"));
                    if (! SendToServer(fd, client_time_pkg))
                    {
                        goto END_WHILE;
                    }
                    break;
                }
                case 'h':
                case 'H':
                default:
                {
                    clock_gettime(CLOCK_REALTIME, &ts);
                    client_heart_pkg.mutable_data()->mutable_heart()->set_tick(ts.tv_nsec);
                    if (! SendToServer(fd, client_heart_pkg))
                    {
                        goto END_WHILE;
                    }
                    break;
                }
            }
        } else if (-1 == recv_size) {
            std::cout << "Recv server data: " << strerror(errno) << std::endl;
            break;
        }
    }

    END_WHILE:
    close(fd);
}

bool SendToServer(int fd, CltSvrPkg & response)
{
    bool send_status = true;

    // alloc buffer for HeartMsg serialize
    string heartTextBuf;
    if (response.SerializeToString(&heartTextBuf)) {
        if (-1 == send(fd, heartTextBuf.data(), heartTextBuf.size(), 0)) {
            send_status = false;
            std::cout << "Send message to server fail. " << std::endl;
        }
        else {
            std::cout << "Send message to server: " << heartTextBuf.size() << std::endl;

            heartTextBuf.clear();
            TextFormat::PrintToString(response, &heartTextBuf);
            std::cout << heartTextBuf << std::endl;
        }
    }
    else {
        std::cout << "Serialize message to string fail. " << std::endl;
    }

    return send_status;
}

CltSvrPkg & BuildPackage(CltSvrPkg & pkg, CmdActions actions) {
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


string GetCurrentTime(const string &fmt) {
    time_t now = time(0);
    struct tm *pstruct = std::localtime(&now);

    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    char szBuf[128] = {0};
    strftime(szBuf, sizeof(szBuf), fmt.c_str(), pstruct);

    return szBuf;
}

char GetOneChar()
{
    char c;
    system("stty -icanon");  // setup for read char with no buffer
    c=getchar();
    fprintf(stdout, "\n");
    system("stty icanon");  // restore origin setting
    return c;
}