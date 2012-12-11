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
#include "D3DResource.h"
#include "windowing/WindowingFactory.h"
#include "utils/log.h"

#ifdef HAS_DX

using namespace std;

CD3DTexture::CD3DTexture()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	m_width = 0;
	m_height = 0;
	m_mipLevels = 0;
	m_usage = 0;
	m_format = D3DFMT_A8R8G8B8;
	m_pool = D3DPOOL_DEFAULT;
	m_texture = NULL;
	m_data = NULL;
	m_pitch = 0;
}

CD3DTexture::~CD3DTexture()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	Release();
	delete[] m_data;
}

/*
	只有材质的实体看上去就像塑料制品，还不足以反映我们这个五彩缤纷的世界，为此
	Direct3D  引入了纹理(Texture)  技术。纹理就是通常所说的贴图，它通过在三维的模型表面
	覆盖上二维图片，使实体具有真实感，比如在家具的表面贴上木纹，或者把草、泥土
	和岩石等图片贴在构成山的图元表面，以得到一个真实的山坡。

	Direct3D  支持多层纹理，最高可达8  层。
*/

bool CD3DTexture::Create(UINT width, UINT height, UINT mipLevels, DWORD usage, D3DFORMAT format, D3DPOOL pool)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、创建纹理
*/
	m_width = width;
	m_height = height;
	m_mipLevels = mipLevels;
	m_usage = usage;
	m_format = format;
	m_pool = pool;
	// create the texture
	Release();
	
	/*	
		函数D3DXCreateTexture  使用说明
		HRESULT WINAPI D3DXCreateTexture(
										LPDIRECT3DDEVICE9 pDevice,   		// Direct3D9的设备对象
										UINT Width,         					// 图像宽度
										UINT Height,        					// 图像高度
										UINT MipLevels,     					// 图片的图层，一般用D3DX_DEFAULT，与图像质量有关
										DWORD Usage,        				//设定这个纹理的使用方法
										D3DFORMAT Format,   				// 每个颜色成分使用的位数
										D3DPOOL Pool,       				// 纹理对象驻留的内存类别
										LPDIRECT3DTEXTURE9 * ppTexture    	// 指向新创建的纹理对象
										);

		高度和宽度是图像的分辨率，设备对象是渲染要用的Direct3D9设备对象。为了创建无错的纹理对象就要
		成功地创建设备对象。为了使用该函数，同样要了解正在创建的纹理的宽度和高度。

		图像的参数MipLevels中Mipmap的总数与图像质量有关。默认情况下，该标识符为0，如果正在创建一个要渲
		染的纹理，那么该标识符为D3DUSAGE_RENDERTARGET，如果该纹理是动态纹理，则该标识符为D3DUSAGE_DYNAMIC。
		本书出于开发目的，多数情况下将该标识符设为0，除了在本章后面的后台渲染演示程序时该标识符的
		设置为非0。

		Format参数将确定每个颜色成分使用的位数，同样，颜色成分的数量确定了整幅图像的大小。从文件加
		载图像时，可以将该参数设为0，这样Direct3D可以根据文件自身的内容选择正确的图像格式。从文件加载
		图像时，也不可能总知道图像的格式。

		Pool参数确定了纹理对象驻留的内存类别。该参数可以取值为D3DPOOL_DEFAULT，这样可以将内存放置到视频
		内存中(这是默认的)；取值为D3DPOOL_MANAGED时，则是将纹理对象保存在系统内存中；当Pool取值
		为D3DPOOL_SYSTEMMEN时，则是将纹理对象保存在计算机的RAM内存中。如果Direct3D设备丢失，则不必重新创建
		资源。使用经过管理的资源可以避免在设备丢失的情况下恢复纹理对象。

		D3DXCreateTexture()函数原型中的最后一个参数通过函数调用正在创建的纹理对象。如果调用该函数后，该参
		数不为NULL(空)，并且如果函数的返回值为D3D_OK，则函数调用成功。
	*/
	HRESULT hr = D3DXCreateTexture(g_Windowing.Get3DDevice(), m_width, m_height, m_mipLevels, m_usage, m_format, m_pool, &m_texture);
	if (FAILED(hr))
	{
		CLog::Log(LOGERROR, __FUNCTION__" - failed 0x%08X", hr);
	}
	else
	{
		D3DSURFACE_DESC desc;
		/*
			获取创建的纹理参数是否与传入的参数相同
			原因为可能D3DXCreateTexture  函数会修正传入的参数，如不是2  的指数值可能会修改为2  的指数值
		*/
		if( D3D_OK == m_texture->GetLevelDesc(0, &desc))
		{
			if(desc.Format != m_format)
				CLog::Log(LOGWARNING, "CD3DTexture::Create - format changed from %d to %d", m_format, desc.Format);
			if(desc.Height != m_height || desc.Width != m_width)
				CLog::Log(LOGWARNING, "CD3DTexture::Create - size changed from %ux%u to %ux%u", m_width, m_height, desc.Width, desc.Height);
		}

		g_Windowing.Register(this); /* windows 系统下见CRenderSystemDX::Register  方法*/
		return true;
	}
	return false;
}

