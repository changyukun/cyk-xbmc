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
#include "DVDFactoryInputStream.h"
#include "DVDInputStream.h"
#include "DVDInputStreamFile.h"
#include "DVDInputStreamNavigator.h"
#include "DVDInputStreamHttp.h"
#include "DVDInputStreamFFmpeg.h"
#include "DVDInputStreamTV.h"
#include "DVDInputStreamRTMP.h"
#ifdef HAVE_LIBBLURAY
#include "DVDInputStreamBluray.h"
#endif
#ifdef HAS_FILESYSTEM_HTSP
#include "DVDInputStreamHTSP.h"
#endif
#ifdef ENABLE_DVDINPUTSTREAM_STACK
#include "DVDInputStreamStack.h"
#endif
#include "FileItem.h"
#include "storage/MediaManager.h"

#include "utils/log.h" /* --- cyk add --------------*/

CDVDInputStream* CDVDFactoryInputStream::CreateInputStream(IDVDPlayer* pPlayer, const std::string& file, const std::string& content)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	CLog::Log(LOGDEBUG,"==========>>> cyk CDVDFactoryInputStream::CreateInputStream    file :: %s", file.c_str());


	CFileItem item(file.c_str(), false);
	if (content != "bluray/iso" && (item.IsDVDFile(false, true) || item.IsDVDImage() ||
#ifdef HAS_DVD_DRIVE
				file.compare(g_mediaManager.TranslateDevicePath("")) == 0 ))
#else
				0 ))
#endif
	{
		CLog::Log(LOGDEBUG,"==========>>> cyk CDVDFactoryInputStream::CreateInputStream 0");
		return (new CDVDInputStreamNavigator(pPlayer));
	}
#ifdef HAVE_LIBBLURAY
	else if (item.IsType(".bdmv") || item.IsType(".mpls") || content == "bluray/iso")
		return new CDVDInputStreamBluray();
#endif
	else if(file.substr(0, 6) == "rtp://"
					|| file.substr(0, 7) == "rtsp://"
					|| file.substr(0, 6) == "sdp://"
					|| file.substr(0, 6) == "udp://"
					|| file.substr(0, 6) == "tcp://"
					|| file.substr(0, 6) == "mms://"
					|| file.substr(0, 7) == "mmst://"
					|| file.substr(0, 7) == "mmsh://")
	{
		CLog::Log(LOGDEBUG,"==========>>> cyk CDVDFactoryInputStream::CreateInputStream 1");
		return new CDVDInputStreamFFmpeg();
	}
	else if(file.substr(0, 8) == "sling://"
					|| file.substr(0, 7) == "myth://"
					|| file.substr(0, 8) == "cmyth://"
					|| file.substr(0, 8) == "gmyth://"
					|| file.substr(0, 6) == "vtp://")
	{
		CLog::Log(LOGDEBUG,"==========>>> cyk CDVDFactoryInputStream::CreateInputStream 2");
		return new CDVDInputStreamTV();
	}
#ifdef ENABLE_DVDINPUTSTREAM_STACK
	else if(file.substr(0, 8) == "stack://")
	{
		CLog::Log(LOGDEBUG,"==========>>> cyk CDVDFactoryInputStream::CreateInputStream 3");
		return new CDVDInputStreamStack();
	}
#endif
#ifdef HAS_LIBRTMP
	else if(file.substr(0, 7) == "rtmp://"
					|| file.substr(0, 8) == "rtmpt://"
					|| file.substr(0, 8) == "rtmpe://"
					|| file.substr(0, 9) == "rtmpte://"
					|| file.substr(0, 8) == "rtmps://")
	{
		CLog::Log(LOGDEBUG,"==========>>> cyk CDVDFactoryInputStream::CreateInputStream 4");
		return new CDVDInputStreamRTMP();
	}
#endif
#ifdef HAS_FILESYSTEM_HTSP
	else if(file.substr(0, 7) == "htsp://")
	{
		CLog::Log(LOGDEBUG,"==========>>> cyk CDVDFactoryInputStream::CreateInputStream 5");
		return new CDVDInputStreamHTSP();
	}
#endif

	// our file interface handles all these types of streams

	CLog::Log(LOGDEBUG,"==========>>> cyk CDVDFactoryInputStream::CreateInputStream 6"); /* 播放文件走的这里*/
	return (new CDVDInputStreamFile());/* 创建此实例的时候会调用基类的构造函数对inputstream 实例的类型设定为DVDSTREAM_TYPE_FILE */
}
