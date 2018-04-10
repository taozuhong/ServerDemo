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
#include "TimeSchema.pb.h"
#include "google/protobuf/text_format.h"

static const int PORT = 9999;
static const char IP[] = "127.0.0.1";

using std::string;
using google::protobuf::TextFormat;


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
    HeartMsg heart_client;
    struct timespec ts = {0};
    clock_gettime(CLOCK_REALTIME, &ts);
    heart_client.set_tick(ts.tv_nsec);


    // alloc buffer for HeartMsg serialize
    string heartTextBuf;

    // read user input and receive server message
    TimeStamp time_server;
    string str_text_format;
    ssize_t recv_size = 0;
    char client_buffer[1024];
    while (true)
    {
        heartTextBuf.clear();
        memset(client_buffer, 0, sizeof(client_buffer));
        recv_size = recv(fd, client_buffer, sizeof(client_buffer), 0);
        if (0 < recv_size)
        {
            std::cout << "Recv server data: " << recv_size << std::endl;
            if (time_server.ParseFromString(client_buffer))
            {
                str_text_format.clear();
                TextFormat::PrintToString(time_server, &str_text_format);
                std::cout << str_text_format << std::endl;
            } else {
                std::cout << "Recv server data: parser data fail" << std::endl;
            }

            getchar();

            // update client tick to server
            clock_gettime(CLOCK_REALTIME, &ts);
            heart_client.set_tick(ts.tv_nsec);
            if (heart_client.SerializeToString(&heartTextBuf)) {
                if (-1 == send(fd, heartTextBuf.data(), heartTextBuf.size(), 0)) {
                    break;
                }
                else {
                    std::cout << "Send message to server: " << heartTextBuf.size() << std::endl;
                }
            }

            str_text_format.clear();
            TextFormat::PrintToString(heart_client, &str_text_format);
            std::cout << str_text_format << std::endl;
        } else if (-1 == recv_size) {
            std::cout << "Recv server data: " << strerror(errno) << std::endl;
            break;
        }
    }

    close(fd);
}