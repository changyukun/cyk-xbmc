/*
 * Copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
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
//#define DEBUG

#include "avcodec.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>

typedef struct ThreadContext
{
	AVCodecContext *avctx; /* 见函数avcodec_thread_init  中对其赋值*/
	HANDLE thread;
	HANDLE work_sem; /* 工作信号量*/
	HANDLE job_sem; /* job  互斥体*/
	HANDLE done_sem;  /* 工作完成信号量*/
	int (*func)(AVCodecContext *c, void *arg);
	int (*func2)(AVCodecContext *c, void *arg, int, int);
	void *arg;
	int argsize;
	int *jobnr;
	int *ret;
	int threadnr;
}ThreadContext;


static unsigned WINAPI attribute_align_arg thread_func(void *v)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、windows  系统的解码线程的实体
*/
	ThreadContext *c= v;

	for(;;)
	{
		int ret, jobnr;


/* ---  第1  步------------------------------------------------------------>  
	等待工作信号量，等不到就停在这里等待，每执行一次
	循环，线程就会停在这里等待，直到有信号被发送出来
	，即函数avcodec_thread_execute  或avcodec_thread_free  得到调用
*/
		//printf("thread_func %X enter wait\n", (int)v); fflush(stdout);
		WaitForSingleObject(c->work_sem, INFINITE); 

		

/* ---  第2  步------------------------------------------------------------>  
	两个函数都为空，线程结束，见avcodec_thread_free 就是通过
	设定fun、fun2  都为空来结束每个线程的
*/		
		// avoid trying to access jobnr if we should quit
		if (!c->func && !c->func2)
			break;



/* ---  第3  步开始------------------------------------------------------>  
	等待互斥体，主要是对公用此代码的n  个线程进行互斥
*/	
		WaitForSingleObject(c->job_sem, INFINITE);
		jobnr = (*c->jobnr)++;
		ReleaseSemaphore(c->job_sem, 1, 0); 
/* 	释放互斥体
   ---  第3  步结束-----------------------------------------------------> */




/* ---  第4  步------------------------------------------------------------>  
	执行相应的功能函数
*/
		//printf("thread_func %X after wait (func=%X)\n", (int)v, (int)c->func); fflush(stdout);
		if(c->func)
			ret= c->func(c->avctx, (uint8_t *)c->arg + jobnr*c->argsize);
		else
			ret= c->func2(c->avctx, c->arg, jobnr, c->threadnr);
		
		if (c->ret)
			c->ret[jobnr] = ret;



/* ---  第5  步------------------------------------------------------------>  
	释放完成信号
*/		
		//printf("thread_func %X signal complete\n", (int)v); fflush(stdout);
		ReleaseSemaphore(c->done_sem, 1, 0);

	}

	return 0;
}

/**
 * Free what has been allocated by avcodec_thread_init().
 * Must be called after decoding has finished, especially do not call while avcodec_thread_execute() is running.
 */
void avcodec_thread_free(AVCodecContext *s)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	ThreadContext *c= s->thread_opaque;
	int i;

	/* 将两个功能函数都设置为空，用于结束线程循环*/
	for(i=0; i<s->thread_count; i++)
	{
		c[i].func= NULL;
		c[i].func2= NULL;
	}

	/* 发送启动信号，n  个线程都会收到此信号，然后从第1  步开始执行*/
	ReleaseSemaphore(c[0].work_sem, s->thread_count, 0);
	
	for(i=0; i<s->thread_count; i++)
	{
		WaitForSingleObject(c[i].thread, INFINITE);
		
		if(c[i].thread)  
			CloseHandle(c[i].thread);
	}
	
	if(c[0].work_sem) 
		CloseHandle(c[0].work_sem);
	
	if(c[0].job_sem)  
		CloseHandle(c[0].job_sem);
	
	if(c[0].done_sem) 
		CloseHandle(c[0].done_sem);

	av_freep(&s->thread_opaque);
}

