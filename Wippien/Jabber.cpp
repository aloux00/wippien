// User.cpp: implementation of the CUser class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Jabber.h"
#include "Settings.h"
#include "Buffer.h"
#include "ComBSTR2.h"
#include "MainDlg.h"
#include "Ethernet.h"
#include "ContactAuthDlg.h"
#include "SDKMessageLink.h"
#include "ChatRoom.h"
#include "MsgWin.h"

#ifndef MIN
#define  MIN(a, b)   ((a)<(b)?(a):(b)) 
#endif
#ifndef MAX
#define  MAX(a, b)   ((a)>(b)?(a):(b)) 
#endif


#ifdef _WODVPNLIB
namespace WODVPNCOMLib
{
#include "\WeOnlyDo\wodVPN\Code\Win32LIB\Win32LIB.h"
}
#endif
#ifdef _WODXMPPLIB
namespace WODXMPPCOMLib
{
#include "\WeOnlyDo\wodXMPP\Code\Win32LIB\Win32LIB.h"
}
#endif

char *WIPPIENINITREQUEST = "WippienInitRequest";
char *WIPPIENINITRESPONSE	= "WippienInitResponse";
char *WIPPIENCONNECT = "WippienConnect";
char *WIPPIENDISCONNECT = "WippienDisconnect";
char *WIPPIENDETAILSTHREAD = "ExchangeDetailsThread";



CJabber *_Jabber = NULL;
extern CMainDlg _MainDlg;
extern CSettings _Settings;
extern CEthernet _Ethernet;
extern CContactAuthDlg *_ContactAuthDlg;
extern CSDKMessageLink *_SDK;

int uuencode(unsigned char *src, unsigned int srclength,char *target, size_t targsize);
void PopulateChatRoomListview(void);
BOOL EditARPEntry(unsigned char IPAddress[4], unsigned char MACAddress[6]);

#ifndef _WODXMPPLIB

// Define the jabber events to be handled:
_ATL_FUNC_INFO ConnectedInfo = {CC_STDCALL, VT_EMPTY, 0};
_ATL_FUNC_INFO DisconnectedInfo = {CC_STDCALL, VT_EMPTY, 2, {VT_I4,VT_BSTR}};
_ATL_FUNC_INFO StateChangeInfo = {CC_STDCALL, VT_EMPTY, 1, {VT_I4}};
_ATL_FUNC_INFO ContactStatusChangeInfo = {CC_STDCALL, VT_EMPTY, 4, {VT_DISPATCH, VT_DISPATCH, VT_I4, VT_I4}};
_ATL_FUNC_INFO IncomingMessageInfo = {CC_STDCALL, VT_EMPTY, 3, {VT_DISPATCH, VT_DISPATCH, VT_DISPATCH}};
_ATL_FUNC_INFO IncomingNotificationInfo = {CC_STDCALL, VT_EMPTY, 3, {VT_DISPATCH, VT_I4, VT_VARIANT}};
_ATL_FUNC_INFO ContactListInfo = {CC_STDCALL, VT_EMPTY, 0};
_ATL_FUNC_INFO ServiceRegisterInfo = {CC_STDCALL, VT_EMPTY, 3, {VT_DISPATCH, VT_BOOL, VT_BSTR}};
_ATL_FUNC_INFO ServiceStatusChangeInfo = {CC_STDCALL, VT_EMPTY, 1, {VT_DISPATCH}};
_ATL_FUNC_INFO ContactAuthRequestInfo = {CC_STDCALL, VT_EMPTY, 2, {VT_DISPATCH, VT_I4|VT_BYREF}};
_ATL_FUNC_INFO VCardDetailsInfo = {CC_STDCALL, VT_EMPTY, 2, {VT_DISPATCH,VT_BOOL}};
_ATL_FUNC_INFO ChatRoomListDoneInfo = {CC_STDCALL, VT_EMPTY, 2, {VT_DISPATCH}};
_ATL_FUNC_INFO ErrorInfo = {CC_STDCALL, VT_EMPTY, 5, {VT_DISPATCH, VT_DISPATCH, VT_DISPATCH, VT_I4, VT_BSTR}};

CJabberEvents::CJabberEvents (CJabber * ppJ)
{
    m_pJ = ppJ;
    DispEventAdvise ( (IUnknown*)m_pJ->m_Jabb);
}

CJabberEvents::~CJabberEvents ()
{
    DispEventUnadvise ( (IUnknown*)m_pJ->m_Jabb);
}
#endif


#ifdef _WODXMPPLIB
void XMPPContactAuthRequest(void *wodXMPP, void  *Contact, WODXMPPCOMLib::XMPPActionsEnum *Action)
#else
void __stdcall CJabberEvents::DispContactAuthRequest(WODXMPPCOMLib::IXMPPContact *Contact, WODXMPPCOMLib::XMPPActionsEnum *Action)
#endif
{
	// add to list of auth requests
	CComBSTR2 j;
	
#ifndef _WODXMPPLIB
	if (SUCCEEDED(Contact->get_JID(&j)))
#else
		char bf[1024];
	int bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, bf, &bfl);
	j.FromUTF8String(bf, strlen(bf));
#endif
	{
		char *jd1 = j.ToUTF8String();
		char *jd2 = strchr(jd1, '/');
		if (jd2)
			*jd2 = 0;
		
		if (_Settings.IsHiddenContact(jd1))
			return;
		
		
		BOOL found = FALSE;
		int i;
		for (i=0;!found && i<(signed)_Settings.m_AuthRequests.size();i++)
		{
			CComBSTR2 b = _Settings.m_AuthRequests[i];
			if (!lstrcmpi(b.ToUTF8String(), jd1)) // if they are same
				found = TRUE;
		}
		
		if (!found)
		{
			CComBSTR2 j3;
			j3.FromUTF8String(jd1, strlen(jd1));
			_Settings.m_AuthRequests.push_back(j3.Copy());
		}
	}
	
	BOOL b;
	_MainDlg.OnBtnAuthDlg(NULL, NULL, NULL, b);
	
	/*		// and now, if needed, reopen authdlg...
	if (!_ContactAuthDlg)
	{
	CContactAuthDlg *_ContactAuthDlg = new CContactAuthDlg();
	RECT rcDefault = { CW_USEDEFAULT, CW_USEDEFAULT, 0, 0 };
	_ContactAuthDlg->Create(NULL, rcDefault, NULL);
	}
	if (_ContactAuthDlg)
	_ContactAuthDlg->ShowWindow(SW_SHOW);
	*/
	
	*Action = (WODXMPPCOMLib::XMPPActionsEnum)2;//::SilentDeny);
}

