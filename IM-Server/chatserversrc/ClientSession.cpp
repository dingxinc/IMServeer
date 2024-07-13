
#include <string.h>
#include <sstream>
#include <list>
#include "../net/tcpconnection.h"
#include "../net/protocolstream.h"
#include "../base/logging.h"
#include "../base/singleton.h"
#include "../jsoncpp-0.5.0/json.h"
#include "ClientSession.h"
#include "Msg.h"
#include "UserManager.h"
#include "IMServer.h"
#include "MsgCacheManager.h"

using namespace net;

ClientSession::ClientSession(const std::shared_ptr<TcpConnection>& conn) :  
TcpSession(conn), 
m_id(0),
m_seq(0)
{
	m_userinfo.userid = 0;
}

ClientSession::~ClientSession()
{

}

void ClientSession::OnRead(const std::shared_ptr<TcpConnection>& conn, Buffer* pBuffer, Timestamp receivTime)
{
    while (true)
    {
        //����һ����ͷ��С
        if (pBuffer->readableBytes() < (size_t)sizeof(msg))
        {
            LOG_INFO << "buffer is not enough for a package header, pBuffer->readableBytes()=" << pBuffer->readableBytes() << ", sizeof(msg)=" << sizeof(msg);
            return;
        }

        //����һ��������С
        msg header;
        memcpy(&header, pBuffer->peek(), sizeof(msg));
        if (pBuffer->readableBytes() < (size_t)header.packagesize + sizeof(msg))
            return;

        pBuffer->retrieve(sizeof(msg));
        std::string inbuf;
        inbuf.append(pBuffer->peek(), header.packagesize);
        pBuffer->retrieve(header.packagesize);
        if (!Process(conn, inbuf.c_str(), inbuf.length()))
        {
            LOG_WARN << "Process error, close TcpConnection";
            conn->forceClose();
        }
    }// end while-loop

}

bool ClientSession::Process(const std::shared_ptr<TcpConnection>& conn, const char* inbuf, size_t length)
{
    yt::BinaryReadStream2 readStream(inbuf, length);
    int cmd;
    if (!readStream.Read(cmd))
    {
        LOG_WARN << "read cmd error !!!";
        return false;
    }

    //int seq;
    if (!readStream.Read(m_seq))
    {
        LOG_WARN << "read seq error !!!";
        return false;
    }

    std::string data;
    size_t datalength;
    if (!readStream.Read(&data, 0, datalength))
    {
        LOG_WARN << "read data error !!!";
        return false;
    }
   
    LOG_INFO << "Recv from client: cmd=" << cmd << ", seq=" << m_seq << ", header.packagesize:" << length << ", data=" << data << ", datalength=" << datalength;
    LOG_DEBUG_BIN((unsigned char*)inbuf, length);

    switch (cmd)
    {
        //������
        case msg_type_heartbeart:
        {
            OnHeartbeatResponse(conn);
        }
            break;

        //ע��
        case msg_type_register:
        {
            OnRegisterResponse(data, conn);
        }
            break;
        
        //��¼
        case msg_type_login:
        {                              
            OnLoginResponse(data, conn);
        }
            break;

        //��ȡ�����б�
        case msg_type_getofriendlist:
        {
            OnGetFriendListResponse(conn);
        }
            break;

        //�����û�
        case msg_type_finduser:
        {
            OnFindUserResponse(data, conn);
        }
            break;

        //�Ӻ���
        case msg_type_operatefriend:
        {        
            OnOperateFriendResponse(data, conn);
        }
            break;

        //�����û���Ϣ
        case msg_type_updateuserinfo:
        {
            OnUpdateUserInfoResponse(data, conn);
        }
            break;
        
        //�޸�����
        case msg_type_modifypassword:
        {
            OnModifyPasswordResponse(data, conn);
        }
            break;
        
        //����Ⱥ
        case msg_type_creategroup:
        {
            OnCreateGroupResponse(data, conn);
        }
            break;

        //��ȡָ��Ⱥ��Ա��Ϣ
        case msg_type_getgroupmembers:
        {
            OnGetGroupMembersResponse(data, conn);
        }
            break;

        //������Ϣ
        case msg_type_chat:
        {
            int32_t target;
            if (!readStream.Read(target))
            {
                LOG_WARN << "read target error !!!";
                return false;
            }
            OnChatResponse(target, data, conn);
        }
            break;
        
        //Ⱥ����Ϣ
        case msg_type_multichat:
        {
            std::string targets;
            size_t targetslength;
            if (!readStream.Read(&targets, 0, targetslength))
            {
                LOG_WARN << "read targets error !!!";
                return false;
            }

            OnMultiChatResponse(targets, data, conn);
        }

            break;

        default:
            //pBuffer->retrieveAll();
            LOG_WARN << "unsupport cmd, cmd:" << cmd << ", data=" << data << ", connection name:" << conn->peerAddress().toIpPort();
            //conn->forceClose();
            return false;
    }// end switch

    ++ m_seq;

    return true;
}

