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
#if (defined HAVE_CONFIG_H) && (!defined WIN32)
  #include "config.h"
#endif
#include "DVDVideoCodecFFmpeg.h"
#include "DVDDemuxers/DVDDemux.h"
#include "DVDStreamInfo.h"
#include "DVDClock.h"
#include "DVDCodecs/DVDCodecs.h"
#include "../../../../utils/Win32Exception.h"
#if defined(_LINUX) || defined(_WIN32)
#include "utils/CPUInfo.h"
#endif
#include "settings/AdvancedSettings.h"
#include "settings/GUISettings.h"
#include "utils/log.h"
#include "boost/shared_ptr.hpp"
#include "threads/Atomics.h"

#ifndef _LINUX
#define RINT(x) ((x) >= 0 ? ((int)((x) + 0.5)) : ((int)((x) - 0.5)))
#else
#include <math.h>
#define RINT lrint
#endif

#include "cores/VideoRenderers/RenderManager.h"

#ifdef HAVE_LIBVDPAU
#include "VDPAU.h"
#endif
#ifdef HAS_DX
#include "DXVA.h"
#endif
#ifdef HAVE_LIBVA
#include "VAAPI.h"
#endif

using namespace boost;

enum PixelFormat CDVDVideoCodecFFmpeg::GetFormat( struct AVCodecContext * avctx , const PixelFormat * fmt )
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	CDVDVideoCodecFFmpeg* ctx  = (CDVDVideoCodecFFmpeg*)avctx->opaque;

	if(!ctx->IsHardwareAllowed()) /* 如果不是硬解，则获取第一个软解的像素格式对应的数据结构类型*/
		return ctx->m_dllAvCodec.avcodec_default_get_format(avctx, fmt);

	/*
		程序执行到此处时应该是确定需要硬解了
	*/

	const PixelFormat * cur = fmt;
	while(*cur != PIX_FMT_NONE)
	{
#ifdef HAVE_LIBVDPAU
		if(CVDPAU::IsVDPAUFormat(*cur) && g_guiSettings.GetBool("videoplayer.usevdpau"))
		{
			if(ctx->GetHardware())
				return *cur;

			CLog::Log(LOGNOTICE,"CDVDVideoCodecFFmpeg::GetFormat - Creating VDPAU(%ix%i)", avctx->width, avctx->height);
			CVDPAU* vdp = new CVDPAU();
			if(vdp->Open(avctx, *cur))
			{
				ctx->SetHardware(vdp);
				return *cur;
			}
			else
				vdp->Release();
		}
#endif
#ifdef HAS_DX
		if(DXVA::CDecoder::Supports(*cur) && g_guiSettings.GetBool("videoplayer.usedxva2"))
		{
			DXVA::CDecoder* dec = new DXVA::CDecoder();
			if(dec->Open(avctx, *cur, ctx->m_uSurfacesCount))
			{
				ctx->SetHardware(dec);
				return *cur;
			}
			else
				dec->Release();
		}
#endif
#ifdef HAVE_LIBVA
		// mpeg4 vaapi decoding is disabled
		if(*cur == PIX_FMT_VAAPI_VLD && g_guiSettings.GetBool("videoplayer.usevaapi") && (avctx->codec_id != CODEC_ID_MPEG4 || g_advancedSettings.m_videoAllowMpeg4VAAPI)) 
		{
			VAAPI::CDecoder* dec = new VAAPI::CDecoder();
			if(dec->Open(avctx, *cur))
			{
				ctx->SetHardware(dec);
				return *cur;
			}
			else
				dec->Release();
		}
#endif
		cur++;
	}
	return ctx->m_dllAvCodec.avcodec_default_get_format(avctx, fmt);
}

CDVDVideoCodecFFmpeg::CDVDVideoCodecFFmpeg() : CDVDVideoCodec()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	m_pCodecContext = NULL;
	m_pConvertFrame = NULL;
	m_pFrame = NULL;
	m_pFilterGraph  = NULL;
	m_pFilterIn     = NULL;
	m_pFilterOut    = NULL;
	m_pFilterLink   = NULL;

	m_iPictureWidth = 0;
	m_iPictureHeight = 0;

	m_uSurfacesCount = 0;

	m_iScreenWidth = 0;
	m_iScreenHeight = 0;
	m_bSoftware = false;
	m_pHardware = NULL;
	m_iLastKeyframe = 0;
	m_dts = DVD_NOPTS_VALUE;
	m_started = false;
}

CDVDVideoCodecFFmpeg::~CDVDVideoCodecFFmpeg()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	Dispose();
}

