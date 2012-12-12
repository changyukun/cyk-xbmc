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


#ifdef HAS_DX

#include "threads/SystemClock.h"
#include "settings/Settings.h"
#include "RenderSystemDX.h"
#include "utils/log.h"
#include "utils/TimeUtils.h"
#include "utils/MathUtils.h"
#include "guilib/GUIWindowManager.h"
#include "threads/SingleLock.h"
#include "guilib/D3DResource.h"
#include "settings/GUISettings.h"
#include "settings/AdvancedSettings.h"
#include "utils/SystemInfo.h"
#include "Application.h"
#include "Util.h"
#include "win32/WIN32Util.h"
#include "video/VideoReferenceClock.h"
#if (D3DX_SDK_VERSION >= 42) //aug 2009 sdk and up use dxerr
  #include <Dxerr.h>
#else
  #include <dxerr9.h>
  #define DXGetErrorString(hr)      DXGetErrorString9(hr)
  #define DXGetErrorDescription(hr) DXGetErrorDescription9(hr)
#endif

using namespace std;

// Dynamic loading of Direct3DCreate9Ex to keep compatibility with 2000/XP.
typedef HRESULT (WINAPI *LPDIRECT3DCREATE9EX)( UINT SDKVersion, IDirect3D9Ex **ppD3D);
static LPDIRECT3DCREATE9EX g_Direct3DCreate9Ex;
static HMODULE             g_D3D9ExHandle;

static bool LoadD3D9Ex()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	g_Direct3DCreate9Ex = (LPDIRECT3DCREATE9EX)GetProcAddress( GetModuleHandle("d3d9.dll"), "Direct3DCreate9Ex" );
	if(g_Direct3DCreate9Ex == NULL)
		return false;
	return true;
}

CRenderSystemDX::CRenderSystemDX() : CRenderSystemBase()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	m_enumRenderingSystem = RENDERING_SYSTEM_DIRECTX;

	m_pD3D        = NULL;
	m_pD3DDevice  = NULL;
	m_devType     = D3DDEVTYPE_HAL;
#if defined(DEBUG_PS) || defined (DEBUG_VS)
	m_devType = D3DDEVTYPE_REF
#endif
	m_hFocusWnd   = NULL;
	m_hDeviceWnd  = NULL;
	m_nBackBufferWidth  = 0;
	m_nBackBufferHeight = 0;
	m_bFullScreenDevice = false;
	m_bVSync          = true;
	m_nDeviceStatus   = S_OK;
	m_stateBlock      = NULL;
	m_inScene         = false;
	m_needNewDevice   = false;
	m_adapter         = D3DADAPTER_DEFAULT;
	m_screenHeight    = 0;
	m_systemFreq      = CurrentHostFrequency();
	m_useD3D9Ex       = false;
	m_defaultD3DUsage = 0;
	m_defaultD3DPool  = D3DPOOL_MANAGED;

	ZeroMemory(&m_D3DPP, sizeof(D3DPRESENT_PARAMETERS));
}

CRenderSystemDX::~CRenderSystemDX()
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

bool CRenderSystemDX::InitRenderSystem()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、初始化渲染系统，见CApplication::Create()  中对此的调用
*/
	m_bVSync = true;

	m_useD3D9Ex = (g_advancedSettings.m_AllowD3D9Ex && g_sysinfo.IsVistaOrHigher() && LoadD3D9Ex());
	m_pD3D = NULL;

	if (m_useD3D9Ex)
	{
		CLog::Log(LOGDEBUG, __FUNCTION__" - trying D3D9Ex...");
		if (FAILED(g_Direct3DCreate9Ex(D3D_SDK_VERSION, (IDirect3D9Ex**) &m_pD3D)))/* 创建一个Direct3D9 的接口*/
		{
			CLog::Log(LOGDEBUG, __FUNCTION__" - D3D9Ex creation failure, falling back to D3D9");
			m_useD3D9Ex = false;
		}
		else
		{
			D3DCAPS9 caps;
			memset(&caps, 0, sizeof(caps));
			m_pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, m_devType, &caps); /* 获取显卡的功能，如是否支持硬件加速*/
			// Evaluate if the driver is WDDM - this detection method is not guaranteed 100%
			if (!g_advancedSettings.m_ForceD3D9Ex && (!(caps.Caps2 & D3DCAPS2_CANSHARERESOURCE) || !(caps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES)))
			{
				CLog::Log(LOGDEBUG, __FUNCTION__" - driver looks like XPDM or earlier, falling back to D3D9");
				m_useD3D9Ex = false;
				m_pD3D->Release();
			}
			else
			{
				CLog::Log(LOGDEBUG, __FUNCTION__" - using D3D9Ex");
			}
		}
	}

	if (!m_useD3D9Ex)
	{
		m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);/* 创建一个Direct3D9 的接口*/
		if(m_pD3D == NULL)
			return false;
	}

	UpdateMonitor();

	if(CreateDevice()==false) /* 创建设备*/
		return false;

	return true;
}

void CRenderSystemDX::SetRenderParams(unsigned int width, unsigned int height, bool fullScreen, float refreshRate)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、此函数实现设置后缓存的宽、高、全屏等相关信息
*/
	m_nBackBufferWidth  = width;
	m_nBackBufferHeight = height;
	m_bFullScreenDevice = fullScreen;
	m_refreshRate       = refreshRate;
}

void CRenderSystemDX::SetMonitor(HMONITOR monitor)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、此函数实质就是利用传入的显示器的句柄，然后找到相应的显卡，然后
			用m_adapter  保存此显卡所对应的序号
