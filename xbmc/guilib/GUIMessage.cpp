/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "GUIMessage.h"
#include "LocalizeStrings.h"

using namespace std;

CStdString CGUIMessage::empty_string;

CGUIMessage::CGUIMessage(int msg, int senderID, int controlID, int param1, int param2)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	m_message = msg;
	m_senderID = senderID;
	m_controlID = controlID;
	m_param1 = param1;
	m_param2 = param2;
	m_pointer = NULL;
}

CGUIMessage::CGUIMessage(int msg, int senderID, int controlID, int param1, int param2, CFileItemList *item)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	m_message = msg;
	m_senderID = senderID;
	m_controlID = controlID;
	m_param1 = param1;
	m_param2 = param2;
	m_pointer = item;
}

CGUIMessage::CGUIMessage(int msg, int senderID, int controlID, int param1, int param2, const CGUIListItemPtr &item)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	m_message = msg;
	m_senderID = senderID;
	m_controlID = controlID;
	m_param1 = param1;
	m_param2 = param2;
	m_pointer = NULL;
	m_item = item;
}

CGUIMessage::CGUIMessage(const CGUIMessage& msg)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	*this = msg;
}

CGUIMessage::~CGUIMessage(void)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
}


int CGUIMessage::GetControlId() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return m_controlID;
}

int CGUIMessage::GetMessage() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return m_message;
}

void* CGUIMessage::GetPointer() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return m_pointer;
}

CGUIListItemPtr CGUIMessage::GetItem() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return m_item;
}

int CGUIMessage::GetParam1() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return m_param1;
}

int CGUIMessage::GetParam2() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return m_param2;
}

int CGUIMessage::GetSenderId() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return m_senderID;
}


const CGUIMessage& CGUIMessage::operator = (const CGUIMessage& msg)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	if (this == &msg) 
		return * this;

	m_message = msg.m_message;
	m_controlID = msg.m_controlID;
	m_param1 = msg.m_param1;
	m_param2 = msg.m_param2;
	m_pointer = msg.m_pointer;
	m_strLabel = msg.m_strLabel;
	m_senderID = msg.m_senderID;
	m_params = msg.m_params;
	m_item = msg.m_item;
	return *this;
}


void CGUIMessage::SetParam1(int param1)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	m_param1 = param1;
}

void CGUIMessage::SetParam2(int param2)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	m_param2 = param2;
}

void CGUIMessage::SetPointer(void* lpVoid)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	m_pointer = lpVoid;
}

void CGUIMessage::SetLabel(const string& strLabel)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	m_strLabel = strLabel;
}

const string& CGUIMessage::GetLabel() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return m_strLabel;
}

void CGUIMessage::SetLabel(int iString)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	m_strLabel = g_localizeStrings.Get(iString);
}

void CGUIMessage::SetStringParam(const CStdString& strParam)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	m_params.clear();
	if (strParam.size())
		m_params.push_back(strParam);
}

void CGUIMessage::SetStringParams(const vector<CStdString> &params)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	m_params = params;
}

const CStdString& CGUIMessage::GetStringParam(size_t param) const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	if (param >= m_params.size())
		return empty_string;
	return m_params[param];
}

size_t CGUIMessage::GetNumStringParams() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return m_params.size();
}