bool CDVDVideoCodecFFmpeg::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、此函数内实现了真正的解码线程的创建
*/
	AVCodec* pCodec;

	if(!m_dllAvUtil.Load() || !m_dllAvCodec.Load() || !m_dllSwScale.Load() || !m_dllAvFilter.Load()) 
		return false;

	m_dllAvCodec.avcodec_register_all(); /* 注册所有的视频解码器*/
	
	m_dllAvFilter.avfilter_register_all(); /* 注册所有的视频filter */

	m_bSoftware     = hints.software;
	
	m_pCodecContext = m_dllAvCodec.avcodec_alloc_context(); /* 见ffmpeg/libavcodec/option.c  文件中定义*/

	pCodec = NULL;

	if (hints.codec == CODEC_ID_H264)
	{
		switch(hints.profile)
		{
			case FF_PROFILE_H264_HIGH_10:
			case FF_PROFILE_H264_HIGH_10_INTRA:
			case FF_PROFILE_H264_HIGH_422:
			case FF_PROFILE_H264_HIGH_422_INTRA:
			case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
			case FF_PROFILE_H264_HIGH_444_INTRA:
			case FF_PROFILE_H264_CAVLC_444:
				m_bSoftware = true;
				break;
		}
	}

#ifdef HAVE_LIBVDPAU
	if(g_guiSettings.GetBool("videoplayer.usevdpau") && !m_bSoftware)
	{
		while((pCodec = m_dllAvCodec.av_codec_next(pCodec))) 
		{
			if(pCodec->id == hints.codec && pCodec->capabilities & CODEC_CAP_HWACCEL_VDPAU) /* 支持硬解*/
			{
				if ((pCodec->id == CODEC_ID_MPEG4) && !g_advancedSettings.m_videoAllowMpeg4VDPAU)
					continue;

				CLog::Log(LOGNOTICE,"CDVDVideoCodecFFmpeg::Open() Creating VDPAU(%ix%i, %d)",hints.width, hints.height, hints.codec);
				CVDPAU* vdp = new CVDPAU();
				m_pCodecContext->codec_id = hints.codec;
				m_pCodecContext->width    = hints.width;
				m_pCodecContext->height   = hints.height;
				m_pCodecContext->coded_width   = hints.width;
				m_pCodecContext->coded_height  = hints.height;
				if(vdp->Open(m_pCodecContext, pCodec->pix_fmts ? pCodec->pix_fmts[0] : PIX_FMT_NONE))
				{
					m_pHardware = vdp;
					m_pCodecContext->codec_id = CODEC_ID_NONE; // ffmpeg will complain if this has been set
					break;
				}
				m_pCodecContext->codec_id = CODEC_ID_NONE; // ffmpeg will complain if this has been set
				CLog::Log(LOGNOTICE,"CDVDVideoCodecFFmpeg::Open() Failed to get VDPAU device");
				vdp->Release();
			}
		}
	}
#endif

	if(pCodec == NULL)
		pCodec = m_dllAvCodec.avcodec_find_decoder(hints.codec); /* 在ffmpeg  注册的所有解码器数据结构中查找相匹配的那个*/

	if(pCodec == NULL)/* 没找到对应的解码器*/
	{
		CLog::Log(LOGDEBUG,"CDVDVideoCodecFFmpeg::Open() Unable to find codec %d", hints.codec);
		return false;
	}

	CLog::Log(LOGNOTICE,"CDVDVideoCodecFFmpeg::Open() Using codec: %s",pCodec->long_name ? pCodec->long_name : pCodec->name);

	/* 对数据结构进行相应的填充*/
	m_pCodecContext->opaque = (void*)this; /* 保存此CDVDVideoCodecFFmpeg  的实例，即此值为CDVDPlayerVideo::m_pVideoCodec  */
	m_pCodecContext->debug_mv = 0;
	m_pCodecContext->debug = 0;
	m_pCodecContext->workaround_bugs = FF_BUG_AUTODETECT;
	m_pCodecContext->get_format = GetFormat; /* 设定获取解码格式的函数，硬解的解码器就是在这里面创建的*/
	m_pCodecContext->codec_tag = hints.codec_tag;

#if defined(__APPLE__) && defined(__arm__)
	// ffmpeg with enabled neon will crash and burn if this is enabled
	m_pCodecContext->flags &= CODEC_FLAG_EMU_EDGE;
#else
	if (pCodec->id != CODEC_ID_H264 && pCodec->capabilities & CODEC_CAP_DR1
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52,69,0)
				&& pCodec->id != CODEC_ID_VP8
#endif
	)
		m_pCodecContext->flags |= CODEC_FLAG_EMU_EDGE;