*/
	if (!m_pD3D)
		return;

	// find the appropriate screen
	for (unsigned int adapter = 0; adapter < m_pD3D->GetAdapterCount(); adapter++)/* 获得显卡的数据*/
	{
		HMONITOR hMonitor = m_pD3D->GetAdapterMonitor(adapter);/* 获得相应的显示器*/
		if (hMonitor == monitor && adapter != m_adapter)
		{
			m_adapter       = adapter;
			m_needNewDevice = true;
			break;
		}
	}
}

bool CRenderSystemDX::ResetRenderSystem(int width, int height, bool fullScreen, float refreshRate)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	HMONITOR hMonitor = MonitorFromWindow(m_hDeviceWnd, MONITOR_DEFAULTTONULL);
	if (hMonitor)
		SetMonitor(hMonitor);

	SetRenderParams(width, height, fullScreen, refreshRate);

	CRect rc;
	rc.SetRect(0, 0, (float)width, (float)height);
	SetViewPort(rc);/* 见函数说明*/

	BuildPresentParameters();

	if (m_useD3D9Ex && !m_needNewDevice)
		m_nDeviceStatus = ((IDirect3DDevice9Ex*)m_pD3DDevice)->ResetEx(&m_D3DPP, m_D3DPP.Windowed ? NULL : &m_D3DDMEX);
	else
	{
		OnDeviceLost();
		OnDeviceReset();
	}

	return true;
}

void CRenderSystemDX::OnMove()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!m_bRenderCreated)
		return;

	HMONITOR currentMonitor = m_pD3D->GetAdapterMonitor(m_adapter);/* 根据显卡的序号返回当前的显示器，见SetMonitor 方法说明*/
	HMONITOR newMonitor = MonitorFromWindow(m_hDeviceWnd, MONITOR_DEFAULTTONULL);
	if (newMonitor != NULL && currentMonitor != newMonitor)
		ResetRenderSystem(m_nBackBufferWidth, m_nBackBufferHeight, m_bFullScreenDevice, m_refreshRate);
}


bool CRenderSystemDX::IsSurfaceFormatOk(D3DFORMAT surfFormat, DWORD usage)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	// Verify the compatibility
	HRESULT hr = m_pD3D->CheckDeviceFormat(m_adapter,
											m_devType,
											m_D3DPP.BackBufferFormat,
											usage,
											D3DRTYPE_SURFACE,
											surfFormat);

	return (SUCCEEDED(hr)) ? true : false;
}

bool CRenderSystemDX::IsTextureFormatOk(D3DFORMAT texFormat, DWORD usage)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	// Verify the compatibility
	HRESULT hr = m_pD3D->CheckDeviceFormat(m_adapter,
											m_devType,
											m_D3DPP.BackBufferFormat,
											usage,
											D3DRTYPE_TEXTURE,
											texFormat);

	return (SUCCEEDED(hr)) ? true : false;
}

BOOL CRenderSystemDX::IsDepthFormatOk(D3DFORMAT DepthFormat, D3DFORMAT RenderTargetFormat)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	// Verify that the depth format exists
	if (!IsSurfaceFormatOk(DepthFormat, D3DUSAGE_DEPTHSTENCIL))
		return false;

	// Verify that the depth format is compatible
	HRESULT hr = m_pD3D->CheckDepthStencilMatch(m_adapter,
												m_devType,
												m_D3DPP.BackBufferFormat,
												RenderTargetFormat,
												DepthFormat);

	return SUCCEEDED(hr);
}

void CRenderSystemDX::BuildPresentParameters()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	GetVersionEx((OSVERSIONINFO *)&osvi);

	ZeroMemory( &m_D3DPP, sizeof(D3DPRESENT_PARAMETERS) );
	m_D3DPP.Windowed           = m_useWindowedDX;
	m_D3DPP.SwapEffect         = D3DSWAPEFFECT_FLIP;
	m_D3DPP.BackBufferCount    = 2;

	if(m_useD3D9Ex && (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion >= 1 || osvi.dwMajorVersion > 6))
	{
#if D3DX_SDK_VERSION >= 42
		//m_D3DPP.SwapEffect       = D3DSWAPEFFECT_FLIPEX;
#else
#   pragma message("D3D SDK version is too old to support D3DSWAPEFFECT_FLIPEX")
		CLog::Log(LOGWARNING, "CRenderSystemDX::BuildPresentParameters - xbmc compiled with an d3d sdk not supporting D3DSWAPEFFECT_FLIPEX");
#endif
	}

	m_D3DPP.hDeviceWindow      = m_hDeviceWnd;
	m_D3DPP.BackBufferWidth    = m_nBackBufferWidth;
	m_D3DPP.BackBufferHeight   = m_nBackBufferHeight;
	m_D3DPP.Flags              = D3DPRESENTFLAG_VIDEO;
	m_D3DPP.PresentationInterval = (m_bVSync) ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
	m_D3DPP.FullScreen_RefreshRateInHz = (m_useWindowedDX) ? 0 : (int)m_refreshRate;
	m_D3DPP.BackBufferFormat   = D3DFMT_X8R8G8B8;
	m_D3DPP.MultiSampleType    = D3DMULTISAMPLE_NONE;
	m_D3DPP.MultiSampleQuality = 0;

	D3DFORMAT zFormat = D3DFMT_D16;
	if      (IsDepthFormatOk(D3DFMT_D32, m_D3DPP.BackBufferFormat))           zFormat = D3DFMT_D32;
	else if (IsDepthFormatOk(D3DFMT_D24S8, m_D3DPP.BackBufferFormat))         zFormat = D3DFMT_D24S8;
	else if (IsDepthFormatOk(D3DFMT_D24X4S4, m_D3DPP.BackBufferFormat))       zFormat = D3DFMT_D24X4S4;
	else if (IsDepthFormatOk(D3DFMT_D24X8, m_D3DPP.BackBufferFormat))         zFormat = D3DFMT_D24X8;
	else if (IsDepthFormatOk(D3DFMT_D16, m_D3DPP.BackBufferFormat))           zFormat = D3DFMT_D16;
	else if (IsDepthFormatOk(D3DFMT_D15S1, m_D3DPP.BackBufferFormat))         zFormat = D3DFMT_D15S1;

	m_D3DPP.EnableAutoDepthStencil = TRUE;
	m_D3DPP.AutoDepthStencilFormat = zFormat;

	if (m_useD3D9Ex)
	{
		ZeroMemory( &m_D3DDMEX, sizeof(D3DDISPLAYMODEEX) );
		m_D3DDMEX.Size             = sizeof(D3DDISPLAYMODEEX);
		m_D3DDMEX.Width            = m_D3DPP.BackBufferWidth;
		m_D3DDMEX.Height           = m_D3DPP.BackBufferHeight;
		m_D3DDMEX.RefreshRate      = m_D3DPP.FullScreen_RefreshRateInHz;
		m_D3DDMEX.Format           = m_D3DPP.BackBufferFormat;
		m_D3DDMEX.ScanLineOrdering = m_interlaced ? D3DSCANLINEORDERING_INTERLACED : D3DSCANLINEORDERING_PROGRESSIVE;
	}
}

