

#pragma once
#include "../net/buffer.h"
#include "TcpSession.h"

struct OnlineUserInfo
{
    int32_t     userid;
    std::string username;
    std::string nickname;
    std::string password;
    int32_t     clienttype;     //�ͻ�������,pc=1, android=2, ios=3
    int32_t     status;         //����״̬ 0���� 1���� 2æµ 3�뿪 4����
};


class ClientSession : public TcpSession
{
public:
    ClientSession(const std::shared_ptr<TcpConnection>& conn);
    virtual ~ClientSession();

    ClientSession(const ClientSession& rhs) = delete;
    ClientSession& operator =(const ClientSession& rhs) = delete;

    //�����ݿɶ�, �ᱻ�������loop����
    void OnRead(const std::shared_ptr<TcpConnection>& conn, Buffer* pBuffer, Timestamp receivTime);   

    int32_t GetUserId()
    {
        return m_userinfo.userid;
    }

    void SendUserStatusChangeMsg(int32_t userid, int type);

private:
    bool Process(const std::shared_ptr<TcpConnection>& conn, const char* inbuf, size_t length);
    
    void OnHeartbeatResponse(const std::shared_ptr<TcpConnection>& conn);
    void OnRegisterResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn);
    void OnLoginResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn);
    void OnGetFriendListResponse(const std::shared_ptr<TcpConnection>& conn);
    void OnFindUserResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn);
    void OnOperateFriendResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn);
    void OnAddGroupResponse(int32_t groupId, const std::shared_ptr<TcpConnection>& conn);
    void OnUpdateUserInfoResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn);
    void OnModifyPasswordResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn);
    void OnCreateGroupResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn);
    void OnGetGroupMembersResponse(const std::string& data, const std::shared_ptr<TcpConnection>& conn);
    void OnChatResponse(int32_t targetid, const std::string& data, const std::shared_ptr<TcpConnection>& conn);
    void OnMultiChatResponse(const std::string& targets, const std::string& data, const std::shared_ptr<TcpConnection>& conn);


    void DeleteFriend(const std::shared_ptr<TcpConnection>& conn, int32_t friendid);

private:
    int32_t           m_id;         //session id
    OnlineUserInfo    m_userinfo;
    int               m_seq;        //��ǰSession���ݰ����к�
};