void ClientSession::OnHeartbeatResponse(const std::shared_ptr<TcpConnection>& conn)
{
    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_heartbeart);
    writeStream.Write(m_seq);
    std::string dummy;
    writeStream.Write(dummy.c_str(), dummy.length());
    writeStream.Flush();

    LOG_INFO << "Response to client: cmd=1000" << ", sessionId=" << m_id;

    Send(outbuf);
}

void ClientSession::OnRegisterResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn)
{
    //{ "user": "13917043329", "nickname" : "balloon", "password" : "123" }
    Json::Reader JsonReader;
    Json::Value JsonRoot;
    if (!JsonReader.parse(data, JsonRoot))
    {
        LOG_WARN << "invalid json: " << data << ", sessionId = " << m_id << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    if (!JsonRoot["username"].isString() || !JsonRoot["nickname"].isString() || !JsonRoot["password"].isString())
    {
        LOG_WARN << "invalid json: " << data << ", sessionId = " << m_id << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    User u;
    u.username = JsonRoot["username"].asString();
    u.nickname = JsonRoot["nickname"].asString();
    u.password = JsonRoot["password"].asString();

    std::string retData;
    User cachedUser;
    cachedUser.userid = 0;
    Singleton<UserManager>::Instance().GetUserInfoByUsername(u.username, cachedUser);
    if (cachedUser.userid != 0)
        retData = "{\"code\": 101, \"msg\": \"registered already\"}";
    else
    {
        if (!Singleton<UserManager>::Instance().AddUser(u))
            retData = "{\"code\": 100, \"msg\": \"register failed\"}";
        else
            retData = "{\"code\": 0, \"msg\": \"ok\"}";
    }
    
    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_register);
    writeStream.Write(m_seq);   
    writeStream.Write(retData.c_str(), retData.length());
    writeStream.Flush();

    LOG_INFO << "Response to client: cmd=msg_type_register" << ", userid=" << u.userid << ", data=" << retData;

    Send(outbuf);
}

void ClientSession::OnLoginResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn)
{
    //{"username": "13917043329", "password": "123", "clienttype": 1, "status": 1}
    Json::Reader JsonReader;
    Json::Value JsonRoot;
    if (!JsonReader.parse(data, JsonRoot))
    {
        LOG_WARN << "invalid json: " << data << ", sessionId = " << m_id  << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    if (!JsonRoot["username"].isString() || !JsonRoot["password"].isString() || !JsonRoot["clienttype"].isInt() || !JsonRoot["status"].isInt())
    {
        LOG_WARN << "invalid json: " << data << ", sessionId = " << m_id << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    string username = JsonRoot["username"].asString();
    string password = JsonRoot["password"].asString();
    std::ostringstream os;
    User cachedUser;
    cachedUser.userid = 0;
    Singleton<UserManager>::Instance().GetUserInfoByUsername(username, cachedUser);
    if (cachedUser.userid == 0)
    {
        //TODO: ��ЩӲ������ַ�Ӧ��ͳһ�ŵ�ĳ���ط�ͳһ����
        os << "{\"code\": 102, \"msg\": \"not registered\"}";
    }
    else
    {
        if (cachedUser.password != password)
            os << "{\"code\": 103, \"msg\": \"incorrect password\"}";
        else
        {
            //��¼�û���Ϣ
            m_userinfo.userid = cachedUser.userid;
            m_userinfo.username = username;
            m_userinfo.nickname = cachedUser.nickname;
            m_userinfo.password = password;
            m_userinfo.clienttype = JsonRoot["clienttype"].asInt();
            m_userinfo.status = JsonRoot["status"].asInt();

            os << "{\"code\": 0, \"msg\": \"ok\", \"userid\": " << m_userinfo.userid << ",\"username\":\"" << cachedUser.username << "\", \"nickname\":\"" 
               << cachedUser.nickname << "\", \"facetype\": " << cachedUser.facetype << ", \"customface\":\"" << cachedUser.customface << "\", \"gender\":" << cachedUser.gender
               << ", \"birthday\":" << cachedUser.birthday << ", \"signature\":\"" << cachedUser.signature << "\", \"address\": \"" << cachedUser.address
               << "\", \"phonenumber\": \"" << cachedUser.phonenumber << "\", \"mail\":\"" << cachedUser.mail << "\"}";
        }
    }
   
    //��¼��ϢӦ��
    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_login);
    writeStream.Write(m_seq);  
    writeStream.Write(os.str().c_str(), os.str().length());
    writeStream.Flush();

    LOG_INFO << "Response to client: cmd=msg_type_login, data=" << os.str() << ", userid=" << m_userinfo.userid;
    
    Send(outbuf);

    //����֪ͨ��Ϣ
    std::list<NotifyMsgCache> listNotifyCache;
    Singleton<MsgCacheManager>::Instance().GetNotifyMsgCache(m_userinfo.userid, listNotifyCache);
    for (const auto &iter : listNotifyCache)
    {
        Send(iter.notifymsg);
    }

    //����������Ϣ
    std::list<ChatMsgCache> listChatCache;
    Singleton<MsgCacheManager>::Instance().GetChatMsgCache(m_userinfo.userid, listChatCache);
    for (const auto &iter : listChatCache)
    {
        Send(iter.chatmsg);
    }

    //�������û�����������Ϣ
    std::list<User> friends;
    Singleton<UserManager>::Instance().GetFriendInfoByUserId(m_userinfo.userid, friends);
    IMServer& imserver = Singleton<IMServer>::Instance();
    for (const auto& iter : friends)
    {
        //�ȿ�Ŀ���û��Ƿ�����
        std::shared_ptr<ClientSession> targetSession;
        imserver.GetSessionByUserId(targetSession, iter.userid);
        if (targetSession)
            targetSession->SendUserStatusChangeMsg(m_userinfo.userid, 1);
    }
}

void ClientSession::OnGetFriendListResponse(const std::shared_ptr<TcpConnection>& conn)
{
    std::list<User> friends;
    Singleton<UserManager>::Instance().GetFriendInfoByUserId(m_userinfo.userid, friends);
	std::string strUserInfo;
    bool userOnline = false;
    IMServer& imserver = Singleton<IMServer>::Instance();
    for (const auto& iter : friends)
    {	
        userOnline = imserver.IsUserSessionExsit(iter.userid);
        /*
        {"code": 0, "msg": "ok", "userinfo":[{"userid": 1,"username":"qqq, 
        "nickname":"qqq, "facetype": 0, "customface":"", "gender":0, "birthday":19900101, 
        "signature":", "address": "", "phonenumber": "", "mail":", "clienttype": 1, "status":1"]}
        */
        ostringstream osSingleUserInfo;
        osSingleUserInfo << "{\"userid\": " << iter.userid << ",\"username\":\"" << iter.username << "\", \"nickname\":\"" << iter.nickname
                         << "\", \"facetype\": " << iter.facetype << ", \"customface\":\"" << iter.customface << "\", \"gender\":" << iter.gender
                         << ", \"birthday\":" << iter.birthday << ", \"signature\":\"" << iter.signature << "\", \"address\": \"" << iter.address
                         << "\", \"phonenumber\": \"" << iter.phonenumber << "\", \"mail\":\"" << iter.mail << "\", \"clienttype\": 1, \"status\":"
                         << (userOnline ? 1 : 0) << "}";

        strUserInfo += osSingleUserInfo.str();
        strUserInfo += ",";
    }
	//ȥ��������Ķ���
	strUserInfo = strUserInfo.substr(0, strUserInfo.length() - 1);
	std::ostringstream os;
	os << "{\"code\": 0, \"msg\": \"ok\", \"userinfo\":[" << strUserInfo << "]}";

	std::string outbuf;
	yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_getofriendlist);
	writeStream.Write(m_seq);
	writeStream.Write(os.str().c_str(), os.str().length());
	writeStream.Flush();

    LOG_INFO << "Response to client: cmd=msg_type_getofriendlist, data=" << os.str() << ", userid=" << m_userinfo.userid;

    Send(outbuf);
}

