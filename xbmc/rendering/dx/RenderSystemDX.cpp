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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	g_Direct3DCreate9Ex = (LPDIRECT3DCREATE9EX)GetProcAddress( GetModuleHandle("d3d9.dll"), "Direct3DCreate9Ex" );
	if(g_Direct3DCreate9Ex == NULL)
		return false;
	return true;
}

CRenderSystemDX::CRenderSystemDX() : CRenderSystemBase()
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
}

bool CRenderSystemDX::InitRenderSystem()
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1����ʼ����Ⱦϵͳ����CApplication::Create()  �жԴ˵ĵ���
*/
	m_bVSync = true;

	m_useD3D9Ex = (g_advancedSettings.m_AllowD3D9Ex && g_sysinfo.IsVistaOrHigher() && LoadD3D9Ex());
	m_pD3D = NULL;

	if (m_useD3D9Ex)
	{
		CLog::Log(LOGDEBUG, __FUNCTION__" - trying D3D9Ex...");
		if (FAILED(g_Direct3DCreate9Ex(D3D_SDK_VERSION, (IDirect3D9Ex**) &m_pD3D)))/* ����һ��Direct3D9 �Ľӿ�*/
		{
			CLog::Log(LOGDEBUG, __FUNCTION__" - D3D9Ex creation failure, falling back to D3D9");
			m_useD3D9Ex = false;
		}
		else
		{
			D3DCAPS9 caps;
			memset(&caps, 0, sizeof(caps));
			m_pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, m_devType, &caps); /* ��ȡ�Կ��Ĺ��ܣ����Ƿ�֧��Ӳ������*/
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
		m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);/* ����һ��Direct3D9 �Ľӿ�*/
		if(m_pD3D == NULL)
			return false;
	}

	UpdateMonitor();

	if(CreateDevice()==false) /* �����豸*/
		return false;

	return true;
}

void CRenderSystemDX::SetRenderParams(unsigned int width, unsigned int height, bool fullScreen, float refreshRate)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1���˺���ʵ�����ú󻺴�Ŀ��ߡ�ȫ���������Ϣ
*/
	m_nBackBufferWidth  = width;
	m_nBackBufferHeight = height;
	m_bFullScreenDevice = fullScreen;
	m_refreshRate       = refreshRate;
}

void CRenderSystemDX::SetMonitor(HMONITOR monitor)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1���˺���ʵ�ʾ������ô������ʾ���ľ����Ȼ���ҵ���Ӧ���Կ���Ȼ��
			��m_adapter  ������Կ�����Ӧ�����
*/
	if (!m_pD3D)
		return;

	// find the appropriate screen
	for (unsigned int adapter = 0; adapter < m_pD3D->GetAdapterCount(); adapter++)/* ����Կ�������*/
	{
		HMONITOR hMonitor = m_pD3D->GetAdapterMonitor(adapter);/* �����Ӧ����ʾ��*/
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	HMONITOR hMonitor = MonitorFromWindow(m_hDeviceWnd, MONITOR_DEFAULTTONULL);
	if (hMonitor)
		SetMonitor(hMonitor);

	SetRenderParams(width, height, fullScreen, refreshRate);

	CRect rc;
	rc.SetRect(0, 0, (float)width, (float)height);
	SetViewPort(rc);/* ������˵��*/

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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	if (!m_bRenderCreated)
		return;

	HMONITOR currentMonitor = m_pD3D->GetAdapterMonitor(m_adapter);/* �����Կ�����ŷ��ص�ǰ����ʾ������SetMonitor ����˵��*/
	HMONITOR newMonitor = MonitorFromWindow(m_hDeviceWnd, MONITOR_DEFAULTTONULL);
	if (newMonitor != NULL && currentMonitor != newMonitor)
		ResetRenderSystem(m_nBackBufferWidth, m_nBackBufferHeight, m_bFullScreenDevice, m_refreshRate);
}