#endif

	/* 保存解码图片的宽、高*/
	// if we don't do this, then some codecs seem to fail.
	m_pCodecContext->coded_height = hints.height;
	m_pCodecContext->coded_width = hints.width;

	if( hints.extradata && hints.extrasize > 0 ) /* 为扩展数据分配内粗，并保存数据*/
	{
		m_pCodecContext->extradata_size = hints.extrasize;
		m_pCodecContext->extradata = (uint8_t*)m_dllAvUtil.av_mallocz(hints.extrasize + FF_INPUT_BUFFER_PADDING_SIZE);
		memcpy(m_pCodecContext->extradata, hints.extradata, hints.extrasize);
	}

	// set acceleration
	m_pCodecContext->dsp_mask = 0;//FF_MM_FORCE | FF_MM_MMX | FF_MM_MMXEXT | FF_MM_SSE;

	// advanced setting override for skip loop filter (see avcodec.h for valid options)
	// TODO: allow per video setting?
	if (g_advancedSettings.m_iSkipLoopFilter != 0)
	{
		m_pCodecContext->skip_loop_filter = (AVDiscard)g_advancedSettings.m_iSkipLoopFilter;
	}

	// set any special options
	for(CDVDCodecOptions::iterator it = options.begin(); it != options.end(); it++)
	{
		if (it->m_name == "surfaces")
			m_uSurfacesCount = std::atoi(it->m_value.c_str());
		else
			m_dllAvUtil.av_set_string3(m_pCodecContext, it->m_name.c_str(), it->m_value.c_str(), 0, NULL);
	}

	int num_threads = std::min(8 /*MAX_THREADS*/, g_cpuInfo.getCPUCount()); /* 获取cpu  的个数，即最多创建8  个解码线程，少于8  个的时候按照cpu  的个数来创建解码线程的个数*/

	if( num_threads > 1 && !hints.software && m_pHardware == NULL // thumbnail extraction fails when run threaded
								&& ( pCodec->id == CODEC_ID_H264
								|| pCodec->id == CODEC_ID_MPEG4 ))
	{
		/* 
			初始化并创建对应个数的解码线程
			linux 系统的见ffmpeg/libavcodec/pthread.c   文件中
			win32 系统的见ffmpeg/libavcodec/w32thread.c  文件中

			新版本的ffmpeg  修改了此方法，将创建线程的实现放到了open 里面了啊，见新版本ffmpeg  的avcodec_open2 方法
		*/
		m_dllAvCodec.avcodec_thread_init(m_pCodecContext, num_threads); 
	}

	if (m_dllAvCodec.avcodec_open(m_pCodecContext, pCodec) < 0) /* 见ffmpeg  对此函数的定义*/
	{
		CLog::Log(LOGDEBUG,"CDVDVideoCodecFFmpeg::Open() Unable to open codec");
		return false;
	}

	m_pFrame = m_dllAvCodec.avcodec_alloc_frame(); /* 见ffmpeg  对此函数的定义，分配存储解码出来的帧的内存空间*/
	if (!m_pFrame) 
		return false;

	if(pCodec->name)
		m_name = CStdString("ff-") + pCodec->name;
	else
		m_name = "ffmpeg";

	if(m_pHardware)
		m_name += "-" + m_pHardware->Name();

	return true;
}

void CDVDVideoCodecFFmpeg::Dispose()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (m_pFrame) m_dllAvUtil.av_free(m_pFrame);
		m_pFrame = NULL;

	if (m_pConvertFrame)
	{
		m_dllAvCodec.avpicture_free(m_pConvertFrame);
		m_dllAvUtil.av_free(m_pConvertFrame);
	}
	m_pConvertFrame = NULL;

	if (m_pCodecContext)
	{
		if (m_pCodecContext->codec) 
			m_dllAvCodec.avcodec_close(m_pCodecContext);
		if (m_pCodecContext->extradata)
		{
			m_dllAvUtil.av_free(m_pCodecContext->extradata);
			m_pCodecContext->extradata = NULL;
			m_pCodecContext->extradata_size = 0;
		}
		m_dllAvUtil.av_free(m_pCodecContext);
		m_pCodecContext = NULL;
	}
	SAFE_RELEASE(m_pHardware);

	FilterClose();

	m_dllAvCodec.Unload();
	m_dllAvUtil.Unload();
	m_dllAvFilter.Unload();
}

