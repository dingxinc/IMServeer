
#include <iostream>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../base/logging.h"
#include "../base/singleton.h"
#include "../mysql/mysqlmanager.h"
#include "../net/eventloop.h"
#include "../net/eventloopthreadpool.h"
#include "UserManager.h"
#include "IMServer.h"

using namespace net;

EventLoop g_mainLoop;

void prog_exit(int signo)
{
    std::cout << "program recv signal [" << signo << "] to exit." << std::endl;

    g_mainLoop.quit();
}

void daemon_run()
{
    int pid;
    signal(SIGCHLD, SIG_IGN);
    //1���ڸ������У�fork�����´����ӽ��̵Ľ���ID��
    //2�����ӽ����У�fork����0��
    //3��������ִ���fork����һ����ֵ��
    pid = fork();
    if (pid < 0)
    {
        std::cout << "fork error" << std::endl;
        exit(-1);
    }
    //�������˳����ӽ��̶�������
    else if (pid > 0) {
        exit(0);
    }
    //֮ǰparent��child������ͬһ��session��,parent�ǻỰ��session������ͷ����,
    //parent������Ϊ�Ự����ͷ���̣����exit����ִ�еĻ�����ô�ӽ��̻��Ϊ�¶����̣�����init������
    //ִ��setsid()֮��,child�����»��һ���µĻỰ(session)id��
    //��ʱparent�˳�֮��,������Ӱ�쵽child�ˡ�
    setsid();
    int fd;
    fd = open("/dev/null", O_RDWR, 0);
    if (fd != -1)
    {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
    }
    if (fd > 2)
        close(fd);
}


int main(int argc, char* argv[])
{
    //�����źŴ���
    signal(SIGCHLD, SIG_DFL);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, prog_exit);
    signal(SIGKILL, prog_exit);
    signal(SIGTERM, prog_exit);

    short port = 0;
    int ch;
    bool bdaemon = false;
    while ((ch = getopt(argc, argv, "d")) != -1)
    {
        switch (ch)
        {
        case 'd':
            bdaemon = true;
            break;
        }
    }

    if (bdaemon)
        daemon_run();


    if (port == 0)
        port = 12345;

    Logger::setLogLevel(Logger::DEBUG);

    //��ʼ�����ݿ�����
    const char* dbserver = "192.168.42.129";
    const char* dbuser = "root";
    const char* dbpassword = "123456";
    const char* dbname = "myim";
    //CMysqlManager ֻ��ע�û��������룬�Լ����ݿ��Ƿ���ڣ� ���е��������������ݿ���������ҳ�Աm_poConn
	if (!Singleton<CMysqlManager>::Instance().Init(dbserver, dbuser, dbpassword, dbname))
    {
        LOG_FATAL << "please check your database config..............";
    }
    //��ֻ�����û���Ĺ�ϵ  
    if (!Singleton<UserManager>::Instance().Init(dbserver, dbuser, dbpassword, dbname))
    {
        LOG_FATAL << "please check your database config..............";
    }

    Singleton<EventLoopThreadPool>::Instance().Init(&g_mainLoop, 4);
    Singleton<EventLoopThreadPool>::Instance().start();

    Singleton<IMServer>::Instance().Init("0.0.0.0", 8000, &g_mainLoop);
    
    g_mainLoop.loop();

    return 0;
}