bool CRenderSystemDX::DestroyRenderSystem()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	DeleteDevice();

	SAFE_RELEASE(m_stateBlock);
	SAFE_RELEASE(m_pD3D);
	SAFE_RELEASE(m_pD3DDevice);

	m_bRenderCreated = false;

	return true;
}

void CRenderSystemDX::DeleteDevice()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	CSingleLock lock(m_resourceSection);

	// tell any shared resources
	for (vector<ID3DResource *>::iterator i = m_resources.begin(); i != m_resources.end(); i++)
		(*i)->OnDestroyDevice();

	SAFE_RELEASE(m_pD3DDevice);
	m_bRenderCreated = false;
}

void CRenderSystemDX::OnDeviceLost()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	CSingleLock lock(m_resourceSection);
	g_windowManager.SendMessage(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_RENDERER_LOST);
	SAFE_RELEASE(m_stateBlock);

	if (m_needNewDevice)
		DeleteDevice();
	else
	{
		// just resetting the device
		for (vector<ID3DResource *>::iterator i = m_resources.begin(); i != m_resources.end(); i++)
			(*i)->OnLostDevice();
	}
}

void CRenderSystemDX::OnDeviceReset()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	CSingleLock lock(m_resourceSection);

	if (m_needNewDevice)
		CreateDevice();
	else
	{
		// just need a reset
		if (m_useD3D9Ex)
			m_nDeviceStatus = ((IDirect3DDevice9Ex*)m_pD3DDevice)->ResetEx(&m_D3DPP, m_D3DPP.Windowed ? NULL : &m_D3DDMEX);
		else
			m_nDeviceStatus = m_pD3DDevice->Reset(&m_D3DPP);
	}

	if (m_nDeviceStatus == S_OK)
	{ // we're back
		for (vector<ID3DResource *>::iterator i = m_resources.begin(); i != m_resources.end(); i++)
			(*i)->OnResetDevice();

		g_windowManager.SendMessage(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_RENDERER_RESET);
	}
	else
	{
		for (vector<ID3DResource *>::iterator i = m_resources.begin(); i != m_resources.end(); i++)
			(*i)->OnLostDevice();
	}
}

