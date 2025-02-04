#include "stdafx.h"
#include "EdoyunIMClient.h"
#include "IniFile.h"
#include "LoginDlg.h"
#include "net/IUProtocolData.h"
#include "IULog.h"
#include "UpdateDlg.h"
#include "EncodingUtil.h"
#include "UserSessionData.h"
#include "CustomMsgDef.h"
#include "Startup.h"
#include "File.h"

CEdoyunClient& CEdoyunClient::GetInstance()
{
	static CEdoyunClient client;
	return client;
}


CEdoyunClient::CEdoyunClient(void) :
	m_SocketClient(&m_RecvMsgThread),
	m_SendMsgThread(&m_SocketClient),
	m_RecvMsgThread(&m_SocketClient),
	m_FileTask(&m_SocketClient)
{
	m_hwndCreateNewGroup = NULL;
	m_ServerTime = 0;

	m_bNetworkAvailable = TRUE;

	m_hwndRegister = NULL;
	m_hwndFindFriend = NULL;
	m_hwndModifyPassword = NULL;

	m_bBuddyIDsAvailable = FALSE;
	m_bBuddyListAvailable = FALSE;

	m_nGroupCount = 0;
	m_bGroupInfoAvailable = FALSE;
	m_bGroupMemberInfoAvailable = FALSE;

	m_SendMsgThread.m_lpUserMgr = &m_UserMgr;
	m_RecvMsgThread.m_lpUserMgr = &m_UserMgr;
	m_RecvMsgThread.m_pFMGClient = this;
	m_FileTask.m_lpFMGClient = this;
}

CEdoyunClient::~CEdoyunClient(void)
{

}

// 初始化客户端
BOOL CEdoyunClient::Init()
{
	BOOL bRet = CreateProxyWnd();	// 创建代理窗口
	if (!bRet)
		return FALSE;

	m_SocketClient.Init();

	m_SendMsgThread.Start();
	m_RecvMsgThread.Start();

	m_SendMsgThread.m_lpFMGClient = this;

	m_FileTask.Start();

	return TRUE;
}

// 反初始化客户端
void CEdoyunClient::UnInit()
{
	DestroyProxyWnd();				// 销毁代理窗口

	m_SocketClient.Uninit();
	m_SendMsgThread.Stop();
	m_RecvMsgThread.Stop();
	m_FileTask.Stop();
	try {
		m_SocketClient.Join();
		m_SendMsgThread.Join();
		m_RecvMsgThread.Join();
	}
	catch (...) {
		//TRACE("exception found");
	}
	m_FileTask.Join();
}

void CEdoyunClient::SetServer(PCTSTR pszServer)
{
	m_SocketClient.SetServer(pszServer);
}

void CEdoyunClient::SetFileServer(PCTSTR pszServer)
{
	m_SocketClient.SetFileServer(pszServer);
}

void CEdoyunClient::SetPort(short port)
{
	m_SocketClient.SetPort(port);
}

void CEdoyunClient::SetFilePort(short port)
{
	m_SocketClient.SetFilePort(port);
}


// 设置UTalk号码和密码
void CEdoyunClient::SetUser(LPCTSTR lpUserAccount, LPCTSTR lpUserPwd)
{
	if (NULL == lpUserAccount || NULL == lpUserPwd || m_UserMgr.m_UserInfo.m_nStatus != STATUS_OFFLINE)
		return;

	m_UserMgr.m_UserInfo.m_strAccount = lpUserAccount;
	m_UserMgr.m_UserInfo.m_strPassword = lpUserPwd;
}

// 设置登录状态
void CEdoyunClient::SetLoginStatus(long nStatus)
{
	m_UserMgr.m_UserInfo.m_nStatus = nStatus;
}

// 设置回调窗口句柄
void CEdoyunClient::SetCallBackWnd(HWND hCallBackWnd)
{
	m_UserMgr.m_hCallBackWnd = hCallBackWnd;
}

void CEdoyunClient::SetRegisterWindow(HWND hwndRegister)
{
	m_hwndRegister = hwndRegister;
}

void CEdoyunClient::SetModifyPasswordWindow(HWND hwndModifyPassword)
{
	m_hwndModifyPassword = hwndModifyPassword;
}

void CEdoyunClient::SetCreateNewGroupWindow(HWND hwndCreateNewGroup)
{
	m_hwndCreateNewGroup = hwndCreateNewGroup;
}

void CEdoyunClient::Register(PCTSTR pszAccountName, PCTSTR pszNickName, PCTSTR pszPassword)
{
	//TODO: 如果此时断网，则直接返回
	CRegisterRequest* pRequest = new CRegisterRequest();
	char szData[64] = { 0 };
	UnicodeToUtf8(pszAccountName, szData, ARRAYSIZE(szData));
	strcpy_s(pRequest->m_szAccountName, ARRAYSIZE(pRequest->m_szAccountName), szData);

	memset(szData, 0, sizeof(szData));
	UnicodeToUtf8(pszNickName, szData, ARRAYSIZE(szData));
	strcpy_s(pRequest->m_szNickName, ARRAYSIZE(pRequest->m_szNickName), szData);

	memset(szData, 0, sizeof(szData));
	UnicodeToUtf8(pszPassword, szData, ARRAYSIZE(szData));
	strcpy_s(pRequest->m_szPassword, ARRAYSIZE(pRequest->m_szPassword), szData);

	m_SendMsgThread.AddItem(pRequest);
}

// 查找好友
BOOL CEdoyunClient::FindFriend(PCTSTR pszAccountName, long nType, HWND hReflectionWnd)
{
	//TODO: 先判断一下是否离线

	if (pszAccountName == NULL || *pszAccountName == NULL)
		return FALSE;

	m_hwndFindFriend = hReflectionWnd;

	CFindFriendRequest* pRequest = new CFindFriendRequest();
	char szData[64] = { 0 };
	UnicodeToUtf8(pszAccountName, szData, ARRAYSIZE(szData));
	strcpy_s(pRequest->m_szAccountName, ARRAYSIZE(pRequest->m_szAccountName), szData);
	pRequest->m_nType = nType;

	m_SendMsgThread.AddItem(pRequest);

	return TRUE;
}

BOOL CEdoyunClient::AddFriend(UINT uAccountToAdd)
{
	//TODO: 先判断是否离线
	if (uAccountToAdd == 0)
		return FALSE;

	COperateFriendRequest* pRequest = new COperateFriendRequest();
	pRequest->m_uCmd = Apply;
	pRequest->m_uAccountID = uAccountToAdd;
	//m_mapAddFriendCache.insert(std::pair<UINT, UINT>(uAccountToAdd, Apply));

	m_SendMsgThread.AddItem(pRequest);
	return TRUE;
}

// 删除好友
BOOL CEdoyunClient::DeleteFriend(UINT uAccountID)
{
	//TODO: 先判断是否离线
	COperateFriendRequest* pRequest = new COperateFriendRequest();
	pRequest->m_uCmd = Delete;
	pRequest->m_uAccountID = uAccountID;

	m_SendMsgThread.AddItem(pRequest);

	return TRUE;
}

BOOL CEdoyunClient::UpdateLogonUserInfo(PCTSTR pszNickName,
	PCTSTR pszSignature,
	UINT uGender,
	long nBirthday,
	PCTSTR pszAddress,
	PCTSTR pszPhone,
	PCTSTR pszMail,
	UINT uSysFaceID,
	PCTSTR pszCustomFacePath,
	BOOL bUseCustomThumb)
{
	if (!m_bNetworkAvailable)
		return FALSE;

	CUpdateLogonUserInfoRequest* pRequest = new CUpdateLogonUserInfoRequest();
	char szData[512] = { 0 };
	if (pszNickName != NULL && *pszNickName != NULL)
	{
		UnicodeToUtf8(pszNickName, szData, ARRAYSIZE(szData));
		strcpy_s(pRequest->m_szNickName, ARRAYSIZE(pRequest->m_szNickName), szData);
	}

	if (pszSignature != NULL && *pszSignature != NULL)
	{
		memset(szData, 0, sizeof(szData));
		UnicodeToUtf8(pszSignature, szData, ARRAYSIZE(szData));
		strcpy_s(pRequest->m_szSignature, ARRAYSIZE(pRequest->m_szSignature), szData);
	}

	if (pszAddress != NULL && *pszAddress != NULL)
	{
		memset(szData, 0, sizeof(szData));
		UnicodeToUtf8(pszAddress, szData, ARRAYSIZE(szData));
		strcpy_s(pRequest->m_szAddress, ARRAYSIZE(pRequest->m_szAddress), szData);
	}

	if (pszPhone != NULL && *pszPhone != NULL)
	{
		memset(szData, 0, sizeof(szData));
		UnicodeToUtf8(pszPhone, szData, ARRAYSIZE(szData));
		strcpy_s(pRequest->m_szPhone, ARRAYSIZE(pRequest->m_szPhone), szData);
	}

	if (pszMail != NULL && *pszMail != NULL)
	{
		memset(szData, 0, sizeof(szData));
		UnicodeToUtf8(pszMail, szData, ARRAYSIZE(szData));
		strcpy_s(pRequest->m_szMail, ARRAYSIZE(pRequest->m_szMail), szData);
	}

	pRequest->m_uGender = uGender;
	pRequest->m_nBirthday = nBirthday;
	pRequest->m_uFaceID = uSysFaceID;

	if (!bUseCustomThumb)
	{
		pRequest->m_bUseCustomThumb = FALSE;
		m_UserMgr.m_UserInfo.m_strCustomFace = _T("");
		m_UserMgr.m_UserInfo.m_nFace = uSysFaceID;
		m_UserMgr.m_UserInfo.m_bUseCustomFace = FALSE;
	}
	else
	{
		pRequest->m_bUseCustomThumb = TRUE;
		if (pszCustomFacePath != NULL && &pszCustomFacePath != NULL)
		{
			_tcscpy_s(pRequest->m_szCustomFace, ARRAYSIZE(pRequest->m_szCustomFace), pszCustomFacePath);
		}
		else
		{
			pRequest->m_bUseCustomThumb = FALSE;
			m_UserMgr.m_UserInfo.m_strCustomFace = _T("");
			m_UserMgr.m_UserInfo.m_bUseCustomFace = FALSE;
		}
	}

	m_SendMsgThread.AddItem(pRequest);

	return TRUE;
}

void CEdoyunClient::StartCheckNetworkStatusTask()
{
	m_CheckNetworkStatusTask.m_pTalkClient = this;
}

void CEdoyunClient::StartHeartbeatTask()
{
	//TODO:
}


void CEdoyunClient::SendHeartbeatMessage()
{
	if (!m_bNetworkAvailable)
		return;

	CHeartbeatMessageRequest* pRequest = new CHeartbeatMessageRequest();
	//m_SendMsgTask.AddItem(pRequest);
}

void CEdoyunClient::ModifyPassword(PCTSTR pszOldPassword, PCTSTR pszNewPassword)
{
	if (!m_bNetworkAvailable)
		return;

	char szData[64] = { 0 };
	CModifyPasswordRequest* pRequest = new CModifyPasswordRequest();
	UnicodeToUtf8(pszOldPassword, szData, ARRAYSIZE(szData));
	strcpy_s(pRequest->m_szOldPassword, ARRAYSIZE(pRequest->m_szOldPassword), szData);
	memset(szData, 0, sizeof(szData));
	UnicodeToUtf8(pszNewPassword, szData, ARRAYSIZE(szData));
	strcpy_s(pRequest->m_szNewPassword, ARRAYSIZE(pRequest->m_szNewPassword), szData);
	m_SendMsgThread.AddItem(pRequest);
}