void ClientSession::OnFindUserResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn)
{
    //{ "type": 1, "username" : "zhangyl" }
    Json::Reader JsonReader;
    Json::Value JsonRoot;
    if (!JsonReader.parse(data, JsonRoot))
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    if (!JsonRoot["type"].isInt() || !JsonRoot["username"].isString())
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    string retData;
    //TODO: Ŀǰֻ֧�ֲ��ҵ����û�
    string username = JsonRoot["username"].asString();
    User cachedUser;
    if (!Singleton<UserManager>::Instance().GetUserInfoByUsername(username, cachedUser))
        retData = "{ \"code\": 0, \"msg\": \"ok\", \"userinfo\": [] }";
    else
    {
        //TODO: �û��Ƚ϶��ʱ��Ӧ��ʹ�ö�̬string
        char szUserInfo[256] = { 0 };
        snprintf(szUserInfo, 256, "{ \"code\": 0, \"msg\": \"ok\", \"userinfo\": [{\"userid\": %d, \"username\": \"%s\", \"nickname\": \"%s\", \"facetype\":%d}] }", cachedUser.userid, cachedUser.username.c_str(), cachedUser.nickname.c_str(), cachedUser.facetype);
        retData = szUserInfo;
    }

    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_finduser);
    writeStream.Write(m_seq);
    writeStream.Write(retData.c_str(), retData.length());
    writeStream.Flush();

    LOG_INFO << "Response to client: cmd=msg_type_finduser, data=" << retData << ", userid=" << m_userinfo.userid;

    Send(outbuf);
}