bool CRenderSystemDX::CreateDevice()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、此函数相当于创建了directx 设备，保存在m_pD3DDevice  中
*/
	// Code based on Ogre 3D engine
	CSingleLock lock(m_resourceSection);

	HRESULT hr;

	if(m_pD3D == NULL)
		return false;

	if(m_hDeviceWnd == NULL)
		return false;

	CLog::Log(LOGDEBUG, __FUNCTION__" on adapter %d", m_adapter);

	BuildPresentParameters(); /* 创建present  参数*/

	D3DCAPS9 caps;
	memset(&caps, 0, sizeof(caps));
	m_pD3D->GetDeviceCaps(m_adapter, m_devType, &caps);

	DWORD VertexProcessingFlags = 0;
	if (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
	{
		/* Activate when the state management of the fixed pipeline is in order,
		to get a bit more performance
		if (caps.DevCaps & D3DDEVCAPS_PUREDEVICE)
		VertexProcessingFlags = D3DCREATE_PUREDEVICE;
		*/
		VertexProcessingFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		CLog::Log(LOGDEBUG, __FUNCTION__" - using hardware vertex processing");
	}
	else
	{
		VertexProcessingFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
		CLog::Log(LOGDEBUG, __FUNCTION__" - using software vertex processing");
	}

	if (m_useD3D9Ex)
	{
		/* 创建设备*/
		hr = ((IDirect3D9Ex*)m_pD3D)->CreateDeviceEx(	m_adapter, 
													m_devType, 
													m_hFocusWnd, /* 传入一个窗口句柄，即此directx 设备与此窗口绑定了*/
													VertexProcessingFlags | D3DCREATE_MULTITHREADED, 
													&m_D3DPP,
													m_D3DPP.Windowed ? NULL : &m_D3DDMEX, 
													(IDirect3DDevice9Ex**)&m_pD3DDevice );
		if (FAILED(hr))
		{
			CLog::Log(LOGWARNING, __FUNCTION__" - initial wanted device config failed");
			// Try a second time, may fail the first time due to back buffer count,
			// which will be corrected down to 1 by the runtime
			hr = ((IDirect3D9Ex*)m_pD3D)->CreateDeviceEx( m_adapter, m_devType, m_hFocusWnd,
			VertexProcessingFlags | D3DCREATE_MULTITHREADED, &m_D3DPP, m_D3DPP.Windowed ? NULL : &m_D3DDMEX, (IDirect3DDevice9Ex**)&m_pD3DDevice );
			if( FAILED( hr ) )
			{
				CLog::Log(LOGERROR, __FUNCTION__" - unable to create a device. %s", GetErrorDescription(hr).c_str());
				return false;
			}
		}
		// Not sure the following actually does something
		((IDirect3DDevice9Ex*)m_pD3DDevice)->SetGPUThreadPriority(7);
	}
	else
	{
		/* 创建设备*/
		hr = m_pD3D->CreateDevice(	m_adapter, 
									m_devType, 
									m_hFocusWnd,
									VertexProcessingFlags | D3DCREATE_MULTITHREADED, 
									&m_D3DPP, 
									&m_pD3DDevice );
		if (FAILED(hr))
		{
			CLog::Log(LOGWARNING, __FUNCTION__" - initial wanted device config failed");
			// Try a second time, may fail the first time due to back buffer count,
			// which will be corrected down to 1 by the runtime
			hr = m_pD3D->CreateDevice( m_adapter, m_devType, m_hFocusWnd,
			VertexProcessingFlags | D3DCREATE_MULTITHREADED, &m_D3DPP, &m_pD3DDevice );
			if( FAILED( hr ) )
			{
				CLog::Log(LOGERROR, __FUNCTION__" - unable to create a device. %s", GetErrorDescription(hr).c_str());
				return false;
			}
		}
	}

	if(m_pD3D->GetAdapterIdentifier(m_adapter, 0, &m_AIdentifier) == D3D_OK) /* 获得默认显卡的信息，保存在m_AIdentifier 中*/
	{
		m_RenderRenderer = (const char*)m_AIdentifier.Description;
		m_RenderVendor   = (const char*)m_AIdentifier.Driver;
		m_RenderVersion.Format("%d.%d.%d.%04d", HIWORD(m_AIdentifier.DriverVersion.HighPart), LOWORD(m_AIdentifier.DriverVersion.HighPart),
		                        			HIWORD(m_AIdentifier.DriverVersion.LowPart) , LOWORD(m_AIdentifier.DriverVersion.LowPart));
	}

	CLog::Log(LOGDEBUG, __FUNCTION__" - adapter %d: %s, %s, VendorId %lu, DeviceId %lu", m_adapter, m_AIdentifier.Driver, m_AIdentifier.Description, m_AIdentifier.VendorId, m_AIdentifier.DeviceId);

	// get our render capabilities
	// re-read caps, there may be changes depending on the vertex processing type
	m_pD3DDevice->GetDeviceCaps(&caps); /* 获取渲染器的能力*/

	m_maxTextureSize = min(caps.MaxTextureWidth, caps.MaxTextureHeight);

	if (g_advancedSettings.m_AllowDynamicTextures && m_useD3D9Ex && (caps.Caps2 & D3DCAPS2_DYNAMICTEXTURES))
	{
		m_defaultD3DUsage = D3DUSAGE_DYNAMIC;
		m_defaultD3DPool  = D3DPOOL_DEFAULT;
		CLog::Log(LOGDEBUG, __FUNCTION__" - using D3DCAPS2_DYNAMICTEXTURES");
	}
	else
	{
		m_defaultD3DUsage = 0;
		m_defaultD3DPool  = D3DPOOL_MANAGED;
	}

	m_renderCaps = 0;

	CLog::Log(LOGDEBUG, __FUNCTION__" - texture caps: 0x%08X", caps.TextureCaps);

	if(IsTextureFormatOk(D3DFMT_DXT1, m_defaultD3DUsage)&& IsTextureFormatOk(D3DFMT_DXT3, m_defaultD3DUsage)&& IsTextureFormatOk(D3DFMT_DXT5, m_defaultD3DUsage))
		m_renderCaps |= RENDER_CAPS_DXT;

	if ((caps.TextureCaps & D3DPTEXTURECAPS_POW2) == 0)
	{ // we're allowed NPOT textures
		m_renderCaps |= RENDER_CAPS_NPOT;
		if (((m_renderCaps & RENDER_CAPS_DXT) != 0) && ((caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) == 0))
			m_renderCaps |= RENDER_CAPS_DXT_NPOT;
	}
	else if ((caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) != 0)
	{ // we're allowed _some_ NPOT textures (namely non-DXT and only with D3DTADDRESS_CLAMP and no wrapping)
		m_renderCaps |= RENDER_CAPS_NPOT;
	}

	// Temporary - allow limiting the caps to debug a texture problem
	if (g_advancedSettings.m_RestrictCapsMask != 0)
		m_renderCaps &= ~g_advancedSettings.m_RestrictCapsMask;

	if (m_renderCaps & RENDER_CAPS_DXT)
		CLog::Log(LOGDEBUG, __FUNCTION__" - RENDER_CAPS_DXT");
	if (m_renderCaps & RENDER_CAPS_NPOT)
		CLog::Log(LOGDEBUG, __FUNCTION__" - RENDER_CAPS_NPOT");
	if (m_renderCaps & RENDER_CAPS_DXT_NPOT)
		CLog::Log(LOGDEBUG, __FUNCTION__" - RENDER_CAPS_DXT_NPOT");

	// nVidia quirk: some NPOT DXT textures of the GUI display with corruption
	// when using D3DPOOL_DEFAULT + D3DUSAGE_DYNAMIC textures (no other choice with D3D9Ex for example)
	// most likely xbmc bug, but no hw to repro & fix properly.
	// affects lots of hw generations - 6xxx, 7xxx, GT220, ION1
	// see ticket #9269
	if(m_defaultD3DUsage == D3DUSAGE_DYNAMIC&& m_defaultD3DPool  == D3DPOOL_DEFAULT&& m_AIdentifier.VendorId == PCIV_nVidia)
	{
		CLog::Log(LOGDEBUG, __FUNCTION__" - nVidia workaround - disabling RENDER_CAPS_DXT_NPOT");
		m_renderCaps &= ~RENDER_CAPS_DXT_NPOT;
	}

	// Intel quirk: DXT texture pitch must be > 64
	// when using D3DPOOL_DEFAULT + D3DUSAGE_DYNAMIC textures (no other choice with D3D9Ex)
	// DXT1:   32 pixels wide is the largest non-working texture width
	// DXT3/5: 16 pixels wide ----------------------------------------
	// Both equal to a pitch of 64. So far no Intel has DXT NPOT (including i3/i5/i7, so just go with the next higher POT.
	// See ticket #9578
	if(m_defaultD3DUsage == D3DUSAGE_DYNAMIC&& m_defaultD3DPool  == D3DPOOL_DEFAULT&& m_AIdentifier.VendorId == PCIV_Intel)
	{
		CLog::Log(LOGDEBUG, __FUNCTION__" - Intel workaround - specifying minimum pitch for compressed textures.");
		m_minDXTPitch = 128;
	}

	D3DDISPLAYMODE mode;
	if (SUCCEEDED(m_pD3DDevice->GetDisplayMode(0, &mode)))
		m_screenHeight = mode.Height;
	else
		m_screenHeight = m_nBackBufferHeight;

/*
	纹理过滤器的作用:
	纹理被映射到屏幕中的三角形上。通常纹理三角形和屏幕三角形是不一样大的。当
	纹理三角形比屏幕三角形小时，纹理三角形会被适当放大。当纹理三角形比屏幕三
	角形大时，纹理三角形会被适当缩小。这两种情况变形都会出现，过滤( filtering )  是一
	种direct3d  用它来帮助这些形变的平滑技术。
*/
	m_pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR ); /* 设定纹理过滤器模式*/
	m_pD3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

	m_bRenderCreated = true;
	m_needNewDevice = false;

	// tell any shared objects about our resurrection
	for (vector<ID3DResource *>::iterator i = m_resources.begin(); i != m_resources.end(); i++)
		(*i)->OnCreateDevice();

	return true;
}

