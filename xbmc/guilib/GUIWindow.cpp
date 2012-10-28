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

#include "system.h"
#include "GUIWindow.h"
#include "GUIWindowManager.h"
#include "Key.h"
#include "LocalizeStrings.h"
#include "GUIControlFactory.h"
#include "GUIControlGroup.h"
#include "GUIControlProfiler.h"
#include "settings/Settings.h"
#ifdef PRE_SKIN_VERSION_9_10_COMPATIBILITY
#include "GUIEditControl.h"
#endif

#include "addons/Skin.h"
#include "GUIInfoManager.h"
#include "utils/log.h"
#include "threads/SingleLock.h"
#include "utils/TimeUtils.h"
#include "input/ButtonTranslator.h"
#include "utils/XMLUtils.h"
#include "GUIAudioManager.h"
#include "Application.h"
#include "utils/Variant.h"

#ifdef HAS_PERFORMANCE_SAMPLE
#include "utils/PerformanceSample.h"
#endif

using namespace std;

CGUIWindow::CGUIWindow(int id, const CStdString &xmlFile)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	SetID(id);
	SetProperty("xmlfile", xmlFile);
	m_idRange = 1;
	m_lastControlID = 0;
	m_overlayState = OVERLAY_STATE_PARENT_WINDOW;   // Use parent or previous window's state
	m_isDialog = false;
	m_needsScaling = true;
	m_windowLoaded = false;
	m_loadOnDemand = true;
	m_closing = false;
	m_active = false;
	m_renderOrder = 0;
	m_dynamicResourceAlloc = true;
	m_previousWindow = WINDOW_INVALID;
	m_animationsEnabled = true;
	m_manualRunActions = false;
	m_exclusiveMouseControl = 0;
	m_clearBackground = 0xff000000; // opaque black -> always clear
}

CGUIWindow::~CGUIWindow(void)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
}

bool CGUIWindow::Load(const CStdString& strFileName, bool bContainsPath)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、加载对应此界面的xml  文件
		2、此方法可能被调用的过程
			1) CGUIWindow::AllocResources ===> 此方法
			2) CGUIWindow::Initialize() ===> 此方法
*/
#ifdef HAS_PERFORMANCE_SAMPLE
	CPerformanceSample aSample("WindowLoad-" + strFileName, true);
#endif

	if (m_windowLoaded || g_SkinInfo == NULL)
		return true;      // no point loading if it's already there

#ifdef _DEBUG
	int64_t start;
	start = CurrentHostCounter();
#endif
	CLog::Log(LOGINFO, "Loading skin file: %s", strFileName.c_str());

	// Find appropriate skin folder + resolution to load from
	CStdString strPath;
	CStdString strLowerPath;
	if (bContainsPath)
		strPath = strFileName;
	else
	{
		// FIXME: strLowerPath needs to eventually go since resToUse can get incorrectly overridden
		strLowerPath =  g_SkinInfo->GetSkinPath(CStdString(strFileName).ToLower(), &m_coordsRes);
		strPath = g_SkinInfo->GetSkinPath(strFileName, &m_coordsRes);
	}

	bool ret = LoadXML(strPath.c_str(), strLowerPath.c_str());

#ifdef _DEBUG
	int64_t end, freq;
	end = CurrentHostCounter();
	freq = CurrentHostFrequency();
	CLog::Log(LOGDEBUG,"Load %s: %.2fms", GetProperty("xmlfile").c_str(), 1000.f * (end - start) / freq);
#endif
	return ret;
}

bool CGUIWindow::LoadXML(const CStdString &strPath, const CStdString &strLowerPath)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、此函数实现了根据xml  文件进行加载

			通过tinyxml  对xml  文件进行解析，解析出来各个节点的
			内容以及相对应的数值，使用方法:

			实例一个TiXmlDocument  类，然后调用LoadFile  方法既可
*/
	TiXmlDocument xmlDoc;
	if ( !xmlDoc.LoadFile(strPath) && !xmlDoc.LoadFile(CStdString(strPath).ToLower()) && !xmlDoc.LoadFile(strLowerPath))
	{
		CLog::Log(LOGERROR, "unable to load:%s, Line %d\n%s", strPath.c_str(), xmlDoc.ErrorRow(), xmlDoc.ErrorDesc());
		SetID(WINDOW_INVALID);
		return false;
	}

	return Load(xmlDoc);
}