#ifdef _WODXMPPLIB
void XMPPStateChange(void *wodXMPP, WODXMPPCOMLib::StatesEnum OldState)
#else
void __stdcall CJabberEvents::DispStateChange(WODXMPPCOMLib::StatesEnum OldState)
#endif
{
	int newst = 0;
#ifndef _WODXMPPLIB
	if (SUCCEEDED(_Jabber->m_Jabb->get_State((WODXMPPCOMLib::StatesEnum *)&newst)))
#else
		WODXMPPCOMLib::XMPP_GetState(wodXMPP, (WODXMPPCOMLib::StatesEnum *)&newst);
#endif
	
	_MainDlg.m_UserList.m_SortedUsersBuffer.Clear();
	WODXMPPCOMLib::StatusEnum newstatus;
	
#ifndef _WODXMPPLIB
	_Jabber->m_Jabb->get_Status(&newstatus);
	CComBSTR2 j;
	VARIANT var;
	var.vt = VT_ERROR;
	if (SUCCEEDED(_Jabber->m_Jabb->get_StatusText(var, &j)))
	{
		char buff[1024];
		strcpy(buff, j.ToString());
		strcpy(buff, _Settings.Translate(buff));
		j.Empty();
		if (SUCCEEDED(_Jabber->m_Jabb->get_StateText(var, &j)))
		{
			strcat(buff, " - ");
			strcat(buff, j.ToString());
		}
		_MainDlg.ShowStatusText(buff);
	}
#else
	WODXMPPCOMLib::XMPP_GetStatus(wodXMPP, &newstatus);
	char stat[1024], stet[1024];
	int slen = 1024;
	WODXMPPCOMLib::XMPP_GetStatusText(wodXMPP, stat, &slen);
	
	if (_MainDlg.IsWindow())
	{
		if (newstatus<7)
		{
			_MainDlg.ShellIcon(NIM_MODIFY, _MainDlg.m_OnlineStatus[newstatus], stat);
		}
		_MainDlg.Invalidate(FALSE);
	}
	
	strcpy(stat, _Settings.Translate(stat));
	strcat(stat, " - ");
	slen = 1024;
	WODXMPPCOMLib::XMPP_GetStateText(wodXMPP, (WODXMPPCOMLib::StatesEnum)newst, stet, &slen);
	strcpy(stet, _Settings.Translate(stet));
	strcat(stat, stet);
	_MainDlg.ShowStatusText(stat);
	
#endif
	
	// let's integrate SDK too
	if (_SDK)
	{
		{
			Buffer b;
			b.PutInt((int)OldState);
			b.PutInt((int)newst);
			if (!_SDK->FireEvent(WM_WIPPIEN_EVENT_STATECHANGE, b.Ptr(), b.Len()))
				return;
		}
	}
	
}

#ifdef _WODXMPPLIB
void XMPPDisconnected(void *wodXMPP, long ErrorCode, char *ErrorText)
#else
void __stdcall CJabberEvents::DispDisconnected (long ErrorCode, BSTR ErrorText)
#endif
{
	
	_MainDlg.m_UserList.m_SortedUsersBuffer.Clear();
	_Jabber->m_Initial = TRUE;
	
	if (_SDK)
	{
		Buffer b;
		b.PutCString(ErrorText);
		
		if (!_SDK->FireEvent(WM_WIPPIEN_EVENT_DISCONNECTED, b.Ptr(), b.Len()))
			return;
	}
	
	
	CComBSTR2 b;
	if (ErrorCode)
	{
		b = ErrorText;
		_MainDlg.ShowStatusText(b.ToString());
		
	}
	else
		_MainDlg.ShowStatusText(_Settings.Translate("Disconnected"));
	
	if (_Jabber->m_DoReconnect)
	{
		_MainDlg.m_ReconnectWait = 15;
		if (_MainDlg.IsWindow())
			_MainDlg.SetTimer(106,1000);
	}
	
	
	
	if (_MainDlg.IsWindow())
	{
		int i;
		for (i=0;i<(signed)_MainDlg.m_UserList.m_Users.size();i++)
		{
			CUser *us = (CUser *)_MainDlg.m_UserList.m_Users[i];
			us->m_Online = FALSE;
			us->m_Changed = TRUE;
		}
		_MainDlg.m_UserList.RefreshUser(NULL, NULL);
		//		_MainDlg.m_UserList.PostMessage(WM_REFRESH, NULL, FALSE);
	}
	_MainDlg.ShellIcon(NIM_MODIFY, _MainDlg.m_OnlineStatus[0], _Settings.Translate("Disconnected")); //offline
	
	
#ifdef _WODXMPPLIB
	XMPPStateChange(wodXMPP, (WODXMPPCOMLib::StatesEnum)1);
#else
	DispStateChange((WODXMPPCOMLib::StatesEnum)1);
#endif
}

#ifdef _WODXMPPLIB
void XMPPConnected(void *wodXMPP)
#else
void __stdcall CJabberEvents::DispConnected ()
#endif
{
	char buff[1024];
	int b = sizeof(buff);
	WODXMPPCOMLib::XMPP_GetLogin(_Jabber->m_Jabb, buff, &b);
	char *j1 = strchr(buff, '/');
	if (j1)
	{
		*j1 = 0;
		j1++;
	}
	CComBSTR2 mj;
	mj.FromUTF8String(buff, strlen(buff));
	_Settings.m_JID.Empty();
	_Settings.m_JID.Attach(mj.Detach());
	if (j1)
		_Settings.m_Resource = j1;
	
	_MainDlg.m_UserList.m_SortedUsersBuffer.Clear();
	_Jabber->m_ConnectTime = GetTickCount();
	//		_MainDlg.ShowStatusText("Connected.");
#ifndef _WODXMPPLIB
	WODXMPPCOMLib::IXMPPServices *serv = NULL;
	if (SUCCEEDED(_Jabber->m_Jabb->get_Services(&serv)))
	{
		serv->Discover(TRUE);
		serv->Release();
	}
	
	// and request vcard
	WODXMPPCOMLib::IXMPPVCard *vc;
	if (SUCCEEDED(_Jabber->m_Jabb->get_VCard(&vc)))
	{
		vc->Receive();
		vc->Release();
	}
	DispStateChange((WODXMPPCOMLib::StatesEnum)0);
#else 
	WODXMPPCOMLib::XMPP_ServicesDiscover(_Jabber->m_Jabb, TRUE);
	WODXMPPCOMLib::XMPP_VCardReceive(_Jabber->m_Jabb);
#endif
	
	if (_Settings.m_DeleteContactsOnConnect)
	{
		_MainDlg.m_UserList.DeleteAllItems();
		// we should now delete all our contacts
		while (_MainDlg.m_UserList.m_Users.size())
		{
			CUser *user = _MainDlg.m_UserList.m_Users[0];
			_MainDlg.m_UserList.m_Users.erase(_MainDlg.m_UserList.m_Users.begin());
#ifdef _WODVPNLIB
			delete user;
#else
			user->Release();
#endif						
		}
	}
	
	
	_MainDlg.ShellIcon(NIM_MODIFY, _MainDlg.m_OnlineStatus[1], _Settings.Translate("Connected")); //online
	if (_SDK)
	{
		if (!_SDK->FireEvent(WM_WIPPIEN_EVENT_CONNECTED,""))
			return;
	}
	
	
#ifdef _WODXMPPLIB
	XMPPStateChange(wodXMPP, (WODXMPPCOMLib::StatesEnum)0);
#else
	DispStateChange((WODXMPPCOMLib::StatesEnum)0);
#endif
	
}
#ifdef _WODXMPPLIB
void XMPPContactStatusChange(void *wodXMPP, void  *Contact, void *ChatRoom, WODXMPPCOMLib::StatusEnum NewStatus, WODXMPPCOMLib::StatusEnum OldStatus)
#else
void __stdcall CJabberEvents::DispContactStatusChange(WODXMPPCOMLib::IXMPPContact *Contact, WODXMPPCOMLib::IXMPPChatRoom *ChatRoom, WODXMPPCOMLib::StatusEnum NewStatus, WODXMPPCOMLib::StatusEnum OldStatus)
#endif
{
	if (ChatRoom)
	{
		char tbchatroom[1024] = {0}, tbjid[1024] = {0};
		int tblen = sizeof(tbchatroom);
#ifndef _WODXMPPLIB
		CComBSTR2 jcontact, jchatroom;
		ChatRoom->get_JID(&jchatroom);
		strcpy(tbchatroom, jchatroom.ToString());
		if (Contact)
		{
			Contact->get_JID(&jcontact);
			strcpy(tbjid, jcontact.ToString());
		}
#else
		WODXMPPCOMLib::XMPP_ChatRoom_GetJID(ChatRoom, tbchatroom, &tblen);
		tblen = sizeof(tbjid);
		WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, tbjid, &tblen);
#endif
		_MainDlg.OnIncomingMessage(tbchatroom, tbjid, "", "");
		_MainDlg.m_UserList.RefreshUser(Contact, tbchatroom);
		
		return;
	}
	
	_MainDlg.m_UserList.m_SortedUsersBuffer.Clear();
	if (!_Jabber->m_Initial)
	{
		
		if (_SDK)
		{
			CComBSTR j;
			
			WODXMPPCOMLib::StatusEnum st;
#ifndef _WODXMPPLIB
			Contact->get_JID(&j);
			Contact->get_Status(&st);
#else
			char bf[1024];
			int bflen = sizeof(bf);
			WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, bf, &bflen);
			j = bf;
			WODXMPPCOMLib::XMPP_Contact_GetStatus(Contact, &st);
#endif
			
			Buffer b;
			b.PutCString(j);
			b.PutInt((int)st);
			b.PutInt((int)OldStatus);
			if (!_SDK->FireEvent(WM_WIPPIEN_EVENT_USERPRESENCECHANGE, b.Ptr(), b.Len()))
				return;
		}
		
		_MainDlg.m_UserList.RefreshUser(Contact, NULL);
	}
}
#ifdef _WODXMPPLIB
void XMPPIncomingNotification(void *wodXMPP, void  *Contact, WODXMPPCOMLib::ContactNotifyEnum NotifyID, VARIANT Data)
#else
void __stdcall CJabberEvents::DispIncomingNotification(WODXMPPCOMLib::IXMPPContact *Contact, WODXMPPCOMLib::ContactNotifyEnum NotifyID, VARIANT Data)
#endif
{
#ifndef _WODXMPPLIB
	CComBSTR2 j;
	Contact->get_JID(&j);
	
	char *jd1 = j.ToString();
	char *jd2 = strchr(jd1, '/');
	if (jd2)
		*jd2 = 0;
#else
	char jd1[1024];
	int jl = sizeof(jd1);
	WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, jd1, &jl);