void CD3DTexture::Release()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	g_Windowing.Unregister(this);
	SAFE_RELEASE(m_texture);
}

bool CD3DTexture::LockRect(UINT level, D3DLOCKED_RECT *lr, const RECT *rect, DWORD flags)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_texture)
	{
		if ((flags & D3DLOCK_DISCARD) && !(m_usage & D3DUSAGE_DYNAMIC))
			flags &= ~D3DLOCK_DISCARD;
		
		return (D3D_OK == m_texture->LockRect(level, lr, rect, flags));
	}
	return false;
}

bool CD3DTexture::UnlockRect(UINT level)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_texture)
		return (D3D_OK == m_texture->UnlockRect(level));
	return false;
}

bool CD3DTexture::GetLevelDesc(UINT level, D3DSURFACE_DESC *desc)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、获取指定层数纹理的描述信息
*/
	if (m_texture)
		return (D3D_OK == m_texture->GetLevelDesc(level, desc));
	return false;
}

bool CD3DTexture::GetSurfaceLevel(UINT level, LPDIRECT3DSURFACE9 *surface)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、获取指定层数的纹理serface
*/
	if (m_texture)
		return (D3D_OK == m_texture->GetSurfaceLevel(level, surface));
	return false;
}

void CD3DTexture::SaveTexture()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_texture)
	{
		delete[] m_data;
		m_data = NULL; 
		
		if(!(m_usage & D3DUSAGE_RENDERTARGET) && !(m_usage & D3DUSAGE_DEPTHSTENCIL) && !(m_pool == D3DPOOL_DEFAULT && (m_usage & D3DUSAGE_DYNAMIC) == 0))
		{
			D3DLOCKED_RECT lr;
			if (LockRect( 0, &lr, NULL, D3DLOCK_READONLY ))
			{
				m_pitch = lr.Pitch;
				unsigned int memUsage = GetMemoryUsage(lr.Pitch);
				m_data = new unsigned char[memUsage];
				memcpy(m_data, lr.pBits, memUsage);
				UnlockRect(0);
			}
		}
	}
	SAFE_RELEASE(m_texture);
}

void CD3DTexture::OnDestroyDevice()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	SaveTexture();
}

void CD3DTexture::OnLostDevice()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_pool == D3DPOOL_DEFAULT)
		SaveTexture();
}

void CD3DTexture::RestoreTexture()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	// yay, we're back - make a new copy of the texture
	if (!m_texture)
	{
		HRESULT hr = D3DXCreateTexture(g_Windowing.Get3DDevice(), m_width, m_height, m_mipLevels, m_usage, m_format, m_pool, &m_texture);
		if (FAILED(hr))
		{
			CLog::Log(LOGERROR, __FUNCTION__": D3DXCreateTexture failed 0x%08X", hr);
		}
		else
		{
			// copy the data to the texture
			D3DLOCKED_RECT lr;
			if (m_texture && m_data && LockRect(0, &lr, NULL, D3DLOCK_DISCARD ))
			{
				if (lr.Pitch == m_pitch)
					memcpy(lr.pBits, m_data, GetMemoryUsage(lr.Pitch));
				else
				{
					UINT minpitch = ((UINT)lr.Pitch < m_pitch) ? lr.Pitch : m_pitch;

					for(UINT i = 0; i < m_height; ++i)
					{
						// Get pointers to the "rows" of pixels in texture
						BYTE* pBits = (BYTE*)lr.pBits + i*lr.Pitch;
						BYTE* pData = m_data + i*m_pitch;
						memcpy(pBits, pData, minpitch);
					}
				}
				UnlockRect(0);
			}
		}

		delete[] m_data;
		m_data = NULL;
		m_pitch = 0;
	}
}

void CD3DTexture::OnCreateDevice()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	RestoreTexture();
}

void CD3DTexture::OnResetDevice()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_pool == D3DPOOL_DEFAULT)
		RestoreTexture();
}


unsigned int CD3DTexture::GetMemoryUsage(unsigned int pitch) const
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	switch (m_format)
	{
		case D3DFMT_DXT1:
		case D3DFMT_DXT3:
		case D3DFMT_DXT5:
			return pitch * m_height / 4;
		default:
			return pitch * m_height;
	}
}

CD3DEffect::CD3DEffect()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	m_effect = NULL;
}

CD3DEffect::~CD3DEffect()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	Release();
}

bool CD3DEffect::Create(const CStdString &effectString, DefinesMap* defines)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	Release();
	m_effectString = effectString;
	m_defines.clear();
	if (defines != NULL)
	m_defines = *defines; //FIXME: is this a copy of all members?
	if (CreateEffect())
	{
		g_Windowing.Register(this);
		return true;
	}
	return false;
}

void CD3DEffect::Release()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	g_Windowing.Unregister(this);
	SAFE_RELEASE(m_effect);
}

void CD3DEffect::OnDestroyDevice()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	SAFE_RELEASE(m_effect);
}