bool CGUIWindow::Load(TiXmlDocument &xmlDoc)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	TiXmlElement* pRootElement = xmlDoc.RootElement(); /* 取出xml  解析结果的根节点*/


	/* 如果根节点的值不是window  直接返回，即xml  描述的不是一个窗口*/
	if (strcmpi(pRootElement->Value(), "window"))
	{
		CLog::Log(LOGERROR, "file : XML file doesnt contain <window>");
		return false;
	}

	// set the scaling resolution so that any control creation or initialisation can
	// be done with respect to the correct aspect ratio
	g_graphicsContext.SetScalingResolution(m_coordsRes, m_needsScaling);

	// Resolve any includes that may be present
	g_SkinInfo->ResolveIncludes(pRootElement);
	// now load in the skin file
	SetDefaults();

	CGUIControlFactory::GetInfoColor(pRootElement, "backgroundcolor", m_clearBackground, GetID());
	CGUIControlFactory::GetActions(pRootElement, "onload", m_loadActions);
	CGUIControlFactory::GetActions(pRootElement, "onunload", m_unloadActions);
	CGUIControlFactory::GetHitRect(pRootElement, m_hitRect);


	/*
		如下遍历xml  的所有节点进行逐个分析
	*/
	
	TiXmlElement *pChild = pRootElement->FirstChildElement();
	while (pChild)
	{
		/* ===================>> 取出节点的值*/
		CStdString strValue = pChild->Value();

		/* ===================>> 根据节点的不同的值做不同的操作*/
		if (strValue == "type" && pChild->FirstChild())
		{
			// if we have are a window type (ie not a dialog), and we have <type>dialog</type>
			// then make this window act like a dialog
			if (!IsDialog() && strcmpi(pChild->FirstChild()->Value(), "dialog") == 0)
				m_isDialog = true;
		}
		else if (strValue == "previouswindow" && pChild->FirstChild())
		{
			m_previousWindow = CButtonTranslator::TranslateWindow(pChild->FirstChild()->Value());
		}
		else if (strValue == "defaultcontrol" && pChild->FirstChild())
		{
			const char *always = pChild->Attribute("always");
			if (always && strcmpi(always, "true") == 0)
				m_defaultAlways = true;
			
			m_defaultControl = atoi(pChild->FirstChild()->Value());
		}
		else if (strValue == "visible" && pChild->FirstChild())
		{
			CStdString condition;
			CGUIControlFactory::GetConditionalVisibility(pRootElement, condition);
			m_visibleCondition = g_infoManager.Register(condition, GetID());
		}
		else if (strValue == "animation" && pChild->FirstChild())
		{
			CRect rect(0, 0, (float)m_coordsRes.iWidth, (float)m_coordsRes.iHeight);
			CAnimation anim;
			anim.Create(pChild, rect, GetID());
			m_animations.push_back(anim);
		}
		else if (strValue == "zorder" && pChild->FirstChild())
		{
			m_renderOrder = atoi(pChild->FirstChild()->Value());
		}
		else if (strValue == "coordinates")
		{
			XMLUtils::GetFloat(pChild, "posx", m_posX);
			XMLUtils::GetFloat(pChild, "posy", m_posY);

			TiXmlElement *originElement = pChild->FirstChildElement("origin");
			while (originElement)
			{
				COrigin origin;
				originElement->QueryFloatAttribute("x", &origin.x);
				originElement->QueryFloatAttribute("y", &origin.y);
				
				if (originElement->FirstChild())
					origin.condition = g_infoManager.Register(originElement->FirstChild()->Value(), GetID());
				
				m_origins.push_back(origin);
				
				originElement = originElement->NextSiblingElement("origin");
			}
		}
		else if (strValue == "camera")
		{ // z is fixed
			pChild->QueryFloatAttribute("x", &m_camera.x);
			pChild->QueryFloatAttribute("y", &m_camera.y);
			m_hasCamera = true;
		}
		else if (strValue == "controls") /* 添加控件*/
		{
			TiXmlElement *pControl = pChild->FirstChildElement();
			while (pControl)
			{
				if (strcmpi(pControl->Value(), "control") == 0)
				{
					LoadControl(pControl, NULL);
				}
				pControl = pControl->NextSiblingElement();
			}
		}
		else if (strValue == "allowoverlay")
		{
			bool overlay = false;
			if (XMLUtils::GetBoolean(pRootElement, "allowoverlay", overlay))
				m_overlayState = overlay ? OVERLAY_STATE_SHOWN : OVERLAY_STATE_HIDDEN;
		}

		/* ===================>> 取出下一个节点*/
		pChild = pChild->NextSiblingElement();
		
	}
	LoadAdditionalTags(pRootElement);

	m_windowLoaded = true;
	OnWindowLoaded();
	return true;
}