bool CRenderSystemDX::PresentRenderImpl(const CDirtyRegionList &dirty)
{
/*
	参数:
		1、dirty : 此参数在此函数中没有使用
		
	返回:
		1、
		
	说明:
		1、此函数实质就是调用Direct3D  的Present  进行前后缓冲的交换，即真的将
			后缓冲显示到显示器上面
		2、搜索direct3d-->present  可以跟踪此函数的被调用过程
*/
	HRESULT hr;

	if (!m_bRenderCreated)
		return false;

	if(m_nDeviceStatus != S_OK)
		return false;

	//CVideoReferenceClock polls GetRasterStatus too,
	//polling it from two threads at the same time is bad
	if (g_advancedSettings.m_sleepBeforeFlip > 0 && g_VideoReferenceClock.ThreadHandle() == NULL)
	{
		//save current thread priority and set thread priority to THREAD_PRIORITY_TIME_CRITICAL
		int priority = GetThreadPriority(GetCurrentThread());
		if (priority != THREAD_PRIORITY_ERROR_RETURN)
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

		D3DRASTER_STATUS rasterStatus;
		int64_t          prev = CurrentHostCounter();

		while (SUCCEEDED(m_pD3DDevice->GetRasterStatus(0, &rasterStatus)))
		{
			//wait for the scanline to go over the given proportion of m_screenHeight mark
			if (!rasterStatus.InVBlank && rasterStatus.ScanLine >= g_advancedSettings.m_sleepBeforeFlip * m_screenHeight)
				break;

			//in theory it's possible this loop never exits, so don't let it run for longer than 100 ms
			int64_t now = CurrentHostCounter();
			if ((now - prev) * 10 > m_systemFreq)
				break;

			Sleep(1);
		}

		//restore thread priority
		if (priority != THREAD_PRIORITY_ERROR_RETURN)
		SetThreadPriority(GetCurrentThread(), priority);
	}

	hr = m_pD3DDevice->Present( NULL, NULL, 0, NULL ); /* Direct3D 的present  方法被调用的过程--->4 ，搜索cyk  direct3d-->present  */

	if( D3DERR_DEVICELOST == hr )
	{
		CLog::Log(LOGDEBUG, "%s - lost device", __FUNCTION__);
		return false;
	}

	if(FAILED(hr))
	{
		CLog::Log(LOGDEBUG, "%s - Present failed. %s", __FUNCTION__, GetErrorDescription(hr).c_str());
		return false;
	}

	return true;
}

