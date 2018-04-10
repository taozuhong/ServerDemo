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
#include "TimeServer.h"
#include "ServerCore.h"

#define STRIP_FLAG_HELP 1
#include "gflags/gflags.h"
#include "glog/logging.h"

DEFINE_bool(h, false, "Show help");
DEFINE_bool(debug, false, "True to enter debug mode");
DEFINE_string(ip, "127.0.0.1", "Default ip address");
DEFINE_int32(port, 9999, "Defafult listen port");

int main(int argc, char* argv[]) {
    gflags::SetVersionString("0.1");
    gflags::SetUsageMessage(App_Usage);
    gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
    if (FLAGS_h) {
        gflags::ShowUsageWithFlagsRestrict(argv[0], App_Usage);
        return 0;
    }


    // initialize google glog library
    google::InitGoogleLogging(static_cast<const char *>(argv[0]));
    LOG(INFO) << "init gflags" << "init glog";

    ServerCore svr_core(FLAGS_debug);
    if (svr_core.Initialize(FLAGS_ip, FLAGS_port)) {
        svr_core.Run();
    }

    // shutdown glog library
    LOG(INFO) << "shutdown glog";
    google::ShutdownGoogleLogging();

    return 0;
}