void CGUIWindow::LoadControl(TiXmlElement* pControl, CGUIControlGroup *pGroup)
{
/*
	参数:
		1、pControl	: 传入一个control  的节点( 分析xml  得到的)
		2、pGroup	:

	返回:
		1、

	说明:
		1、
*/
	// get control type
	CGUIControlFactory factory;

	CRect rect(0, 0, (float)m_coordsRes.iWidth, (float)m_coordsRes.iHeight);
	
	if (pGroup)
	{
		rect.x1 = pGroup->GetXPosition();
		rect.y1 = pGroup->GetYPosition();
		rect.x2 = rect.x1 + pGroup->GetWidth();
		rect.y2 = rect.y1 + pGroup->GetHeight();
	}
	
	CGUIControl* pGUIControl = factory.Create(GetID(), rect, pControl); /* 创建一个控件，见函数内部*/
	
	if (pGUIControl)
	{
		float maxX = pGUIControl->GetXPosition() + pGUIControl->GetWidth();
		if (maxX > m_width)
		{
			m_width = maxX;
		}

		float maxY = pGUIControl->GetYPosition() + pGUIControl->GetHeight();
		if (maxY > m_height)
		{
			m_height = maxY;
		}

		/* 
			如果是一个控件组，则将此控件添加到控
			件组，否则添加到窗体中
		*/
		// if we are in a group, add to the group, else add to our window
		if (pGroup)
			pGroup->AddControl(pGUIControl);
		else
			AddControl(pGUIControl);

		
		// if the new control is a group, then add it's controls
		/*
			注意IsGroup  方法的实现

			基类CGUIControl  中对此方法实现返回为false
			类CGUIControlGroup  中对此方法实现返回为true

			因此在CGUIControlFactory::Create  中new  的各个控件的时候，如果那个
			控件是继承基类CGUIControl  的，则IsGroup  方法返回为false，如果是
			继承CGUIControlGroup  类的，则IsGroup  方法返回的为true
		*/
		if (pGUIControl->IsGroup())
		{
			TiXmlElement *pSubControl = pControl->FirstChildElement("control");
			while (pSubControl)
			{
				LoadControl(pSubControl, (CGUIControlGroup *)pGUIControl);
				pSubControl = pSubControl->NextSiblingElement("control");
			}
		}
	}
}

void CGUIWindow::OnWindowLoaded()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	DynamicResourceAlloc(true);
}

void CGUIWindow::CenterWindow()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	m_posX = (m_coordsRes.iWidth - GetWidth()) / 2;
	m_posY = (m_coordsRes.iHeight - GetHeight()) / 2;
}

void CGUIWindow::DoProcess(unsigned int currentTime, CDirtyRegionList &dirtyregions)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	g_graphicsContext.SetRenderingResolution(m_coordsRes, m_needsScaling);
	unsigned int size = g_graphicsContext.AddGUITransform();
	CGUIControlGroup::DoProcess(currentTime, dirtyregions);
	if (size != g_graphicsContext.RemoveTransform())
		CLog::Log(LOGERROR, "Unbalanced UI transforms (was %d)", size);
}

void CGUIWindow::DoRender()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	// If we're rendering from a different thread, then we should wait for the main
	// app thread to finish AllocResources(), as dynamic resources (images in particular)
	// will try and be allocated from 2 different threads, which causes nasty things
	// to occur.
	if (!m_bAllocated) 
		return;

	g_graphicsContext.SetRenderingResolution(m_coordsRes, m_needsScaling);

	unsigned int size = g_graphicsContext.AddGUITransform();
	CGUIControlGroup::DoRender();
	if (size != g_graphicsContext.RemoveTransform())
		CLog::Log(LOGERROR, "Unbalanced UI transforms (was %d)", size);

	if (CGUIControlProfiler::IsRunning()) 
		CGUIControlProfiler::Instance().EndFrame();
}