void CDVDVideoCodecFFmpeg::SetDropState(bool bDrop)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if( m_pCodecContext )
	{
		// i don't know exactly how high this should be set
		// couldn't find any good docs on it. think it varies
		// from codec to codec on what it does

		//  2 seem to be to high.. it causes video to be ruined on following images
		if( bDrop )
		{
			m_pCodecContext->skip_frame = AVDISCARD_NONREF;
			m_pCodecContext->skip_idct = AVDISCARD_NONREF;
			m_pCodecContext->skip_loop_filter = AVDISCARD_NONREF;
		}
		else
		{
			m_pCodecContext->skip_frame = AVDISCARD_DEFAULT;
			m_pCodecContext->skip_idct = AVDISCARD_DEFAULT;
			m_pCodecContext->skip_loop_filter = AVDISCARD_DEFAULT;
		}
	}
}

unsigned int CDVDVideoCodecFFmpeg::SetFilters(unsigned int flags)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	m_filters_next.Empty();

	if(m_pHardware) /* 硬解不需要设定*/
		return 0;

	if(flags & FILTER_DEINTERLACE_YADIF)
	{
		if(flags & FILTER_DEINTERLACE_HALFED)
			m_filters_next = "yadif=0:-1"; /* 见全局变量avfilter_vf_yadif 滤镜*/
		else
			m_filters_next = "yadif=1:-1"; /* 见全局变量avfilter_vf_yadif 滤镜*/

		if(flags & FILTER_DEINTERLACE_FLAGGED)
			m_filters_next += ":1";

		flags &= ~FILTER_DEINTERLACE_ANY | FILTER_DEINTERLACE_YADIF;
	}

	return flags;
}

union pts_union
{
	double  pts_d;
	int64_t pts_i;
};

static int64_t pts_dtoi(double pts)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	pts_union u;
	u.pts_d = pts;
	return u.pts_i;
}

static double pts_itod(int64_t pts)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	pts_union u;
	u.pts_i = pts;
	return u.pts_d;
}