void CEdoyunClient::CreateNewGroup(PCTSTR pszGroupName)
{
	if (!m_bNetworkAvailable)
		return;

	char szData[64] = { 0 };
	CCreateNewGroupRequest* pRequest = new CCreateNewGroupRequest();
	UnicodeToUtf8(pszGroupName, szData, ARRAYSIZE(szData));
	strcpy_s(pRequest->m_szGroupName, ARRAYSIZE(pRequest->m_szGroupName), szData);
	m_SendMsgThread.AddItem(pRequest);
}

void CEdoyunClient::ResponseAddFriendApply(UINT uAccountID, UINT uCmd)
{
	if (uCmd != Agree && uCmd != Refuse)
		return;

	COperateFriendRequest* pRequest = new COperateFriendRequest();

	pRequest->m_uAccountID = uAccountID;
	pRequest->m_uCmd = uCmd;

	//m_mapAddFriendCache.insert(std::pair<UINT, UINT>(uAccountID, uCmd));

	m_SendMsgThread.AddItem(pRequest);
}

// 登录
void CEdoyunClient::Login()
{
	if (!IsOffline() || m_UserMgr.m_UserInfo.m_strAccount.empty() || m_UserMgr.m_UserInfo.m_strPassword.empty())
		return;

	CLoginRequest* pLoginRequest = new CLoginRequest();
	if (pLoginRequest == NULL)
		return;

	char szData[64] = { 0 };
	UnicodeToUtf8(m_UserMgr.m_UserInfo.m_strAccount.c_str(), szData, ARRAYSIZE(szData));
	strcpy_s(pLoginRequest->m_szAccountName, ARRAYSIZE(pLoginRequest->m_szAccountName), szData);

	memset(szData, 0, sizeof(szData));
	UnicodeToUtf8(m_UserMgr.m_UserInfo.m_strPassword.c_str(), szData, ARRAYSIZE(szData));
	strcpy_s(pLoginRequest->m_szPassword, ARRAYSIZE(pLoginRequest->m_szPassword), szData);

	pLoginRequest->m_nStatus = STATUS_ONLINE;

	if (IsMobileNumber(m_UserMgr.m_UserInfo.m_strAccount.c_str()))
		pLoginRequest->m_nLoginType = LOGIN_USE_MOBILE_NUMBER;
	else
		pLoginRequest->m_nLoginType = LOGIN_USE_ACCOUNT;


	m_SendMsgThread.AddItem(pLoginRequest);
}

void CEdoyunClient::GetFriendList()
{
	CUserBasicInfoRequest* pBasicInfoRequest = new CUserBasicInfoRequest();
	pBasicInfoRequest->m_setAccountID.insert(m_UserMgr.m_UserInfo.m_uUserID);
	m_SendMsgThread.AddItem(pBasicInfoRequest);
}

void CEdoyunClient::GetGroupMembers(int32_t groupid)
{
	CGroupBasicInfoRequest* pGroupInfoRequest = new CGroupBasicInfoRequest();
	pGroupInfoRequest->m_groupid = groupid;
	m_SendMsgThread.AddItem(pGroupInfoRequest);
}

// 注销
BOOL CEdoyunClient::Logout()
{
	if (IsOffline())
		return FALSE;

	return FALSE;
}

// 取消登录
void CEdoyunClient::CancelLogin()
{
	m_SendMsgThread.DeleteAllItems();
}

void CEdoyunClient::SetFindFriendWindow(HWND hwndFindFriend)
{
	m_hwndFindFriend = hwndFindFriend;
}

// 改变在线状态
void CEdoyunClient::ChangeStatus(long nStatus)
{
	if (IsOffline())
		return;
}

// 更新好友列表
void CEdoyunClient::UpdateBuddyList()
{
	if (IsOffline())
		return;
}

// 更新群列表
void CEdoyunClient::UpdateGroupList()
{
	if (IsOffline())
		return;
}

// 更新最近联系人列表
void CEdoyunClient::UpdateRecentList()
{
	if (IsOffline())
		return;
}

// 更新好友信息
void CEdoyunClient::UpdateBuddyInfo(UINT nUTalkUin)
{
	if (IsOffline())
		return;
}

// 更新群成员信息
void CEdoyunClient::UpdateGroupMemberInfo(UINT nGroupCode, UINT nUTalkUin)
{
	if (IsOffline())
		return;
}

// 更新群信息
void CEdoyunClient::UpdateGroupInfo(UINT nGroupCode)
{
	if (IsOffline())
		return;

}

// 更新好友号码
void CEdoyunClient::UpdateBuddyNum(UINT nUTalkUin)
{
	if (IsOffline())
		return;

	//TODO:
}

// 更新群成员号码
void CEdoyunClient::UpdateGroupMemberNum(UINT nGroupCode, UINT nUTalkUin)
{
	if (IsOffline())
		return;

	//TODO
}

// 更新群成员号码
void CEdoyunClient::UpdateGroupMemberNum(UINT nGroupCode, std::vector<UINT>* arrUTalkUin)
{
	//TODO
}

// 更新群号码
void CEdoyunClient::UpdateGroupNum(UINT nGroupCode)
{
	if (IsOffline())
		return;

	//TODO:
}

// 更新好友个性签名
void CEdoyunClient::UpdateBuddySign(UINT nUTalkUin)
{
	if (IsOffline())
		return;

	//TODO:
}

// 更新群成员个性签名
void CEdoyunClient::UpdateGroupMemberSign(UINT nGroupCode, UINT nUTalkUin)
{
	if (IsOffline())
		return;

	//TODO:
}

// 修改UTalk个性签名
void CEdoyunClient::ModifyUTalkSign(LPCTSTR lpSign)
{
	const CBuddyInfo& currentLogonUser = m_UserMgr.m_UserInfo;

	UpdateLogonUserInfo(currentLogonUser.m_strNickName.c_str(),
		lpSign,
		currentLogonUser.m_nGender,
		currentLogonUser.m_nBirthday,
		currentLogonUser.m_strAddress.c_str(),
		currentLogonUser.m_strMobile.c_str(),
		currentLogonUser.m_strEmail.c_str(),
		currentLogonUser.m_nFace,
		currentLogonUser.m_strRawCustomFace.c_str(),
		currentLogonUser.m_bUseCustomFace);
}

// 更新好友头像
void CEdoyunClient::UpdateBuddyHeadPic(UINT nUTalkUin, UINT nUTalkNum)
{
	if (IsOffline())
		return;
	//TODO：
}

// 更新群成员头像
void CEdoyunClient::UpdateGroupMemberHeadPic(UINT nGroupCode, UINT nUTalkUin, UINT nUTalkNum)
{
	if (IsOffline())
		return;

	//TODO:
}

// 更新群头像
void CEdoyunClient::UpdateGroupHeadPic(UINT nGroupCode, UINT nGroupNum)
{
	if (IsOffline())
		return;

	//TODO:
}

// 更新群表情信令
void CEdoyunClient::UpdateGroupFaceSignal()
{
	if (IsOffline())
		return;
}

// 发送好友消息
BOOL CEdoyunClient::SendBuddyMsg(UINT nFromUin, const tstring& strFromNickName, UINT nToUin, const tstring& strToNickName, time_t nTime, const tstring& strChatMsg, HWND hwndFrom/* = NULL*/)
{
	return m_SendMsgThread.AddBuddyMsg(nFromUin, strFromNickName, nToUin, strToNickName, nTime, strChatMsg, hwndFrom);
}

// 发送群消息
BOOL CEdoyunClient::SendGroupMsg(UINT nGroupId, time_t nTime, LPCTSTR lpMsg, HWND hwndFrom)
{
	if (IsOffline())
		return FALSE;

	return m_SendMsgThread.AddBuddyMsg(m_UserMgr.m_UserInfo.m_uUserID, m_UserMgr.m_UserInfo.m_strAccount, nGroupId, _T(""), nTime, lpMsg, hwndFrom);
}

// 发送临时会话消息
BOOL CEdoyunClient::SendSessMsg(UINT nGroupId, UINT nToUin, time_t nTime, LPCTSTR lpMsg)
{
	if (IsOffline())
		return FALSE;

	return m_SendMsgThread.AddSessMsg(nGroupId, nToUin, nTime, lpMsg);
}


BOOL CEdoyunClient::SendMultiChatMsg(const std::set<UINT> setAccountID, time_t nTime, LPCTSTR lpMsg, HWND hwndFrom/*=NULL*/)
{
	if (IsOffline())
		return FALSE;
	//群发消息
	return m_SendMsgThread.AddMultiMsg(setAccountID, nTime, lpMsg, hwndFrom);
}

// 是否离线状态
BOOL CEdoyunClient::IsOffline()
{
	return (STATUS_OFFLINE == m_UserMgr.m_UserInfo.m_nStatus) ? TRUE : FALSE;
}

// 获取在线状态
long CEdoyunClient::GetStatus()
{
	return m_UserMgr.m_UserInfo.m_nStatus;
}

// 获取验证码图片
BOOL CEdoyunClient::GetVerifyCodePic(const BYTE*& lpData, DWORD& dwSize)
{
	//lpData = (const BYTE*)m_UserMgr.m_VerifyCodePic.GetData();
	//dwSize = m_UserMgr.m_VerifyCodePic.GetSize();

	//if (lpData != NULL && dwSize > 0)
	//{
	//	return TRUE;
	//}
	//else
	//{
	//	lpData = NULL;
	//	dwSize = 0;
	//	return FALSE;
	//}
	return FALSE;
}

void CEdoyunClient::SetBuddyListAvailable(BOOL bAvailable)
{
	m_bBuddyListAvailable = bAvailable;
}

BOOL CEdoyunClient::IsBuddyListAvailable()
{
	return m_bBuddyListAvailable;
}

// 获取用户信息
CBuddyInfo* CEdoyunClient::GetUserInfo(UINT uAccountID/*=0*/)
{
	if (uAccountID == 0 || uAccountID == m_UserMgr.m_UserInfo.m_uUserID)
		return &m_UserMgr.m_UserInfo;

	CBuddyTeamInfo* pTeamInfo = NULL;
	CBuddyInfo* pBuddyInfo = NULL;
	for (size_t i = 0; i < m_UserMgr.m_BuddyList.m_arrBuddyTeamInfo.size(); ++i)
	{
		pTeamInfo = m_UserMgr.m_BuddyList.m_arrBuddyTeamInfo[i];
		for (size_t j = 0; j < pTeamInfo->m_arrBuddyInfo.size(); ++j)
		{
			pBuddyInfo = pTeamInfo->m_arrBuddyInfo[j];
			if (uAccountID == pBuddyInfo->m_uUserID)
				return pBuddyInfo;
		}
	}


	CGroupInfo* pGroupInfo = NULL;
	for (size_t i = 0; i < m_UserMgr.m_GroupList.m_arrGroupInfo.size(); ++i)
	{
		pGroupInfo = m_UserMgr.m_GroupList.m_arrGroupInfo[i];
		for (size_t j = 0; j < pGroupInfo->m_arrMember.size(); ++j)
		{
			pBuddyInfo = pGroupInfo->m_arrMember[j];
			if (uAccountID == pBuddyInfo->m_uUserID)
				return pBuddyInfo;
		}
	}

	return NULL;
}