void CD3DEffect::OnCreateDevice()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
  	CreateEffect();
}

bool CD3DEffect::SetFloatArray(D3DXHANDLE handle, const float* val, unsigned int count)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if(m_effect)
		return (D3D_OK == m_effect->SetFloatArray(handle, val, count));
	return false;
}

bool CD3DEffect::SetMatrix(D3DXHANDLE handle, const D3DXMATRIX* mat)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_effect)
		return (D3D_OK == m_effect->SetMatrix(handle, mat));
	return false;
}

bool CD3DEffect::SetTechnique(D3DXHANDLE handle)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_effect)
		return (D3D_OK == m_effect->SetTechnique(handle));
	return false;
}

bool CD3DEffect::SetTexture(D3DXHANDLE handle, CD3DTexture &texture)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_effect)
		return (D3D_OK == m_effect->SetTexture(handle, texture.Get()));
	return false;
}

bool CD3DEffect::Begin(UINT *passes, DWORD flags)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_effect)
		return (D3D_OK == m_effect->Begin(passes, flags));
	return false;
}

bool CD3DEffect::BeginPass(UINT pass)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_effect)
		return (D3D_OK == m_effect->BeginPass(pass));
	return false;
}

bool CD3DEffect::EndPass()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_effect)
		return (D3D_OK == m_effect->EndPass());
	return false;
}

bool CD3DEffect::End()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_effect)
		return (D3D_OK == m_effect->End());
	return false;
}

bool CD3DEffect::CreateEffect()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	HRESULT hr;
	LPD3DXBUFFER pError = NULL;

	std::vector<D3DXMACRO> definemacros;

	for( DefinesMap::const_iterator it = m_defines.begin(); it != m_defines.end(); ++it )
	{
		D3DXMACRO m;
		m.Name = it->first.c_str();
		if (it->second.IsEmpty())
			m.Definition = NULL;
		else
			m.Definition = it->second.c_str();
		definemacros.push_back( m );
	}

	definemacros.push_back(D3DXMACRO());
	definemacros.back().Name = 0;
	definemacros.back().Definition = 0;

	hr = D3DXCreateEffect(g_Windowing.Get3DDevice(),  m_effectString, m_effectString.length(), &definemacros[0], NULL, 0, NULL, &m_effect, &pError );
	if(hr == S_OK)
		return true;
	else if(pError)
	{
		CStdString error;
		error.assign((const char*)pError->GetBufferPointer(), pError->GetBufferSize());
		CLog::Log(LOGERROR, "%s", error.c_str());
	}
	return false;
}

void CD3DEffect::OnLostDevice()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_effect)
		m_effect->OnLostDevice();
}

void CD3DEffect::OnResetDevice()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_effect)
		m_effect->OnResetDevice();
}

CD3DVertexBuffer::CD3DVertexBuffer()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	m_length = 0;
	m_usage = 0;
	m_fvf = 0;
	m_pool = D3DPOOL_DEFAULT;
	m_vertex = NULL;
	m_data = NULL;
}

CD3DVertexBuffer::~CD3DVertexBuffer()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	Release();
	delete[] m_data;
}

bool CD3DVertexBuffer::Create(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	m_length = length;
	m_usage = usage;
	m_fvf = fvf;
	m_pool = pool;

	// create the vertex buffer
	Release();
	if (CreateVertexBuffer())
	{
		g_Windowing.Register(this);
		return true;
	}
	return false;
}

void CD3DVertexBuffer::Release()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	g_Windowing.Unregister(this);
	SAFE_RELEASE(m_vertex);
}

bool CD3DVertexBuffer::Lock(UINT level, UINT size, void **data, DWORD flags)
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_vertex)
		return (D3D_OK == m_vertex->Lock(level, size, data, flags));
	return false;
}

bool CD3DVertexBuffer::Unlock()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_vertex)
		return (D3D_OK == m_vertex->Unlock());
	return false;
}

void CD3DVertexBuffer::OnDestroyDevice()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (m_vertex)
	{
		delete[] m_data;
		m_data = NULL;
		void* data;
		if (Lock(0, 0, &data, 0))
		{
			m_data = new BYTE[m_length];
			memcpy(m_data, data, m_length);
			Unlock();
		}
	}
	SAFE_RELEASE(m_vertex);
}

void CD3DVertexBuffer::OnCreateDevice()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	// yay, we're back - make a new copy of the vertices
	if (!m_vertex && m_data && CreateVertexBuffer())
	{
		void *data = NULL;
		if (Lock(0, 0, &data, 0))
		{
			memcpy(data, m_data, m_length);
			Unlock();
		}
		delete[] m_data;
		m_data = NULL;
	}
}

bool CD3DVertexBuffer::CreateVertexBuffer()
{
/*
	参数:
		1、

	返回:
		1、

	说明:
		1、
*/
	if (D3D_OK == g_Windowing.Get3DDevice()->CreateVertexBuffer(m_length, m_usage, m_fvf, m_pool, &m_vertex, NULL))
		return true;
	return false;
}

#endif