int CDVDVideoCodecFFmpeg::Decode(BYTE* pData, int iSize, double dts, double pts)
{
/*
	参数:
		1、pData	: 传入包的数据
		2、iSize	: 传入包数据的长度
		3、dts		: 传入包的dts  值
		4、pts		: 传入包的pts  值
		
	返回:
		1、
		
	说明:
		1、此函数相当于解码一帧的数据
*/
	int iGotPicture = 0, len = 0;

	if (!m_pCodecContext)
		return VC_ERROR;

	if(pData)
		m_iLastKeyframe++; /* 对key  帧的数量进行统计*/


/* ------------------------------->  第1  步
	判断是否为硬解，如果是硬解则直接调用硬解码器进行解码，解码
	成功就直接返回，如果失败了就进行ffmpeg  解码
*/

	shared_ptr<CSingleLock> lock;
	if(m_pHardware) /* 是否硬解码*/
	{
		CCriticalSection* section = m_pHardware->Section();
		if(section)
			lock = shared_ptr<CSingleLock>(new CSingleLock(*section));

		int result;
		if(pData)
			result = m_pHardware->Check(m_pCodecContext);
		else
			result = m_pHardware->Decode(m_pCodecContext, NULL);

		if(result) /* 硬解成功就返回，失败了继续进行软解*/
			return result;
	}

	/* 
		程序执行到此处应该是软解码或者是硬解失败
	*/

	if(m_pFilterGraph)
	{
		int result = 0;
		if(pData == NULL)
			result = FilterProcess(NULL);
		if(result)
			return result;
	}

	m_dts = dts;
	m_pCodecContext->reordered_opaque = pts_dtoi(pts);

	AVPacket avpkt; /* 定义一个解码数据包的数据结构*/
	
	m_dllAvCodec.av_init_packet(&avpkt); /* 初始化视频包*/
	
	avpkt.data = pData;
	avpkt.size = iSize;
	/* We lie, but this flag is only used by pngdec.c.
	* Setting it correctly would allow CorePNG decoding. */
	avpkt.flags = AV_PKT_FLAG_KEY;

/* ------------------------------->  第2  步
	程序执行到此处时需要解码的包数据为两种情况
		A、需要软解的
		B、硬解失败的
		
	调用ffmpeg  对包数据进行解码
*/
	len = m_dllAvCodec.avcodec_decode_video2(m_pCodecContext, m_pFrame, &iGotPicture, &avpkt); /* 进入ffmpeg  解码函数，即进入ffmpeg  进行解码*/

	if(m_iLastKeyframe < m_pCodecContext->has_b_frames + 2) /* 统计关键帧的个数*/
		m_iLastKeyframe = m_pCodecContext->has_b_frames + 2;

	if (len < 0)
	{
		CLog::Log(LOGERROR, "%s - avcodec_decode_video returned failure", __FUNCTION__);
		return VC_ERROR;
	}

	if (len != iSize && m_pCodecContext->skip_frame != AVDISCARD_NONREF)
		CLog::Log(LOGWARNING, "%s - avcodec_decode_video didn't consume the full packet. size: %d, consumed: %d", __FUNCTION__, iSize, len);

	if (!iGotPicture)
		return VC_BUFFER;

	if(m_pFrame->key_frame) /* 是否为key 帧*/
	{
		m_started = true;
		m_iLastKeyframe = m_pCodecContext->has_b_frames + 2;
	}

	/* put a limit on convergence count to avoid huge mem usage on streams without keyframes */
	if(m_iLastKeyframe > 300)
		m_iLastKeyframe = 300;

	/* h264 doesn't always have keyframes + won't output before first keyframe anyway */
	if(m_pCodecContext->codec_id == CODEC_ID_H264)
		m_started = true;


/* ------------------------------->  第3  步
	根据图像的像素格式、软硬解等信息对第2  步中解码出来的帧进行转换
*/

	/* 像素格式既不是PIX_FMT_YUV420P ，也不是PIX_FMT_YUVJ420P，还需要软解，则需要对帧进行转换*/
	if(m_pCodecContext->pix_fmt != PIX_FMT_YUV420P && m_pCodecContext->pix_fmt != PIX_FMT_YUVJ420P && m_pHardware == NULL) 
	{
		if (!m_dllSwScale.IsLoaded() && !m_dllSwScale.Load()) /* 加载ffmpeg  动态库*/
			return VC_ERROR;

		if (!m_pConvertFrame) /* 还没有分配保存转换后帧结果的内存空间*/
		{
			// Allocate an AVFrame structure
			m_pConvertFrame = (AVPicture*)m_dllAvUtil.av_mallocz(sizeof(AVPicture)); /* 分配数据结构空间*/
			
			// Due to a bug in swsscale we need to allocate one extra line of data /* 分配扩展线性数据空间*/
			if(m_dllAvCodec.avpicture_alloc( m_pConvertFrame
								                 , PIX_FMT_YUV420P
								                 , m_pCodecContext->width
								                 , m_pCodecContext->height+1) < 0)
			{
				m_dllAvUtil.av_free(m_pConvertFrame);
				m_pConvertFrame = NULL;
				return VC_ERROR;
			}
		}

		// convert the picture
		/* 获取ffmpeg  中帧转换的设备描述上下文，即context */
		struct SwsContext *context = m_dllSwScale.sws_getContext(m_pCodecContext->width, 
															m_pCodecContext->height,
												                     m_pCodecContext->pix_fmt, 
												                     m_pCodecContext->width, 
												                     m_pCodecContext->height,
												                     PIX_FMT_YUV420P, 
												                     SWS_FAST_BILINEAR | SwScaleCPUFlags(), 
												                     NULL, 
												                     NULL, 
												                     NULL);
		/* 获取上下文失败，返回*/
		if(context == NULL)
		{
			CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::Decode - unable to obtain sws context for w:%i, h:%i, pixfmt: %i", m_pCodecContext->width, m_pCodecContext->height, m_pCodecContext->pix_fmt);
			return VC_ERROR;
		}

		/* 调用ffmpeg  对帧进行转换，结果保存于m_pConvertFrame  中*/
		m_dllSwScale.sws_scale(context
						      , m_pFrame->data
						      , m_pFrame->linesize
						      , 0
						      , m_pCodecContext->height
						      , m_pConvertFrame->data
						      , m_pConvertFrame->linesize);

		m_dllSwScale.sws_freeContext(context);
	}
	else /* 硬解、PIX_FMT_YUV420P、PIX_FMT_YUVJ420P  情况下不需要进行帧转换，所以如果分配了转换帧的保存空间就释放掉*/
	{
		// no need to convert, just free any existing convert buffers
		if (m_pConvertFrame)
		{
			m_dllAvCodec.avpicture_free(m_pConvertFrame);
			m_dllAvUtil.av_free(m_pConvertFrame);
			m_pConvertFrame = NULL;
		}
	}

	/* 打开滤镜处理。。。。。。。*/
	// try to setup new filters
	if (!m_filters.Equals(m_filters_next))
	{
		m_filters = m_filters_next;
		if(FilterOpen(m_filters) < 0)
			FilterClose();
	}


/* ------------------------------->  第4 步
	对经过第3  步转换后的帧进行再处理( 见第2  步中的两种数据，做不同的处理)
		A、需要软解的:  调用filter  进行处理
		B、硬解失败的:  再调用硬解码器进行一次解码
*/

	int result;
	if(m_pHardware)
		result = m_pHardware->Decode(m_pCodecContext, m_pFrame); /* 调用硬解*/
	else if(m_pFilterGraph)
		result = FilterProcess(m_pFrame); /* filter 处理*/
	else
		result = VC_PICTURE | VC_BUFFER;

	if(result & VC_FLUSHED)
		Reset();

	return result;
}