void CGUIWindow::Render()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CGUIControlGroup::Render();
	// Check to see if we should close at this point
	// We check after the controls have finished rendering, as we may have to close due to
	// the controls rendering after the window has finished it's animation
	// we call the base class instead of this class so that we can find the change
	if (m_closing && !CGUIControlGroup::IsAnimating(ANIM_TYPE_WINDOW_CLOSE))
		Close(true);
}

void CGUIWindow::Close_Internal(bool forceClose /*= false*/, int nextWindowID /*= 0*/, bool enableSound /*= true*/)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CSingleLock lock(g_graphicsContext);

	if (!m_active)
		return;

	forceClose |= (nextWindowID == WINDOW_FULLSCREEN_VIDEO);
	if (!forceClose && HasAnimation(ANIM_TYPE_WINDOW_CLOSE))
	{
		if (!m_closing)
		{
			if (enableSound && IsSoundEnabled())
				g_audioManager.PlayWindowSound(GetID(), SOUND_DEINIT);

			// Perform the window out effect
			QueueAnimation(ANIM_TYPE_WINDOW_CLOSE);
			m_closing = true;
		}
		return;
	}

	m_closing = false;
	CGUIMessage msg(GUI_MSG_WINDOW_DEINIT, 0, 0);
	OnMessage(msg);
}

void CGUIWindow::Close(bool forceClose /*= false*/, int nextWindowID /*= 0*/, bool enableSound /*= true*/)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (!g_application.IsCurrentThread())
	{
		// make sure graphics lock is not held
		CSingleExit leaveIt(g_graphicsContext);
		g_application.getApplicationMessenger().Close(this, forceClose, true, nextWindowID, enableSound);
	}
	else
		Close_Internal(forceClose, nextWindowID, enableSound);
}

bool CGUIWindow::OnAction(const CAction &action)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (action.IsMouse() || action.IsGesture())
		return EVENT_RESULT_UNHANDLED != OnMouseAction(action);

	CGUIControl *focusedControl = GetFocusedControl();
	if (focusedControl)
	{
		if (focusedControl->OnAction(action))
			return true;
	}
	else
	{
		// no control has focus?
		// set focus to the default control then
		CGUIMessage msg(GUI_MSG_SETFOCUS, GetID(), m_defaultControl);
		OnMessage(msg);
	}

	// default implementations
	if (action.GetID() == ACTION_NAV_BACK || action.GetID() == ACTION_PREVIOUS_MENU)
		return OnBack(action.GetID());

	return false;
}

CPoint CGUIWindow::GetPosition() const
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	for (unsigned int i = 0; i < m_origins.size(); i++)
	{
		// no condition implies true
		if (!m_origins[i].condition || g_infoManager.GetBoolValue(m_origins[i].condition))
		{ // found origin
			return CPoint(m_origins[i].x, m_origins[i].y);
		}
	}
	return CGUIControlGroup::GetPosition();
}

// OnMouseAction - called by OnAction()
EVENT_RESULT CGUIWindow::OnMouseAction(const CAction &action)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	g_graphicsContext.SetScalingResolution(m_coordsRes, m_needsScaling);
	CPoint mousePoint(action.GetAmount(0), action.GetAmount(1));
	g_graphicsContext.InvertFinalCoords(mousePoint.x, mousePoint.y);

	// create the mouse event
	CMouseEvent event(action.GetID(), action.GetHoldTime(), action.GetAmount(2), action.GetAmount(3));
	if (m_exclusiveMouseControl)
	{
		CGUIControl *child = (CGUIControl *)GetControl(m_exclusiveMouseControl);
		if (child)
		{
			CPoint renderPos = child->GetRenderPosition() - CPoint(child->GetXPosition(), child->GetYPosition());
			return child->OnMouseEvent(mousePoint - renderPos, event);
		}
	}

	UnfocusFromPoint(mousePoint);

	return SendMouseEvent(mousePoint, event);
}