#endif
	
	_MainDlg.OnIncomingNotification(jd1, (int)NotifyID, Data);
}
#ifdef _WODXMPPLIB
void XMPPIncomingMessage(void *wodXMPP, void  *Contact, void *ChatRoom, void  *Message)
#else
void __stdcall CJabberEvents::DispIncomingMessage(WODXMPPCOMLib::IXMPPContact *Contact, WODXMPPCOMLib::IXMPPChatRoom *ChatRoom, WODXMPPCOMLib::IXMPPMessage *Message)
#endif
{
	WODXMPPCOMLib::MessageTypesEnum msgtype = (WODXMPPCOMLib::MessageTypesEnum)0;
	if (Message)
#ifndef _WODXMPPLIB
		Message->get_Type(&msgtype);
#else
	WODXMPPCOMLib::XMPP_Message_GetType(Message, &msgtype);
#endif

	if (Contact && msgtype!=/*MsgGroupChat*/2)
	{		
		// is it from mediator?
		CComBSTR jid;
		CComBSTR2 jd;
#ifndef _WODXMPPLIB
		if (SUCCEEDED(Contact->get_JID(&jd)))
#else
			char jdbf[1024];
		int jdbflen = sizeof(jdbf);
		WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, jdbf, &jdbflen);
		jd = jdbf;
#endif
		{
			char *jd1 = jd.ToString();
			if (_Settings.IsHiddenContact(jd1))
				return;
			
			if (!ChatRoom)
			{
				char *jd2 = strchr(jd1, '/');
				if (jd2)
					*jd2 = 0;
			}
			jid = jd1;
			
		}
		
		// is it headline
		//		WODXMPPCOMLib::MessageTypesEnum ms;
		
		//		if (SUCCEEDED(Message->get_Type(&ms)))
		{
			CComBSTR subj;
			char subjbuff[1024];
			int subjbufflen = sizeof(subjbuff);
			int consumetextcount = 0;
			if (msgtype == (WODXMPPCOMLib::MessageTypesEnum)3 || 
				msgtype == (WODXMPPCOMLib::MessageTypesEnum)0)//::Normal)
			{
				//				ATLTRACE("Got message from remote peer\r\n");
#ifndef _WODXMPPLIB
				if (SUCCEEDED(Message->get_Subject(&subj)))
				{
					CComBSTR2 s1 = subj;
					strcpy(subjbuff, s1.ToString());
				}
#else
				WODXMPPCOMLib::XMPP_Message_GetSubject(Message, subjbuff, &subjbufflen);
				subj = subjbuff;
				{
				}
#endif
			}
			else
			{
				// check for alien contacts
				CComBSTR2 j;
#ifndef _WODXMPPLIB
				Contact->get_JID(&j);
#else
				subjbufflen = sizeof(subjbuff);
				WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, subjbuff, &subjbufflen);
				j = subjbuff;				
#endif
				CUser *user = _MainDlg.m_UserList.GetUserByJID(j);
				if (user && user->m_IsAlienWippien)
				{
					CComBSTR2 r;
#ifndef _WODXMPPLIB
					if (SUCCEEDED(Message->get_Text(&r)))
#else
						subjbufflen = sizeof(subjbuff);
					WODXMPPCOMLib::XMPP_Message_GetText(Message, subjbuff, &subjbufflen);
					r = subjbuff;
#endif
					char *r1 = r.ToString();
					if (*r1 == '[')
					{
						if (!strncmp(r1+1, WIPPIENINITREQUEST, strlen(WIPPIENINITREQUEST)))
						{
							subj = WIPPIENINITREQUEST;
							msgtype = (WODXMPPCOMLib::MessageTypesEnum)0;
							consumetextcount += strlen(WIPPIENINITREQUEST)+2;
						}
						else
							if (!strncmp(r1+1, WIPPIENINITRESPONSE, strlen(WIPPIENINITRESPONSE)))
							{
								subj = WIPPIENINITRESPONSE;
								msgtype = (WODXMPPCOMLib::MessageTypesEnum)0;
								consumetextcount += strlen(WIPPIENINITRESPONSE)+2;
							}
							else
								if (!strncmp(r1+1, WIPPIENCONNECT, strlen(WIPPIENCONNECT)))
								{
									subj = WIPPIENCONNECT;
									msgtype = (WODXMPPCOMLib::MessageTypesEnum)0;
								}
								else
									if (!strncmp(r1+1, WIPPIENDISCONNECT, strlen(WIPPIENDISCONNECT)))
									{
										subj = WIPPIENDISCONNECT;
										msgtype = (WODXMPPCOMLib::MessageTypesEnum)0;
									}
					}
				}
				
			}
			
			if (msgtype == (WODXMPPCOMLib::MessageTypesEnum)3 ||
				msgtype == (WODXMPPCOMLib::MessageTypesEnum)0 )//::MsgHeadline or normal)
			{
				{
					if (subj == WIPPIENINITREQUEST && Contact)
					{
						if (!_Ethernet.m_Available) return; // ignore if ethernet is not available
						
						Buffer in, out;
						CComBSTR2 r;
#ifndef _WODXMPPLIB
						if (SUCCEEDED(Message->get_Text(&r)))
#else
							subjbufflen = sizeof(subjbuff);
						WODXMPPCOMLib::XMPP_Message_GetText(Message, subjbuff, &subjbufflen);
						r = subjbuff;
#endif
						{
							char *r1 = r.ToString();
							if (consumetextcount)
							{
								r1 += consumetextcount;
								char *r2 = strchr(r1, ']');
								if (r2)
									*r2 = 0;
							}
							in.Append(r1);
							
							_Settings.FromHex(&in, &out);
							
							// save this for user
							CComBSTR j;
							CComBSTR2 capa;
#ifndef _WODXMPPLIB
							Contact->get_Capabilities(&capa);
							if (SUCCEEDED(Contact->get_JID(&j)))
#else
								subjbufflen = sizeof(subjbuff);
							subjbuff[0] = 0;
							WODXMPPCOMLib::XMPP_Contact_GetCapabilities(Contact, subjbuff, &subjbufflen);
							capa = subjbuff;
							
							subjbufflen = sizeof(subjbuff);
							WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, subjbuff, &subjbufflen);
							j = subjbuff;
							
#endif
							{
								CUser *user = _MainDlg.m_UserList.GetUserByJID(j);
								if (user)
								{
									user->DumpToFileFixed("Got WIPPIENINITREQUEST\r\n");
									
									// since this message arrived, he *MUST* be using wippien, isn't he?
									user->m_IsUsingWippien3 = TRUE;
									
									if (user->m_IsUsingWippien3)
									{
										if (out.Len()>=4)
										{
											user->m_HisVirtualIP = out.GetInt();
											if (out.Len()>=6)
											{
												memcpy(user->m_MAC, out.Ptr(), 6);
												out.Consume(6);
												// set up ARP table for this MAC
												EditARPEntry((unsigned char *)&user->m_HisVirtualIP, user->m_MAC);
											}
											else
											{
												// calculate MAC too
												if (!memcmp(user->m_MAC, "\0\0\0\0\0\0", 6))
												{
													user->m_MAC[0] = _Settings.m_MAC[0];
													user->m_MAC[1] = _Settings.m_MAC[1];
													memcpy(&user->m_MAC[2], &user->m_HisVirtualIP, 4);
												}
											}
											
											//  also private key
											if (user->m_RSA)
												RSA_free(user->m_RSA);
											user->m_RSA = RSA_new();
											
											user->m_RSA->e = BN_new();
											out.GetBignum2(user->m_RSA->e);
											user->m_RSA->n = BN_new();
											out.GetBignum2(user->m_RSA->n);
											
											if (out.Len())
											{
												user->m_RemoteWippienState = (WippienState)out.GetChar();
												user->DumpToFile("RemoteWippienState set to %s\r\n", WippienStateString[user->m_RemoteWippienState]);
											}
											
											if (out.Len())
											{
												int olen = out.GetInt();
												if (olen)
												{
													while (out.Len()>4)
													{
														int ij = out.GetInt();
														if (ij > (signed)out.Len())
															ij = out.Len();
														char buff[1024];
														memset(buff, 0, sizeof(buff));
														memcpy(buff, out.Ptr(), ij);
														out.Consume(ij);
														if (out.Len() && _Settings.m_AllowAnyMediator)
															_Settings.AddLinkMediator(buff, out.GetInt());															
													}
												}
											}
											//user->DumpToFile("Before WIPPIENINITREQUEST, myrandom=%d, hisrandom=%d", user->m_MyRandom, user->m_HisRandom);
											if (out.Len())
											{
												//user->DumpToFile("In WIPPIENINITREQUEST, myrandom=%d, hisrandom=%d", user->m_MyRandom, user->m_HisRandom);
												// random number for remote peer
												user->m_HisRandom = out.GetInt();
											}
											//user->DumpToFile("After WIPPIENINITREQUEST, myrandom=%d, hisrandom=%d", user->m_MyRandom, user->m_HisRandom);
											
											user->m_Changed = TRUE;
											_MainDlg.m_UserList.RefreshUser(Contact, NULL);
										}
										user->m_WippienState = WipWaitingInitResponse;
										user->DumpToFile("WippienState set to %s\r\n", WippienStateString[user->m_WippienState]);
										if (!user->m_Block)
										{
											user->SetTimer(rand()%100, 3);
										}
									}
								}
							}
						}
					}
					else
					if (subj == WIPPIENINITRESPONSE && Contact)
					{
						if (!_Ethernet.m_Available) return; // ignore if ethernet is not available
						
						Buffer in, out;
						CComBSTR2 r;
#ifndef _WODXMPPLIB
						if (SUCCEEDED(Message->get_Text(&r)))
#else
							char textbuff[1024];
						int tblen = sizeof(textbuff);
						WODXMPPCOMLib::XMPP_Message_GetText(Message, textbuff, &tblen);
						r = textbuff;
#endif
						{
							char *r1 = r.ToString();
							if (consumetextcount)
							{
								r1 += consumetextcount;
								char *r2 = strchr(r1, ']');
								if (r2)
									*r2 = 0;
							}
							in.Append(r1);
							
							_Settings.FromHex(&in, &out);
							
							// save this for user
							CComBSTR j;
#ifndef _WODXMPPLIB
							if (SUCCEEDED(Contact->get_JID(&j)))
#else
								tblen = sizeof(textbuff);
							WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, textbuff, &tblen);
							j = textbuff;
#endif
							{
								CUser *user = _MainDlg.m_UserList.GetUserByJID(j);
								if (user)
								{
									user->DumpToFileFixed("Got WIPPIENINITRESPONSE\r\n");
									//									ATLTRACE("Got WIPPIENINITRESPONSE from %s\r\n", user->m_JID);
									
									// if we're trying to connect now..
									if (user->m_WippienState >= WipConnecting)
										return;
									
									
									char src[8192], dst[8192];
									memcpy(src, out.Ptr(), 128);
									if (RSA_private_decrypt(128, (unsigned char *)src, (unsigned char *)dst,  _Settings.m_RSA, RSA_PKCS1_PADDING) < 0)
									{
										user->m_IsUsingWippien3 = TRUE; // obviously...
										user->m_WippienState = WipWaitingInitRequest;
										user->DumpToFile("WippienState set to %s\r\n", WippienStateString[user->m_WippienState]);
										user->SetTimer(rand()%100, 3);
										//										user->NotifyDisconnect();
										return;
									}
									
									out.Consume(128);
									if (out.Len())
									{
										user->m_RemoteWippienState = (WippienState)out.GetChar();
										user->DumpToFile("RemoteWippienState set to %s\r\n", WippienStateString[user->m_RemoteWippienState]);
									}
									
									if (out.Len()/* && user->m_MyMediatorOffer[0]*/)
									{
										// user sent us his choice of mediators
										char *med = out.GetString(NULL);
										if (med)
										{
											//											ATLTRACE("Got mediator offer for %s from %s\r\n", med, user->m_JID);
											
											int port = out.GetInt();
											if (port)
											{
												user->m_HisMediatorChoice = out.GetInt();
												int ch = 0;
												if ((user->m_HisMediatorChoice + user->m_MyMediatorChoice) % 2 != 0)
												{
													//													ATLTRACE("selecting MAX\r\n");
													ch = MAX(user->m_HisMediatorChoice, user->m_MyMediatorChoice);
												}
												else
												{
													//													ATLTRACE("selecting MIN\r\n");
													ch = MIN(user->m_HisMediatorChoice, user->m_MyMediatorChoice);
												}
												if (ch == user->m_HisMediatorChoice)
												{
													strcpy(user->m_HisMediatorOffer, med);
													user->m_HisMediatorPort = port;
												} 
												else
												{
													strcpy(user->m_HisMediatorOffer, user->m_MyMediatorOffer);
													user->m_HisMediatorPort = user->m_MyMediatorPort;
												}
												//												user->m_MyMediatorOffer[0] = 0;
												ATLTRACE("Selected mediator %s for %s/%s\r\n", user->m_HisMediatorOffer, user->m_JID, user->m_Resource);
											}											
											free(med);
										}
									}
									
									
									// and XOR with ours
									for (int i = 0; i < 16; i++)
										user->m_SharedKey[i] = user->m_MyKey[i] ^ dst[i + 24];
									
									int klen = uuencode((unsigned char *)user->m_SharedKey, 16, dst, 8192);
									src[klen] = 0;
									EnterCriticalSection(&user->m_CritCS);
#ifdef _WODVPNLIB
									WODVPNCOMLib::VPN_Stop(user->m_wodVPN);
									WODVPNCOMLib::VPN_SetPassword(user->m_wodVPN, dst);
#else
									user->m_wodVPN->raw_Stop();
									user->m_wodVPN->Password = dst;
									//									user->m_wodVPN->Start(0);
#endif
									LeaveCriticalSection(&user->m_CritCS);
									
									user->m_Changed = TRUE;
									_MainDlg.m_UserList.RefreshUser(Contact, NULL);
									
									user->m_WippienState = WipDisconnected;
									user->DumpToFile("WippienState set to %s\r\n", WippienStateString[user->m_WippienState]);
									if (!user->m_Block)
									{
										user->SetTimer(rand()%100, 3);
										if (_Settings.m_AutoConnectVPNOnStartup)
											user->SendConnectionRequest(TRUE);
									}
									
								}
							}
						}
					}
					else
					if (subj == WIPPIENDISCONNECT && Contact)
					{
						//						ATLTRACE("Got DISCONNECT message\r\n");
						// reinit...
						CComBSTR j;
#ifndef _WODXMPPLIB
						if (SUCCEEDED(Contact->get_JID(&j)))
#else
							char tb[1024];
						int tblen = sizeof(tb);
						WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, tb, &tblen);
						j = tb;
#endif
						{
							CUser *user = _MainDlg.m_UserList.GetUserByJID(j);
							if (user)
							{
								user->DumpToFileFixed("Got WIPPIENDISCONNECT\r\n");
								user->ReInit(TRUE);
							}
						}
					}
					else
					if (subj == WIPPIENCONNECT && Contact)
					{
						// reinit...
						CComBSTR j;
#ifndef _WODXMPPLIB
						if (SUCCEEDED(Contact->get_JID(&j)))
#else
							char tb[1024];
						int tblen = sizeof(tb);
						WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, tb, &tblen);
						j = tb;