bool CRenderSystemDX::IsSurfaceFormatOk(D3DFORMAT surfFormat, DWORD usage)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1���˺����൱�ڴ�����directx �豸��������m_pD3DDevice  ��
*/
	// Code based on Ogre 3D engine
	CSingleLock lock(m_resourceSection);

	HRESULT hr;

	if(m_pD3D == NULL)
		return false;

	if(m_hDeviceWnd == NULL)
		return false;

	CLog::Log(LOGDEBUG, __FUNCTION__" on adapter %d", m_adapter);

	BuildPresentParameters(); /* ����present  ����*/

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
		/* �����豸*/
		hr = ((IDirect3D9Ex*)m_pD3D)->CreateDeviceEx(	m_adapter, 
													m_devType, 
													m_hFocusWnd, /* ����һ�����ھ��������directx �豸��˴��ڰ���*/
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
		/* �����豸*/
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

	if(m_pD3D->GetAdapterIdentifier(m_adapter, 0, &m_AIdentifier) == D3D_OK) /* ���Ĭ���Կ�����Ϣ��������m_AIdentifier ��*/
	{
		m_RenderRenderer = (const char*)m_AIdentifier.Description;
		m_RenderVendor   = (const char*)m_AIdentifier.Driver;
		m_RenderVersion.Format("%d.%d.%d.%04d", HIWORD(m_AIdentifier.DriverVersion.HighPart), LOWORD(m_AIdentifier.DriverVersion.HighPart),
		                        			HIWORD(m_AIdentifier.DriverVersion.LowPart) , LOWORD(m_AIdentifier.DriverVersion.LowPart));
	}

	CLog::Log(LOGDEBUG, __FUNCTION__" - adapter %d: %s, %s, VendorId %lu, DeviceId %lu", m_adapter, m_AIdentifier.Driver, m_AIdentifier.Description, m_AIdentifier.VendorId, m_AIdentifier.DeviceId);

	// get our render capabilities
	// re-read caps, there may be changes depending on the vertex processing type
	m_pD3DDevice->GetDeviceCaps(&caps); /* ��ȡ��Ⱦ��������*/

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
	���������������:
	����ӳ�䵽��Ļ�е��������ϡ�ͨ�����������κ���Ļ�������ǲ�һ����ġ���
	���������α���Ļ������Сʱ�����������λᱻ�ʵ��Ŵ󡣵����������α���Ļ��
	���δ�ʱ�����������λᱻ�ʵ���С��������������ζ�����֣�����( filtering )  ��һ
	��direct3d  ������������Щ�α��ƽ��������
*/
	m_pD3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR ); /* �趨���������ģʽ*/
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
	����:
		1��dirty : �˲����ڴ˺�����û��ʹ��
		
	����:
		1��
		
	˵��:
		1���˺���ʵ�ʾ��ǵ���Direct3D  ��Present  ����ǰ�󻺳�Ľ���������Ľ�
			�󻺳���ʾ����ʾ������
		2������direct3d-->present  ���Ը��ٴ˺����ı����ù���
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

	hr = m_pD3DDevice->Present( NULL, NULL, 0, NULL ); /* Direct3D ��present  ���������õĹ���--->4 ������cyk  direct3d-->present  */

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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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

	if(FAILED(hr = m_pD3DDevice->BeginScene())) /* ����*/
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	m_inScene = false;

	if (!m_bRenderCreated)
		return false;

	if(m_nDeviceStatus != S_OK)
		return false;

	HRESULT hr = m_pD3DDevice->EndScene(); /* ����*/
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return false;
}

bool CRenderSystemDX::PresentRender(const CDirtyRegionList &dirty)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��������PresentRenderImpl  ��˵��������Direct3D  ��Present  ����ǰ�󻺳�Ľ���
*/
	if (!m_bRenderCreated)
		return false;

	bool result = PresentRenderImpl(dirty); /* Direct3D ��present  ���������õĹ���--->3 ������cyk  direct3d-->present  */

	return result;
}

void CRenderSystemDX::SetVSync(bool enable)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	if (!m_bRenderCreated)
		return;

	SAFE_RELEASE(m_stateBlock);
	m_pD3DDevice->CreateStateBlock(D3DSBT_ALL, &m_stateBlock);
}

void CRenderSystemDX::ApplyStateBlock()
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	if (!m_bRenderCreated)
		return;

	if (m_stateBlock)
		m_stateBlock->Apply();
}

void CRenderSystemDX::SetCameraPosition(const CPoint &camera, int screenWidth, int screenHeight)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1���趨�����λ��
		2���������еĴ���ע��
		3����ϸ��Ϣ�����ϲ���

		���ƶ�ͼԪʵ���һ�㷽����:  
			1������ΪͼԪ���ø��Ե�����任���󡢲��ʡ�����( �ӽǾ���ͶӰ���󡢵�
				�ⶼ����ȫ���Բ���������һ�μ���)
			2��Ȼ��Ը�ͼԪ����DrawPrimitive ����
*/
	if (!m_bRenderCreated)
		return;

	// grab the viewport dimensions and location
	D3DVIEWPORT9 viewport;
	m_pD3DDevice->GetViewport(&viewport); /* �Ȼ�ȡviewport  */
	float w = viewport.Width*0.5f; /* ȡ��viewport ���е�*/
	float h = viewport.Height*0.5f; /* ȡ��viewport ���е�*/

	CPoint offset = camera - CPoint(screenWidth*0.5f, screenHeight*0.5f); /* ���㴫���������ĵ���viewport �е��ƫ��*/

	/* ======    ����任========================================
		�����ڽ�����άʵ�����ѧģ��ʱ��ͨ����ʵ���ĳһ��Ϊ����ԭ�㣬����һ�����壬
		����Ȼ����������ԭ�㣬�������ɵ�����ϵ��Ϊ��������ϵ(Local Coordinates)��ʵ������λ
		��ĳ������(World Space) �У�������������������ϵ(World Coordinates)�������Ҫ��ʵ��ı���
		����任���������꣬����任����Ϊ����任��

		����任�Ĳ�������:
		1�����Ȱ�ʵ���������������ϵ��ԭ�㣬ʹ��������ϵ�غ�
		2��������ռ��У���ʵ�����ƽ���ƶ������Ӧ��ƽ�Ʊ任��Tt  ���ɺ���D3DXMatrixTranslation ���
		3����ƽ�ƺ��ʵ���������Z ����תһ���Ƕ�(�Ƕȴ���0  ��ʾ��Z �������ԭ�㿴ȥ����ת
			����Ϊ˳ʱ�룻��֮Ϊ��ʱ�룬��ͬ)����Ӧ����ת�任��Tz ��D3DXMatrixRotationZ ����
		4����ʵ���������Y ����תһ���Ƕȣ���D3DXMatrixRotationY ����任��Ty
		5����ʵ���������X ����תһ���Ƕȣ���D3DXMatrixRotationX ����任��Tx
		6������ʵ��������ţ����������������ϵ���ֱ�ΪSx��Sy��Sz���ò�����Ӧ�ı任
			��Ts ���ɺ���D3DXMatrixScaling ���
		7�����յ�����任����Tw = Ts * Tx * Ty * Tz * Tt ����Direct3D �У�����˷��ú���D3DXMatrixMultiply ʵ�֣�
			ע�����˳��Ϊ����������
	*/
	// world view.  Until this is moved onto the GPU (via a vertex shader for instance), we set it to the identity
	// here.
	D3DXMATRIX mtxWorld;
	D3DXMatrixIdentity(&mtxWorld);
	m_pD3DDevice->SetTransform(D3DTS_WORLD, &mtxWorld); /* ����任*/


	

	/* ======    �ӽǱ任========================================
		ʵ��ȷ���󣬽�����Ҫȷ���۲�������������ϵ�еķ�λ�����仰˵����������������
		ϵ����η�����������۲���( �����)  �������ľ��󣬾���Direct3D ������ʾ�����ݡ�

		ȷ���۲�����Ҫ������:
		1���۲��ߵ�����
		2�����߷���Ϊһ��ʸ��������Direct3D  �������ϵ�һ�������������ʱ���߷�����Ǵ�
			�۲���ָ���Ŀ��㣬������ʾ��ֱ��һЩ��
		3���Ϸ���ͨ�׵�˵�����ǹ۲��ߵ�ͷ��������һ��ʸ����ʾ��

		ȷ�����Թ۲���Ϊԭ�㣬����Ϊz  �ᣬ�Ϸ��������һ������ΪY  ��( X ��������ַ����
		����Ϊ�ҷ���)����͹������ӽ�����ϵ��������Ҫ��ʵ�����������ռ�ת�����ӽ���
		��ռ䣬�������任�ͳ�Ϊ�ӽǱ任(View Transformation)
	*/
	// camera view.  Multiply the Y coord by -1 then translate so that everything is relative to the camera
	// position.
	D3DXMATRIX flipY, translate, mtxView;
	D3DXMatrixScaling(&flipY, 1.0f, -1.0f, 1.0f);
	D3DXMatrixTranslation(&translate, -(viewport.X + w + offset.x), -(viewport.Y + h + offset.y), 2*h);
	D3DXMatrixMultiply(&mtxView, &translate, &flipY);
	m_pD3DDevice->SetTransform(D3DTS_VIEW, &mtxView); /* �ӽǱ任*/

	/* ======    ͶӰ�任========================================
		ʵ��ת�����ӽǿռ�󣬻�Ҫ����ͶӰ�任(Projection Transformation)����ά��ʵ�������ʾ��
		��ά�ļ������Ļ�ϡ�����ȷ����������Ļ����������н�����ôͶӰ�任����
		����������ľ�ͷ��

		Direct3D ʹ��͸��ͶӰ�任(Perspective Transformation)����ʱ���ӽǿռ��У�����������һ����
		����Ϊ���ĵ���̨(Viewing Frustum)������һ���㴦��һ�����ֲ�����ָ�ķ������ǰ��
		һ�ȴ����������͸�������������־������������̨��ǰ�ü�ƽ�棬��ա�Զɽ
		�ȱ����Ǻ�ü�ƽ�棬���Ŀ��ӷ�Χ�Ǿ��ͶӰ�任��λ�ڿ�����̨�ڵľ���Ͷ
		Ӱ��ǰ�ü�ƽ�棬���ڲ���͸��ͶӰ������۲���Զ�Ķ�����С���Ӷ�������ʵ��

		͸��ͶӰ�任���ĸ�������:
		1��ǰ�ü�ƽ��Ŀ��w
		2��ǰ�ü�ƽ��ĸ߶�h
		3��ǰ�ü�ƽ�浽ԭ��ľ���z1
		4����ü�ƽ�浽ԭ��ľ���z2

		����w��h ���������Ǻ�ֱ�ۣ����ʵ��Ӧ���У�����fov ��aspect ����w��h������fov ��Y 
		�����ϵĿ��ӽǶȣ�ͨ��ȡ45�ȣ�aspect ��ǰ�ü�ƽ��ĸ߶�����֮�ȣ�ͨ��ȡ1
		(�������ζ��壬��֪h = 2 * z1 * tg(fov/2) ,   w = h/aspect ) 

		�����ĸ��������ú���D3DXMatrixPerspectiveFovLH�����ɻ��ͶӰ�任����
	*/
	// projection onto screen space
	D3DXMATRIX mtxProjection;
	D3DXMatrixPerspectiveOffCenterLH(&mtxProjection, (-w - offset.x)*0.5f, (w - offset.x)*0.5f, (-h + offset.y)*0.5f, (h + offset.y)*0.5f, h, 100*h);
	m_pD3DDevice->SetTransform(D3DTS_PROJECTION, &mtxProjection); /* ͶӰ�任*/

	m_world = mtxWorld;
	m_view = mtxView;
	m_projection = mtxProjection;
	m_viewPort = viewport;
}