// 获取好友列表
CBuddyList* CEdoyunClient::GetBuddyList()
{
	return &m_UserMgr.m_BuddyList;
}

// 获取群列表
CGroupList* CEdoyunClient::GetGroupList()
{
	return &m_UserMgr.m_GroupList;
}

// 获取最近联系人列表
CRecentList* CEdoyunClient::GetRecentList()
{
	return &m_UserMgr.m_RecentList;
}

// 获取消息列表
CMessageList* CEdoyunClient::GetMessageList()
{
	return &m_UserMgr.m_MsgList;
}

// 获取消息记录管理器
CMessageLogger* CEdoyunClient::GetMsgLogger()
{
	return &m_UserMgr.m_MsgLogger;
}

// 获取用户文件夹存放路径
tstring CEdoyunClient::GetUserFolder()
{
	return m_UserMgr.GetUserFolder();
}

// 获取个人文件夹存放路径
tstring CEdoyunClient::GetPersonalFolder(UINT nUserNum/* = 0*/)
{
	return m_UserMgr.GetPersonalFolder();
}

// 获取聊天图片存放路径
tstring CEdoyunClient::GetChatPicFolder(UINT nUserNum/* = 0*/)
{
	return m_UserMgr.GetChatPicFolder();
}

// 获取用户头像图片全路径文件名
tstring CEdoyunClient::GetUserHeadPicFullName(UINT nUserNum/* = 0*/)
{
	return m_UserMgr.GetUserHeadPicFullName(nUserNum);
}

// 获取好友头像图片全路径文件名
tstring CEdoyunClient::GetBuddyHeadPicFullName(UINT nUTalkNum)
{
	return m_UserMgr.GetBuddyHeadPicFullName(nUTalkNum);
}

// 获取群头像图片全路径文件名
tstring CEdoyunClient::GetGroupHeadPicFullName(UINT nGroupNum)
{
	return m_UserMgr.GetGroupHeadPicFullName(nGroupNum);
}

// 获取群成员头像图片全路径文件名
tstring CEdoyunClient::GetSessHeadPicFullName(UINT nUTalkNum)
{
	return m_UserMgr.GetSessHeadPicFullName(nUTalkNum);
}

// 获取聊天图片全路径文件名
tstring CEdoyunClient::GetChatPicFullName(LPCTSTR lpszFileName)
{
	return m_UserMgr.GetChatPicFullName(lpszFileName);
}

// 获取消息记录全路径文件名
tstring CEdoyunClient::GetMsgLogFullName(UINT nUserNum/* = 0*/)
{
	return m_UserMgr.GetMsgLogFullName();
}

// 判断是否需要更新好友头像
BOOL CEdoyunClient::IsNeedUpdateBuddyHeadPic(UINT nUTalkNum)
{
	return m_UserMgr.IsNeedUpdateBuddyHeadPic(nUTalkNum);
}

// 判断是否需要更新群头像
BOOL CEdoyunClient::IsNeedUpdateGroupHeadPic(UINT nGroupNum)
{
	return m_UserMgr.IsNeedUpdateGroupHeadPic(nGroupNum);
}

// 判断是否需要更新群成员头像
BOOL CEdoyunClient::IsNeedUpdateSessHeadPic(UINT nUTalkNum)
{
	return m_UserMgr.IsNeedUpdateSessHeadPic(nUTalkNum);
}

// 获取服务器时间
void CEdoyunClient::RequestServerTime()
{
	//m_IUProtocol.RequestServerTime();
}

time_t CEdoyunClient::GetCurrentTime()
{
	//return m_IUProtocol.GetServerTime();

	return 0;
}

void CEdoyunClient::OnNetworkStatusChange(UINT message, WPARAM wParam, LPARAM lParam)
{
	BOOL bAvailable = (wParam > 0 ? TRUE : FALSE);

	//新状态和当前的一致，保持不变
	if (m_bNetworkAvailable == bAvailable)
		return;

	m_bNetworkAvailable = bAvailable;

	if (m_bNetworkAvailable)
	{
		CIULog::Log(LOG_WARNING, __FUNCSIG__, _T("Local connection is on service, try to relogon."));
		GoOnline();
	}
	else
	{
		CIULog::Log(LOG_WARNING, __FUNCSIG__, _T("Local connection is out of service, make the user offline."));
		GoOffline();
	}

}

void CEdoyunClient::OnHeartbeatResult(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (wParam == HEARTBEAT_DEAD)
	{
		GoOffline();
	}
}

void CEdoyunClient::OnRegisterResult(UINT message, WPARAM wParam, LPARAM lParam)
{
	::SendMessage(m_hwndRegister, message, wParam, lParam);
}

void CEdoyunClient::OnLoginResult(UINT message, WPARAM wParam, LPARAM lParam)
{
	CLoginResult* pLoginResult = (CLoginResult*)lParam;
	if (pLoginResult == NULL)
		return;

	long nLoginResultCode = pLoginResult->m_LoginResultCode;
	UINT uAccountID = pLoginResult->m_uAccountID;

	if (uAccountID > 0)
		m_UserMgr.m_UserInfo.m_uUserID = uAccountID;
	if (nLoginResultCode == LOGIN_SUCCESS)
	{
		m_UserMgr.m_UserInfo.m_nStatus = STATUS_ONLINE;
		TCHAR szAccountName[64] = { 0 };
		Utf8ToUnicode(pLoginResult->m_szAccountName, szAccountName, ARRAYSIZE(szAccountName));
		m_UserMgr.m_UserInfo.m_strAccount = szAccountName;
		TCHAR szNickName[64] = { 0 };
		Utf8ToUnicode(pLoginResult->m_szNickName, szNickName, ARRAYSIZE(szNickName));
		m_UserMgr.m_UserInfo.m_strNickName = szNickName;
		m_UserMgr.m_UserInfo.m_nFace = pLoginResult->m_nFace;
		m_UserMgr.m_UserInfo.m_nGender = pLoginResult->m_nGender;
		m_UserMgr.m_UserInfo.m_nBirthday = pLoginResult->m_nBirthday;

		TCHAR szSignature[512] = { 0 };
		Utf8ToUnicode(pLoginResult->m_szSignature, szSignature, ARRAYSIZE(szSignature));
		m_UserMgr.m_UserInfo.m_strSign = szSignature;

		TCHAR szAddress[512] = { 0 };
		Utf8ToUnicode(pLoginResult->m_szAddress, szAddress, ARRAYSIZE(szAddress));
		m_UserMgr.m_UserInfo.m_strAddress = szAddress;

		TCHAR szPhoneNumber[64] = { 0 };
		Utf8ToUnicode(pLoginResult->m_szPhoneNumber, szPhoneNumber, ARRAYSIZE(szPhoneNumber));
		m_UserMgr.m_UserInfo.m_strMobile = szPhoneNumber;

		TCHAR szMail[512] = { 0 };
		Utf8ToUnicode(pLoginResult->m_szMail, szMail, ARRAYSIZE(szMail));
		m_UserMgr.m_UserInfo.m_strEmail = szMail;

		m_UserMgr.m_UserInfo.m_strCustomFaceName = pLoginResult->m_szCustomFace;
		if (!m_UserMgr.m_UserInfo.m_strCustomFaceName.IsEmpty())
		{
			m_UserMgr.m_UserInfo.m_bUseCustomFace = TRUE;
			CString cachedUserThumb;
			cachedUserThumb.Format(_T("%s%d.png"), m_UserMgr.GetCustomUserThumbFolder().c_str(), uAccountID);
			if (Edoyun::CPath::IsFileExist(cachedUserThumb))
			{
				TCHAR szCachedThumbMd5[64] = { 0 };
				GetFileMd5ValueW(cachedUserThumb, szCachedThumbMd5, ARRAYSIZE(szCachedThumbMd5));

				TCHAR szThumbMd5Unicode[64] = { 0 };
				Utf8ToUnicode(pLoginResult->m_szCustomFace, szThumbMd5Unicode, ARRAYSIZE(szThumbMd5Unicode));
				if (_tcsncmp(szCachedThumbMd5, szThumbMd5Unicode, 32) == 0)
				{
					m_UserMgr.m_UserInfo.m_bUseCustomFace = TRUE;
					m_UserMgr.m_UserInfo.m_bCustomFaceAvailable = TRUE;
				}
				//头像不存在，下载该头像
				else
				{
					CFileItemRequest* pFileItem = new CFileItemRequest();
					pFileItem->m_nFileType = FILE_ITEM_DOWNLOAD_USER_THUMB;
					pFileItem->m_uAccountID = uAccountID;
					strcpy_s(pFileItem->m_szUtfFilePath, ARRAYSIZE(pFileItem->m_szUtfFilePath), pLoginResult->m_szCustomFace);
					m_FileTask.AddItem(pFileItem);
				}
			}
			else
			{
				//下载头像
				CFileItemRequest* pFileItem = new CFileItemRequest();
				pFileItem->m_nFileType = FILE_ITEM_DOWNLOAD_USER_THUMB;
				pFileItem->m_uAccountID = uAccountID;
				strcpy_s(pFileItem->m_szUtfFilePath, ARRAYSIZE(pFileItem->m_szUtfFilePath), pLoginResult->m_szCustomFace);
				m_FileTask.AddItem(pFileItem);
			}
		}
	}

	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_LOGIN_RESULT, 0, nLoginResultCode);
	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_BUDDY_INFO, 0, (LPARAM)m_UserMgr.m_UserInfo.m_uUserID);

	delete pLoginResult;
}