EVENT_RESULT CGUIWindow::OnMouseEvent(const CPoint &point, const CMouseEvent &event)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (event.m_id == ACTION_MOUSE_RIGHT_CLICK)
	{ // no control found to absorb this click - go to previous menu
		return OnAction(CAction(ACTION_PREVIOUS_MENU)) ? EVENT_RESULT_HANDLED : EVENT_RESULT_UNHANDLED;
	}
	return EVENT_RESULT_UNHANDLED;
}

/// \brief Called on window open.
///  * Restores the control state(s)
///  * Sets initial visibility of controls
///  * Queue WindowOpen animation
///  * Set overlay state
/// Override this function and do any window-specific initialisation such
/// as filling control contents and setting control focus before
/// calling the base method.
void CGUIWindow::OnInitWindow()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	//  Play the window specific init sound
	if (IsSoundEnabled()) /* 窗口初始化时是否播放音乐*/
		g_audioManager.PlayWindowSound(GetID(), SOUND_INIT);

	// set our rendered state
	m_hasRendered = false;
	m_closing = false;
	m_active = true;
	ResetAnimations();  // we need to reset our animations as those windows that don't dynamically allocate
                      	// need their anims reset. An alternative solution is turning off all non-dynamic
                      	// allocation (which in some respects may be nicer, but it kills hdd spindown and the like)

	// set our initial control visibility before restoring control state and
	// focusing the default control, and again afterward to make sure that
	// any controls that depend on the state of the focused control (and or on
	// control states) are active.
	SetInitialVisibility();
	RestoreControlStates();
	SetInitialVisibility();
	QueueAnimation(ANIM_TYPE_WINDOW_OPEN);
	g_windowManager.ShowOverlay(m_overlayState);

	if (!m_manualRunActions)
	{
		RunLoadActions();
	}
}

// Called on window close.
//  * Saves control state(s)
// Override this function and call the base class before doing any dynamic memory freeing
void CGUIWindow::OnDeinitWindow(int nextWindowID)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (!m_manualRunActions)
	{
		RunUnloadActions();
	}

	SaveControlStates();
	m_active = false;
}