void CDVDVideoCodecFFmpeg::Reset()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	m_started = false;
	m_iLastKeyframe = m_pCodecContext->has_b_frames;
	m_dllAvCodec.avcodec_flush_buffers(m_pCodecContext);

	if (m_pHardware)
		m_pHardware->Reset();

	if (m_pConvertFrame)
	{
		m_dllAvCodec.avpicture_free(m_pConvertFrame);
		m_dllAvUtil.av_free(m_pConvertFrame);
		m_pConvertFrame = NULL;
	}
	m_filters = "";
	FilterClose();
}

bool CDVDVideoCodecFFmpeg::GetPictureCommon(DVDVideoPicture* pDvdVideoPicture)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	pDvdVideoPicture->iWidth = m_pCodecContext->width;
	pDvdVideoPicture->iHeight = m_pCodecContext->height;

	if(m_pFilterLink)
	{
		pDvdVideoPicture->iWidth  = m_pFilterLink->cur_buf->video->w;
		pDvdVideoPicture->iHeight = m_pFilterLink->cur_buf->video->h;
	}

	/* crop of 10 pixels if demuxer asked it */
	if(m_pCodecContext->coded_width  && m_pCodecContext->coded_width  < (int)pDvdVideoPicture->iWidth && m_pCodecContext->coded_width  > (int)pDvdVideoPicture->iWidth  - 10)
		pDvdVideoPicture->iWidth = m_pCodecContext->coded_width;

	if(m_pCodecContext->coded_height && m_pCodecContext->coded_height < (int)pDvdVideoPicture->iHeight && m_pCodecContext->coded_height > (int)pDvdVideoPicture->iHeight - 10)
		pDvdVideoPicture->iHeight = m_pCodecContext->coded_height;

	double aspect_ratio;

	/* use variable in the frame */
	AVRational pixel_aspect = m_pCodecContext->sample_aspect_ratio;
	if (m_pFilterLink)
#ifdef HAVE_AVFILTERBUFFERREFVIDEOPROPS_SAMPLE_ASPECT_RATIO
		pixel_aspect = m_pFilterLink->cur_buf->video->sample_aspect_ratio;
#else
		pixel_aspect = m_pFilterLink->cur_buf->video->pixel_aspect;
#endif

	if (pixel_aspect.num == 0)
		aspect_ratio = 0;
	else
		aspect_ratio = av_q2d(pixel_aspect) * pDvdVideoPicture->iWidth / pDvdVideoPicture->iHeight;

	if (aspect_ratio <= 0.0)
		aspect_ratio = (float)pDvdVideoPicture->iWidth / (float)pDvdVideoPicture->iHeight;

	/* XXX: we suppose the screen has a 1.0 pixel ratio */ // CDVDVideo will compensate it.
	pDvdVideoPicture->iDisplayHeight = pDvdVideoPicture->iHeight;
	pDvdVideoPicture->iDisplayWidth  = ((int)RINT(pDvdVideoPicture->iHeight * aspect_ratio)) & -3;
	if (pDvdVideoPicture->iDisplayWidth > pDvdVideoPicture->iWidth)
	{
		pDvdVideoPicture->iDisplayWidth  = pDvdVideoPicture->iWidth;
		pDvdVideoPicture->iDisplayHeight = ((int)RINT(pDvdVideoPicture->iWidth / aspect_ratio)) & -3;
	}


	pDvdVideoPicture->pts = DVD_NOPTS_VALUE;

	if (!m_pFrame)
		return false;

	pDvdVideoPicture->iRepeatPicture = 0.5 * m_pFrame->repeat_pict;
	pDvdVideoPicture->iFlags = DVP_FLAG_ALLOCATED;
	pDvdVideoPicture->iFlags |= m_pFrame->interlaced_frame ? DVP_FLAG_INTERLACED : 0;
	pDvdVideoPicture->iFlags |= m_pFrame->top_field_first ? DVP_FLAG_TOP_FIELD_FIRST: 0;
	if(m_pCodecContext->pix_fmt == PIX_FMT_YUVJ420P)
		pDvdVideoPicture->color_range = 1;

	pDvdVideoPicture->chroma_position = m_pCodecContext->chroma_sample_location;
	pDvdVideoPicture->color_primaries = m_pCodecContext->color_primaries;
	pDvdVideoPicture->color_transfer = m_pCodecContext->color_trc;

	pDvdVideoPicture->qscale_table = m_pFrame->qscale_table;
	pDvdVideoPicture->qscale_stride = m_pFrame->qstride;

	switch (m_pFrame->qscale_type)
	{
		case FF_QSCALE_TYPE_MPEG1:
			pDvdVideoPicture->qscale_type = DVP_QSCALE_MPEG1;
			break;
		case FF_QSCALE_TYPE_MPEG2:
			pDvdVideoPicture->qscale_type = DVP_QSCALE_MPEG2;
			break;
		case FF_QSCALE_TYPE_H264:
			pDvdVideoPicture->qscale_type = DVP_QSCALE_H264;
			break;
		default:
			pDvdVideoPicture->qscale_type = DVP_QSCALE_UNKNOWN;
	}

	pDvdVideoPicture->dts = m_dts;
	m_dts = DVD_NOPTS_VALUE;
	if (m_pFrame->reordered_opaque)
		pDvdVideoPicture->pts = pts_itod(m_pFrame->reordered_opaque);
	else
		pDvdVideoPicture->pts = DVD_NOPTS_VALUE;

	if(!m_started)
		pDvdVideoPicture->iFlags |= DVP_FLAG_DROPPED;

	return true;
}

