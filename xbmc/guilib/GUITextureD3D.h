/*!
\file GUITextureD3D.h
\brief
*/

#ifndef GUILIB_GUITEXTURED3D_H
#define GUILIB_GUITEXTURED3D_H

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

#include "GUITexture.h"

#ifdef HAS_DX

/*
	gui 部分的纹理相关操作的类

	需要有纹理的控件，在其定义的类中都会有1  个或者几个数据成员(焦点的纹理，失去焦点的纹理等等)，
	如CGUIButtonControl  类中就有m_imgFocus 、 m_imgNoFocus 两个用于保存焦点、失去焦点的纹理相关域成员，在其构造
	函数中通常就会去这些域成员进行赋值，如见CGUIButtonControl  类的构造函数
*/
class CGUITextureD3D : public CGUITextureBase
{
public:
	CGUITextureD3D(float posX, float posY, float width, float height, const CTextureInfo& texture);
	static void DrawQuad(const CRect &coords, color_t color, CBaseTexture *texture = NULL, const CRect *texCoords = NULL);
	
protected:
	void Begin(color_t color);
	void Draw(float *x, float *y, float *z, const CRect &texture, const CRect &diffuse, int orientation);
	void End();
	
private:
	color_t m_col;
};

#endif

#endif