bool CGUIWindow::OnMessage(CGUIMessage& message)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	switch ( message.GetMessage() )
	{
		case GUI_MSG_WINDOW_INIT:
			{
				CLog::Log(LOGDEBUG, "------ Window Init (%s) ------", GetProperty("xmlfile").c_str());
				
				if (m_dynamicResourceAlloc || !m_bAllocated) 
					AllocResources(); /* 见函数内部会间接调用分析xml 创建各个控件等等操作*/
				
				OnInitWindow();
				return true;
			}
			break;

		case GUI_MSG_WINDOW_DEINIT:
			{
				CLog::Log(LOGDEBUG, "------ Window Deinit (%s) ------", GetProperty("xmlfile").c_str());
				OnDeinitWindow(message.GetParam1());
				
				// now free the window
				if (m_dynamicResourceAlloc) 
					FreeResources();
				
				return true;
			}
			break;

		case GUI_MSG_CLICKED:
			{
				// a specific control was clicked
				CLICK_EVENT clickEvent = m_mapClickEvents[ message.GetSenderId() ];

				// determine if there are any handlers for this event
				if (clickEvent.HasAHandler())
				{
					// fire the message to all handlers
					clickEvent.Fire(message);
				}
				break;
			}

		case GUI_MSG_SELCHANGED:
			{
				// a selection within a specific control has changed
				SELECTED_EVENT selectedEvent = m_mapSelectedEvents[ message.GetSenderId() ];

				// determine if there are any handlers for this event
				if (selectedEvent.HasAHandler())
				{
					// fire the message to all handlers
					selectedEvent.Fire(message);
				}
				break;
			}
		
		case GUI_MSG_FOCUSED:
			{ // a control has been focused
				if (HasID(message.GetSenderId()))
				{
					m_focusedControl = message.GetControlId();
					return true;
				}
				break;
			}
		
		case GUI_MSG_LOSTFOCUS:
			{
				// nothing to do at the window level when we lose focus
				return true;
			}
		
		case GUI_MSG_MOVE:
			{
				if (HasID(message.GetSenderId()))
					return OnMove(message.GetControlId(), message.GetParam1());
				break;
			}
		
		case GUI_MSG_SETFOCUS:
			{
				//      CLog::Log(LOGDEBUG,"set focus to control:%i window:%i (%i)\n", message.GetControlId(),message.GetSenderId(), GetID());
				if ( message.GetControlId() )
				{
					// first unfocus the current control
					CGUIControl *control = GetFocusedControl();
					if (control)
					{
						CGUIMessage msgLostFocus(GUI_MSG_LOSTFOCUS, GetID(), control->GetID(), message.GetControlId());
						control->OnMessage(msgLostFocus);
					}

					// get the control to focus
					CGUIControl* pFocusedControl = GetFirstFocusableControl(message.GetControlId());
					if (!pFocusedControl)
						pFocusedControl = (CGUIControl *)GetControl(message.GetControlId());

					// and focus it
					if (pFocusedControl)
						return pFocusedControl->OnMessage(message);
				}
				return true;
			}
			break;
			
		case GUI_MSG_EXCLUSIVE_MOUSE:
			{
				m_exclusiveMouseControl = message.GetSenderId();
				return true;
			}
			break;
			
		case GUI_MSG_GESTURE_NOTIFY:
			{
				CAction action(ACTION_GESTURE_NOTIFY, 0, (float)message.GetParam1(), (float)message.GetParam2(), 0, 0);
				EVENT_RESULT result = OnMouseAction(action);
				message.SetParam1(result);
				return result != EVENT_RESULT_UNHANDLED;
			}
		
		case GUI_MSG_ADD_CONTROL:
			{
				if (message.GetPointer())
				{
					CGUIControl *control = (CGUIControl *)message.GetPointer();
					control->AllocResources();
					AddControl(control);
				}
				return true;
			}
		
		case GUI_MSG_REMOVE_CONTROL:
			{
				if (message.GetPointer())
				{
					CGUIControl *control = (CGUIControl *)message.GetPointer();
					RemoveControl(control);
					control->FreeResources(true);
					delete control;
				}
				return true;
			}
		
		case GUI_MSG_NOTIFY_ALL:
			{
				// only process those notifications that come from this window, or those intended for every window
				if (HasID(message.GetSenderId()) || !message.GetSenderId())
				{
					if (message.GetParam1() == GUI_MSG_PAGE_CHANGE ||
									message.GetParam1() == GUI_MSG_REFRESH_THUMBS ||
									message.GetParam1() == GUI_MSG_REFRESH_LIST ||
									message.GetParam1() == GUI_MSG_WINDOW_RESIZE)
					{ // alter the message accordingly, and send to all controls
						for (iControls it = m_children.begin(); it != m_children.end(); ++it)
						{
							CGUIControl *control = *it;
							CGUIMessage msg(message.GetParam1(), message.GetControlId(), control->GetID(), message.GetParam2());
							control->OnMessage(msg);
						}
					}
				}
			}
			break;
	}

	return SendControlMessage(message);
}

void CGUIWindow::AllocResources(bool forceLoad /*= FALSE */)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CSingleLock lock(g_graphicsContext);

#ifdef _DEBUG
	int64_t start;
	start = CurrentHostCounter();
#endif
	// load skin xml fil
	CStdString xmlFile = GetProperty("xmlfile").asString();
	bool bHasPath=false;
	if (xmlFile.Find("\\") > -1 || xmlFile.Find("/") > -1 )
		bHasPath = true;
	
	if (xmlFile.size() && (forceLoad || m_loadOnDemand || !m_windowLoaded))
		Load(xmlFile,bHasPath); /* 见函数内部，对xml  进行加载*/

	int64_t slend;
	slend = CurrentHostCounter();

	// and now allocate resources
	CGUIControlGroup::AllocResources();

#ifdef _DEBUG
	int64_t end, freq;
	end = CurrentHostCounter();
	freq = CurrentHostFrequency();
	CLog::Log(LOGDEBUG,"Alloc resources: %.2fms (%.2f ms skin load)", 1000.f * (end - start) / freq, 1000.f * (slend - start) / freq);
#endif

	m_bAllocated = true;
}

void CGUIWindow::FreeResources(bool forceUnload /*= FALSE */)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	m_bAllocated = false;
	CGUIControlGroup::FreeResources();
	//g_TextureManager.Dump();
	// unload the skin
	if (m_loadOnDemand || forceUnload)
		ClearAll();
}

void CGUIWindow::DynamicResourceAlloc(bool bOnOff)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	m_dynamicResourceAlloc = bOnOff;
	CGUIControlGroup::DynamicResourceAlloc(bOnOff);
}

