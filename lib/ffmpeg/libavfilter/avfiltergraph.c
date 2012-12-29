/*
 * filter graphs
 * Copyright (c) 2008 Vitor Sessak
 * Copyright (c) 2007 Bobby Bingham
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <ctype.h>
#include <string.h>

#include "avfilter.h"
#include "avfiltergraph.h"
#include "internal.h"

AVFilterGraph *avfilter_graph_alloc(void)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、实质就是分配一个AVFilterGraph  的内存空间
*/
    	return av_mallocz(sizeof(AVFilterGraph));
}

void avfilter_graph_free(AVFilterGraph *graph)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、释放掉传入的数据结构，及其内部指向的各个filter  的内存空间
*/
	if (!graph)
		return;
	
	for (; graph->filter_count > 0; graph->filter_count --)
		avfilter_free(graph->filters[graph->filter_count - 1]);
	
	av_freep(&graph->scale_sws_opts);
	av_freep(&graph->filters);
}

int avfilter_graph_add_filter(AVFilterGraph *graph, AVFilterContext *filter)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、实质就是将传入的滤镜上下文数据结构插入到数据结构graph  的队列中，并更新其队列的计数
*/
	AVFilterContext **filters = av_realloc(graph->filters, sizeof(AVFilterContext*) * (graph->filter_count+1));
	if (!filters)
		return AVERROR(ENOMEM);

	graph->filters = filters;
	graph->filters[graph->filter_count++] = filter;

	return 0;
}

/*
	滤镜链表的工作过程:
	
	1、首先调用函数avfilter_request_frame(AVFilterLink *link)  来申请视频数据，传递给此函数的参数
		是整个滤镜链表的最后一个"  滤镜链数据结构"，通过这个"  滤镜链数据结构"，此
		函数内部会嵌入式的调用自身，一直向上找到滤镜链表头，然后调用滤镜链表头
		的那个滤镜的request_frame  函数进行数据获取

		如get_filtered_video_frame 函数中的调用

	2、在第1  步中找到了头滤镜的request_frame  函数( 如: input_request_frame )，通常这个request_frame 
		函数中会分为如下几个步骤完成功能
		
		A、调用一个获取一帧数据、并对数据解码后得到一个数据的包
			( 如: input_request_frame() --> get_video_frame() )
			
		B、根据得到的数据包、滤镜的相关信息等分配一个AVFilterBufferRef  的空间，即保存数据
			( 如: input_request_frame() --> avfilter_get_video_buffer() )
			
		C、调用函数avfilter_start_frame(AVFilterLink *link, AVFilterBufferRef *picref) 进行处理，通常此函数的第一个
			参数就是整个滤镜链表的第一个"  滤镜链数据结构"，第二个参数就是B  步骤中
			分配并填充了数据的AVFilterBufferRef  数据结构

			在函数avfilter_start_frame  中就会按照滤镜链表逐级的向下处理AVFilterBufferRef ，每一级滤镜
			会根据自己的需求决定是对上级滤镜传递过来的AVFilterBufferRef  结构是拷贝还是引用
			相见avfilter_start_frame 函数的代码

			( 如: input_request_frame() --> avfilter_start_frame() )
			
		D、调用函数avfilter_draw_slice(AVFilterLink *link, int y, int h, int slice_dir) 进行处理，通常此函数的第一个
			参数就是整个滤镜链表的第一个"  滤镜链数据结构"

			在函数avfilter_draw_slice 中就会按照滤镜链表逐级的向下处理，直到最后一个滤镜

			( 如: input_request_frame() --> avfilter_draw_slice() )

		F、调用函数avfilter_end_frame(AVFilterLink *link) 进行处理，通常参数为整个滤镜链表的第一个
			"  滤镜链数据结构"

			在函数avfilter_draw_slice 中就会按照滤镜链表逐级的向下处理，直到最后一个滤镜

			( 如: input_request_frame() --> avfilter_end_frame() )
*/

int avfilter_graph_create_filter(AVFilterContext **filt_ctx, 
								AVFilter *filt,
								const char *name, 
								const char *args, 
								void *opaque,
								AVFilterGraph *graph_ctx)
{
/*
	参数:
		1、filt_ctx		: 用于返回的
		2、filt			: 传入一个filter  指针
		3、name		: 传入一个名字
		4、args			: 传入参数( 最后会传递给滤镜filt  的初始化函数)
		5、opaque		: 传入参数( 最后会传递给滤镜filt  的初始化函数)
		6、graph_ctx	: 传入一个AVFilterGraph  指针
		
		
	返回:
		1、
		
	说明:
		1、此函数的功能:
			a、创建了一个滤镜上下文的内存空间
			b、对创建的滤镜上下文数据结构进行了填充，其中参数filt  就被设置到filt_ctx -> filter 中了
			c、调用了传入滤镜filt  的初始化函数，其中参数4、5  作为初始化函数的参数
			d、将创建的这个滤镜上下文添加到了参数6  所指定的滤镜图表数据结构中，并更新
				滤镜图表中的滤镜上下文的个数
*/
	int ret;

	if ((ret = avfilter_open(filt_ctx, filt, name)) < 0) /* 见avfilter_open  函数*/
		goto fail;
	
	if ((ret = avfilter_init_filter(*filt_ctx, args, opaque)) < 0) /* 见avfilter_init_filter  函数*/
		goto fail;
	
	if ((ret = avfilter_graph_add_filter(graph_ctx, *filt_ctx)) < 0) /* 见avfilter_graph_add_filter  函数*/
		goto fail;
	
	return 0;

fail:
	if (*filt_ctx)
		avfilter_free(*filt_ctx);
	
	*filt_ctx = NULL;
	return ret;
}