void ClientSession::OnOperateFriendResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn)
{
    Json::Reader JsonReader;
    Json::Value JsonRoot;
    if (!JsonReader.parse(data, JsonRoot))
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    if (!JsonRoot["type"].isInt() || !JsonRoot["userid"].isInt())
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    int type = JsonRoot["type"].asInt();
    int32_t targetUserid = JsonRoot["userid"].asInt();
    if (targetUserid >= GROUPID_BOUBDARY)
    {
        if (type == 4)
        {
            //��Ⱥ
            DeleteFriend(conn, targetUserid);
            return;
        }

        //��Ⱥֱ��ͬ��
        OnAddGroupResponse(targetUserid, conn);
        return;
    }

    char szData[256] = { 0 };
    //ɾ������
    if (type == 4)
    {
        DeleteFriend(conn, targetUserid);
        return;
    }
    //�����Ӻ�������
    if (type == 1)
    {
        //{"userid": 9, "type": 1, }        
        snprintf(szData, 256, "{\"userid\":%d, \"type\":2, \"username\": \"%s\"}", m_userinfo.userid, m_userinfo.username.c_str());
    }
    //Ӧ��Ӻ���
    else if (type == 3)
    {
        if (!JsonRoot["accept"].isInt())
        {
            LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << "client: " << conn->peerAddress().toIpPort();
            return;
        }

        int accept = JsonRoot["accept"].asInt();
        //���ܼӺ�������󣬽������ѹ�ϵ
        if (accept == 1)
        {
            int smallid = m_userinfo.userid;
            int greatid = targetUserid;
            //���ݿ����滥Ϊ���ѵ�������id��С�����ȣ������ں�
            if (smallid > greatid)
            {
                smallid = targetUserid;
                greatid = m_userinfo.userid;
            }

            if (!Singleton<UserManager>::Instance().MakeFriendRelationship(smallid, greatid))
            {
                LOG_ERROR << "make relationship error: " << data << ", userid: " << m_userinfo.userid << "client: " << conn->peerAddress().toIpPort();
                return;
            }
        }

        //{ "userid": 9, "type" : 3, "userid" : 9, "username" : "xxx", "accept" : 1 }
        snprintf(szData, 256, "{\"userid\": %d, \"type\": 3, \"username\": \"%s\", \"accept\": %d}", m_userinfo.userid, m_userinfo.username.c_str(), accept);

        //��ʾ�Լ���ǰ�û��Ӻ��ѳɹ�
        User targetUser;
        if (!Singleton<UserManager>::Instance().GetUserInfoByUserId(targetUserid, targetUser))
        {
            LOG_ERROR << "Get Userinfo by id error, targetuserid: " << targetUserid << ", userid: " << m_userinfo.userid << ", data: "<< data << ", client: " << conn->peerAddress().toIpPort();
            return;
        }
        char szSelfData[256] = { 0 };
        snprintf(szSelfData, 256, "{\"userid\": %d, \"type\": 3, \"username\": \"%s\", \"accept\": %d}", targetUser.userid, targetUser.username.c_str(), accept);
        std::string outbufx;
        yt::BinaryWriteStream3 writeStream(&outbufx);
        writeStream.Write(msg_type_operatefriend);
        writeStream.Write(m_seq);
        writeStream.Write(szSelfData, strlen(szSelfData));
        writeStream.Flush();

        Send(outbufx);
        LOG_INFO << "Response to client: cmd=msg_type_addfriend, data=" << szSelfData << ", userid=" << m_userinfo.userid;
    }

    //��ʾ�Է��Ӻ��ѳɹ�
    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_operatefriend);
    writeStream.Write(m_seq);
    writeStream.Write(szData, strlen(szData));
    writeStream.Flush();

    //�ȿ�Ŀ���û��Ƿ�����
    std::shared_ptr<ClientSession> targetSession;
    Singleton<IMServer>::Instance().GetSessionByUserId(targetSession, targetUserid);
    //Ŀ���û������ߣ����������Ϣ
    if (!targetSession)
    {
        Singleton<MsgCacheManager>::Instance().AddNotifyMsgCache(targetUserid, outbuf);
        LOG_INFO << "userid: " << targetUserid << " is not online, cache notify msg, msg: " << outbuf;
        return;
    }

    targetSession->Send(outbuf);

    LOG_INFO << "Response to client: cmd=msg_type_addfriend, data=" << data << ", userid=" << targetUserid;
}