void CGUIWindow::ClearAll()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	OnWindowUnload();
	CGUIControlGroup::ClearAll();
	m_windowLoaded = false;
	m_dynamicResourceAlloc = true;
}

bool CGUIWindow::Initialize()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (!g_windowManager.Initialized())
		return false;     // can't load if we have no skin yet
		
	return Load(GetProperty("xmlfile").asString());
}

void CGUIWindow::SetInitialVisibility()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	// reset our info manager caches
	g_infoManager.ResetCache();
	CGUIControlGroup::SetInitialVisibility();
}

bool CGUIWindow::IsActive() const
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	return g_windowManager.IsWindowActive(GetID());
}

bool CGUIWindow::CheckAnimation(ANIMATION_TYPE animType)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	// special cases first
	if (animType == ANIM_TYPE_WINDOW_CLOSE)
	{
		if (!m_bAllocated || !m_hasRendered) // can't render an animation if we aren't allocated or haven't rendered
			return false;
		// make sure we update our visibility prior to queuing the window close anim
		for (unsigned int i = 0; i < m_children.size(); i++)
			m_children[i]->UpdateVisibility();
	}
	return true;
}

bool CGUIWindow::IsAnimating(ANIMATION_TYPE animType)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (!m_animationsEnabled)
		return false;
	if (animType == ANIM_TYPE_WINDOW_CLOSE)
		return m_closing;
	return CGUIControlGroup::IsAnimating(animType);
}

bool CGUIWindow::Animate(unsigned int currentTime)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_animationsEnabled)
		return CGUIControlGroup::Animate(currentTime);
	else
	{
		m_transform.Reset();
		return false;
	}
}

void CGUIWindow::DisableAnimations()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	m_animationsEnabled = false;
}

// returns true if the control group with id groupID has controlID as
// its focused control
bool CGUIWindow::ControlGroupHasFocus(int groupID, int controlID)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	// 1.  Run through and get control with groupID (assume unique)
	// 2.  Get it's selected item.
	CGUIControl *group = GetFirstFocusableControl(groupID);
	if (!group) 
		group = (CGUIControl *)GetControl(groupID);

	if (group && group->IsGroup())
	{
		if (controlID == 0)
		{ // just want to know if the group is focused
			return group->HasFocus();
		}
		else
		{
			CGUIMessage message(GUI_MSG_ITEM_SELECTED, GetID(), group->GetID());
			group->OnMessage(message);
			return (controlID == (int) message.GetParam1());
		}
	}
	return false;
}

void CGUIWindow::SaveControlStates()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	ResetControlStates();
	if (!m_defaultAlways)
		m_lastControlID = GetFocusedControlID();
	for (iControls it = m_children.begin(); it != m_children.end(); ++it)
		(*it)->SaveStates(m_controlStates);
}

void CGUIWindow::RestoreControlStates()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	for (vector<CControlState>::iterator it = m_controlStates.begin(); it != m_controlStates.end(); ++it)
	{
		CGUIMessage message(GUI_MSG_ITEM_SELECT, GetID(), (*it).m_id, (*it).m_data);
		OnMessage(message);
	}
	int focusControl = (!m_defaultAlways && m_lastControlID) ? m_lastControlID : m_defaultControl;
	SET_CONTROL_FOCUS(focusControl, 0);
}

void CGUIWindow::ResetControlStates()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	m_lastControlID = 0;
	m_focusedControl = 0;
	m_controlStates.clear();
}

bool CGUIWindow::OnBack(int actionID)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	g_windowManager.PreviousWindow();
	return true;
}

bool CGUIWindow::OnMove(int fromControl, int moveAction)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	const CGUIControl *control = GetFirstFocusableControl(fromControl);
	if (!control) 
		control = GetControl(fromControl);
	
	if (!control)
	{ // no current control??
		CLog::Log(LOGERROR, "Unable to find control %i in window %u",
		fromControl, GetID());
		return false;
	}
	
	vector<int> moveHistory;
	int nextControl = fromControl;
	while (control)
	{ // grab the next control direction
		moveHistory.push_back(nextControl);
		nextControl = control->GetNextControl(moveAction);
		// check our history - if the nextControl is in it, we can't focus it
		for (unsigned int i = 0; i < moveHistory.size(); i++)
		{
			if (nextControl == moveHistory[i])
				return false; // no control to focus so do nothing
		}
		control = GetFirstFocusableControl(nextControl);
		if (control)
			break;  // found a focusable control
		control = GetControl(nextControl); // grab the next control and try again
	}
	
	if (!control)
		return false;   // no control to focus
	// if we get here we have our new control so focus it (and unfocus the current control)
	SET_CONTROL_FOCUS(nextControl, 0);
	return true;
}