#endif
						{
							CUser *user = _MainDlg.m_UserList.GetUserByJID(j);
							if (user)
							{
								user->DumpToFileFixed("Got WIPPIENCONNECT\r\n");
								if (user->m_Block)
									return;
								
								//								ATLTRACE("Got CONNECT message from %s\r\n", user->m_JID);
								Buffer in, out;
								CComBSTR2 r;
#ifndef _WODXMPPLIB
								if (SUCCEEDED(Message->get_Text(&r)))
#else
									tblen = sizeof(tb);
								WODXMPPCOMLib::XMPP_Message_GetText(Message, tb, &tblen);
								r = tb;
#endif
								{
									char *r1 = r.ToString();
									if (r1)
									{
										in.Append(r1);
										if (in.Len())
										{		
											_Settings.FromHex(&in, &out);
											if (out.Len())
											{
												user->m_RemoteWippienState = (WippienState)out.GetChar();
												user->DumpToFile("RemoteWippienState set to %s\r\n", WippienStateString[user->m_RemoteWippienState]);
											}
										}
									}
								}
							}
							if (user && !user->m_Block)
							{
								if (user->m_IsUsingWippien3)
									user->SendConnectionRequest(FALSE);
								else
								{
									user->m_IsUsingWippien3 = TRUE;
									user->SetTimer(rand()%100, 3);
								}
							}
						}
					}			
					else
						goto valid_message_hack;
				}
			}
			else
			{				
				if (!ChatRoom || msgtype != /*MgrGroupChat*/2)
				{
				
valid_message_hack:

					CComBSTR2 j;
	#ifndef _WODXMPPLIB
					Contact->get_JID(&j);
	#else
					char tb[1024];
					int tblen = sizeof(tb);
					WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, tb, &tblen);
					j = tb;
	#endif
					if (msgtype == (WODXMPPCOMLib::MessageTypesEnum)4)//::error)
					{
						if (Message)
						{
							BOOL show = TRUE;
							CComBSTR2 j1;
	#ifndef _WODXMPPLIB
							Message->get_Thread(&j1);
	#else
							tblen = sizeof(tb);
							WODXMPPCOMLib::XMPP_Message_GetThread(Message, tb, &tblen);
							j1 = tb;
	#endif
							if (j1.Length())
							{
								char *j2 = j1.ToString();
								if (strcmp(j2, WIPPIENDETAILSTHREAD))
									_MainDlg.OnIncomingMessage(NULL, j.ToString(), _Settings.Translate("Could not deliver message."), "");
							}
						}
					}
					else
					{
						CComBSTR2 t;
	#ifndef _WODXMPPLIB
						Message->get_Text(&t);
	#else
						tblen = sizeof(tb);
						WODXMPPCOMLib::XMPP_Message_GetText(Message, tb, &tblen);
						t.FromUTF8String(tb);
	#endif
						CComBSTR2 ht;
	#ifndef _WODXMPPLIB
						Message->get_HTMLText(&ht);
	#else
						tblen = sizeof(tb);
						WODXMPPCOMLib::XMPP_Message_GetHTMLText(Message, tb, &tblen);
						ht.FromUTF8String(tb);
	#endif
						_MainDlg.OnIncomingMessage(NULL, j.ToString(), t.ToString(), ht.ToString());
					}
				}
			}
		}
		
	}
	else
	{
		if (ChatRoom)
		{
			CComBSTR2 t;
			char tb[8192];
			int tblen = sizeof(tb);
#ifndef _WODXMPPLIB
			Message->get_Text(&t);
#else
			WODXMPPCOMLib::XMPP_Message_GetText(Message, tb, &tblen);
			t.FromUTF8String(tb);
#endif
			CComBSTR2 ht;
#ifndef _WODXMPPLIB
			Message->get_HTMLText(&ht);
#else
			tblen = sizeof(tb);
			WODXMPPCOMLib::XMPP_Message_GetHTMLText(Message, tb, &tblen);
			ht.FromUTF8String(tb);
#endif
			
			CComBSTR2 contactjid;
#ifndef _WODXMPPLIB
			Contact->get_JID(&contactjid);
#else
			tblen = sizeof(tb);
			WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, tb, &tblen);
			contactjid = tb;
#endif
			
#ifndef _WODXMPPLIB
			CComBSTR2 tb2;
			ChatRoom->get_JID(&tb2);
			strcpy(tb, tb2.ToString());
#else
			tblen = sizeof(tb);
			WODXMPPCOMLib::XMPP_ChatRoom_GetJID(ChatRoom, tb, &tblen);
#endif		
			
			_MainDlg.OnIncomingMessage(tb, contactjid.ToString(), t.ToString(), ht.ToString());
			
			return;
				}
	}
}
#ifdef _WODXMPPLIB
void XMPPContactList(void *wodXMPP)
#else
void __stdcall CJabberEvents::DispContactList()
#endif
{
	_MainDlg.m_UserList.m_SortedUsersBuffer.Clear();
	_Jabber->m_Initial = FALSE;
	//_MainDlg.m_UserList.RefreshUser(NULL, NULL);
	_MainDlg.m_UserList.InitialUserList();
}
#ifdef _WODXMPPLIB
void XMPPServiceRegister(void *wodXMPP, void  *Service, BOOL Success, char *Instructions)
#else
void __stdcall CJabberEvents::DispServiceRegister (WODXMPPCOMLib::IXMPPService *Service, VARIANT_BOOL Success, BSTR Instructions)
#endif
{
	_MainDlg.m_UserList.m_SortedUsersBuffer.Clear();
	CComBSTR2 i = Instructions;
	if (_Jabber->m_ServiceRegisterHwnd)
	{
		SetWindowText(_Jabber->m_ServiceRegisterHwnd, i.ToString());
	}
}
#ifdef _WODXMPPLIB
void XMPPServiceStatusChange(void *wodXMPP, void  *Service)
#else
void __stdcall CJabberEvents::DispServiceStatusChange (WODXMPPCOMLib::IXMPPService *Service)
#endif
{
	_MainDlg.m_UserList.m_SortedUsersBuffer.Clear();
	if (_Jabber->m_ServiceRefreshHwnd)
	{
		PostMessage(_Jabber->m_ServiceRefreshHwnd, WM_REFRESH, 0, 0);
	}
	
	char jid[1024] = {0};
	int l = sizeof(jid);
#ifdef _WODXMPPLIB
	WODXMPPCOMLib::XMPP_Service_GetJID(Service, jid, &l);
#else
	CComBSTR2 j_1;
	Service->get_JID(&j_1);
	strcpy(jid, j_1.ToString());
#endif
	
	
	int i;
	for (i=0;i<(signed)_MainDlg.m_ChatRooms.size();i++)
	{
		CChatRoom *r = (CChatRoom *)_MainDlg.m_ChatRooms[i];
		if (r->m_DoOpen)
		{
			char *j1 = strchr(r->m_JID, '@');
			if (j1)
			{
				j1++;
				if (!strcmp(j1, jid))
				{
					r->m_DoOpen = FALSE;
					// attempt to open
					
					
#ifndef _WODXMPPLIB
					WODXMPPCOMLib::IXMPPChatRoom *croom = NULL;
					WODXMPPCOMLib::IXMPPChatRooms *rs = NULL;
					_Jabber->m_Jabb->get_ChatRooms(&rs);
					if (rs)
					{
						CComBSTR n = r->m_JID;
						VARIANT var;
						var.vt = VT_BSTR;
						var.bstrVal = n;
						rs->get_Room(var, &croom);
						if (!croom)
						{
							
							WODXMPPCOMLib::IXMPPServices *serv = NULL;
							if (SUCCEEDED(_Jabber->m_Jabb->get_Services(&serv)))
							{
								CComBSTR2 n1 = n;
								char *n2 = n1.ToString();
								char *n3 = strchr(n2, '@');
								if (n3)
								{
									*n3 = 0;
									n3++;
									CComBSTR n4 = n3;
									CComBSTR n5 = n2;
									
									var.vt = VT_BSTR;
									var.bstrVal = n4;
									WODXMPPCOMLib::IXMPPService *s = NULL;
									serv->get_Item(var, &s);
									
									
									var.vt = VT_DISPATCH;
									var.pdispVal = s;
									rs->raw_Add(n5, var, &croom);
									
									if (s)
										s->Release();
								}
								serv->Release();
							}
						}
						if (croom)
						{
							CComBSTR p = r->m_Password;
							croom->put_Password(p);
							CComBSTR2 n1 = r->m_Nick;
							croom->put_Nick(n1);
							croom->put_ShowMyself(VARIANT_FALSE);
							croom->Join();
							croom->Release();
						}
					}
#else
					void *chatroom = NULL;
					char buff[1024] = {0};
					strcpy(buff, r->m_JID);
					WODXMPPCOMLib::XMPP_GetChatRoomByName(_Jabber->m_Jabb, buff, &chatroom);
					if (!chatroom)
					{
						strcpy(buff, r->m_JID);
						WODXMPPCOMLib::XMPP_ChatRooms_Add(_Jabber->m_Jabb, buff, &chatroom);
					}
					if (chatroom)
					{
						WODXMPPCOMLib::XMPP_ChatRoom_SetPassword(chatroom, r->m_Password);
						CComBSTR2 n = r->m_Nick;
						WODXMPPCOMLib::XMPP_ChatRoom_SetNick(chatroom, n.ToString());
						WODXMPPCOMLib::XMPP_ChatRoom_SetShowMyself(chatroom, FALSE);
						WODXMPPCOMLib::XMPP_ChatRoom_Join(chatroom);
						WODXMPPCOMLib::XMPP_ChatRoom_Free(chatroom);
					}
#endif
				}
			}
		}
	}
}
#ifdef _WODXMPPLIB
void XMPPVCardDetails(void *wodXMPP, void  *Contact, BOOL Partial)
#else
void __stdcall CJabberEvents::DispVCardDetails(WODXMPPCOMLib::IXMPPContact *Contact, VARIANT_BOOL Partial)
#endif
{
	//	if (Contact)
	//	{
	_MainDlg.m_UserList.OnVCard(Contact, Partial?TRUE:FALSE, TRUE);
	//	}
}