void ClientSession::OnAddGroupResponse(int32_t groupId, const std::shared_ptr<TcpConnection>& conn)
{
    if (!Singleton<UserManager>::Instance().MakeFriendRelationship(m_userinfo.userid, groupId))
    {
        LOG_ERROR << "make relationship error, groupId: " << groupId << ", userid: " << m_userinfo.userid << "client: " << conn->peerAddress().toIpPort();
        return;
    }
    
    User groupUser;
    if (!Singleton<UserManager>::Instance().GetUserInfoByUserId(groupId, groupUser))
    {
        LOG_ERROR << "Get group info by id error, targetuserid: " << groupId << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }
    char szSelfData[256] = { 0 };
    snprintf(szSelfData, 256, "{\"userid\": %d, \"type\": 3, \"username\": \"%s\", \"accept\": 3}", groupUser.userid, groupUser.username.c_str());
    std::string outbufx;
    yt::BinaryWriteStream3 writeStream(&outbufx);
    writeStream.Write(msg_type_operatefriend);
    writeStream.Write(m_seq);
    writeStream.Write(szSelfData, strlen(szSelfData));
    writeStream.Flush();

    Send(outbufx);
    LOG_INFO << "Response to client: cmd=msg_type_addfriend, data=" << szSelfData << ", userid=" << m_userinfo.userid;

    //����������Ⱥ��Ա����Ⱥ��Ϣ�����仯����Ϣ
    std::list<User> friends;
    Singleton<UserManager>::Instance().GetFriendInfoByUserId(groupId, friends);
    IMServer& imserver = Singleton<IMServer>::Instance();
    for (const auto& iter : friends)
    {
        //�ȿ�Ŀ���û��Ƿ�����
        std::shared_ptr<ClientSession> targetSession;
        imserver.GetSessionByUserId(targetSession, iter.userid);
        if (targetSession)
            targetSession->SendUserStatusChangeMsg(groupId, 3);
    }
}

void ClientSession::OnUpdateUserInfoResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn)
{
    Json::Reader JsonReader;
    Json::Value JsonRoot;
    if (!JsonReader.parse(data, JsonRoot))
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    if (!JsonRoot["nickname"].isString() || !JsonRoot["facetype"].isInt() || 
        !JsonRoot["customface"].isString() || !JsonRoot["gender"].isInt() || 
        !JsonRoot["birthday"].isInt() || !JsonRoot["signature"].isString() || 
        !JsonRoot["address"].isString() || !JsonRoot["phonenumber"].isString() || 
        !JsonRoot["mail"].isString())
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    User newuserinfo;
    newuserinfo.nickname = JsonRoot["nickname"].asString();
    newuserinfo.facetype = JsonRoot["facetype"].asInt();
    newuserinfo.customface = JsonRoot["customface"].asString();
    newuserinfo.gender = JsonRoot["gender"].asInt();
    newuserinfo.birthday = JsonRoot["birthday"].asInt();
    newuserinfo.signature = JsonRoot["signature"].asString();
    newuserinfo.address = JsonRoot["address"].asString();
    newuserinfo.phonenumber = JsonRoot["phonenumber"].asString();
    newuserinfo.mail = JsonRoot["mail"].asString();
    
    ostringstream retdata;
    ostringstream currentuserinfo;
    if (!Singleton<UserManager>::Instance().UpdateUserInfo(m_userinfo.userid, newuserinfo))
    {
        retdata << "{ \"code\": 104, \"msg\": \"update user info failed\" }";
    }
    else
    {
        /*
        { "code": 0, "msg" : "ok", "userid" : 2, "username" : "xxxx", 
         "nickname":"zzz", "facetype" : 26, "customface" : "", "gender" : 0, "birthday" : 19900101, 
         "signature" : "xxxx", "address": "", "phonenumber": "", "mail":""}
        */
        currentuserinfo << "\"userid\": " << m_userinfo.userid << ",\"username\":\"" << m_userinfo.username
                        << "\", \"nickname\":\"" << newuserinfo.nickname
                        << "\", \"facetype\": " << newuserinfo.facetype << ", \"customface\":\"" << newuserinfo.customface
                        << "\", \"gender\":" << newuserinfo.gender
                        << ", \"birthday\":" << newuserinfo.birthday << ", \"signature\":\"" << newuserinfo.signature
                        << "\", \"address\": \"" << newuserinfo.address
                        << "\", \"phonenumber\": \"" << newuserinfo.phonenumber << "\", \"mail\":\""
                        << newuserinfo.mail;
        retdata << "{\"code\": 0, \"msg\": \"ok\"," << currentuserinfo.str()  << "\"}";
    }

    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_updateuserinfo);
    writeStream.Write(m_seq);
    writeStream.Write(retdata.str().c_str(), retdata.str().length());
    writeStream.Flush();

    //Ӧ��ͻ���
    Send(outbuf);

    LOG_INFO << "Response to client: cmd=msg_type_updateuserinfo, data=" << retdata.str() << ", userid=" << m_userinfo.userid;

    //���������ߺ������͸�����Ϣ�����ı���Ϣ
    std::list<User> friends;
    Singleton<UserManager>::Instance().GetFriendInfoByUserId(m_userinfo.userid, friends);
    IMServer& imserver = Singleton<IMServer>::Instance();
    for (const auto& iter : friends)
    {
        //�ȿ�Ŀ���û��Ƿ�����
        std::shared_ptr<ClientSession> targetSession;
        imserver.GetSessionByUserId(targetSession, iter.userid);
        if (targetSession)
            targetSession->SendUserStatusChangeMsg(m_userinfo.userid, 3);
    }
}