int ff_avfilter_graph_check_validity(AVFilterGraph *graph, AVClass *log_ctx)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、实质就是对滤镜图表中所有的滤镜上下文进行判断，判断是否
			所有的输入、输出io  都已经连接完成
*/
	AVFilterContext *filt;
	int i, j;

	for (i = 0; i < graph->filter_count; i++) /* 遍历图表中所有滤镜的上下文*/
	{
		filt = graph->filters[i];

		for (j = 0; j < filt->input_count; j++) /* 遍历输入io  */
		{
			if (!filt->inputs[j] || !filt->inputs[j]->src) /* 如果输入io   为空，或者于此输入io  相连接的源为空，即有问题了*/
			{
				av_log(log_ctx, AV_LOG_ERROR,
								"Input pad \"%s\" for the filter \"%s\" of type \"%s\" not connected to any source\n",
								filt->input_pads[j].name, filt->name, filt->filter->name);
				return -1;
			}
		}

		for (j = 0; j < filt->output_count; j++)  /* 遍历输出io  */ 
		{
			if (!filt->outputs[j] || !filt->outputs[j]->dst) /* 如果输出io   为空，或者于此输出io  相连接的源为空，即有问题了*/
			{
				av_log(log_ctx, AV_LOG_ERROR,
								"Output pad \"%s\" for the filter \"%s\" of type \"%s\" not connected to any destination\n",
								filt->output_pads[j].name, filt->name, filt->filter->name);
				return -1;
			}
		}
	}

    return 0;
}

int ff_avfilter_graph_config_links(AVFilterGraph *graph, AVClass *log_ctx)
{
    AVFilterContext *filt;
    int i, ret;

    for (i=0; i < graph->filter_count; i++) {
        filt = graph->filters[i];

        if (!filt->output_count) {
            if ((ret = avfilter_config_links(filt)))
                return ret;
        }
    }

    return 0;
}

AVFilterContext *avfilter_graph_get_filter(AVFilterGraph *graph, char *name)
{
    int i;

    for (i = 0; i < graph->filter_count; i++)
        if (graph->filters[i]->name && !strcmp(name, graph->filters[i]->name))
            return graph->filters[i];

    return NULL;
}

static int query_formats(AVFilterGraph *graph, AVClass *log_ctx)
{
    int i, j, ret;
    int scaler_count = 0;
    char inst_name[30];

    /* ask all the sub-filters for their supported media formats */
    for (i = 0; i < graph->filter_count; i++) {
        if (graph->filters[i]->filter->query_formats)
            graph->filters[i]->filter->query_formats(graph->filters[i]);
        else
            avfilter_default_query_formats(graph->filters[i]);
    }

    /* go through and merge as many format lists as possible */
    for (i = 0; i < graph->filter_count; i++) {
        AVFilterContext *filter = graph->filters[i];

        for (j = 0; j < filter->input_count; j++) {
            AVFilterLink *link = filter->inputs[j];
            if (link && link->in_formats != link->out_formats) {
                if (!avfilter_merge_formats(link->in_formats,
                                            link->out_formats)) {
                    AVFilterContext *scale;
                    char scale_args[256];
                    /* couldn't merge format lists. auto-insert scale filter */
                    snprintf(inst_name, sizeof(inst_name), "auto-inserted scaler %d",
                             scaler_count++);
                    snprintf(scale_args, sizeof(scale_args), "0:0:%s", graph->scale_sws_opts);
                    if ((ret = avfilter_graph_create_filter(&scale, avfilter_get_by_name("scale"),
                                                            inst_name, scale_args, NULL, graph)) < 0)
                        return ret;
                    if ((ret = avfilter_insert_filter(link, scale, 0, 0)) < 0)
                        return ret;

                    scale->filter->query_formats(scale);
                    if (((link = scale-> inputs[0]) &&
                         !avfilter_merge_formats(link->in_formats, link->out_formats)) ||
                        ((link = scale->outputs[0]) &&
                         !avfilter_merge_formats(link->in_formats, link->out_formats))) {
                        av_log(log_ctx, AV_LOG_ERROR,
                               "Impossible to convert between the formats supported by the filter "
                               "'%s' and the filter '%s'\n", link->src->name, link->dst->name);
                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}

static void pick_format(AVFilterLink *link)
{
    if (!link || !link->in_formats)
        return;

    link->in_formats->format_count = 1;
    link->format = link->in_formats->formats[0];

    avfilter_formats_unref(&link->in_formats);
    avfilter_formats_unref(&link->out_formats);
}

static void pick_formats(AVFilterGraph *graph)
{
    int i, j;

    for (i = 0; i < graph->filter_count; i++) {
        AVFilterContext *filter = graph->filters[i];

        for (j = 0; j < filter->input_count; j++)
            pick_format(filter->inputs[j]);
        for (j = 0; j < filter->output_count; j++)
            pick_format(filter->outputs[j]);
    }
}

int ff_avfilter_graph_config_formats(AVFilterGraph *graph, AVClass *log_ctx)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
    /* find supported formats from sub-filters, and merge along links */
    if (query_formats(graph, log_ctx))
        return -1;

    /* Once everything is merged, it's possible that we'll still have
     * multiple valid media format choices. We pick the first one. */
    pick_formats(graph);

    return 0;
}

int avfilter_graph_config(AVFilterGraph *graphctx, AVClass *log_ctx)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、实质就是对滤镜图表进行再次的确认
*/
	int ret;

	if ((ret = ff_avfilter_graph_check_validity(graphctx, log_ctx))) /* 见函数分析*/
		return ret;
	
	if ((ret = ff_avfilter_graph_config_formats(graphctx, log_ctx))) /* 见函数分析*/
		return ret;
	
	if ((ret = ff_avfilter_graph_config_links(graphctx, log_ctx)))
		return ret;

	return 0;
}
