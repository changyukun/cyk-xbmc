/*
 *      Copyright (C) 2005-2011 Team XBMC
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

#include "GUIAction.h"
#include "utils/StringUtils.h"
#include "GUIWindowManager.h"
#include "GUIControl.h"
#include "GUIInfoManager.h"

using namespace std;

CGUIAction::CGUIAction()
{
/*
	����:
		1��

	����:
		1��

	˵��:
		1��
*/
  	m_sendThreadMessages = false;
}

bool CGUIAction::Execute(int controlID, int parentID, int direction /*= 0*/) const
{
/*
	����:
		1��

	����:
		1��

	˵��:
		1���˺�����m_actions  �����е����е�Ԫ�����д�����
*/
	if (m_actions.size() == 0) 
		return false;
	
	bool retval = false;
	
	CGUIAction copy(*this); /* �Ե���ʵ����һ�����ƣ��൱���ĸ�CGUIAction  ʵ�����ô˷�����copy �����ĸ�ʵ��*/
	
	for (ciActions it = copy.m_actions.begin() ; it != copy.m_actions.end() ; it++)/* ���������ڵ�ÿ����Ԫ��ÿ����Ԫ��Ϊһ�������붯����ƥ���*/
	{
		if (it->condition.IsEmpty() || g_infoManager.EvaluateBool(it->condition))
		{
			if (StringUtils::IsInteger(it->action))/* ��һ���ƶ���������Ϣ��������Ӧ�Ĵ��ڷ����ƶ�����Ϣ*/
			{
				CGUIMessage msg(GUI_MSG_MOVE, parentID, controlID, direction);
				if (parentID)/* ���������id ��Ч���򸸴��ڷ��;���*/
				{
					CGUIWindow *pWindow = g_windowManager.GetWindow(parentID);
					if (pWindow)
					{
						retval |= pWindow->OnMessage(msg);
						continue;
					}
				}
				retval |= g_windowManager.SendMessage(msg);
			}
			else /* ��һ��ִ�ж�������Ϣ��������Ӧ�Ĵ��ڷ���ִ�е���Ϣ*/
			{
				CGUIMessage msg(GUI_MSG_EXECUTE, controlID, parentID);
				msg.SetStringParam(it->action);
				
				if (m_sendThreadMessages)
					g_windowManager.SendThreadMessage(msg);
				else
					g_windowManager.SendMessage(msg);
				
				retval |= true;
			}
		}
	}
	
	return retval;
}

int CGUIAction::GetNavigation() const
{
/*
	����:
		1��

	����:
		1��

	˵��:
		1��
*/
	for (ciActions it = m_actions.begin() ; it != m_actions.end() ; it++)
	{
		if (StringUtils::IsInteger(it->action))/* ��һ������*/
		{
			if (it->condition.IsEmpty() || g_infoManager.EvaluateBool(it->condition))
				return atoi(it->action.c_str());
		}
	}
	return 0;
}

void CGUIAction::SetNavigation(int id)
{
/*
	����:
		1��

	����:
		1��

	˵��:
		1���趨һ��������id����������ӵ�����m_actions  ��
*/
	if (id == 0) 
		return;
	
	CStdString strId;
	strId.Format("%i", id);
	for (iActions it = m_actions.begin() ; it != m_actions.end() ; it++)
	{
		if (StringUtils::IsInteger(it->action) && it->condition.IsEmpty())
		{
			it->action = strId;
			return;
		}
	}
	cond_action_pair pair;
	pair.action = strId;
	m_actions.push_back(pair);
}

bool CGUIAction::HasActionsMeetingCondition() const
{
/*
	����:
		1��

	����:
		1��

	˵��:
		1��
*/
	for (ciActions it = m_actions.begin() ; it != m_actions.end() ; it++)
	{
		if (it->condition.IsEmpty() || g_infoManager.EvaluateBool(it->condition))
			return true;
	}
	return false;
}