void CEdoyunClient::OnUpdateUserBasicInfo(UINT message, WPARAM wParam, LPARAM lParam)
{
	CUserBasicInfoResult* pResult = (CUserBasicInfoResult*)lParam;
	if (pResult == NULL)
		return;

	m_UserMgr.ClearUserInfo();
	//TODO: 暂且先创建一个默认好友分组
	CBuddyTeamInfo* pTeamInfo = new CBuddyTeamInfo();
	pTeamInfo->m_nIndex = 0;
	pTeamInfo->m_strName = _T("我的好友");
	m_UserMgr.m_BuddyList.m_arrBuddyTeamInfo.push_back(pTeamInfo);

	//TODO: 暂且先创建一个默认群分组

	CBuddyInfo* pBuddyInfo = NULL;
	TCHAR szAccountName[32] = { 0 };
	TCHAR szNickName[32] = { 0 };
	TCHAR szSignature[256] = { 0 };
	TCHAR szPhoneNumber[32] = { 0 };
	TCHAR szMail[32] = { 0 };

	TCHAR szGroupAccount[32];

	CGroupInfo* pGroupInfo = NULL;
	for (auto& iter : pResult->m_listUserBasicInfo)
	{
		Utf8ToUnicode(iter->szAccountName, szAccountName, ARRAYSIZE(szAccountName));
		Utf8ToUnicode(iter->szNickName, szNickName, ARRAYSIZE(szNickName));
		Utf8ToUnicode(iter->szSignature, szSignature, ARRAYSIZE(szSignature));
		Utf8ToUnicode(iter->szPhoneNumber, szPhoneNumber, ARRAYSIZE(szPhoneNumber));
		Utf8ToUnicode(iter->szMail, szMail, ARRAYSIZE(szMail));

		if (iter->uAccountID < 0xFFFFFFF)
		{
			pBuddyInfo = new CBuddyInfo();
			pBuddyInfo->m_uUserID = iter->uAccountID;

			pBuddyInfo->m_strAccount = szAccountName;

			pBuddyInfo->m_strNickName = szNickName;

			pBuddyInfo->m_strSign = szSignature;

			pBuddyInfo->m_strMobile = szPhoneNumber;

			pBuddyInfo->m_strEmail = szMail;
			pBuddyInfo->m_nStatus = iter->nStatus;
			pBuddyInfo->m_nClientType = iter->clientType;
			pBuddyInfo->m_nFace = iter->uFaceID;
			pBuddyInfo->m_nBirthday = iter->nBirthday;
			pBuddyInfo->m_nGender = iter->nGender;

			pBuddyInfo->m_strCustomFaceName = iter->customFace;
			if (!pBuddyInfo->m_strCustomFaceName.IsEmpty())
			{
				pBuddyInfo->m_bUseCustomFace = TRUE;
				CString cachedUserThumb;
				cachedUserThumb.Format(_T("%s%d.png"), m_UserMgr.GetCustomUserThumbFolder().c_str(), iter->uAccountID);
				if (Edoyun::CPath::IsFileExist(cachedUserThumb))
				{
					TCHAR szCachedThumbMd5[64] = { 0 };
					GetFileMd5ValueW(cachedUserThumb, szCachedThumbMd5, ARRAYSIZE(szCachedThumbMd5));

					TCHAR szThumbMd5Unicode[64] = { 0 };
					Utf8ToUnicode(iter->customFace, szThumbMd5Unicode, ARRAYSIZE(szThumbMd5Unicode));
					if (_tcsncmp(szCachedThumbMd5, szThumbMd5Unicode, 32) == 0)
					{
						pBuddyInfo->m_bUseCustomFace = TRUE;
						pBuddyInfo->m_bCustomFaceAvailable = TRUE;
					}
					//头像不存在，下载该头像
					else
					{
						//下载头像
						CFileItemRequest* pFileItem = new CFileItemRequest();
						pFileItem->m_nFileType = FILE_ITEM_DOWNLOAD_USER_THUMB;
						pFileItem->m_uAccountID = iter->uAccountID;
						strcpy_s(pFileItem->m_szUtfFilePath, ARRAYSIZE(pFileItem->m_szUtfFilePath), iter->customFace);
						m_FileTask.AddItem(pFileItem);
					}
				}
				else
				{
					//下载头像
					CFileItemRequest* pFileItem = new CFileItemRequest();
					pFileItem->m_nFileType = FILE_ITEM_DOWNLOAD_USER_THUMB;
					pFileItem->m_uAccountID = iter->uAccountID;
					strcpy_s(pFileItem->m_szUtfFilePath, ARRAYSIZE(pFileItem->m_szUtfFilePath), iter->customFace);
					m_FileTask.AddItem(pFileItem);
				}
			}

			pBuddyInfo->m_nTeamIndex = 0;
			pTeamInfo->m_arrBuddyInfo.push_back(pBuddyInfo);
		}
		else
		{
			pGroupInfo = new CGroupInfo();
			pGroupInfo->m_nGroupId = iter->uAccountID;
			pGroupInfo->m_nGroupCode = iter->uAccountID;
			pGroupInfo->m_strName = szNickName;
			memset(szGroupAccount, 0, sizeof(szGroupAccount));
			_stprintf_s(szGroupAccount, ARRAYSIZE(szGroupAccount), _T("%d"), iter->uAccountID);
			pGroupInfo->m_strAccount = szGroupAccount;

			m_UserMgr.m_GroupList.AddGroup(pGroupInfo);

			GetGroupMembers(iter->uAccountID);
		}

		DEL(iter);
	}


	delete pResult;

	//::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_BUDDY_INFO, 0, (LPARAM)m_UserMgr.m_UserInfo.m_uUserID);
	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_BUDDY_LIST, 0, 0);
	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_RECENT_LIST, 0, 0);
	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_GROUP_LIST, 0, 0);
}

void CEdoyunClient::OnUpdateGroupBasicInfo(UINT message, WPARAM wParam, LPARAM lParam)
{
	CGroupBasicInfoResult* pResult = (CGroupBasicInfoResult*)lParam;
	if (pResult == NULL)
		return;

	CBuddyInfo* pBuddyInfo = NULL;
	TCHAR szAccountName[32] = { 0 };
	TCHAR szNickName[32] = { 0 };
	TCHAR szSignature[256] = { 0 };
	TCHAR szPhoneNumber[32] = { 0 };
	TCHAR szMail[32] = { 0 };

	//TCHAR szGroupAccount[32];

	CGroupInfo* pGroupInfo = m_UserMgr.m_GroupList.GetGroupById(pResult->m_groupid);

	for (auto& iter : pResult->m_listUserBasicInfo)
	{
		Utf8ToUnicode(iter->szAccountName, szAccountName, ARRAYSIZE(szAccountName));
		Utf8ToUnicode(iter->szNickName, szNickName, ARRAYSIZE(szNickName));
		Utf8ToUnicode(iter->szSignature, szSignature, ARRAYSIZE(szSignature));
		Utf8ToUnicode(iter->szPhoneNumber, szPhoneNumber, ARRAYSIZE(szPhoneNumber));
		Utf8ToUnicode(iter->szMail, szMail, ARRAYSIZE(szMail));


		pBuddyInfo = new CBuddyInfo();
		pBuddyInfo->m_uUserID = iter->uAccountID;

		pBuddyInfo->m_strAccount = szAccountName;

		pBuddyInfo->m_strNickName = szNickName;

		pBuddyInfo->m_strSign = szSignature;

		pBuddyInfo->m_strMobile = szPhoneNumber;

		pBuddyInfo->m_strEmail = szMail;
		pBuddyInfo->m_nStatus = iter->nStatus;
		pBuddyInfo->m_nClientType = iter->clientType;
		pBuddyInfo->m_nFace = iter->uFaceID;
		pBuddyInfo->m_nBirthday = iter->nBirthday;
		pBuddyInfo->m_nGender = iter->nGender;

		pBuddyInfo->m_strCustomFaceName = iter->customFace;
		if (!pBuddyInfo->m_strCustomFaceName.IsEmpty())
		{
			pBuddyInfo->m_bUseCustomFace = TRUE;
			CString cachedUserThumb;
			cachedUserThumb.Format(_T("%s%d.png"), m_UserMgr.GetCustomUserThumbFolder().c_str(), iter->uAccountID);
			if (Edoyun::CPath::IsFileExist(cachedUserThumb))
			{
				TCHAR szCachedThumbMd5[64] = { 0 };
				GetFileMd5ValueW(cachedUserThumb, szCachedThumbMd5, ARRAYSIZE(szCachedThumbMd5));

				TCHAR szThumbMd5Unicode[64] = { 0 };
				Utf8ToUnicode(iter->customFace, szThumbMd5Unicode, ARRAYSIZE(szThumbMd5Unicode));
				if (_tcsncmp(szCachedThumbMd5, szThumbMd5Unicode, 32) == 0)
				{
					pBuddyInfo->m_bUseCustomFace = TRUE;
					pBuddyInfo->m_bCustomFaceAvailable = TRUE;
				}
				//头像不存在，下载该头像
				else
				{
					//下载头像
					CFileItemRequest* pFileItem = new CFileItemRequest();
					pFileItem->m_nFileType = FILE_ITEM_DOWNLOAD_USER_THUMB;
					pFileItem->m_uAccountID = iter->uAccountID;
					strcpy_s(pFileItem->m_szUtfFilePath, ARRAYSIZE(pFileItem->m_szUtfFilePath), iter->customFace);
					m_FileTask.AddItem(pFileItem);
				}
			}
			else
			{
				//下载头像
				CFileItemRequest* pFileItem = new CFileItemRequest();
				pFileItem->m_nFileType = FILE_ITEM_DOWNLOAD_USER_THUMB;
				pFileItem->m_uAccountID = iter->uAccountID;
				strcpy_s(pFileItem->m_szUtfFilePath, ARRAYSIZE(pFileItem->m_szUtfFilePath), iter->customFace);
				m_FileTask.AddItem(pFileItem);
			}
		}

		pBuddyInfo->m_nTeamIndex = 0;

		if (pGroupInfo != NULL)
		{
			pGroupInfo->AddMember(pBuddyInfo);
		}


		DEL(iter);
	}

	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_GROUP_INFO, pResult->m_groupid, 0);

	delete pResult;

	//::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_BUDDY_INFO, 0, (LPARAM)m_UserMgr.m_UserInfo.m_uUserID);
	//::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_BUDDY_LIST, 0, 0);
	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_RECENT_LIST, 0, 0);
	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_GROUP_LIST, 0, 0);
}

void CEdoyunClient::OnModifyInfoResult(UINT message, WPARAM wParam, LPARAM lParam)
{
	CUpdateLogonUserInfoResult* pResult = (CUpdateLogonUserInfoResult*)lParam;
	if (pResult == NULL)
		delete pResult;

	TCHAR szNickName[64] = { 0 };
	Utf8ToUnicode(pResult->m_szNickName, szNickName, ARRAYSIZE(szNickName));
	m_UserMgr.m_UserInfo.m_strNickName = szNickName;
	m_UserMgr.m_UserInfo.m_nFace = pResult->m_uFaceID;
	m_UserMgr.m_UserInfo.m_nGender = pResult->m_uGender;
	m_UserMgr.m_UserInfo.m_nBirthday = pResult->m_nBirthday;

	TCHAR szSignature[512] = { 0 };
	Utf8ToUnicode(pResult->m_szSignature, szSignature, ARRAYSIZE(szSignature));
	m_UserMgr.m_UserInfo.m_strSign = szSignature;

	TCHAR szAddress[512] = { 0 };
	Utf8ToUnicode(pResult->m_szAddress, szAddress, ARRAYSIZE(szAddress));
	m_UserMgr.m_UserInfo.m_strAddress = szAddress;

	TCHAR szPhoneNumber[64] = { 0 };
	Utf8ToUnicode(pResult->m_szPhone, szPhoneNumber, ARRAYSIZE(szPhoneNumber));
	m_UserMgr.m_UserInfo.m_strMobile = szPhoneNumber;

	TCHAR szMail[512] = { 0 };
	Utf8ToUnicode(pResult->m_szMail, szMail, ARRAYSIZE(szMail));
	m_UserMgr.m_UserInfo.m_strEmail = szMail;

	m_UserMgr.m_UserInfo.m_strCustomFaceName = pResult->m_szCustomFace;
	if (!m_UserMgr.m_UserInfo.m_strCustomFaceName.IsEmpty())
	{
		m_UserMgr.m_UserInfo.m_bUseCustomFace = TRUE;
		CString cachedUserThumb;
		cachedUserThumb.Format(_T("%s%d.png"), m_UserMgr.GetCustomUserThumbFolder().c_str(), m_UserMgr.m_UserInfo.m_uUserID);
		if (Edoyun::CPath::IsFileExist(cachedUserThumb))
		{
			TCHAR szCachedThumbMd5[64] = { 0 };
			GetFileMd5ValueW(cachedUserThumb, szCachedThumbMd5, ARRAYSIZE(szCachedThumbMd5));

			TCHAR szThumbMd5Unicode[64] = { 0 };
			Utf8ToUnicode(pResult->m_szCustomFace, szThumbMd5Unicode, ARRAYSIZE(szThumbMd5Unicode));
			if (_tcsncmp(szCachedThumbMd5, szThumbMd5Unicode, 32) == 0)
			{
				m_UserMgr.m_UserInfo.m_bUseCustomFace = TRUE;
				m_UserMgr.m_UserInfo.m_bCustomFaceAvailable = TRUE;
			}
			//头像不存在，下载该头像
			else
			{
				CFileItemRequest* pFileItem = new CFileItemRequest();
				pFileItem->m_nFileType = FILE_ITEM_DOWNLOAD_USER_THUMB;
				pFileItem->m_uAccountID = m_UserMgr.m_UserInfo.m_uUserID;
				strcpy_s(pFileItem->m_szUtfFilePath, ARRAYSIZE(pFileItem->m_szUtfFilePath), pResult->m_szCustomFace);
				m_FileTask.AddItem(pFileItem);
			}

		}
		//下载头像
		else
		{
			CFileItemRequest* pFileItem = new CFileItemRequest();
			pFileItem->m_nFileType = FILE_ITEM_DOWNLOAD_USER_THUMB;
			pFileItem->m_uAccountID = m_UserMgr.m_UserInfo.m_uUserID;
			strcpy_s(pFileItem->m_szUtfFilePath, ARRAYSIZE(pFileItem->m_szUtfFilePath), pResult->m_szCustomFace);
			m_FileTask.AddItem(pFileItem);
		}

	}

	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_BUDDY_INFO, 0, (LPARAM)m_UserMgr.m_UserInfo.m_uUserID);
	//::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_GROUP_INFO, (WPARAM)uAccountID, 0);

	delete pResult;
}