#ifdef _WODXMPPLIB
void XMPPChatRoomListDone(void *wodXMPP, void *Service)
#else
void __stdcall CJabberEvents::DispChatRoomListDone(WODXMPPCOMLib::IXMPPService *Service)
#endif
{
	if (!Service)
		return;
	
	PopulateChatRoomListview();
}

#ifdef _WODXMPPLIB
void XMPPError(void *wodXMPP, void *Contact, void *ChatRoom, void *Service, long ErrorCode, char *ErrorText)
#else
void __stdcall CJabberEvents::DispError(WODXMPPCOMLib::IXMPPContact *Contact, WODXMPPCOMLib::IXMPPChatRoom *ChatRoom, WODXMPPCOMLib::IXMPPMessage *Message, long ErrorCode, BSTR ErrorText)
#endif
{
	CComBSTR t = "XMPP Error";
	CComBSTR2 e = ErrorText;
	char *err = e.ToString();
	
	if (_MainDlg.m_NewPassword.Length())
	{
		_MainDlg.m_NewPassword.Empty();
		_MainDlg.MessageBox(_Settings.Translate(err), _Settings.Translate("Failed to change password"), MB_OK);
	}
	else
		if (Contact)
		{
#ifdef _WODXMPPLIB
			char buff[1024];
			int buflen = sizeof(buff);
		WODXMPPCOMLib::XMPP_Contact_GetJID(Contact, buff, &buflen);
		t += ": ";
		t = buff;
#else
		CComBSTR t2;
		Contact->get_JID(&t2);
		t += ": ";
		t += t2;
#endif
	}
//	CComBSTR2 t1 = t;
//	MessageBox(NULL, err, t1.ToString(), MB_OK | MB_ICONHAND);
}