bool CRenderSystemDX::BeginRender()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!m_bRenderCreated)
		return false;

	DWORD oldStatus = m_nDeviceStatus;
	if (m_useD3D9Ex)
	{
		m_nDeviceStatus = ((IDirect3DDevice9Ex*)m_pD3DDevice)->CheckDeviceState(m_hDeviceWnd);

		// handling of new D3D9 extensions return values. Others fallback to regular D3D9 handling.
		switch(m_nDeviceStatus)
		{
			case S_PRESENT_MODE_CHANGED:
				// Timing leads us here on occasion.
				BuildPresentParameters();
				m_nDeviceStatus = ((IDirect3DDevice9Ex*)m_pD3DDevice)->ResetEx(&m_D3DPP, m_D3DPP.Windowed ? NULL : &m_D3DDMEX);
				break;
			case S_PRESENT_OCCLUDED:
				m_nDeviceStatus = D3D_OK;
				break;
			case D3DERR_DEVICEHUNG:
				CLog::Log(LOGERROR, "D3DERR_DEVICEHUNG");
				m_nDeviceStatus = D3DERR_DEVICELOST;
				m_needNewDevice = true;
				break;
			case D3DERR_OUTOFVIDEOMEMORY:
				CLog::Log(LOGERROR, "D3DERR_OUTOFVIDEOMEMORY");
				m_nDeviceStatus = D3DERR_DEVICELOST;
				m_needNewDevice = true;
				break;
			case D3DERR_DEVICEREMOVED:
				CLog::Log(LOGERROR, "D3DERR_DEVICEREMOVED");
				m_nDeviceStatus = D3DERR_DEVICELOST;
				m_needNewDevice = true;
				// fixme: also needs to re-enumerate and switch to another screen
				break;
		}
	}
	else
	{
		m_nDeviceStatus = m_pD3DDevice->TestCooperativeLevel();
	}

	if( FAILED( m_nDeviceStatus ) )
	{
		// The device has been lost but cannot be reset at this time.
		// Therefore, rendering is not possible and we'll have to return
		// and try again at a later time.
		if( m_nDeviceStatus == D3DERR_DEVICELOST )
		{
			if (m_nDeviceStatus != oldStatus)
				CLog::Log(LOGDEBUG, "D3DERR_DEVICELOST");
			OnDeviceLost();
			return false;
		}

		// The device has been lost but it can be reset at this time.
		if( m_nDeviceStatus == D3DERR_DEVICENOTRESET )
		{
			OnDeviceReset();
			if( FAILED(m_nDeviceStatus ) )
			{
				CLog::Log(LOGINFO, "m_pD3DDevice->Reset failed");
				return false;
			}
		}
	}

	HRESULT hr;

	if(FAILED(hr = m_pD3DDevice->BeginScene())) /* 启动*/
	{
		CLog::Log(LOGERROR, "m_pD3DDevice->BeginScene() failed. %s", CRenderSystemDX::GetErrorDescription(hr).c_str());
		// When XBMC caught an exception after BeginScene(), EndScene() may never been called
		// and thus all following BeginScene() will fail too.
		if(FAILED(hr = m_pD3DDevice->EndScene()))
			CLog::Log(LOGERROR, "m_pD3DDevice->EndScene() failed. %s", CRenderSystemDX::GetErrorDescription(hr).c_str());
		return false;
	}

	IDirect3DSurface9 *pBackBuffer;
	m_pD3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
	m_pD3DDevice->SetRenderTarget(0, pBackBuffer);
	pBackBuffer->Release();

	m_inScene = true;
	return true;
}

bool CRenderSystemDX::EndRender()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	m_inScene = false;

	if (!m_bRenderCreated)
		return false;

	if(m_nDeviceStatus != S_OK)
		return false;

	HRESULT hr = m_pD3DDevice->EndScene(); /* 结束*/
	if(FAILED(hr))
	{
		CLog::Log(LOGERROR, "m_pD3DDevice->EndScene() failed. %s", CRenderSystemDX::GetErrorDescription(hr).c_str());
		return false;
	}

	return true;
}

bool CRenderSystemDX::ClearBuffers(color_t color)
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

	if (!m_bRenderCreated)
		return false;

	if( FAILED( hr = m_pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, color,  1.0,  0) ) )
		return false;

	return true;
}

bool CRenderSystemDX::IsExtSupported(const char* extension)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	return false;
}

bool CRenderSystemDX::PresentRender(const CDirtyRegionList &dirty)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、见函数PresentRenderImpl  的说明，调用Direct3D  的Present  进行前后缓冲的交换
*/
	if (!m_bRenderCreated)
		return false;

	bool result = PresentRenderImpl(dirty); /* Direct3D 的present  方法被调用的过程--->3 ，搜索cyk  direct3d-->present  */

	return result;
}

void CRenderSystemDX::SetVSync(bool enable)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (m_bVSync != enable)
	{
		bool inScene(m_inScene);
		if (m_inScene)
			EndRender();
		m_bVSync = enable;
		ResetRenderSystem(m_nBackBufferWidth, m_nBackBufferHeight, m_bFullScreenDevice, m_refreshRate);
		if (inScene)
			BeginRender();
	}
}

void CRenderSystemDX::CaptureStateBlock()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!m_bRenderCreated)
		return;

	SAFE_RELEASE(m_stateBlock);
	m_pD3DDevice->CreateStateBlock(D3DSBT_ALL, &m_stateBlock);
}

void CRenderSystemDX::ApplyStateBlock()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!m_bRenderCreated)
		return;

	if (m_stateBlock)
		m_stateBlock->Apply();
}

void CRenderSystemDX::SetCameraPosition(const CPoint &camera, int screenWidth, int screenHeight)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、设定摄像机位置
		2、见函数中的代码注释
		3、详细信息请网上查找

		绘制多图元实体的一般方法是:  
			1、首先为图元设置各自的世界变换矩阵、材质、纹理( 视角矩阵、投影矩阵、灯
				光都属于全局性参数，设置一次即可)
			2、然后对该图元调用DrawPrimitive 方法