void CRenderSystemDX::Project(float &x, float &y, float &z)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	if (!m_bRenderCreated)
		return;
}

void CRenderSystemDX::RestoreHardwareTransform()
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	if (!m_bRenderCreated)
		return;
}

void CRenderSystemDX::GetViewPort(CRect& viewPort)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1����ȡviewport 
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1������viewport 
		2��Present() �ǽ����ǵĺ󱸻��彻����ǰ���塣��SetViewPort �൱���ں󱸻����ĳ��
			ָ�������ͼ��������ǵ�Present()�ǽ���������ӿڵĺ󻺳彻����ȥ��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1������CD3DTexture  ��create  ������ʵ�ʾ��ǽ������ָ�������뵽������
*/
	CSingleLock lock(m_resourceSection);
	m_resources.push_back(resource);
}

void CRenderSystemDX::Unregister(ID3DResource* resource)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	CSingleLock lock(m_resourceSection);
	vector<ID3DResource*>::iterator i = find(m_resources.begin(), m_resources.end(), resource);
	if (i != m_resources.end())
		m_resources.erase(i);
}

CStdString CRenderSystemDX::GetErrorDescription(HRESULT hr)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	CStdString strError;
	strError.Format("%X - %s (%s)", hr, DXGetErrorString(hr), DXGetErrorDescription(hr));

	return strError;
}

#endif
