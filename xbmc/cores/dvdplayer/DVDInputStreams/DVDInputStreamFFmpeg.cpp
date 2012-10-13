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

#include "DVDInputStreamFFmpeg.h"

using namespace XFILE;

CDVDInputStreamFFmpeg::CDVDInputStreamFFmpeg()
  : CDVDInputStream(DVDSTREAM_TYPE_FFMPEG)
{

}

CDVDInputStreamFFmpeg::~CDVDInputStreamFFmpeg()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	Close();
}

bool CDVDInputStreamFFmpeg::IsEOF()
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

bool CDVDInputStreamFFmpeg::Open(const char* strFile, const std::string& content)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (!CDVDInputStream::Open(strFile, content))
		return false;

	return true;
}

// close file and reset everyting
void CDVDInputStreamFFmpeg::Close()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	CDVDInputStream::Close();
}

int CDVDInputStreamFFmpeg::Read(BYTE* buf, int buf_size)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	return -1;
}

__int64 CDVDInputStreamFFmpeg::GetLength()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	return 0;
}

__int64 CDVDInputStreamFFmpeg::Seek(__int64 offset, int whence)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	return -1;
}

