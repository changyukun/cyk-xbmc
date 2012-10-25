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
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	m_sendThreadMessages = false;
}

bool CGUIAction::Execute(int controlID, int parentID, int direction /*= 0*/) const
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、此函数对m_actions  容器中的所有单元都进行处理了
*/
	if (m_actions.size() == 0) 
		return false;
	
	bool retval = false;
	
	CGUIAction copy(*this); /* 对调用实例的一个复制，相当于哪个CGUIAction  实例调用此方法，copy 就是哪个实例*/
	
	for (ciActions it = copy.m_actions.begin() ; it != copy.m_actions.end() ; it++)/* 遍历容器内的每个单元，每个单元都为一个条件与动作的匹配对*/
	{
		if (it->condition.IsEmpty() || g_infoManager.EvaluateBool(it->condition))
		{
			if (StringUtils::IsInteger(it->action))/* 是一个移动动作的消息，则向相应的窗口发送移动的消息*/
			{
				CGUIMessage msg(GUI_MSG_MOVE, parentID, controlID, direction);
				if (parentID)/* 如果父窗口id 有效则向父窗口发送就行*/
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
			else /* 是一个执行动作的消息，则向相应的窗口发送执行的消息*/
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
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	for (ciActions it = m_actions.begin() ; it != m_actions.end() ; it++)
	{
		if (StringUtils::IsInteger(it->action))/* 是一个动作*/
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
	参数:
		1、

	返回:
		1、

	说明:
		1、设定一个动作的id，并将其添加到容器m_actions  中
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
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	for (ciActions it = m_actions.begin() ; it != m_actions.end() ; it++)
	{
		if (it->condition.IsEmpty() || g_infoManager.EvaluateBool(it->condition))
			return true;
	}
	return false;
}
