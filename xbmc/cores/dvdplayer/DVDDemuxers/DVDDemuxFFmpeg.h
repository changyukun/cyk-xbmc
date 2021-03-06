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

#include "DVDDemux.h"
#include "DllAvFormat.h"
#include "DllAvCodec.h"
#include "DllAvUtil.h"

#include "threads/CriticalSection.h"
#include "threads/SystemClock.h"

class CDVDDemuxFFmpeg;

class CDemuxStreamVideoFFmpeg
  : public CDemuxStreamVideo
{
  CDVDDemuxFFmpeg *m_parent;
  AVStream*        m_stream;
public:
  CDemuxStreamVideoFFmpeg(CDVDDemuxFFmpeg *parent, AVStream* stream)
    : m_parent(parent)
    , m_stream(stream)
  {}
  virtual void GetStreamInfo(std::string& strInfo);
};


class CDemuxStreamAudioFFmpeg
  : public CDemuxStreamAudio
{
  CDVDDemuxFFmpeg *m_parent;
  AVStream*        m_stream;
public:
  CDemuxStreamAudioFFmpeg(CDVDDemuxFFmpeg *parent, AVStream* stream)
    : m_parent(parent)
    , m_stream(stream)
  {}
  std::string m_description;

  virtual void GetStreamInfo(std::string& strInfo);
  virtual void GetStreamName(std::string& strInfo);
};

class CDemuxStreamSubtitleFFmpeg
  : public CDemuxStreamSubtitle
{
  CDVDDemuxFFmpeg *m_parent;
  AVStream*        m_stream;
public:
  CDemuxStreamSubtitleFFmpeg(CDVDDemuxFFmpeg *parent, AVStream* stream)
    : m_parent(parent)
    , m_stream(stream)
  {}
  std::string m_description;

  virtual void GetStreamInfo(std::string& strInfo);
  virtual void GetStreamName(std::string& strInfo);

};

#define FFMPEG_FILE_BUFFER_SIZE   32768 // default reading size for ffmpeg
#define FFMPEG_DVDNAV_BUFFER_SIZE 2048  // for dvd's

class CDVDDemuxFFmpeg : public CDVDDemux
{
public:
	CDVDDemuxFFmpeg();
	virtual ~CDVDDemuxFFmpeg();

	bool Open(CDVDInputStream* pInput);
	void Dispose();
	void Reset();
	void Flush();
	void Abort();
	void SetSpeed(int iSpeed);
	virtual std::string GetFileName();

	DemuxPacket* Read();

	bool SeekTime(int time, bool backwords = false, double* startpts = NULL);
	bool SeekByte(__int64 pos);
	int GetStreamLength();
	CDemuxStream* GetStream(int iStreamId);
	int GetNrOfStreams();

	bool SeekChapter(int chapter, double* startpts = NULL);
	int GetChapterCount();
	int GetChapter();
	void GetChapterName(std::string& strChapterName);
	virtual void GetStreamCodecName(int iStreamId, CStdString &strName);

	bool Aborted();

	AVFormatContext* m_pFormatContext;	/* 
											通过调用m_dllAvFormat->av_open_input_stream()  方法对输入的input 进行分析后
											得到此内容的值，见open 方法 
										*/

protected:
	friend class CDemuxStreamAudioFFmpeg;
	friend class CDemuxStreamVideoFFmpeg;
	friend class CDemuxStreamSubtitleFFmpeg;

	int ReadFrame(AVPacket *packet);
	void AddStream(int iId);

	double ConvertTimestamp(int64_t pts, int den, int num);
	void UpdateCurrentPTS();

	CCriticalSection m_critSection;
#define MAX_STREAMS 100
	CDemuxStream* m_streams[MAX_STREAMS];/* 保存各个流的信息，即流信息的数组，一个id 流占用数组的一个单元*/             // maximum number of streams that ffmpeg can handle

	
	ByteIOContext* m_ioContext; 	/* 
									视频流的文件与ffmpeg 动态库之间的描述数据结构，见open 方法中
									对m_dllAvFormat->av_alloc_put_byte()  的调用，即传入了文件的read、seek、buffer 等
									参数将视频文件(实质是文件cache) 与ffmpeg 关联
								*/

	
	
	DllAvFormat m_dllAvFormat; 	
	DllAvCodec  m_dllAvCodec;
	DllAvUtil   m_dllAvUtil;

	double   m_iCurrentPts; // used for stream length estimation
	bool     m_bMatroska;
	bool     m_bAVI;
	int      m_speed;
	unsigned m_program;
	XbmcThreads::EndTime  m_timeout;

	CDVDInputStream* m_pInput; /* 保存输入模块的实例，见open 方法说明*/
};