bool CDVDVideoCodecFFmpeg::GetPicture(DVDVideoPicture* pDvdVideoPicture)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if(m_pHardware)
		return m_pHardware->GetPicture(m_pCodecContext, m_pFrame, pDvdVideoPicture);

	if(!GetPictureCommon(pDvdVideoPicture))
		return false;

	if(m_pConvertFrame)
	{
		for (int i = 0; i < 4; i++)
			pDvdVideoPicture->data[i]      = m_pConvertFrame->data[i];
		
		for (int i = 0; i < 4; i++)
			pDvdVideoPicture->iLineSize[i] = m_pConvertFrame->linesize[i];
	}
	else
	{
		for (int i = 0; i < 4; i++)
			pDvdVideoPicture->data[i]      = m_pFrame->data[i];
		
		for (int i = 0; i < 4; i++)
			pDvdVideoPicture->iLineSize[i] = m_pFrame->linesize[i];
	}

	pDvdVideoPicture->iFlags |= pDvdVideoPicture->data[0] ? 0 : DVP_FLAG_DROPPED;
	pDvdVideoPicture->format = DVDVideoPicture::FMT_YUV420P;
	pDvdVideoPicture->extended_format = 0;

	return true;
}

int CDVDVideoCodecFFmpeg::FilterOpen(const CStdString& filters)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、打开滤镜
*/
	int result;

	if (m_pFilterGraph)
		FilterClose();

	if (filters.IsEmpty())
		return 0;

	if (!(m_pFilterGraph = m_dllAvFilter.avfilter_graph_alloc())) /* 分配m_pFilterGraph  的内存空间*/
	{
		CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::FilterOpen - unable to alloc filter graph");
		return -1;
	}

	// CrHasher HACK (if an alternative becomes available use it!): In order to display the output
	// produced by a combination of filters we insert "nullsink" as the last filter and we use
	// its input pin as our output pin.
	//
	// input --> .. --> last_filter --> [in] nullsink [null]     [in] --> output
	//                                   |                        |
	//                                   |                        |
	//                                   +------------------------+
	//
	AVFilter* srcFilter = m_dllAvFilter.avfilter_get_by_name("buffer"); 	/* 获取名字为buffer  的filter，实质找到的是全局变量avfilter_vsrc_buffer  */
	AVFilter* outFilter = m_dllAvFilter.avfilter_get_by_name("nullsink"); 	/* 获取名字为nullsink  的filter，实质找到的是全局变量avfilter_vsink_nullsink  */ 			// should be last filter in the graph for now

	CStdString args;

	args.Format("%d:%d:%d:%d:%d:%d:%d",
				m_pCodecContext->width,
				m_pCodecContext->height,
				m_pCodecContext->pix_fmt,
				m_pCodecContext->time_base.num,
				m_pCodecContext->time_base.den,
				m_pCodecContext->sample_aspect_ratio.num,
				m_pCodecContext->sample_aspect_ratio.den);

	if ((result = m_dllAvFilter.avfilter_graph_create_filter(&m_pFilterIn, srcFilter, "src", args, NULL, m_pFilterGraph)) < 0) /* 见此函数的说明*/
	{
		CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::FilterOpen - avfilter_graph_create_filter: src");
		return result;
	}

	if ((result = m_dllAvFilter.avfilter_graph_create_filter(&m_pFilterOut, outFilter, "out", NULL, NULL/*nullsink=>NULL*/, m_pFilterGraph)) < 0) /* 见此函数的说明*/
	{
		CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::FilterOpen - avfilter_graph_create_filter: out");
		return result;
	}

	if (!filters.empty())
	{
		AVFilterInOut* outputs = m_dllAvFilter.avfilter_inout_alloc();	/* 分配一个输出 */
		AVFilterInOut* inputs  = m_dllAvFilter.avfilter_inout_alloc();	/* 分配一个输入 */

		outputs->name    	= m_dllAvUtil.av_strdup("in");
		outputs->filter_ctx	= m_pFilterIn;
		outputs->pad_idx 	= 0;
		outputs->next    	= NULL;

		inputs->name    	= m_dllAvUtil.av_strdup("out");
		inputs->filter_ctx 	= m_pFilterOut;
		inputs->pad_idx 	= 0;
		inputs->next    	= NULL;

		if ((result = m_dllAvFilter.avfilter_graph_parse(m_pFilterGraph, (const char*)m_filters.c_str(), &inputs, &outputs, NULL)) < 0) /* 见函数说明*/
		{
			CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::FilterOpen - avfilter_graph_parse");
			return result;
		}

		m_dllAvFilter.avfilter_inout_free(&outputs);
		m_dllAvFilter.avfilter_inout_free(&inputs);
	}
	else
	{
		if ((result = m_dllAvFilter.avfilter_link(m_pFilterIn, 0, m_pFilterOut, 0)) < 0) /* 对输入、输出两个滤镜进行关联*/
		{
			CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::FilterOpen - avfilter_link");
			return result;
		}
	}

	if ((result = m_dllAvFilter.avfilter_graph_config(m_pFilterGraph, NULL)) < 0)
	{
		CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::FilterOpen - avfilter_graph_config");
		return result;
	}

	return result;
}