#ifdef _WODXMPPLIB
void XMPPChatRoomData(void *wodXMPP, void *Room)
{
	char buff[8192];
	int l = sizeof(buff);
	WODXMPPCOMLib::XMPP_ChatRoom_GetJID(Room, buff, &l);
	long h = 0;
	
	int i;
	for (i = 0; i < (signed)_MainDlg.m_ChatRooms.size(); i++)
	{
		CChatRoom *room = _MainDlg.m_ChatRooms[i];
		if (!lstrcmpi(room->m_JID, buff))
		{
			h = (long)room->m_MessageWin->m_hWnd;
			break;
		}
	}

	strcat(buff, " configuration");


	void *settings = NULL;
	WODXMPPCOMLib::XMPP_ChatRoom_SettingsGet(Room, &settings);
	if (settings)
	{
		int success = FALSE;
		WODXMPPCOMLib::XMPP_Vars_Show(settings, buff, h, &success);
		if (success)
			WODXMPPCOMLib::XMPP_ChatRoom_SaveSettings(Room);

		WODXMPPCOMLib::XMPP_Vars_Free(settings);
	}
}
#endif



CJabber::CJabber(void)
{
#ifdef _WODXMPPLIB
	memset(&m_Events, 0, sizeof(m_Events));

	m_Events.Connected = XMPPConnected;
	m_Events.Disconnected = XMPPDisconnected;
	m_Events.StateChange = XMPPStateChange;
	m_Events.ContactStatusChange = XMPPContactStatusChange;
	m_Events.IncomingMessage = XMPPIncomingMessage;
	m_Events.IncomingNotification = XMPPIncomingNotification;
	m_Events.ContactList = XMPPContactList;
	m_Events.ServiceRegister = XMPPServiceRegister;
	m_Events.ServiceStatusChange = XMPPServiceStatusChange;
	m_Events.ContactAuthRequest = XMPPContactAuthRequest;
	m_Events.VCardDetails = XMPPVCardDetails;
	m_Events.ChatRoomListDone = XMPPChatRoomListDone;
	m_Events.Error = XMPPError;
	m_Events.ChatRoomData = XMPPChatRoomData;

		
	m_Jabb = WODXMPPCOMLib::_XMPP_Create(&m_Events);
#else
	m_Jabb.CoCreateInstance(__uuidof(WODXMPPCOMLib::wodXMPPCom));
	m_Events = NULL;
#endif

#ifdef WODXMPP_LICENSE_KEY
	CComBSTR blic(WODXMPP_LICENSE_KEY);
	m_Jabb->put_LicenseKey(blic);
#endif	

	m_Initial = TRUE;
	m_ServiceRegisterHwnd = NULL;
	m_ServiceRefreshHwnd = NULL;
	m_DoReconnect = TRUE;
}

