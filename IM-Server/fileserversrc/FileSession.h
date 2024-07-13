
#pragma once
#include "../net/buffer.h"
#include "TcpSession.h"

class FileSession : public TcpSession
{
public:
    FileSession(const std::shared_ptr<TcpConnection>& conn);
    virtual ~FileSession();

    FileSession(const FileSession& rhs) = delete;
    FileSession& operator =(const FileSession& rhs) = delete;

    //�����ݿɶ�, �ᱻ�������loop����
    void OnRead(const std::shared_ptr<TcpConnection>& conn, Buffer* pBuffer, Timestamp receivTime);   

    void SendUserStatusChangeMsg(int32_t userid, int type);

private:
    bool Process(const std::shared_ptr<TcpConnection>& conn, const char* inbuf, size_t length);
    
    void OnUploadFileResponse(const std::string& filemd5, int32_t offset, int32_t filesize, const std::string& filedata, const std::shared_ptr<TcpConnection>& conn);
    void OnDownloadFileResponse(const std::string& filemd5, int32_t offset, int32_t filesize, const std::shared_ptr<TcpConnection>& conn);

    void ResetFile();

private:
    int32_t           m_id;         //session id
    int               m_seq;        //��ǰSession���ݰ����к�

    //��ǰ�ļ���Ϣ
    FILE*             m_fp{};
    int32_t           m_offset{};
    int32_t           m_filesize{};
};