void CEdoyunClient::OnRecvUserStatusChangeData(UINT message, WPARAM wParam, LPARAM lParam)
{
	CFriendStatus* pFriendStatus = (CFriendStatus*)lParam;
	if (pFriendStatus == NULL)
		return;

	//m_RecvMsgTask.AddMsgData(pFriendStatus);
}

void CEdoyunClient::OnRecvAddFriendRequest(UINT message, WPARAM wParam, LPARAM lParam)
{
	//TODO: 这种加好友的逻辑太烂，需要优化。
	COperateFriendResult* pOperateFriendResult = (COperateFriendResult*)lParam;
	if (pOperateFriendResult == NULL)
		return;

	BOOL bAlreadyExist = FALSE;
	std::map<UINT, UINT>::iterator iter = m_mapAddFriendCache.begin();
	for (; iter != m_mapAddFriendCache.end(); ++iter)
	{
		if (pOperateFriendResult->m_uCmd == Apply && pOperateFriendResult->m_uCmd == iter->second && iter->first == pOperateFriendResult->m_uAccountID)
		{
			bAlreadyExist = TRUE;
			break;
		}
		//else if(pOperateFriendResult->m_uCmd==Agree && pOperateFriendResult->m_uCmd==iter->second && iter->first==pOperateFriendResult->m_uAccountID)
		//{
		//	bAlreadyExist = TRUE;
		//	//马上要更新好友列表了，先将m_bBuddyListAvailable设置为FALSE，以便缓存新加的好友在线通知
		//	StartGetUserInfoTask(USER_INFO_TYPE_SELF);
		//	StartGetUserInfoTask(USER_INFO_TYPE_FRIENDS);
		//	break;
		//}
		else
		{
			//马上要更新好友列表了，先将m_bBuddyListAvailable设置为FALSE，以便缓存新加的好友在线通知
			m_bBuddyListAvailable = FALSE;
			m_mapAddFriendCache.erase(iter);
			break;
		}

	}
	if (bAlreadyExist)
		delete pOperateFriendResult;
	else
	{
		::PostMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_RECVADDFRIENDREQUSET, 0, (LPARAM)pOperateFriendResult);
	}

}

void CEdoyunClient::OnUserStatusChange(UINT message, WPARAM wParam, LPARAM lParam)
{
	CFriendStatus* pFriendStatus = (CFriendStatus*)lParam;
	if (pFriendStatus == NULL)
		return;

	UINT uAccountID = pFriendStatus->m_uAccountID;
	long nFlag = pFriendStatus->m_nStatus;
	//long nStatus = ParseBuddyStatus(nFlag);



	//AtlTrace(_T("AccountID=%u, Status=%d\n"), uAccountID, nStatus);
	//如果用户信息暂时不可用，先缓存起来
	//if(!m_bBuddyListAvailable || !m_bGroupMemberInfoAvailable)
	//{
	//	std::map<UINT, long>::iterator iter = m_mapUserStatusCache.find(uAccountID);
	//	if(iter != m_mapUserStatusCache.end())
	//		iter->second = nStatus;
	//	else
	//		m_mapUserStatusCache.insert(std::make_pair(uAccountID, nStatus));

	//	return;
	//}

	if (pFriendStatus->m_type == 1 || pFriendStatus->m_type == 2)
	{
		SetBuddyStatus(uAccountID, nFlag);

		//自己
		//if(uAccountID == m_UserMgr.m_UserInfo.m_uUserID)
		//{
		//	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_SELF_STATUS_CHANGE, (WPARAM)uAccountID, (LPARAM)nStatus);
		//}
		//else
		{
			::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_STATUS_CHANGE_MSG, 0, (LPARAM)uAccountID);
			::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_RECENT_LIST, 0, 0);
			::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_CHATDLG_USERINFO, (WPARAM)uAccountID, 0);
			::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_GROUP_INFO, (WPARAM)uAccountID, 0);
		}
	}
	else if (pFriendStatus->m_type == 3)
	{
		//好友信息有更新，重新拉下好友列表(TODO: 这操作太耗流量，后期优化成只拉更新的好友信息)
		GetFriendList();
	}

	delete pFriendStatus;
}

void CEdoyunClient::OnSendConfirmMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	CUploadFileResult* pUploadFileResult = (CUploadFileResult*)lParam;
	if (pUploadFileResult == NULL)
		return;

	if (wParam != 0)
	{
		//图片上传失败
		delete pUploadFileResult;
		return;
	}

	//上传图片结果
	if (pUploadFileResult->m_nFileType == FILE_ITEM_UPLOAD_CHAT_IMAGE)
	{
		time_t nTime = time(NULL);
		TCHAR szMd5[64] = { 0 };
		AnsiToUnicode(pUploadFileResult->m_szMd5, szMd5, ARRAYSIZE(szMd5));
		CString strImageName;
		strImageName.Format(_T("%s.%s"), szMd5, Edoyun::CPath::GetExtension(pUploadFileResult->m_szLocalName).c_str());
		long nWidth = 0;
		long nHeight = 0;
		GetImageWidthAndHeight(pUploadFileResult->m_szLocalName, nWidth, nHeight);
		char szUtf8FileName[MAX_PATH] = { 0 };
		UnicodeToUtf8(strImageName, szUtf8FileName, ARRAYSIZE(szUtf8FileName));
		CStringA strImageAcquireMsg;
		//if (pUploadFileResult->m_bSuccessful)
		//    strImageAcquireMsg.Format("{\"msgType\":2,\"time\":%llu,\"clientType\":1,\"content\":[{\"pic\":[\"%s\",\"%s\",%u,%d,%d]}]}", nTime, szUtf8FileName, pUploadFileResult->m_szRemoteName, pUploadFileResult->m_dwFileSize, nWidth, nHeight);
		//else
		//    strImageAcquireMsg.Format("{\"msgType\":2,\"time\":%llu,\"clientType\":1,\"content\":[{\"pic\":[\"%s\",\"\",%u,%d,%d]}]}", nTime, szUtf8FileName, pUploadFileResult->m_dwFileSize, nWidth, nHeight);

		if (pUploadFileResult->m_bSuccessful)
			strImageAcquireMsg.Format("{\"msgType\":2,\"time\":%llu,\"clientType\":1,\"content\":[{\"pic\":[\"%s\",\"%s\",%u,%d,%d]}]}", nTime, szUtf8FileName, pUploadFileResult->m_szRemoteName, pUploadFileResult->m_dwFileSize, nWidth, nHeight);
		else
			strImageAcquireMsg.Format("{\"msgType\":2,\"time\":%llu,\"clientType\":1,\"content\":[{\"pic\":[\"%s\",\"\",%u,%d,%d]}]}", nTime, szUtf8FileName, pUploadFileResult->m_dwFileSize, nWidth, nHeight);

		long nBodyLength = strImageAcquireMsg.GetLength() + 1;
		char* pszMsgBody = new char[nBodyLength];
		memset(pszMsgBody, 0, nBodyLength);
		strcpy_s(pszMsgBody, nBodyLength, strImageAcquireMsg);
		CSendChatConfirmImageMessage* pConfirm = new CSendChatConfirmImageMessage();
		pConfirm->m_hwndChat = pUploadFileResult->m_hwndReflection;
		pConfirm->m_pszConfirmBody = pszMsgBody;
		pConfirm->m_uConfirmBodySize = nBodyLength - 1;
		pConfirm->m_uSenderID = pUploadFileResult->m_uSenderID;
		pConfirm->m_setTargetIDs = pUploadFileResult->m_setTargetIDs;
		if (pConfirm->m_setTargetIDs.size() > 1)
			pConfirm->m_nType = CHAT_CONFIRM_TYPE_MULTI;
		else
			pConfirm->m_nType = CHAT_CONFIRM_TYPE_SINGLE;

		//SendBuddyMsg(UINT nFromUin, const tstring& strFromNickName, UINT nToUin, const tstring& strToNickName, time_t nTime, const tstring& strChatMsg, HWND hwndFrom/* = NULL*/)

		m_SendMsgThread.AddItem(pConfirm);
	}

	delete pUploadFileResult;
}

void CEdoyunClient::OnUpdateChatMsgID(UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT uAccountID = (UINT)wParam;
	UINT uMsgID = (UINT)lParam;
	m_UserMgr.SetMsgID(uAccountID, uMsgID);
}

void CEdoyunClient::OnFindFriend(UINT message, WPARAM wParam, LPARAM lParam)
{
	::PostMessage(m_hwndFindFriend, FMG_MSG_FINDFREIND, 0, lParam);
}

void CEdoyunClient::OnBuddyCustomFaceAvailable(UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT uAccountID = (UINT)wParam;
	if (uAccountID == 0)
		return;

	if (uAccountID == m_UserMgr.m_UserInfo.m_uUserID)
		m_UserMgr.m_UserInfo.m_bCustomFaceAvailable = TRUE;
	else
	{
		CBuddyInfo* pBuddyInfo = m_UserMgr.m_BuddyList.GetBuddy(uAccountID);
		if (pBuddyInfo == NULL)
			return;

		pBuddyInfo->m_bCustomFaceAvailable = TRUE;
	}

	::PostMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_BUDDY_INFO, 0, (LPARAM)uAccountID);
	::PostMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_RECENT_LIST, 0, 0);
}