CJabber::~CJabber()
{
	Disconnect();
#ifndef _WODXMPPLIB
	m_Jabb.Release();
#else
	WODXMPPCOMLib::_XMPP_Destroy(m_Jabb);
#endif
}

void CJabber::Connect(char *JID, char *pass, char *hostname, int port, BOOL usessl)
{
	m_ConnectTime = GetTickCount();
	char *a = strchr(JID, '/');
	if (a)
		*a = 0;

	CComBSTR2 _l;
	_l.FromUTF8String(JID, strlen(JID));
	CComBSTR l;
	l.Attach(_l.Detach());
	CComBSTR p = pass,h = hostname;
	int prt = port;
	
	l += "/";
	if (*_Settings.m_Resource)
		l += _Settings.m_Resource;
	else
	{
		if (_Ethernet.m_Available)
			l += WIPPIENIM3;
		else
			l += "WippienNoAdapter";
	}


#ifndef _WODXMPPLIB
	if (_Ethernet.m_Available)
	{
		CComBSTR caps;
		m_Jabb->get_Capabilities(&caps);
		CComBSTR2 caps2 = caps;
		if (!strstr(caps2.ToString(), WIPPIENIM))
		{
			if (caps.Length())
				caps += " ";
			caps += "Wippien";
			m_Jabb->put_Capabilities(caps);
		}
	}

	m_Jabb->put_Login(l);
	m_Jabb->put_Password(p);
	if (prt)
		m_Jabb->put_Port(prt);
	if (usessl)
		m_Jabb->put_Security((WODXMPPCOMLib::SecurityEnum)3);
	else
		m_Jabb->put_Security((WODXMPPCOMLib::SecurityEnum)1);

#else
	if (_Ethernet.m_Available)
	{
		char buff[1024] = {0};
		int bflen = sizeof(buff);
		WODXMPPCOMLib::XMPP_GetCapabilities(m_Jabb, buff, &bflen);
		if (!strstr(buff, WIPPIENIM3))
		{
			if (strlen(buff))
				strcat(buff, " ");
			strcat(buff, WIPPIENIM3);
			strcat(buff, " ");
			strcat(buff, WIPPIENIM4);
			WODXMPPCOMLib::XMPP_SetCapabilities(m_Jabb, buff);
		}
	}

	CComBSTR2 l1 = l;
	WODXMPPCOMLib::XMPP_SetLogin(m_Jabb, l1.ToUTF8String());
	WODXMPPCOMLib::XMPP_SetPassword(m_Jabb, pass);
	if (prt)
		WODXMPPCOMLib::XMPP_SetPort(m_Jabb, prt);
	if (usessl)
		WODXMPPCOMLib::XMPP_SetSecurity(m_Jabb, (WODXMPPCOMLib::SecurityEnum)3); // security implicit
	else
		WODXMPPCOMLib::XMPP_SetSecurity(m_Jabb, (WODXMPPCOMLib::SecurityEnum)1); // security allowed

//		m_Jabb->put_Protocol = 3;

#endif
		


	m_Initial = TRUE;
#ifndef _WODXMPPLIB
	VARIANT var;
	if (h.Length())
	{
		var.vt = VT_BSTR;
		var.bstrVal = h;
	}
	else
		var.vt = VT_ERROR;
	
	if (m_Events)
		delete m_Events;
	m_Events = new CJabberEvents(this);

	m_Jabb->put_DebugFile(_Settings.m_JabberDebugFile);
	HRESULT hr;
	hr = m_Jabb->raw_Connect(var);
#else
	long hr;
	CComBSTR2 df = _Settings.m_JabberDebugFile;
	WODXMPPCOMLib::XMPP_SetDebugFile(m_Jabb, df.ToString());
	hr = WODXMPPCOMLib::XMPP_Connect(m_Jabb, hostname);
#endif

	if (FAILED(hr))
		ShowError();
}

void CJabber::Disconnect(void)
{
#ifndef _WODXMPPLIB
	m_Jabb->Disconnect();

	if (m_Events)
		delete m_Events;
	m_Events = NULL;
#else
	WODXMPPCOMLib::XMPP_Disconnect(m_Jabb);
#endif
}

