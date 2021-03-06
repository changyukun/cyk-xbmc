/*!
\file GUIWindowManager.h
\brief
*/

#ifndef GUILIB_CGUIWindowManager_H
#define GUILIB_CGUIWindowManager_H

#pragma once

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

#include "GUIWindow.h"
#include "IWindowManagerCallback.h"
#include "IMsgTargetCallback.h"
#include "DirtyRegionTracker.h"

class CGUIDialog;

#define WINDOW_ID_MASK 0xffff

/*!
 \ingroup winman
 \brief
 */
class CGUIWindowManager
{
public:
	CGUIWindowManager(void);
	virtual ~CGUIWindowManager(void);
	bool SendMessage(CGUIMessage& message);
	bool SendMessage(int message, int senderID, int destID, int param1 = 0, int param2 = 0);
	bool SendMessage(CGUIMessage& message, int window);
	void Initialize();
	void Add(CGUIWindow* pWindow);
	void AddUniqueInstance(CGUIWindow *window);
	void AddCustomWindow(CGUIWindow* pWindow);
	void Remove(int id);
	void Delete(int id);
	void ActivateWindow(int iWindowID, const CStdString &strPath = "");
	void ChangeActiveWindow(int iNewID, const CStdString &strPath = "");
	void ActivateWindow(int iWindowID, const std::vector<CStdString>& params, bool swappingWindows = false);
	void PreviousWindow();

	void CloseDialogs(bool forceClose = false);

	// OnAction() runs through our active dialogs and windows and sends the message
	// off to the callbacks (application, python, playlist player) and to the
	// currently focused window(s).  Returns true only if the message is handled.
	bool OnAction(const CAction &action);

	/*! \brief Process active controls allowing them to animate before rendering.
	*/
	void Process(unsigned int currentTime);

	/*! \brief Mark the screen as dirty, forcing a redraw at the next Render()
	*/
	void MarkDirty();

	/*! \brief Get the current dirty region
	*/
	CDirtyRegionList GetDirty() { return m_tracker.GetDirtyRegions(); }

	/*! \brief Rendering of the current window and any dialogs
	Render is called every frame to draw the current window and any dialogs.
	It should only be called from the application thread.
	Returns true only if it has rendered something.
	*/
	bool Render();

	/*! \brief Per-frame updating of the current window and any dialogs
	FrameMove is called every frame to update the current window and any dialogs
	on screen. It should only be called from the application thread.
	*/
	void FrameMove();

	/*! \brief Return whether the window manager is initialized.
	The window manager is initialized on skin load - if the skin isn't yet loaded,
	no windows should be able to be initialized.
	\return true if the window manager is initialized, false otherwise.
	*/
	bool Initialized() const { return m_initialized; };

	CGUIWindow* GetWindow(int id) const;
	void ProcessRenderLoop(bool renderOnly = false);
	void SetCallback(IWindowManagerCallback& callback);
	void DeInitialize();

	void RouteToWindow(CGUIWindow* dialog);
	void AddModeless(CGUIWindow* dialog);
	void RemoveDialog(int id);
	int GetTopMostModalDialogID(bool ignoreClosing = false) const;

	void SendThreadMessage(CGUIMessage& message);
	void SendThreadMessage(CGUIMessage& message, int window);
	void DispatchThreadMessages();
	void AddMsgTarget( IMsgTargetCallback* pMsgTarget );
	int GetActiveWindow() const;
	int GetFocusedWindow() const;
	bool HasModalDialog() const;
	bool HasDialogOnScreen() const;
	bool IsWindowActive(int id, bool ignoreClosing = true) const;
	bool IsWindowVisible(int id) const;
	bool IsWindowTopMost(int id) const;
	bool IsWindowActive(const CStdString &xmlFile, bool ignoreClosing = true) const;
	bool IsWindowVisible(const CStdString &xmlFile) const;
	bool IsWindowTopMost(const CStdString &xmlFile) const;
	bool IsOverlayAllowed() const;
	void ShowOverlay(CGUIWindow::OVERLAY_STATE state);
	void GetActiveModelessWindows(std::vector<int> &ids);
#ifdef _DEBUG
	void DumpTextureUse();
#endif
private:
	void RenderPass();

	void LoadNotOnDemandWindows();
	void UnloadNotOnDemandWindows();
	void HideOverlay(CGUIWindow::OVERLAY_STATE state);
	void AddToWindowHistory(int newWindowID);
	void ClearWindowHistory();
	void CloseWindowSync(CGUIWindow *window, int nextWindowID = 0);
	CGUIWindow *GetTopMostDialog() const;

	friend class CApplicationMessenger;
	void ActivateWindow_Internal(int windowID, const std::vector<CStdString> &params, bool swappingWindows);

	typedef std::map<int, CGUIWindow *> WindowMap; /* 将id 号与window 窗体实例实现一个映射，构成一个类似表的形式*/
	WindowMap m_mapWindows; /* 相当于所有窗体的容器，调用delete /Remove  方法的窗体从此容器中清除，然后添加到了m_deleteWindows 容器中*/
	std::vector <CGUIWindow*> m_vecCustomWindows; /* 保存用户添加的窗体的容器，见方法AddCustomWindow */
	std::vector <CGUIWindow*> m_activeDialogs;
	std::vector <CGUIWindow*> m_deleteWindows; /* 已经删除的窗体构成一个容器*/
	typedef std::vector<CGUIWindow*>::iterator iDialog;
	typedef std::vector<CGUIWindow*>::const_iterator ciDialog;
	typedef std::vector<CGUIWindow*>::reverse_iterator rDialog;
	typedef std::vector<CGUIWindow*>::const_reverse_iterator crDialog;

	std::stack<int> m_windowHistory; /* 窗体操作栈，即窗体操作的历史记录，即窗体被激活先后顺序的容器，保存的是窗体的id 号*/

	IWindowManagerCallback* m_pCallback; /* 见方法SetCallback 的调用*/
	
	std::vector < std::pair<CGUIMessage*,int> > m_vecThreadMessages; /* 用于保存线程相关信息的容器，具体见方法SendThreadMessage 及DispatchThreadMessages  的说明*/
	CCriticalSection m_critSection;
	
	std::vector <IMsgTargetCallback*> m_vecMsgTargets; /* 	此容器内保存的为所有为非窗体类、并且需要接收gui 消息
														的实例，通过方法CGUIWindowManager::AddMsgTarget() 向其中添加实例
														的。

														此容器内的所有实例如何得到gui 消息的，详见CGUIWindowManager::SendMessage
														，即当gui 调用CGUIWindowManager::SendMessage 发送消息的时候就会遍历
														此容器内的所有实例，然后调用每个实例的OnMessage 方法
													*/

	bool m_bShowOverlay;
	int  m_iNested;
	bool m_initialized;

	CDirtyRegionTracker m_tracker;
};

/*!
 \ingroup winman
 \brief
 */
extern CGUIWindowManager g_windowManager;
#endif