*/
	if (!m_bRenderCreated)
		return;

	// grab the viewport dimensions and location
	D3DVIEWPORT9 viewport;
	m_pD3DDevice->GetViewport(&viewport); /* 先获取viewport  */
	float w = viewport.Width*0.5f; /* 取出viewport 的中点*/
	float h = viewport.Height*0.5f; /* 取出viewport 的中点*/

	CPoint offset = camera - CPoint(screenWidth*0.5f, screenHeight*0.5f); /* 计算传入的摄像机的点与viewport 中点的偏移*/

	/* ======    世界变换========================================
		我们在建立三维实体的数学模型时，通常以实体的某一点为坐标原点，比如一个球体，
		很自然就用球心做原点，这样构成的坐标系称为本地坐标系(Local Coordinates)。实体总是位
		于某个场景(World Space) 中，而场景采用世界坐标系(World Coordinates)，因此需要把实体的本地
		坐标变换成世界坐标，这个变换被称为世界变换。

		具体变换的步骤如下:
		1、首先把实体放置在世界坐标系的原点，使两个坐标系重合
		2、在世界空间中，对实体进行平行移动，其对应的平移变换阵Tt  可由函数D3DXMatrixTranslation 求得
		3、把平移后的实体沿自身的Z 轴旋转一个角度(角度大于0  表示从Z 轴的正向朝原点看去，旋转
			方向为顺时针；反之为逆时针，下同)，对应的旋转变换阵Tz 用D3DXMatrixRotationZ 计算
		4、把实体沿自身的Y 轴旋转一个角度，用D3DXMatrixRotationY 求出变换阵Ty
		5、把实体沿自身的X 轴旋转一个角度，用D3DXMatrixRotationX 求出变换阵Tx
		6、最后对实体进行缩放，假设三个轴的缩放系数分别为Sx、Sy、Sz，该操作对应的变换
			阵Ts 可由函数D3DXMatrixScaling 求得
		7、最终的世界变换矩阵Tw = Ts * Tx * Ty * Tz * Tt ，在Direct3D 中，矩阵乘法用函数D3DXMatrixMultiply 实现，
			注意相乘顺序为操作的逆序。
	*/
	// world view.  Until this is moved onto the GPU (via a vertex shader for instance), we set it to the identity
	// here.
	D3DXMATRIX mtxWorld;
	D3DXMatrixIdentity(&mtxWorld);
	m_pD3DDevice->SetTransform(D3DTS_WORLD, &mtxWorld); /* 世界变换*/


	

	/* ======    视角变换========================================
		实体确定后，接下来要确定观察者在世界坐标系中的方位，换句话说，就是在世界坐标
		系中如何放置摄像机。观察者( 摄像机)  所看到的景象，就是Direct3D 窗口显示的内容。

		确定观察者需要三个量:
		1、观察者的坐标
		2、视线方向，为一个矢量，不过Direct3D  用视线上的一个点来替代，此时视线方向就是从
			观察者指向该目标点，这样表示更直观一些。
		3、上方向，通俗地说，就是观察者的头顶方向，用一个矢量表示。

		确定后以观察者为原点，视线为z  轴，上方向或它的一个分量为Y  轴( X 轴可由左手法则得
		出，为右方向)，这就构成了视角坐标系，我们需要把实体从世界坐标空间转换到视角坐
		标空间，这个坐标变换就称为视角变换(View Transformation)
	*/
	// camera view.  Multiply the Y coord by -1 then translate so that everything is relative to the camera
	// position.
	D3DXMATRIX flipY, translate, mtxView;
	D3DXMatrixScaling(&flipY, 1.0f, -1.0f, 1.0f);
	D3DXMatrixTranslation(&translate, -(viewport.X + w + offset.x), -(viewport.Y + h + offset.y), 2*h);
	D3DXMatrixMultiply(&mtxView, &translate, &flipY);
	m_pD3DDevice->SetTransform(D3DTS_VIEW, &mtxView); /* 视角变换*/

	/* ======    投影变换========================================
		实体转换到视角空间后，还要经过投影变换(Projection Transformation)，三维的实体才能显示在
		二维的计算机屏幕上。打个比方，如果把屏幕看做照相机中胶卷，那么投影变换就相
		当于照相机的镜头。

		Direct3D 使用透视投影变换(Perspective Transformation)，此时在视角空间中，可视区域是一个以
		视线为轴心的棱台(Viewing Frustum)。想像一下你处在一个伸手不见五指的房间里，面前有
		一扇窗户，你可以透过窗户看到各种景物。窗户就是棱台的前裁剪平面，天空、远山
		等背景是后裁剪平面，其间的可视范围是景深。投影变换把位于可视棱台内的景物投
		影到前裁剪平面，由于采用透视投影，距离观察者远的对象会变小，从而更具真实感

		透视投影变换由四个量决定:
		1、前裁剪平面的宽度w
		2、前裁剪平面的高度h
		3、前裁剪平面到原点的距离z1
		4、后裁剪平面到原点的距离z2

		由于w、h 用起来不是很直观，因此实际应用中，常用fov 和aspect 代替w、h，其中fov 是Y 
		方向上的可视角度，通常取45度；aspect 是前裁剪平面的高度与宽度之比，通常取1
		(由三角形定义，可知h = 2 * z1 * tg(fov/2) ,   w = h/aspect ) 

		用这四个量来调用函数D3DXMatrixPerspectiveFovLH，即可获得投影变换矩阵
	*/
	// projection onto screen space
	D3DXMATRIX mtxProjection;
	D3DXMatrixPerspectiveOffCenterLH(&mtxProjection, (-w - offset.x)*0.5f, (w - offset.x)*0.5f, (-h + offset.y)*0.5f, (h + offset.y)*0.5f, h, 100*h);
	m_pD3DDevice->SetTransform(D3DTS_PROJECTION, &mtxProjection); /* 投影变换*/

	m_world = mtxWorld;
	m_view = mtxView;
	m_projection = mtxProjection;
	m_viewPort = viewport;
}

void CRenderSystemDX::Project(float &x, float &y, float &z)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	D3DXVECTOR3 vScreenCoord;
	D3DXVECTOR3 vLocation(x, y, z);

	D3DXVec3Project(&vScreenCoord, &vLocation, &m_viewPort, &m_projection, &m_view, &m_world);
	x = vScreenCoord.x;
	y = vScreenCoord.y;
	z = 0;
}