void CEdoyunClient::OnModifyPasswordResult(UINT message, WPARAM wParam, LPARAM lParam)
{
	::PostMessage(m_hwndModifyPassword, message, wParam, lParam);
}

void CEdoyunClient::OnCreateNewGroupResult(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (m_hwndCreateNewGroup)
		::PostMessage(m_hwndCreateNewGroup, message, wParam, lParam);
}

void CEdoyunClient::OnDeleteFriendResult(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (wParam != DELETE_FRIEND_SUCCESS)
		return;

	UINT uAccountID = (UINT)lParam;
	if (!IsGroupTarget(uAccountID))
	{
		m_UserMgr.DeleteFriend(uAccountID);
		m_UserMgr.m_MsgList.DelMsgSender(FMG_MSG_TYPE_BUDDY, uAccountID);
	}
	else
	{
		m_UserMgr.ExitGroup(uAccountID);
		m_UserMgr.m_MsgList.DelMsgSender(FMG_MSG_TYPE_GROUP, uAccountID);
		if (m_UserMgr.m_RecentList.DeleteRecentItem(uAccountID))
			::PostMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_RECENT_LIST, 0, 0);
	}

	if (m_UserMgr.m_MsgList.GetMsgSenderCount() <= 0)
		::PostMessage(m_UserMgr.m_hCallBackWnd, WM_CANCEL_FLASH, 0, 0);

	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_DELETEFRIEND, DELETE_FRIEND_SUCCESS, lParam);
}

void CEdoyunClient::OnUpdateBuddyList(UINT message, WPARAM wParam, LPARAM lParam)
{
	::SendMessage(m_UserMgr.m_hCallBackWnd, message, wParam, lParam);
}

void CEdoyunClient::OnUpdateGroupList(UINT message, WPARAM wParam, LPARAM lParam)
{
	//BOOL bSuccess = FALSE;
	//CGroupListResult* lpGroupListResult = (CGroupListResult*)lParam;
	//if (lpGroupListResult != NULL)
	//{
	//	for (int i = 0; i < (int)lpGroupListResult->m_arrGroupInfo.size(); i++)
	//	{
	//		CGroupInfo* lpGroupInfo = lpGroupListResult->m_arrGroupInfo[i];
	//		if (lpGroupInfo != NULL)
	//			m_UserMgr.m_GroupList.AddGroup(lpGroupInfo);
	//	}
	//	lpGroupListResult->m_arrGroupInfo.clear();
	//	delete lpGroupListResult;
	//	bSuccess = TRUE;
	//}
	//::SendMessage(m_UserMgr.m_hCallBackWnd, message, NULL, bSuccess);
}

void CEdoyunClient::OnUpdateRecentList(UINT message, WPARAM wParam, LPARAM lParam)
{
	::SendMessage(m_UserMgr.m_hCallBackWnd, message, 0, 0);
}

void CEdoyunClient::OnBuddyMsg(UINT message, WPARAM wParam, LPARAM lParam)
{
	CBuddyMessage* lpBuddyMsg = (CBuddyMessage*)lParam;
	if (NULL == lpBuddyMsg)
		return;

	UINT nUTalkUin = lpBuddyMsg->m_nFromUin;
	UINT nMsgId = lpBuddyMsg->m_nMsgId;

	m_UserMgr.m_MsgList.AddMsg(FMG_MSG_TYPE_BUDDY, lpBuddyMsg->m_nFromUin, 0, (void*)lpBuddyMsg);

	//更新最近会话列表以在最近会话列表中显示消息的个数
	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_RECENT_LIST, 0, 0);
	::SendMessage(m_UserMgr.m_hCallBackWnd, message, nUTalkUin, lParam);
}

void CEdoyunClient::OnGroupMsg(UINT message, WPARAM wParam, LPARAM lParam)
{
	CBuddyMessage* lpGroupMsg = (CBuddyMessage*)lParam;
	if (NULL == lpGroupMsg)
		return;

	UINT nGroupCode = lpGroupMsg->m_nToUin;
	UINT nMsgId = lpGroupMsg->m_nMsgId;

	m_UserMgr.m_MsgList.AddMsg(FMG_MSG_TYPE_GROUP,
		lpGroupMsg->m_nToUin, lpGroupMsg->m_nToUin, (void*)lpGroupMsg);

	//更新最近会话列表以在最近会话列表中显示消息的个数
	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_RECENT_LIST, 0, 0);
	::SendMessage(m_UserMgr.m_hCallBackWnd, message, nGroupCode, nMsgId);
}

void CEdoyunClient::OnSessMsg(UINT message, WPARAM wParam, LPARAM lParam)
{
	CSessMessage* lpSessMsg = (CSessMessage*)lParam;
	if (NULL == lpSessMsg)
		return;

	UINT nUTalkUin = lpSessMsg->m_nFromUin;
	UINT nMsgId = lpSessMsg->m_nMsgId;
	UINT nGroupCode = 0;

	CGroupInfo* lpGroupInfo = m_UserMgr.m_GroupList.GetGroupById(lpSessMsg->m_nGroupId);
	if (lpGroupInfo != NULL)
	{
		nGroupCode = lpGroupInfo->m_nGroupCode;
		CBuddyInfo* lpBuddyInfo = lpGroupInfo->GetMemberByUin(lpSessMsg->m_nFromUin);
		if (NULL == lpBuddyInfo)
		{
			lpBuddyInfo = new CBuddyInfo;
			if (lpBuddyInfo != NULL)
			{
				lpBuddyInfo->Reset();
				lpBuddyInfo->m_uUserID = lpSessMsg->m_nFromUin;
				//lpBuddyInfo->m_nUTalkNum = lpSessMsg->m_nUTalkNum;
				lpGroupInfo->m_arrMember.push_back(lpBuddyInfo);
			}
			UpdateGroupMemberInfo(nGroupCode, lpSessMsg->m_nFromUin);
		}
	}

	m_UserMgr.m_MsgList.AddMsg(FMG_MSG_TYPE_SESS,
		lpSessMsg->m_nFromUin, nGroupCode, (void*)lpSessMsg);

	::SendMessage(m_UserMgr.m_hCallBackWnd, message, nUTalkUin, nMsgId);
}

void CEdoyunClient::OnSysGroupMsg(UINT message, WPARAM wParam, LPARAM lParam)
{
	//CSysGroupMessage* lpSysGroupMsg = (CSysGroupMessage*)lParam;
	//if (NULL == lpSysGroupMsg)
	//	return;

	//UINT nGroupCode = lpSysGroupMsg->m_nGroupCode;

	//m_UserMgr.m_MsgList.AddMsg(FMG_MSG_TYPE_SYSGROUP, lpSysGroupMsg->m_nGroupCode, 
	//	lpSysGroupMsg->m_nGroupCode, (void*)lpSysGroupMsg);

	//::SendMessage(m_UserMgr.m_hCallBackWnd, message, 0, nGroupCode);
}

void CEdoyunClient::OnStatusChangeMsg(UINT message, WPARAM wParam, LPARAM lParam)
{
	//UINT nUTalkUin = 0;
	//CStatusChangeMessage* lpStatusChangeMsg = (CStatusChangeMessage*)lParam;
	//if (NULL == lpStatusChangeMsg)
	//	return;
	//
	//nUTalkUin = lpStatusChangeMsg->m_nUTalkUin;
	//CBuddyInfo* lpBuddyInfo = m_UserMgr.m_BuddyList.GetBuddy(nUTalkUin);
	//if (lpBuddyInfo != NULL)
	//{
	//	lpBuddyInfo->m_nStatus = lpStatusChangeMsg->m_nStatus;
	//	lpBuddyInfo->m_nClientType = lpStatusChangeMsg->m_nClientType;
	//	CBuddyTeamInfo* lpBuddyTeamInfo = m_UserMgr.m_BuddyList.GetBuddyTeam(lpBuddyInfo->m_nTeamIndex);
	//	if (lpBuddyTeamInfo != NULL)
	//		lpBuddyTeamInfo->Sort();
	//}
	//delete lpStatusChangeMsg;
	//::SendMessage(m_UserMgr.m_hCallBackWnd, message, 0, nUTalkUin);
}

void CEdoyunClient::OnKickMsg(UINT message, WPARAM wParam, LPARAM lParam)
{
	//CKickMessage* lpKickMsg = (CKickMessage*)lParam;
	//if (NULL == lpKickMsg)
	//	return;
	//
	//delete lpKickMsg;
	//m_UserMgr.m_UserInfo.m_nStatus = STATUS_OFFLINE;
	//m_ThreadPool.RemoveAllTask();
	//::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_KICK_MSG, 0, 0);
}

void CEdoyunClient::OnUpdateBuddyNumber(UINT message, WPARAM wParam, LPARAM lParam)
{
	//UINT nUTalkUin = 0;
	//CGetUTalkNumResult* lpGetUTalkNumResult = (CGetUTalkNumResult*)lParam;
	//if (lpGetUTalkNumResult != NULL)
	//{
	//	nUTalkUin = lpGetUTalkNumResult->m_nUTalkUin;
	//	CBuddyInfo* lpBuddyInfo = m_UserMgr.m_BuddyList.GetBuddy(nUTalkUin);
	//	if (lpBuddyInfo != NULL)
	//		lpBuddyInfo->SetUTalkNum(lpGetUTalkNumResult);
	//	delete lpGetUTalkNumResult;
	//}
	//::SendMessage(m_UserMgr.m_hCallBackWnd, message, 0, nUTalkUin);
}

void CEdoyunClient::OnUpdateGMemberNumber(UINT message, WPARAM wParam, LPARAM lParam)
{
	//UINT nGroupCode = (UINT)wParam;
	//UINT nUTalkUin = 0;
	//CGetUTalkNumResult* lpGetUTalkNumResult = (CGetUTalkNumResult*)lParam;
	//if (nGroupCode != 0 && lpGetUTalkNumResult != NULL)
	//{
	//	nUTalkUin = lpGetUTalkNumResult->m_nUTalkUin;
	//	CBuddyInfo* lpBuddyInfo = m_UserMgr.m_GroupList.GetGroupMemberByCode(nGroupCode, nUTalkUin);
	//	if (lpBuddyInfo != NULL)
	//		lpBuddyInfo->SetUTalkNum(lpGetUTalkNumResult);
	//	delete lpGetUTalkNumResult;
	//}
	//::SendMessage(m_UserMgr.m_hCallBackWnd, message, nGroupCode, nUTalkUin);
}

void CEdoyunClient::OnUpdateGroupNumber(UINT message, WPARAM wParam, LPARAM lParam)
{
	//UINT nGroupCode = (UINT)wParam;
	//CGetUTalkNumResult* lpGetUTalkNumResult = (CGetUTalkNumResult*)lParam;
	//if (nGroupCode != 0 && lpGetUTalkNumResult != NULL)
	//{
	//	CGroupInfo* lpGroupInfo = m_UserMgr.m_GroupList.GetGroupByCode(nGroupCode);
	//	if (lpGroupInfo != NULL)
	//		lpGroupInfo->SetGroupNumber(lpGetUTalkNumResult);
	//	delete lpGetUTalkNumResult;
	//}
	//::SendMessage(m_UserMgr.m_hCallBackWnd, message, 0, nGroupCode);
}

