
#pragma once
#include <list>
#include <stdint.h>
#include <string>
#include <mutex>

struct NotifyMsgCache
{
    int32_t     userid;
    std::string notifymsg;  
};

struct ChatMsgCache
{
    int32_t     userid;
    std::string chatmsg;
};

class MsgCacheManager final
{
public:
    MsgCacheManager();
    ~MsgCacheManager();

    MsgCacheManager(const MsgCacheManager& rhs) = delete;
    MsgCacheManager& operator =(const MsgCacheManager& rhs) = delete;

    bool AddNotifyMsgCache(int32_t userid, const std::string& cache);
    void GetNotifyMsgCache(int32_t userid, std::list<NotifyMsgCache>& cached);

    bool AddChatMsgCache(int32_t userid, const std::string& cache);
    void GetChatMsgCache(int32_t userid, std::list<ChatMsgCache>& cached);


private:
    std::list<NotifyMsgCache>       m_listNotifyMsgCache;    //通知类消息缓存，比如加好友消息
    std::mutex                      m_mtNotifyMsgCache;
    std::list<ChatMsgCache>         m_listChatMsgCache;      //聊天消息缓存
    std::mutex                      m_mtChatMsgCache;
};