void CDVDVideoCodecFFmpeg::FilterClose()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (m_pFilterGraph)
	{
		m_dllAvFilter.avfilter_graph_free(&m_pFilterGraph);

		// Disposed by above code
		m_pFilterIn   = NULL;
		m_pFilterOut  = NULL;
		m_pFilterLink = NULL;
	}
}

int CDVDVideoCodecFFmpeg::FilterProcess(AVFrame* frame)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	int result, frames;

	m_pFilterLink = m_pFilterOut->inputs[0];

	if (frame)
	{
#if LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(2,13,0)
		result = m_dllAvFilter.av_vsrc_buffer_add_frame(m_pFilterIn, frame, 0);
#elif LIBAVFILTER_VERSION_INT >= AV_VERSION_INT(2,7,0)
		result = m_dllAvFilter.av_vsrc_buffer_add_frame(m_pFilterIn, frame);
#elif LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(53,3,0)
		result = m_dllAvFilter.av_vsrc_buffer_add_frame(m_pFilterIn, frame, frame->pts);
#else
		result = m_dllAvFilter.av_vsrc_buffer_add_frame(m_pFilterIn, frame, frame->pts, m_pCodecContext->sample_aspect_ratio);
#endif

		if (result < 0)
		{
			CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::FilterProcess - av_vsrc_buffer_add_frame");
			return VC_ERROR;
		}
	}

	if ((frames = m_dllAvFilter.avfilter_poll_frame(m_pFilterLink)) < 0)
	{
		CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::FilterProcess - avfilter_poll_frame");
		return VC_ERROR;
	}

	if (frames > 0)
	{
		if (m_pFilterLink->cur_buf)
		{
			m_dllAvFilter.avfilter_unref_buffer(m_pFilterLink->cur_buf);
			m_pFilterLink->cur_buf = NULL;
		}

		if ((result = m_dllAvFilter.avfilter_request_frame(m_pFilterLink)) < 0)
		{
			CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::FilterProcess - avfilter_request_frame");
			return VC_ERROR;
		}

		if (!m_pFilterLink->cur_buf)
		{
			CLog::Log(LOGERROR, "CDVDVideoCodecFFmpeg::FilterProcess - cur_buf");
			return VC_ERROR;
		}

		if(frame == NULL)
			m_pFrame->reordered_opaque = 0;
		else
			m_pFrame->repeat_pict      = -(frames - 1);

		m_pFrame->interlaced_frame = m_pFilterLink->cur_buf->video->interlaced;
		m_pFrame->top_field_first  = m_pFilterLink->cur_buf->video->top_field_first;

		memcpy(m_pFrame->linesize, m_pFilterLink->cur_buf->linesize, 4*sizeof(int));
		memcpy(m_pFrame->data    , m_pFilterLink->cur_buf->data    , 4*sizeof(uint8_t*));

		if(frames > 1)
			return VC_PICTURE;
		else
			return VC_PICTURE | VC_BUFFER;
	}

	return VC_BUFFER;
}

unsigned CDVDVideoCodecFFmpeg::GetConvergeCount()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if(m_pHardware)
		return m_iLastKeyframe;
	else
		return 0;
}