void CEdoyunClient::OnUpdateBuddySign(UINT message, WPARAM wParam, LPARAM lParam)
{
	//UINT nUTalkUin = 0;
	//CGetSignResult* lpGetSignResult = (CGetSignResult*)lParam;
	//if (lpGetSignResult != NULL)
	//{
	//	nUTalkUin = lpGetSignResult->m_nUTalkUin;
	//	if (m_UserMgr.m_UserInfo.m_nUTalkUin == nUTalkUin)		// 更新用户个性签名
	//	{
	//		m_UserMgr.m_UserInfo.SetUTalkSign(lpGetSignResult);
	//	}
	//	else											// 更新好友个性签名
	//	{
	//		CBuddyInfo* lpBuddyInfo = m_UserMgr.m_BuddyList.GetBuddy(nUTalkUin);
	//		if (lpBuddyInfo != NULL)
	//			lpBuddyInfo->SetUTalkSign(lpGetSignResult);
	//	}
	//	delete lpGetSignResult;
	//}
	//::SendMessage(m_UserMgr.m_hCallBackWnd, message, NULL, nUTalkUin);
}

void CEdoyunClient::OnUpdateGMemberSign(UINT message, WPARAM wParam, LPARAM lParam)
{
	//UINT nGroupCode = (UINT)wParam;
	//UINT nUTalkUin = 0;
	//CGetSignResult* lpGetSignResult = (CGetSignResult*)lParam;

	//if (nGroupCode != 0 && lpGetSignResult != NULL)
	//{
	//	nUTalkUin = lpGetSignResult->m_nUTalkUin;
	//	CBuddyInfo* lpBuddyInfo = m_UserMgr.m_GroupList.GetGroupMemberByCode(nGroupCode, nUTalkUin);
	//	if (lpBuddyInfo != NULL)
	//		lpBuddyInfo->SetUTalkSign(lpGetSignResult);
	//	delete lpGetSignResult;
	//}
	//::SendMessage(m_UserMgr.m_hCallBackWnd, message, nGroupCode, nUTalkUin);
}

void CEdoyunClient::OnUpdateBuddyInfo(UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT uUserID = m_UserMgr.m_UserInfo.m_uUserID;

	CBuddyInfo* lpBuddyInfo = m_UserMgr.m_BuddyList.GetBuddy(uUserID);
	if (lpBuddyInfo != NULL)
		lpBuddyInfo->SetBuddyInfo(lpBuddyInfo);

	::SendMessage(m_UserMgr.m_hCallBackWnd, message, NULL, 0);
}

void CEdoyunClient::OnUpdateGMemberInfo(UINT message, WPARAM wParam, LPARAM lParam)
{
	//UINT nGroupCode = (UINT)wParam;
	//UINT nUTalkUin = 0;
	//CBuddyInfoResult* lpBuddyInfoResult = (CBuddyInfoResult*)lParam;

	//if (nGroupCode != 0 && lpBuddyInfoResult != NULL)
	//{
	//	//nUTalkUin = lpBuddyInfoResult->m_nUTalkUin;
	//	CBuddyInfo* lpBuddyInfo = m_UserMgr.m_GroupList.GetGroupMemberByCode(nGroupCode, nUTalkUin);
	//	if (lpBuddyInfo != NULL)
	//		lpBuddyInfo->SetBuddyInfo(lpBuddyInfoResult);
	//	delete lpBuddyInfoResult;
	//}
	//::SendMessage(m_UserMgr.m_hCallBackWnd, message, nGroupCode, nUTalkUin);
}

void CEdoyunClient::OnUpdateGroupInfo(UINT message, WPARAM wParam, LPARAM lParam)
{
	//UINT nGroupCode = 0;
	//CGroupInfoResult* lpGroupInfoResult = (CGroupInfoResult*)lParam;
	//if (lpGroupInfoResult != NULL)
	//{
	//	nGroupCode = lpGroupInfoResult->m_nGroupCode;
	//	CGroupInfo* lpGroupInfo = m_UserMgr.m_GroupList.GetGroupByCode(nGroupCode);
	//	if (lpGroupInfo != NULL)
	//		lpGroupInfo->SetGroupInfo(lpGroupInfoResult);
	//	delete lpGroupInfoResult;
	//}

	UINT nGroupCode = (UINT)lParam;
	::SendMessage(m_UserMgr.m_hCallBackWnd, message, 0, nGroupCode);
}

void CEdoyunClient::OnChangeStatusResult(UINT message, WPARAM wParam, LPARAM lParam)
{
	BOOL bSuccess = (BOOL)wParam;
	long nNewStatus = lParam;

	if (bSuccess)
		m_UserMgr.m_UserInfo.m_nStatus = nNewStatus;

	::SendMessage(m_UserMgr.m_hCallBackWnd, message, wParam, lParam);
}

void CEdoyunClient::OnTargetInfoChange(UINT message, WPARAM wParam, LPARAM lParam)
{
	CTargetInfoChangeResult* pResult = (CTargetInfoChangeResult*)lParam;
	if (pResult == NULL)
		return;

	UINT uAccountID = pResult->m_uAccountID;
	delete pResult;

	if (uAccountID == 0 || !IsGroupTarget(uAccountID))
		return;

	//群组账号只有基本信息和ID列表，没有扩展信息
	//CUserBasicInfoRequest* pBasicInfoRequest = new CUserBasicInfoRequest();
	//pBasicInfoRequest->m_setAccountID.insert(uAccountID);
	//m_SendMsgTask.AddItem(pBasicInfoRequest);

	m_bGroupMemberInfoAvailable = FALSE;
	CLoginUserFriendsIDRequest* pFriendsIDRequest = new CLoginUserFriendsIDRequest();
	pFriendsIDRequest->m_uAccountID = uAccountID;
	//m_SendMsgTask.AddItem(pFriendsIDRequest);
}

void CEdoyunClient::OnInternal_GetBuddyData(UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT nUTalkUin = (UINT)wParam;
	RMT_BUDDY_DATA* lpBuddyData = (RMT_BUDDY_DATA*)lParam;
	if (NULL == lpBuddyData)
		return;

	CBuddyInfo* lpBuddyInfo = m_UserMgr.m_BuddyList.GetBuddy(nUTalkUin);
	if (NULL == lpBuddyInfo)
		return;

	lpBuddyData->nUTalkNum = lpBuddyInfo->m_uUserID;

	int nMaxCnt = sizeof(lpBuddyData->szNickName) / sizeof(TCHAR);
	if (!lpBuddyInfo->m_strMarkName.empty())
		_tcsncpy(lpBuddyData->szNickName, lpBuddyInfo->m_strMarkName.c_str(), nMaxCnt);
	else
		_tcsncpy(lpBuddyData->szNickName, lpBuddyInfo->m_strNickName.c_str(), nMaxCnt);
	lpBuddyData->szNickName[nMaxCnt - 1] = _T('\0');
}

void CEdoyunClient::OnInternal_GetGroupData(UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT nGroupCode = (UINT)wParam;
	RMT_GROUP_DATA* lpGroupData = (RMT_GROUP_DATA*)lParam;
	if (NULL == lpGroupData)
		return;

	CGroupInfo* lpGroupInfo = m_UserMgr.m_GroupList.GetGroupByCode(nGroupCode);
	if (NULL == lpGroupInfo)
		return;

	lpGroupData->bHasGroupInfo = lpGroupInfo->m_bHasGroupInfo;
	lpGroupData->nGroupNum = lpGroupInfo->m_nGroupNumber;
}

void CEdoyunClient::OnInternal_GetGMemberData(UINT message, WPARAM wParam, LPARAM lParam)
{
	RMT_GMEMBER_REQ* lpGMemberReq = (RMT_GMEMBER_REQ*)wParam;
	RMT_BUDDY_DATA* lpGMemberData = (RMT_BUDDY_DATA*)lParam;
	if (NULL == lpGMemberReq || NULL == lpGMemberData)
		return;

	CGroupInfo* lpGroupInfo = m_UserMgr.m_GroupList.GetGroupByCode(lpGMemberReq->nGroupCode);
	if (NULL == lpGroupInfo)
		return;

	CBuddyInfo* lpBuddyInfo = lpGroupInfo->GetMemberByUin(lpGMemberReq->nUTalkUin);
	if (NULL == lpBuddyInfo)
		return;

	lpGMemberData->nUTalkNum = lpBuddyInfo->m_uUserID;

	int nMaxCnt = sizeof(lpGMemberData->szNickName) / sizeof(TCHAR);
	_tcsncpy(lpGMemberData->szNickName, lpBuddyInfo->m_strNickName.c_str(), nMaxCnt);
	lpGMemberData->szNickName[nMaxCnt - 1] = _T('\0');
}

UINT CEdoyunClient::OnInternal_GroupId2Code(UINT message, WPARAM wParam, LPARAM lParam)
{
	CGroupInfo* lpGroupInfo = m_UserMgr.m_GroupList.GetGroupById(lParam);
	return ((lpGroupInfo != NULL) ? lpGroupInfo->m_nGroupCode : 0);
}
// 加载用户设置信息
void CEdoyunClient::LoadUserConfig()
{
	if (m_UserMgr.m_UserInfo.m_strAccount.empty())
		return;

	PCTSTR pszAccount = m_UserMgr.m_UserInfo.m_strAccount.c_str();

	TCHAR szConfigPath[MAX_PATH] = { 0 };
	_stprintf_s(szConfigPath, MAX_PATH, _T("%sUsers\\%s\\%s.cfg"), g_szHomePath, pszAccount, pszAccount);

	m_UserConfig.LoadConfig(szConfigPath);
}

void CEdoyunClient::SaveUserConfig()
{
	if (m_UserMgr.m_UserInfo.m_strAccount.empty())
		return;

	PCTSTR pszAccount = m_UserMgr.m_UserInfo.m_strAccount.c_str();

	TCHAR szConfigPath[MAX_PATH] = { 0 };
	_stprintf_s(szConfigPath, MAX_PATH, _T("%sUsers\\%s\\%s.cfg"), g_szHomePath, pszAccount, pszAccount);

	m_UserConfig.SaveConfig(szConfigPath);
}

BOOL CEdoyunClient::SetBuddyStatus(UINT uAccountID, long nStatus)
{
	//好友上线
	if (uAccountID != m_UserMgr.m_UserInfo.m_uUserID)
	{
		m_UserMgr.SetStatus(uAccountID, nStatus);
		return TRUE;
	}

	return FALSE;
}


long CEdoyunClient::ParseBuddyStatus(long nFlag)
{
	long nOnlineClientType = OFFLINE_CLIENT_BOTH;

	if (nFlag & 0x40)
		nOnlineClientType = ONLINE_CLIENT_PC;
	else if (nFlag & 0x80)
		nOnlineClientType = ONLINE_CLIENT_MOBILE;

	long nStatus = STATUS_OFFLINE;
	if (nOnlineClientType == ONLINE_CLIENT_PC)
	{
		if (nFlag & 0x01)
			nStatus = STATUS_ONLINE;
		else if (nFlag & 0x02)
			nStatus = STATUS_INVISIBLE;
		else if (nFlag & 0x03)
			nStatus = STATUS_BUSY;
		else if (nFlag & 0x04)
			nStatus = STATUS_AWAY;

	}
	else if (nOnlineClientType == ONLINE_CLIENT_MOBILE)
	{
		nStatus = STATUS_MOBILE_ONLINE;
	}

	return nStatus;
}