static int avcodec_thread_execute(AVCodecContext *s, int (*func)(AVCodecContext *c2, void *arg2),void *arg, int *ret, int count, int size)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	ThreadContext *c= s->thread_opaque;
	int i;
	int jobnr = 0;

	assert(s == c->avctx);

	/* note, we can be certain that this is not called with the same AVCodecContext by different threads at the same time */

	/* 设定相应的功能函数*/
	for(i=0; i<s->thread_count; i++)
	{
		c[i].arg= arg;
		c[i].argsize= size;
		c[i].func= func;
		c[i].ret= ret;
		c[i].jobnr = &jobnr;
	}


	/* 发送启动信号，n  个线程都会收到此信号，然后从第1  步开始执行*/
	ReleaseSemaphore(c[0].work_sem, count, 0); 


	/* 每个线程都在等待线程的第5  步发出完成信号*/
	for(i=0; i<count; i++)
		WaitForSingleObject(c[0].done_sem, INFINITE); 

	return 0;
}

static int avcodec_thread_execute2(AVCodecContext *s, int (*func)(AVCodecContext *c2, void *arg2, int, int),void *arg, int *ret, int count)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	ThreadContext *c= s->thread_opaque;
	int i;

	/* 设定相应的功能函数*/
	for(i=0; i<s->thread_count; i++)
		c[i].func2 = func;
	
	avcodec_thread_execute(s, NULL, arg, ret, count, 0); /* 见函数说明*/
}

int avcodec_thread_init(AVCodecContext *s, int thread_count)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、windows  系统的解码线程创建函数
		2、此函数实质创建了n  个线程，在每个线程中( 所有线程共用一段代
			码，见thread_func  函数，即为线程实体)。每个线程死循环开始都会等待
			一个工"  作信号量" 即work_sem ，所有线程等待的是同一个信号量，都是
			c[0].work_sem ，见此函数中创建线程的参数可知。

		3、参看线程函数thread_func  的说明
*/
	int i;
	ThreadContext *c;
	uint32_t threadid;

	s->thread_count= thread_count;
	
	av_log(NULL, AV_LOG_INFO, "[w32thread] thread count = %d\n", thread_count);
	
	if (thread_count <= 1)
		return 0;

	assert(!s->thread_opaque);
	
	c= av_mallocz(sizeof(ThreadContext)*thread_count);
	
	s->thread_opaque= c;
	
	if(!(c[0].work_sem = CreateSemaphore(NULL, 0, INT_MAX, NULL))) /* 相当于一个信号量*/
		goto fail;
	
	if(!(c[0].job_sem  = CreateSemaphore(NULL, 1, 1, NULL))) /* 相当于一个互斥体*/
		goto fail;
	
	if(!(c[0].done_sem = CreateSemaphore(NULL, 0, INT_MAX, NULL))) /* 相当于一个信号量*/
		goto fail;

	for(i=0; i<thread_count; i++)
	{
		//printf("init semaphors %d\n", i); fflush(stdout);
		
		c[i].avctx= s; /* 指向传入的AVCodecContext 结构体*/
		
		av_log(NULL, AV_LOG_INFO, "[w32thread] init semaphors %d\n", i+1);
		
		c[i].work_sem = c[0].work_sem;
		c[i].job_sem  = c[0].job_sem;
		c[i].done_sem = c[0].done_sem;
		c[i].threadnr = i;

		//printf("create thread %d\n", i); fflush(stdout);
		av_log(NULL, AV_LOG_INFO, "[w32thread] create thread %d\n", i+1);
		
		c[i].thread = (HANDLE)_beginthreadex(NULL, 0, thread_func, &c[i], 0, &threadid );
		
		if( !c[i].thread ) 
			goto fail;
	}
	
	//printf("init done\n"); fflush(stdout);
	av_log(NULL, AV_LOG_INFO, "[w32thread] init done\n");
	s->execute= avcodec_thread_execute;
	s->execute2= avcodec_thread_execute2;

	return 0;
	
fail:
	avcodec_thread_free(s);
	return -1;
}