#ifndef _WODXMPPLIB
void CJabber::Message(WODXMPPCOMLib::IXMPPContact *Contact, char *JID, char *MessageText, char *HtmlText)
#else
void CJabber::Message(void *Contact, char *JID, char *MessageText, char *HtmlText)
#endif
{
#ifndef _WODXMPPLIB
	CComPtr<WODXMPPCOMLib::IXMPPMessage> msg;
	msg.CoCreateInstance(__uuidof(WODXMPPCOMLib::XMPPMessage));
#else
	void *msg = WODXMPPCOMLib::XMPP_Message_New();

#endif
	BOOL sendhtml = FALSE;

	CComBSTR2 res;

#ifndef _WODXMPPLIB
	WODXMPPCOMLib::IXMPPContact *c = Contact;
	WODXMPPCOMLib::IXMPPContact *ct = NULL;
	if (!c)
	{
		CComBSTR j = JID;
		VARIANT var;
		var.vt = VT_BSTR;
		var.bstrVal = j;
		WODXMPPCOMLib::IXMPPContacts *cts = NULL;
		WODXMPPCOMLib::IXMPPContact *ct = NULL;

		if (SUCCEEDED(_Jabber->m_Jabb->get_Contacts(&cts)))
		{
			if (SUCCEEDED(cts->get_Item(var, &ct)))
			{
				c = ct;
			}
			cts->Release();
		}
	}
#else
	void *c = NULL;
	WODXMPPCOMLib::XMPP_ContactsGetContactByJID(_Jabber->m_Jabb, JID, &c);
#endif
	if (c)
	{
#ifndef _WODXMPPLIB
		// also check capabilities
		c->get_Capabilities(&res);
//		if (strstr(res.ToString(), WIPPIENIM))
//			sendhtml = TRUE;

		c->get_Resource(&res);
//		if (strstr(res.ToString(), WIPPIENIM))
//			sendhtml = TRUE;
		
#else
		char tb[1024] = {0};
		int tblen = sizeof(tb);
		WODXMPPCOMLib::XMPP_Contact_GetCapabilities(c, tb, &tblen);
//		if (strstr(tb, WIPPIENIM))
//			sendhtml = TRUE;

		if (!sendhtml)
		{
			tb[0] = 0;
			tblen = sizeof(tb);
			WODXMPPCOMLib::XMPP_Contact_GetResource(c, tb, &tblen);
//			if (strstr(tb, WIPPIENIM))
//				sendhtml = TRUE;
		}

		
		WODXMPPCOMLib::XMPP_Contacts_Free(c);
#endif
	}
#ifndef _WODXMPPLIB
	if (ct)
		ct->Release();
#endif
	
#ifndef _WODXMPPLIB
	CComBSTR t = MessageText;
	msg->put_Type((WODXMPPCOMLib::MessageTypesEnum)1/*MsgChat*/);
	msg->put_Text(t);
	if (/*sendhtml*/0)
	{
		CComBSTR ht = HtmlText;
		msg->put_HTMLText(ht);
	}
#else
	WODXMPPCOMLib::XMPP_Message_SetType(msg, (WODXMPPCOMLib::MessageTypesEnum)1);
	WODXMPPCOMLib::XMPP_Message_SetText(msg, MessageText);
	if (/*sendhtml*/0)
	{
		WODXMPPCOMLib::XMPP_Message_SetHTMLText(msg, HtmlText);
	}
#endif

	HRESULT hr = S_OK;
	if (Contact)
	{
#ifndef _WODXMPPLIB
		hr = Contact->raw_SendMessage(msg);
#else
		hr = WODXMPPCOMLib::XMPP_Contact_SendMessage(Contact, msg);
#endif
	}
	else
	{
#ifndef _WODXMPPLIB
		CComBSTR j(JID);
		hr = m_Jabb->raw_SendMessage(j, msg);
#else
		hr = WODXMPPCOMLib::XMPP_SendMessage(m_Jabb, JID, msg);
#endif
	}

	if (FAILED(hr))
	{
		ShowError();
	}
#ifdef _WODXMPPLIB
	WODXMPPCOMLib::XMPP_Message_Free(msg);
#endif
}

#ifndef _WODXMPPLIB
void CJabber::ChatRoomMessage(WODXMPPCOMLib::IXMPPChatRoom *ChatRoom, char *MessageText, char *HtmlText)
#else
void CJabber::ChatRoomMessage(void *ChatRoom, char *MessageText, char *HtmlText)
#endif
{
#ifndef _WODXMPPLIB
	CComPtr<WODXMPPCOMLib::IXMPPMessage> msg;
	msg.CoCreateInstance(__uuidof(WODXMPPCOMLib::XMPPMessage));
#else
	void *msg = WODXMPPCOMLib::XMPP_Message_New();

#endif

#ifndef _WODXMPPLIB
	CComBSTR t = MessageText;
	msg->put_Type((WODXMPPCOMLib::MessageTypesEnum)2/*MsgGroupChat*/);
	msg->put_Text(t);
	if (0)
	{
		CComBSTR ht = HtmlText;
		msg->put_HTMLText(ht);
	}

#else
	WODXMPPCOMLib::XMPP_Message_SetType(msg, (WODXMPPCOMLib::MessageTypesEnum)2);
	WODXMPPCOMLib::XMPP_Message_SetText(msg, MessageText);
	if (0)
	{
		WODXMPPCOMLib::XMPP_Message_SetHTMLText(msg, HtmlText);
	}
#endif

	HRESULT hr = E_FAIL;
	if (ChatRoom)
	{
#ifndef _WODXMPPLIB
		hr = ChatRoom->raw_SendMessage(msg);
#else
		hr = WODXMPPCOMLib::XMPP_ChatRoom_SendMessage(ChatRoom, msg);
#endif
	}
	if (FAILED(hr))
	{
		ShowError();
	}
#ifdef _WODXMPPLIB
	WODXMPPCOMLib::XMPP_Message_Free(msg);
#endif
}

void CJabber::ExchangeWippienDetails(CUser *User, char *Subj, Buffer *Text)
{
#ifndef _WODXMPPLIB
	CComPtr<WODXMPPCOMLib::IXMPPMessage> msg;
	msg.CoCreateInstance(__uuidof(WODXMPPCOMLib::XMPPMessage));
#else
	void *msg = WODXMPPCOMLib::XMPP_Message_New();
#endif


#ifndef _WODXMPPLIB
	msg->put_Type((WODXMPPCOMLib::MessageTypesEnum)/*WODXMPPCOMLib::MessageTypesEnum::MsgNormal*/User->m_IsUsingWippien4?0:3);
	CComBSTR t = Subj;
	msg->put_Subject(t);
	CComBSTR thr = "ExchangeDetailsThread";
	msg->put_Thread(thr);
#else
	WODXMPPCOMLib::XMPP_Message_SetType(msg, (WODXMPPCOMLib::MessageTypesEnum)(User->m_IsUsingWippien4?0:3));
	WODXMPPCOMLib::XMPP_Message_SetSubject(msg, Subj);
	WODXMPPCOMLib::XMPP_Message_SetThread(msg, "ExchangeDetailsThread");
#endif



	if (Text && Text->Len())
	{
		Buffer out;
		if (User->m_IsAlienWippien)
		{
			out.Append("[");
			out.Append(Subj);
			out.Append(" ");
		}
		_Settings.ToHex(Text, &out);
		if (User->m_IsAlienWippien)
		{
			out.Append("]");
		}
		out.Append("\0", 1);
#ifndef _WODXMPPLIB
		CComBSTR t1 = out.Ptr();
		msg->put_Text(t1);
#else
		WODXMPPCOMLib::XMPP_Message_SetText(msg, out.Ptr());
#endif
	}

	if (User->m_IsUsingWippien3)
	{
#ifndef _WODXMPPLIB
	CComBSTR j5 = User->m_IsWippien->Ptr();
	HRESULT hr = _Jabber->m_Jabb->raw_SendMessage(j5, msg);
#else
	Buffer bf;
	bf.Append(User->m_JID);
	if (*User->m_Resource)
	{
		bf.Append("/");
		bf.Append(User->m_Resource);
	}
	HRESULT hr = WODXMPPCOMLib::XMPP_SendMessage(_Jabber->m_Jabb, bf.Ptr(), msg);
#endif
		if (FAILED(hr))
		{
			ShowError();
		}
	}

#ifdef _WODXMPPLIB
	WODXMPPCOMLib::XMPP_Message_Free(msg);
#endif


}

void CJabber::ShowError(void)
{
#ifndef _WODXMPPLIB
	CComBSTR2 text;
	_Jabber->m_Jabb->get_LastErrorText(&text);
	if (text.Length())
	{
		_MainDlg.ShowStatusText(text.ToString());
	}
#else
	char t[1024];
	int tlen = sizeof(t);
	WODXMPPCOMLib::XMPP_GetLastErrorText(_Jabber->m_Jabb, t, &tlen);
	if (tlen)
	{
		_MainDlg.ShowStatusText(t);
	}
#endif

}