void CEdoyunClient::GoOnline()
{
	if (m_bNetworkAvailable)
		Login();
}

void CEdoyunClient::GoOffline()
{
	//m_IUProtocol.Disconnect();
	//m_IUProtocol.DisconnectFileServer();

	m_UserMgr.ResetToOfflineStatus();
	//m_HeartbeatTask.Stop();

	//m_mapUserStatusCache.clear();

	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_BUDDY_INFO, 0, (LPARAM)m_UserMgr.m_UserInfo.m_uUserID);
	//::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_BUDDY_LIST, 0, 0);
	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_RECENT_LIST, 0, 0);
	::SendMessage(m_UserMgr.m_hCallBackWnd, FMG_MSG_UPDATE_GROUP_LIST, 0, 0);
}

void CEdoyunClient::CacheBuddyStatus()
{
	CBuddyTeamInfo* pBuddyTeamInfo = NULL;
	CBuddyInfo* pBuddyInfo = NULL;
	size_t nTeamCount = m_UserMgr.m_BuddyList.m_arrBuddyTeamInfo.size();
	size_t nBuddyCount = 0;
	for (size_t i = 0; i < nTeamCount; ++i)
	{
		pBuddyTeamInfo = m_UserMgr.m_BuddyList.m_arrBuddyTeamInfo[i];
		if (pBuddyTeamInfo == NULL)
			continue;

		nBuddyCount = pBuddyTeamInfo->m_arrBuddyInfo.size();

		for (size_t j = 0; j < nBuddyCount; ++j)
		{
			pBuddyInfo = pBuddyTeamInfo->m_arrBuddyInfo[j];
			if (pBuddyInfo == NULL)
				continue;
			if (pBuddyInfo->m_nStatus != STATUS_OFFLINE)
				m_mapUserStatusCache.insert(std::pair<UINT, long>(pBuddyInfo->m_uUserID, pBuddyInfo->m_nStatus));
		}
	}

	//群成员的在线状态也要缓存
	CGroupInfo* pGroupInfo = NULL;
	for (std::vector<CGroupInfo*>::iterator iter = m_UserMgr.m_GroupList.m_arrGroupInfo.begin();
		iter != m_UserMgr.m_GroupList.m_arrGroupInfo.end();
		++iter)
	{
		pGroupInfo = *iter;
		if (pGroupInfo == NULL)
			continue;;

		for (size_t j = 0; j < pGroupInfo->m_arrMember.size(); ++j)
		{
			pBuddyInfo = pGroupInfo->m_arrMember[j];
			if (pBuddyInfo == NULL)
				continue;

			if (pBuddyInfo->m_nStatus != STATUS_OFFLINE)
				m_mapUserStatusCache.insert(std::pair<UINT, long>(pBuddyInfo->m_uUserID, pBuddyInfo->m_nStatus));
		}
	}
}

// 创建代理窗口
BOOL CEdoyunClient::CreateProxyWnd()
{
	WNDCLASSEX wcex;
	LPCTSTR szWindowClass = _T("UTALK_PROXY_WND");
	HWND hWnd;

	DestroyProxyWnd();	// 销毁代理窗口

	HINSTANCE hInstance = ::GetModuleHandle(NULL);

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = ProxyWndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = NULL;
	wcex.hCursor = NULL;
	wcex.hbrBackground = NULL;
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = NULL;

	if (!::RegisterClassEx(&wcex))
		return FALSE;

	hWnd = ::CreateWindow(szWindowClass, NULL, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
	if (NULL == hWnd)
		return FALSE;

	::SetWindowLong(hWnd, GWL_USERDATA, (LONG)this);

	m_RecvMsgThread.SetProxyWnd(hWnd);
	m_UserMgr.m_hProxyWnd = hWnd;

	return TRUE;
}

// 销毁代理窗口
BOOL CEdoyunClient::DestroyProxyWnd()
{
	if (m_UserMgr.m_hProxyWnd != NULL)
	{
		::DestroyWindow(m_UserMgr.m_hProxyWnd);
		m_UserMgr.m_hProxyWnd = NULL;
	}
	return TRUE;
}

LRESULT CALLBACK CEdoyunClient::ProxyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	CEdoyunClient* lpFMGClient = (CEdoyunClient*)::GetWindowLong(hWnd, GWL_USERDATA);
	if (NULL == lpFMGClient)
		return ::DefWindowProc(hWnd, message, wParam, lParam);

	if (message < FMG_MSG_FIRST || message > FMG_MSG_LAST)
		return ::DefWindowProc(hWnd, message, wParam, lParam);

	switch (message)
	{
	case FMG_MSG_HEARTBEAT:
		lpFMGClient->OnHeartbeatResult(message, wParam, lParam);
		break;

	case FMG_MSG_NETWORK_STATUS_CHANGE:
		lpFMGClient->OnNetworkStatusChange(message, wParam, lParam);
		break;

	case FMG_MSG_REGISTER:				// 注册结果
		lpFMGClient->OnRegisterResult(message, wParam, lParam);
		break;
	case FMG_MSG_LOGIN_RESULT:			// 登录返回消息
		lpFMGClient->OnLoginResult(message, wParam, lParam);
		break;
	case FMG_MSG_LOGOUT_RESULT:			// 注销返回消息
	case FMG_MSG_UPDATE_BUDDY_HEADPIC:	// 更新好友头像
		//::MessageBox(NULL, _T("Change headpic"), _T("Change head"), MB_OK);
	case FMG_MSG_UPDATE_GMEMBER_HEADPIC:	// 更新群成员头像
	case FMG_MSG_UPDATE_GROUP_HEADPIC:	// 更新群头像
		::SendMessage(lpFMGClient->m_UserMgr.m_hCallBackWnd, message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_USER_BASIC_INFO:	//收到用户的基本信息
		lpFMGClient->OnUpdateUserBasicInfo(message, wParam, lParam);
		break;

	case FMG_MSG_UPDATE_GROUP_BASIC_INFO:
		lpFMGClient->OnUpdateGroupBasicInfo(message, wParam, lParam);
		break;

	case FMG_MSG_MODIFY_USER_INFO:				//修改个人信息结果
		lpFMGClient->OnModifyInfoResult(message, wParam, lParam);
		break;
	case FMG_MSG_RECV_USER_STATUS_CHANGE_DATA:
		lpFMGClient->OnRecvUserStatusChangeData(message, wParam, lParam);
		break;

	case FMG_MSG_USER_STATUS_CHANGE:
		lpFMGClient->OnUserStatusChange(message, wParam, lParam);
		break;

	case FMG_MSG_UPLOAD_USER_THUMB:
		lpFMGClient->OnSendConfirmMessage(message, wParam, lParam);
		break;

	case FMG_MSG_UPDATE_USER_CHAT_MSG_ID:
		lpFMGClient->OnUpdateChatMsgID(message, wParam, lParam);
		break;
	case FMG_MSG_FINDFREIND:
		lpFMGClient->OnFindFriend(message, wParam, lParam);
		break;

	case FMG_MSG_DELETEFRIEND:
		lpFMGClient->OnDeleteFriendResult(message, wParam, lParam);
		break;

	case FMG_MSG_RECVADDFRIENDREQUSET:
		lpFMGClient->OnRecvAddFriendRequest(message, wParam, lParam);
		break;

	case FMG_MSG_CUSTOMFACE_AVAILABLE:
		lpFMGClient->OnBuddyCustomFaceAvailable(message, wParam, lParam);
		break;

	case FMG_MSG_MODIFY_PASSWORD_RESULT:
		lpFMGClient->OnModifyPasswordResult(message, wParam, lParam);
		break;

	case FMG_MSG_CREATE_NEW_GROUP_RESULT:
		lpFMGClient->OnCreateNewGroupResult(message, wParam, lParam);
		break;

	case FMG_MSG_UPDATE_BUDDY_LIST:				//更新好友列表
		lpFMGClient->OnUpdateBuddyList(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_GROUP_LIST:		// 更新群列表消息
		lpFMGClient->OnUpdateGroupList(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_RECENT_LIST:		// 更新最近联系人列表消息
		lpFMGClient->OnUpdateRecentList(message, wParam, lParam);
		break;
	case FMG_MSG_BUDDY_MSG:				// 好友消息
		lpFMGClient->OnBuddyMsg(message, wParam, lParam);
		break;
	case FMG_MSG_GROUP_MSG:				// 群消息
		lpFMGClient->OnGroupMsg(message, wParam, lParam);
		break;
	case FMG_MSG_SESS_MSG:				// 临时会话消息
		lpFMGClient->OnSessMsg(message, wParam, lParam);
		break;
	case FMG_MSG_STATUS_CHANGE_MSG:		// 好友状态改变消息
		lpFMGClient->OnStatusChangeMsg(message, wParam, lParam);
		break;
	case FMG_MSG_KICK_MSG:				// 被踢下线消息
		lpFMGClient->OnKickMsg(message, wParam, lParam);
		break;
	case FMG_MSG_SYS_GROUP_MSG:			// 群系统消息
		lpFMGClient->OnSysGroupMsg(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_BUDDY_NUMBER:	// 更新好友号码
		lpFMGClient->OnUpdateBuddyNumber(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_GMEMBER_NUMBER:	// 更新群成员号码
		lpFMGClient->OnUpdateGMemberNumber(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_GROUP_NUMBER:	// 更新群号码
		lpFMGClient->OnUpdateGroupNumber(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_BUDDY_SIGN:		// 更新好友个性签名
		lpFMGClient->OnUpdateBuddySign(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_GMEMBER_SIGN:	// 更新群成员个性签名
		lpFMGClient->OnUpdateGMemberSign(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_BUDDY_INFO:		// 更新用户信息
		lpFMGClient->OnUpdateBuddyInfo(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_GMEMBER_INFO:	// 更新群成员信息
		lpFMGClient->OnUpdateGMemberInfo(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_GROUP_INFO:		// 更新群信息
		lpFMGClient->OnUpdateGroupInfo(message, wParam, lParam);
		break;
	case FMG_MSG_UPDATE_C2CMSGSIG:		// 更新临时会话信令
		//lpFMGClient->OnUpdateC2CMsgSig(message, wParam, lParam);
		break;
	case FMG_MSG_CHANGE_STATUS_RESULT:	// 改变在线状态返回消息
		lpFMGClient->OnChangeStatusResult(message, wParam, lParam);
		break;
	case FMG_MSG_TARGET_INFO_CHANGE:		//有用户信息发生改变：
		lpFMGClient->OnTargetInfoChange(message, wParam, lParam);
		break;

	case FMG_MSG_INTERNAL_GETBUDDYDATA:
		lpFMGClient->OnInternal_GetBuddyData(message, wParam, lParam);
		break;
	case FMG_MSG_INTERNAL_GETGROUPDATA:
		lpFMGClient->OnInternal_GetGroupData(message, wParam, lParam);
		break;
	case FMG_MSG_INTERNAL_GETGMEMBERDATA:
		lpFMGClient->OnInternal_GetGMemberData(message, wParam, lParam);
		break;
	case FMG_MSG_INTERNAL_GROUPID2CODE:
		return lpFMGClient->OnInternal_GroupId2Code(message, wParam, lParam);
		break;

	default:
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
