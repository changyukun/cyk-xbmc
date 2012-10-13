#pragma once
/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://xbmc.org
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

#include "tinyXML/tinyxml.h"
#include "cores/IPlayer.h"
#include "PlayerCoreFactory.h"
#include "cores/dvdplayer/DVDPlayer.h"
#include "cores/paplayer/PAPlayer.h"
#include "cores/ExternalPlayer/ExternalPlayer.h"
#include "utils/log.h"

class CPlayerCoreConfig
{
friend class CPlayerCoreFactory;

public:
	CPlayerCoreConfig(CStdString name, const EPLAYERCORES eCore, const TiXmlElement* pConfig)
	{
		m_name = name;
		m_eCore = eCore;
		m_bPlaysAudio = false;
		m_bPlaysVideo = false;

		if (pConfig)
		{
			m_config = (TiXmlElement*)pConfig->Clone();
			const char *szAudio = pConfig->Attribute("audio");
			const char *szVideo = pConfig->Attribute("video");
			m_bPlaysAudio = szAudio && stricmp(szAudio, "true") == 0;
			m_bPlaysVideo = szVideo && stricmp(szVideo, "true") == 0;
		}
		else
		{
			m_config = NULL;
		}
		CLog::Log(LOGDEBUG, "CPlayerCoreConfig::<ctor>: created player %s for core %d", m_name.c_str(), m_eCore);
	}

	virtual ~CPlayerCoreConfig()
	{
		SAFE_DELETE(m_config);
	}

	const CStdString& GetName() const
	{
		return m_name;
	}

	IPlayer* CreatePlayer(IPlayerCallback& callback) const
	{
	/*
		参数:
			1、callback : 传入一个IPlayerCallback 的实例，通常就是CApplication 类的实例
			
		返回:
			1、
			
		说明:
			1、实质就是根据此类实例的域成员m_eCore 的类型来创建不同的播放器，并进行初始化操作
	*/
		IPlayer* pPlayer;
		switch(m_eCore)
		{
			case EPC_MPLAYER:
			case EPC_DVDPLAYER: 
				pPlayer = new CDVDPlayer(callback);
				CLog::Log(LOGDEBUG,"==========>>> cyk CDVDPlayer(callback)");
				break;
				
			case EPC_PAPLAYER: 
				pPlayer = new PAPlayer(callback); 
				CLog::Log(LOGDEBUG,"==========>>> cyk PAPlayer(callback)");
				break;
				
			case EPC_EXTPLAYER: 
				pPlayer = new CExternalPlayer(callback); 
				CLog::Log(LOGDEBUG,"==========>>> cyk CExternalPlayer(callback)");
				break;
				
			default: 
				return NULL;
		}

		if (pPlayer->Initialize(m_config))
		{
			return pPlayer;
		}
		else
		{
			SAFE_DELETE(pPlayer);
			return NULL;
		}
	}

private:
	CStdString m_name;
	bool m_bPlaysAudio;
	bool m_bPlaysVideo;
	EPLAYERCORES m_eCore;
	TiXmlElement* m_config;
};