void ClientSession::OnModifyPasswordResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn)
{
    Json::Reader JsonReader;
    Json::Value JsonRoot;
    if (!JsonReader.parse(data, JsonRoot))
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    if (!JsonRoot["oldpassword"].isString() || !JsonRoot["newpassword"].isString())
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    string oldpass = JsonRoot["oldpassword"].asString();
    string newPass = JsonRoot["newpassword"].asString();

    string retdata;
    User cachedUser;
    if (!Singleton<UserManager>::Instance().GetUserInfoByUserId(m_userinfo.userid, cachedUser))
    {
        LOG_ERROR << "get userinfo error, userid: " << m_userinfo.userid << ", data: " << data << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    if (cachedUser.password != oldpass)
    {
        retdata = "{\"code\": 103, \"msg\": \"incorrect old password\"}";
    }
    else
    {       
        if (!Singleton<UserManager>::Instance().ModifyUserPassword(m_userinfo.userid, newPass))
        {
            retdata = "{\"code\": 105, \"msg\": \"modify password error\"}";
            LOG_ERROR << "modify password error, userid: " << m_userinfo.userid << ", data: " << data << ", client: " << conn->peerAddress().toIpPort();
        }
        else
            retdata = "{\"code\": 0, \"msg\": \"ok\"}";
    }

    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_modifypassword);
    writeStream.Write(m_seq);
    writeStream.Write(retdata.c_str(), retdata.length());
    writeStream.Flush();

    //Ӧ��ͻ���
    Send(outbuf);

    LOG_INFO << "Response to client: cmd=msg_type_modifypassword, data=" << data << ", userid=" << m_userinfo.userid;
}