bool CRenderSystemDX::TestRender()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	static unsigned int lastTime = 0;
	static float delta = 0;

	unsigned int thisTime = XbmcThreads::SystemClockMillis();

	if(thisTime - lastTime > 10)
	{
		lastTime = thisTime;
		delta++;
	}

	CLog::Log(LOGINFO, "Delta =  %d", delta);

	if(delta > m_nBackBufferWidth)
		delta = 0;

	LPDIRECT3DVERTEXBUFFER9 pVB = NULL;

	// A structure for our custom vertex type
	struct CUSTOMVERTEX
	{
		FLOAT x, y, z, rhw; // The transformed position for the vertex
		DWORD color;        // The vertex color
	};

	// Our custom FVF, which describes our custom vertex structure
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)

	// Initialize three vertices for rendering a triangle
	CUSTOMVERTEX vertices[] =
	{
		{ delta + 100.0f,  50.0f, 0.5f, 1.0f, 0xffff0000, }, // x, y, z, rhw, color
		{ delta+200.0f, 250.0f, 0.5f, 1.0f, 0xff00ff00, },
		{  delta, 250.0f, 0.5f, 1.0f, 0xff00ffff, },
	};

	// Create the vertex buffer. Here we are allocating enough memory
	// (from the default pool) to hold all our 3 custom vertices. We also
	// specify the FVF, so the vertex buffer knows what data it contains.
	if( FAILED( m_pD3DDevice->CreateVertexBuffer( 3 * sizeof( CUSTOMVERTEX ), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &pVB, NULL ) ) )
	{
		return false;
	}

	// Now we fill the vertex buffer. To do this, we need to Lock() the VB to
	// gain access to the vertices. This mechanism is required becuase vertex
	// buffers may be in device memory.
	VOID* pVertices;
	
	if( FAILED( pVB->Lock( 0, sizeof( vertices ), ( void** )&pVertices, 0 ) ) )
		return false;
	
	memcpy( pVertices, vertices, sizeof( vertices ) );
	pVB->Unlock();

	m_pD3DDevice->SetStreamSource( 0, pVB, 0, sizeof( CUSTOMVERTEX ) );
	m_pD3DDevice->SetFVF( D3DFVF_CUSTOMVERTEX );
	m_pD3DDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 1 );

	pVB->Release();

	return true;
}

void CRenderSystemDX::ApplyHardwareTransform(const TransformMatrix &finalMatrix)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!m_bRenderCreated)
		return;
}

void CRenderSystemDX::RestoreHardwareTransform()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!m_bRenderCreated)
		return;
}

void CRenderSystemDX::GetViewPort(CRect& viewPort)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、获取viewport 
*/
	if (!m_bRenderCreated)
		return;

	D3DVIEWPORT9 d3dviewport;
	m_pD3DDevice->GetViewport(&d3dviewport);

	viewPort.x1 = (float)d3dviewport.X;
	viewPort.y1 = (float)d3dviewport.Y;
	viewPort.x2 = (float)d3dviewport.X + d3dviewport.Width;
	viewPort.y2 = (float)d3dviewport.Y + d3dviewport.Height;
}

void CRenderSystemDX::SetViewPort(CRect& viewPort)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、设置viewport 
		2、Present() 是将我们的后备缓冲交换至前缓冲。而SetViewPort 相当于在后备缓冲的某个
			指定区域绘图，因此我们的Present()是将包含多个视口的后缓冲交换出去。
*/
	if (!m_bRenderCreated)
		return;

	D3DVIEWPORT9 newviewport;

	newviewport.MinZ   = 0.0f;
	newviewport.MaxZ   = 1.0f;
	newviewport.X      = (DWORD)viewPort.x1;
	newviewport.Y      = (DWORD)viewPort.y1;
	newviewport.Width  = (DWORD)(viewPort.x2 - viewPort.x1);
	newviewport.Height = (DWORD)(viewPort.y2 - viewPort.y1);
	m_pD3DDevice->SetViewport(&newviewport);
}

void CRenderSystemDX::SetScissors(const CRect& rect)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!m_bRenderCreated)
		return;

	RECT scissor;
	scissor.left   = MathUtils::round_int(rect.x1);
	scissor.top    = MathUtils::round_int(rect.y1);
	scissor.right  = MathUtils::round_int(rect.x2);
	scissor.bottom = MathUtils::round_int(rect.y2);
	m_pD3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
	m_pD3DDevice->SetScissorRect(&scissor);
}

void CRenderSystemDX::ResetScissors()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!m_bRenderCreated)
		return;

	RECT scissor;
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = m_nBackBufferWidth;
	scissor.bottom = m_nBackBufferHeight;
	m_pD3DDevice->SetScissorRect(&scissor);
	m_pD3DDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
}

void CRenderSystemDX::Register(ID3DResource *resource)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、见类CD3DTexture  的create  方法，实质就是将传入的指针对象插入到容器中
*/
	CSingleLock lock(m_resourceSection);
	m_resources.push_back(resource);
}

void CRenderSystemDX::Unregister(ID3DResource* resource)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	CSingleLock lock(m_resourceSection);
	vector<ID3DResource*>::iterator i = find(m_resources.begin(), m_resources.end(), resource);
	if (i != m_resources.end())
		m_resources.erase(i);
}

CStdString CRenderSystemDX::GetErrorDescription(HRESULT hr)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	CStdString strError;
	strError.Format("%X - %s (%s)", hr, DXGetErrorString(hr), DXGetErrorDescription(hr));

	return strError;
}

#endif
