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


#include "WinSystemWin32DX.h"
#include "settings/GUISettings.h"
#include "guilib/gui3d.h"

#ifdef HAS_DX

CWinSystemWin32DX::CWinSystemWin32DX() : CRenderSystemDX()
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

CWinSystemWin32DX::~CWinSystemWin32DX()
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

bool CWinSystemWin32DX::UseWindowedDX(bool fullScreen)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	return (g_guiSettings.GetBool("videoscreen.fakefullscreen") || !fullScreen);
}

bool CWinSystemWin32DX::CreateNewWindow(CStdString name, bool fullScreen, RESOLUTION_INFO& res, PHANDLE_EVENT_FUNC userFunction)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CWinSystemWin32::CreateNewWindow(name, fullScreen, res, userFunction); /* 调用父类的创建窗口函数，见此函数的说明*/

	if(m_hWnd == NULL)
		return false;

	SetFocusWnd(m_hWnd);/* 此处将窗口句柄设定到了d3d 中，即渲染等操作所对应的窗口*/
	SetDeviceWnd(m_hWnd);
	CRenderSystemDX::m_interlaced = ((res.dwFlags & D3DPRESENTFLAG_INTERLACED) != 0);
	CRenderSystemDX::m_useWindowedDX = UseWindowedDX(fullScreen);
	SetRenderParams(m_nWidth, m_nHeight, fullScreen, res.fRefreshRate);
	SetMonitor(GetMonitor(res.iScreen).hMonitor);

	return true;
}

void CWinSystemWin32DX::UpdateMonitor()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	SetMonitor(GetMonitor(m_nScreen).hMonitor);
}

bool CWinSystemWin32DX::ResizeWindow(int newWidth, int newHeight, int newLeft, int newTop)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	CWinSystemWin32::ResizeWindow(newWidth, newHeight, newLeft, newTop);
	CRenderSystemDX::ResetRenderSystem(newWidth, newHeight, false, 0);

	return true;
}

void CWinSystemWin32DX::OnMove(int x, int y)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	CRenderSystemDX::OnMove();
}

bool CWinSystemWin32DX::SetFullScreen(bool fullScreen, RESOLUTION_INFO& res, bool blankOtherDisplays)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	// When going DX fullscreen -> windowed, we must reset the D3D device first to
	// get it out of fullscreen mode because it restores a former resolution.
	// We then change to the mode we want.
	// In other cases, set the window/mode then reset the D3D device.

	bool FS2Windowed = !m_useWindowedDX && UseWindowedDX(fullScreen);

	SetMonitor(GetMonitor(res.iScreen).hMonitor);
	CRenderSystemDX::m_interlaced = ((res.dwFlags & D3DPRESENTFLAG_INTERLACED) != 0);
	CRenderSystemDX::m_useWindowedDX = UseWindowedDX(fullScreen);

	if (FS2Windowed)
	CRenderSystemDX::ResetRenderSystem(res.iWidth, res.iHeight, fullScreen, res.fRefreshRate);

	CWinSystemWin32::SetFullScreen(fullScreen, res, blankOtherDisplays);
	CRenderSystemDX::ResetRenderSystem(res.iWidth, res.iHeight, fullScreen, res.fRefreshRate);

	return true;
}

#endif