void CGUIWindow::SetDefaults()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	m_renderOrder = 0;
	m_defaultAlways = false;
	m_defaultControl = 0;
	m_posX = m_posY = m_width = m_height = 0;
	m_overlayState = OVERLAY_STATE_PARENT_WINDOW;   // Use parent or previous window's state
	m_visibleCondition = 0;
	m_previousWindow = WINDOW_INVALID;
	m_animations.clear();
	m_origins.clear();
	m_hasCamera = false;
	m_animationsEnabled = true;
	m_clearBackground = 0xff000000; // opaque black -> clear
	m_hitRect.SetRect(0, 0, (float)m_coordsRes.iWidth, (float)m_coordsRes.iHeight);
}

CRect CGUIWindow::GetScaledBounds() const
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CSingleLock lock(g_graphicsContext);
	g_graphicsContext.SetScalingResolution(m_coordsRes, m_needsScaling);
	CPoint pos(GetPosition());
	CRect rect(pos.x, pos.y, pos.x + m_width, pos.y + m_height);
	float z = 0;
	g_graphicsContext.ScaleFinalCoords(rect.x1, rect.y1, z);
	g_graphicsContext.ScaleFinalCoords(rect.x2, rect.y2, z);
	return rect;
}

void CGUIWindow::OnEditChanged(int id, CStdString &text)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CGUIMessage msg(GUI_MSG_ITEM_SELECTED, GetID(), id);
	OnMessage(msg);
	text = msg.GetLabel();
}

bool CGUIWindow::SendMessage(int message, int id, int param1 /* = 0*/, int param2 /* = 0*/)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CGUIMessage msg(message, GetID(), id, param1, param2);
	return OnMessage(msg);
}

#ifdef _DEBUG
void CGUIWindow::DumpTextureUse()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CLog::Log(LOGDEBUG, "%s for window %u", __FUNCTION__, GetID());
	CGUIControlGroup::DumpTextureUse();
}
#endif

void CGUIWindow::ChangeButtonToEdit(int id, bool singleLabel /* = false*/)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
#ifdef PRE_SKIN_VERSION_9_10_COMPATIBILITY
	CGUIControl *name = (CGUIControl *)GetControl(id);
	if (name && name->GetControlType() == CGUIControl::GUICONTROL_BUTTON)
	{ // change it to an edit control
		CGUIEditControl *edit = new CGUIEditControl(*(const CGUIButtonControl *)name);
		if (edit)
		{
			if (singleLabel)
				edit->SetLabel("");
			InsertControl(edit, name);
			RemoveControl(name);
			name->FreeResources();
			delete name;
		}
	}
#endif
}

void CGUIWindow::SetProperty(const CStdString &strKey, const CVariant &value)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CSingleLock lock(*this);
	m_mapProperties[strKey] = value;
}

CVariant CGUIWindow::GetProperty(const CStdString &strKey) const
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CSingleLock lock(*this);
	std::map<CStdString, CVariant, icompare>::const_iterator iter = m_mapProperties.find(strKey);
	if (iter == m_mapProperties.end())
		return CVariant(CVariant::VariantTypeNull);

	return iter->second;
}

void CGUIWindow::ClearProperties()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CSingleLock lock(*this);
	m_mapProperties.clear();
}

void CGUIWindow::SetRunActionsManually()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	m_manualRunActions = true;
}

void CGUIWindow::RunLoadActions()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	m_loadActions.Execute(GetID(), GetParentID());
}

void CGUIWindow::RunUnloadActions()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	m_unloadActions.Execute(GetID(), GetParentID());
}

void CGUIWindow::ClearBackground()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	m_clearBackground.Update();
	color_t color = m_clearBackground;
	if (color)
		g_graphicsContext.Clear(color);
}
