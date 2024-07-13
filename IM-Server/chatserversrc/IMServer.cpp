
#include "../net/inetaddress.h"
#include "../base/logging.h"
#include "../base/singleton.h"
#include "IMServer.h"
#include "ClientSession.h"
#include "UserManager.h"

bool IMServer::Init(const char* ip, short port, EventLoop* loop)
{
    //�����ݿ��м��������û���Ϣ
    //TODO: ��������ݿ��˺���Ϣͳһ��һ���ط�
    if (!Singleton<UserManager>::Instance().Init("192.168.42.129", "root", "123456", "myim"))
    {
        LOG_ERROR << "Load users from db error";
        return false;
    }
    
    InetAddress addr(ip, port);
    m_server.reset(new TcpServer(loop, addr, "YFY-MYIMSERVER", TcpServer::kReusePort));
    m_server->setConnectionCallback(std::bind(&IMServer::OnConnection, this, std::placeholders::_1));
    //��������
    m_server->start();

    return true;
}

void IMServer::OnConnection(std::shared_ptr<TcpConnection> conn)
{
    if (conn->connected())
    {
        LOG_INFO << "client connected:" << conn->peerAddress().toIpPort();
        ++ m_baseUserId;
        std::shared_ptr<ClientSession> spSession(new ClientSession(conn));
        conn->setMessageCallback(std::bind(&ClientSession::OnRead, spSession.get(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        std::lock_guard<std::mutex> guard(m_sessionMutex);
        m_sessions.push_back(spSession);
    }
    else
    {
        OnClose(conn);
    }
}

void IMServer::OnClose(const std::shared_ptr<TcpConnection>& conn)
{
    //�Ƿ����û�����
    //bool bUserOffline = false;
    UserManager& userManager = Singleton<UserManager>::Instance();

    //TODO: �����Ĵ����߼�̫���ң���Ҫ�Ż�
    std::lock_guard<std::mutex> guard(m_sessionMutex);
    for (auto iter = m_sessions.begin(); iter != m_sessions.end(); ++iter)
    {
        if ((*iter)->GetConnectionPtr() == NULL)
        {
            LOG_ERROR << "connection is NULL";
            break;
        }
        
        if ((*iter)->GetConnectionPtr() == conn)
        {
            //���������ߺ��ѣ������������������Ϣ
            std::list<User> friends;
            int32_t offlineUserId = (*iter)->GetUserId();
            userManager.GetFriendInfoByUserId(offlineUserId, friends);
            for (const auto& iter2 : friends)
            {
                std::shared_ptr<ClientSession> targetSession;
                for (auto& iter3 : m_sessions)
                {
                    if (iter2.userid == iter3->GetUserId())                 
                        iter3->SendUserStatusChangeMsg(offlineUserId, 2);
                }
            }
            
            //�û�����
            m_sessions.erase(iter);
            //bUserOffline = true;
            LOG_INFO << "client disconnected: " << conn->peerAddress().toIpPort();
            break;
        }
    }

    LOG_INFO << "current online user count: " << m_sessions.size();
}

void IMServer::GetSessions(std::list<std::shared_ptr<ClientSession>>& sessions)
{
    std::lock_guard<std::mutex> guard(m_sessionMutex);
    sessions = m_sessions;
}

bool IMServer::GetSessionByUserId(std::shared_ptr<ClientSession>& session, int32_t userid)
{
    std::lock_guard<std::mutex> guard(m_sessionMutex);
    std::shared_ptr<ClientSession> tmpSession;
    for (const auto& iter : m_sessions)
    {
        tmpSession = iter;
        if (iter->GetUserId() == userid)
        {
            session = tmpSession;
            return true;
        }
    }

    return false;
}

bool IMServer::IsUserSessionExsit(int32_t userid)
{
    std::lock_guard<std::mutex> guard(m_sessionMutex);
    for (const auto& iter : m_sessions)
    {
        if (iter->GetUserId() == userid)
        {
            return true;
        }
    }

    return false;
}