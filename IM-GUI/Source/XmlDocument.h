#pragma once
/*
* 头文件中，不能定义全局变量（除非加extern） 和 实现静态成员变量
*/

#import "msxml3.dll"//COM专用的 属性包括no_namespace rename("EOF","adoEFO")
//import "libid:xxxxxx-xxxx-xxxx-xxxxxxxx" version("3.0") lcid("4")
//dll ocx tlb olb  可以是相对路径，也可以是绝对路径 
//LIB
#include <msxml2.h>
#include <string>

//不同版本的项目之间切换的时候，不用改代码
#if defined(UNICODE) || defined(_UNICODE)
	typedef std::wstring tstring;
#else
	typedef std::string tstring;
#endif

class CXmlNodeList;//预声明，告知编译器，该定义会提前使用
//使用形式只有两种：引用、指针

class CXmlNode
{
public:
	CXmlNode(void);
	~CXmlNode(void);

public:
	BOOL SelectSingleNode(LPCTSTR pNodeName, CXmlNode& objXmlNode);  // 在节点对象中查询单个节点
	BOOL SelectNodes(LPCTSTR pNodeName, CXmlNodeList& objXmlNodeList);// 查询多个节点
	BOOL GetFirstChildNode(LPCTSTR pNodeName, CXmlNode& objXmlNode);
	BOOL GetNextSiblingNode(LPCTSTR pNodeName, CXmlNode& objXmlNode);
	BOOL GetFirstChildNode(CXmlNode& objXmlNode);
	BOOL GetNextSiblingNode(CXmlNode& objXmlNode);
	std::wstring GetNodeName();  // 获取节点名字
	std::wstring GetText();      // 获取文本
	int GetTextInt();
	std::wstring GetAttribute(LPCTSTR lpAttributeName);
	int GetAttributeInt(LPCTSTR lpAttributeName);
	void Release();
	void Attach(IXMLDOMNode* pXMLNode);  // 把xml指针绑定到类对象上
	IXMLDOMNode* Detach();               // 解绑

private:
	IXMLDOMNode* m_pXMLNode;
};

class CXmlNodeList
{
public:
	CXmlNodeList(void);
	~CXmlNodeList(void);

public:
	int GetLength();
	BOOL GetItem(int nIndex, CXmlNode& objXmlNode);
	void Release();
	void Attach(IXMLDOMNodeList* pXMLNodeList);
	IXMLDOMNodeList* Detach();

private:
	IXMLDOMNodeList* m_pXMLNodeList;
};

class CXmlDocument
{
public:
	CXmlDocument(void);
	~CXmlDocument(void);

public:
	BOOL Load(LPCTSTR pPath);
	BOOL LoadXml(LPCTSTR pXml);
	BOOL SelectSingleNode(LPCTSTR pNodeName, CXmlNode& objXmlNode);
	BOOL SelectNodes(LPCTSTR pNodeName, CXmlNodeList& objXmlNodeList);
	void Release();

private:
	IXMLDOMDocument2* m_pXMLDoc;
};