void ClientSession::OnCreateGroupResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn)
{
    Json::Reader JsonReader;
    Json::Value JsonRoot;
    if (!JsonReader.parse(data, JsonRoot))
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    if (!JsonRoot["groupname"].isString())
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    ostringstream retdata;
    string groupname = JsonRoot["groupname"].asString();
    int32_t groupid;
    if (!Singleton<UserManager>::Instance().AddGroup(groupname.c_str(), m_userinfo.userid, groupid))
    {
        LOG_WARN << "Add group error, data: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        retdata << "{ \"code\": 106, \"msg\" : \"create group error\"}";
    }
    else
    {
        retdata << "{\"code\": 0, \"msg\": \"ok\", \"groupid\":" << groupid << ", \"groupname\": \"" << groupname << "\"}";
    }

    //�����ɹ��Ժ���û��Զ���Ⱥ
    if (!Singleton<UserManager>::Instance().MakeFriendRelationship(m_userinfo.userid, groupid))
    {
        LOG_ERROR << "join in group, errordata: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_creategroup);
    writeStream.Write(m_seq);
    writeStream.Write(retdata.str().c_str(), retdata.str().length());
    writeStream.Flush();

    //Ӧ��ͻ��ˣ���Ⱥ�ɹ�
    Send(outbuf);

    LOG_INFO << "Response to client: cmd=msg_type_creategroup, data=" << retdata.str() << ", userid=" << m_userinfo.userid;

    //Ӧ��ͻ��ˣ��ɹ���Ⱥ
    {
        char szSelfData[256] = { 0 };
        snprintf(szSelfData, 256, "{\"userid\": %d, \"type\": 3, \"username\": \"%s\", \"accept\": 1}", groupid, groupname.c_str());
        std::string outbufx;
        yt::BinaryWriteStream3 writeStream(&outbufx);
        writeStream.Write(msg_type_operatefriend);
        writeStream.Write(m_seq);
        writeStream.Write(szSelfData, strlen(szSelfData));
        writeStream.Flush();

        Send(outbufx);
        LOG_INFO << "Response to client: cmd=msg_type_addfriend, data=" << szSelfData << ", userid=" << m_userinfo.userid;
    }
}

void ClientSession::OnGetGroupMembersResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn)
{
    //{"groupid": Ⱥid}
    Json::Reader JsonReader;
    Json::Value JsonRoot;
    if (!JsonReader.parse(data, JsonRoot))
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    if (!JsonRoot["groupid"].isInt())
    {
        LOG_WARN << "invalid json: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    int32_t groupid = JsonRoot["groupid"].asInt();
    
    std::list<User> friends;
    Singleton<UserManager>::Instance().GetFriendInfoByUserId(groupid, friends);
    std::string strUserInfo;
    bool userOnline = false;
    IMServer& imserver = Singleton<IMServer>::Instance();
    for (const auto& iter : friends)
    {
        userOnline = imserver.IsUserSessionExsit(iter.userid);
        /*
        {"code": 0, "msg": "ok", "members":[{"userid": 1,"username":"qqq,
        "nickname":"qqq, "facetype": 0, "customface":"", "gender":0, "birthday":19900101,
        "signature":", "address": "", "phonenumber": "", "mail":", "clienttype": 1, "status":1"]}
        */
        ostringstream osSingleUserInfo;
        osSingleUserInfo << "{\"userid\": " << iter.userid << ",\"username\":\"" << iter.username << "\", \"nickname\":\"" << iter.nickname
            << "\", \"facetype\": " << iter.facetype << ", \"customface\":\"" << iter.customface << "\", \"gender\":" << iter.gender
            << ", \"birthday\":" << iter.birthday << ", \"signature\":\"" << iter.signature << "\", \"address\": \"" << iter.address
            << "\", \"phonenumber\": \"" << iter.phonenumber << "\", \"mail\":\"" << iter.mail << "\", \"clienttype\": 1, \"status\":"
            << (userOnline ? 1 : 0) << "}";

        strUserInfo += osSingleUserInfo.str();
        strUserInfo += ",";
    }
    //ȥ��������Ķ���
    strUserInfo = strUserInfo.substr(0, strUserInfo.length() - 1);
    std::ostringstream os;
    os << "{\"code\": 0, \"msg\": \"ok\", \"groupid\": " << groupid << ", \"members\":[" << strUserInfo << "]}";

    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_getgroupmembers);
    writeStream.Write(m_seq);
    writeStream.Write(os.str().c_str(), os.str().length());
    writeStream.Flush();

    LOG_INFO << "Response to client: cmd=msg_type_getgroupmembers, data=" << os.str() << ", userid=" << m_userinfo.userid;

    Send(outbuf);
}

void ClientSession::SendUserStatusChangeMsg(int32_t userid, int type)
{
    string data; 
    //�û�����
    if (type == 1)
    {
        data = "{\"type\": 1, \"onlinestatus\": 1}";
    }
    //�û�����
    else if (type == 2)
    {
        data = "{\"type\": 2, \"onlinestatus\": 0}";
    }
    //�����ǳơ�ͷ��ǩ������Ϣ����
    else if (type == 3)
    {
        data = "{\"type\": 3}";
    }

    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_userstatuschange);
    writeStream.Write(m_seq);
    writeStream.Write(data.c_str(), data.length());
    writeStream.Write(userid);
    writeStream.Flush();

    Send(outbuf);

    LOG_INFO << "Send to client: cmd=msg_type_userstatuschange, data=" << data << ", userid=" << m_userinfo.userid;
}

void ClientSession::OnChatResponse(int32_t targetid, const std::string& data, const std::shared_ptr<TcpConnection>& conn)
{
    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_chat);
    writeStream.Write(m_seq);
    writeStream.Write(data.c_str(), data.length());
    //��Ϣ������
    writeStream.Write(m_userinfo.userid);
    //��Ϣ������
    writeStream.Write(targetid);
    writeStream.Flush();

    UserManager& userMgr = Singleton<UserManager>::Instance();
    //д����Ϣ��¼
    if (!userMgr.SaveChatMsgToDb(m_userinfo.userid, targetid, data))
    {
        LOG_ERROR << "Write chat msg to db error, , senderid = " << m_userinfo.userid << ", targetid = " << targetid << ", chatmsg:" << data;
    }

    IMServer& imserver = Singleton<IMServer>::Instance();
    MsgCacheManager& msgCacheMgr = Singleton<MsgCacheManager>::Instance();
    //������Ϣ
    if (targetid < GROUPID_BOUBDARY)
    {
        //�ȿ�Ŀ���û��Ƿ�����
        std::shared_ptr<ClientSession> targetSession;
        imserver.GetSessionByUserId(targetSession, targetid);
        //Ŀ���û������ߣ����������Ϣ
        if (!targetSession)
        {
            msgCacheMgr.AddChatMsgCache(targetid, outbuf);
            return;
        }

        targetSession->Send(outbuf);
        return;
    }

    //Ⱥ����Ϣ
    std::list<User> friends;
    userMgr.GetFriendInfoByUserId(targetid, friends);
    std::string strUserInfo;
    bool userOnline = false;
    for (const auto& iter : friends)
    {
        //�ȿ�Ŀ���û��Ƿ�����
        std::shared_ptr<ClientSession> targetSession;
        imserver.GetSessionByUserId(targetSession, iter.userid);
        //Ŀ���û������ߣ����������Ϣ
        if (!targetSession)
        {
            msgCacheMgr.AddChatMsgCache(iter.userid, outbuf);
           continue;
        }

        targetSession->Send(outbuf);
    }
    
}

