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

#include "RenderSystem.h"

CRenderSystemBase::CRenderSystemBase()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	m_bRenderCreated = false;
	m_bVSync = true;
	m_maxTextureSize = 2048;
	m_RenderVersionMajor = 0;
	m_RenderVersionMinor = 0;
	m_renderCaps = 0;
	m_renderQuirks = 0;
	m_minDXTPitch = 0;
}

CRenderSystemBase::~CRenderSystemBase()
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

void CRenderSystemBase::GetRenderVersion(unsigned int& major, unsigned int& minor) const
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	major = m_RenderVersionMajor;
	minor = m_RenderVersionMinor;
}

bool CRenderSystemBase::SupportsNPOT(bool dxt) const
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (dxt)
		return (m_renderCaps & RENDER_CAPS_DXT_NPOT) == RENDER_CAPS_DXT_NPOT;
	return (m_renderCaps & RENDER_CAPS_NPOT) == RENDER_CAPS_NPOT;
}

bool CRenderSystemBase::SupportsDXT() const
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	return (m_renderCaps & RENDER_CAPS_DXT) == RENDER_CAPS_DXT;
}

bool CRenderSystemBase::SupportsBGRA() const
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	return (m_renderCaps & RENDER_CAPS_BGRA) == RENDER_CAPS_BGRA;
}

bool CRenderSystemBase::SupportsBGRAApple() const
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	return (m_renderCaps & RENDER_CAPS_BGRA_APPLE) == RENDER_CAPS_BGRA_APPLE;
}