void ClientSession::OnMultiChatResponse(const std::string& targets, const std::string& data, const std::shared_ptr<TcpConnection>& conn)
{
    Json::Reader JsonReader;
    Json::Value JsonRoot;
    if (!JsonReader.parse(targets, JsonRoot))
    {
        LOG_ERROR << "invalid json: targets: " << targets  << "data: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    if (!JsonRoot["targets"].isArray())
    {
        LOG_ERROR << "invalid json: targets: " << targets << "data: " << data << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    for (uint32_t i = 0; i < JsonRoot["targets"].size(); ++i)
    {
        OnChatResponse(JsonRoot["targets"][i].asInt(), data, conn);
    }

    LOG_INFO << "Send to client: cmd=msg_type_multichat, targets: " << targets << "data : " << data << ", userid : " << m_userinfo.userid << ", client : " << conn->peerAddress().toIpPort();
}

void ClientSession::DeleteFriend(const std::shared_ptr<TcpConnection>& conn, int32_t friendid)
{
    int32_t smallerid = friendid;
    int32_t greaterid = m_userinfo.userid;
    if (smallerid > greaterid)
    {
        smallerid = m_userinfo.userid;
        greaterid = friendid;
    }

    if (!Singleton<UserManager>::Instance().ReleaseFriendRelationship(smallerid, greaterid))
    {
        LOG_ERROR << "Delete friend error, friendid: " << friendid << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    User cachedUser;
    if (!Singleton<UserManager>::Instance().GetUserInfoByUserId(friendid, cachedUser))
    {
        LOG_ERROR << "Delete friend - Get user error, friendid: " << friendid << ", userid: " << m_userinfo.userid << ", client: " << conn->peerAddress().toIpPort();
        return;
    }

    char szData[256] = { 0 };
    //��������ɾ����һ��
    //{"userid": 9, "type": 1, }        
    snprintf(szData, 256, "{\"userid\":%d, \"type\":5, \"username\": \"%s\"}", friendid, cachedUser.username.c_str());
    std::string outbuf;
    yt::BinaryWriteStream3 writeStream(&outbuf);
    writeStream.Write(msg_type_operatefriend);
    writeStream.Write(m_seq);
    writeStream.Write(szData, strlen(szData));
    writeStream.Flush();

    Send(outbuf);

    LOG_INFO << "Send to client: cmd=msg_type_operatefriend, data=" << szData << ", userid=" << m_userinfo.userid;

    //������ɾ����һ��
    //ɾ��������Ϣ
    if (friendid < GROUPID_BOUBDARY)
    {
        outbuf.clear();
        //�ȿ�Ŀ���û��Ƿ�����
        std::shared_ptr<ClientSession> targetSession;
        Singleton<IMServer>::Instance().GetSessionByUserId(targetSession, friendid);
        //���������û����������Ϣ
        if (targetSession)
        {
            memset(szData, 0, sizeof(szData));
            snprintf(szData, 256, "{\"userid\":%d, \"type\":5, \"username\": \"%s\"}", m_userinfo.userid, m_userinfo.username.c_str());
            outbuf.clear();
            writeStream.Clear();
            writeStream.Write(msg_type_operatefriend);
            writeStream.Write(m_seq);
            writeStream.Write(szData, strlen(szData));
            writeStream.Flush();

            targetSession->Send(outbuf);

            LOG_INFO << "Send to client: cmd=msg_type_operatefriend, data=" << szData << ", userid=" << friendid;
        }

        return;
    }
    
    //��Ⱥ��Ϣ
    //����������Ⱥ��Ա����Ⱥ��Ϣ�����仯����Ϣ
    std::list<User> friends;
    Singleton<UserManager>::Instance().GetFriendInfoByUserId(friendid, friends);
    IMServer& imserver = Singleton<IMServer>::Instance();
    for (const auto& iter : friends)
    {
        //�ȿ�Ŀ���û��Ƿ�����
        std::shared_ptr<ClientSession> targetSession;
        imserver.GetSessionByUserId(targetSession, iter.userid);
        if (targetSession)
            targetSession->SendUserStatusChangeMsg(friendid, 3);
    }

}