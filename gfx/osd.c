
/*****************************************************************************
文件名称	: osd.c
版权所有	: Tech Home 2010-2015
文件功能	: 提供统一的osd 操作接口

作者			|					修订					|	日期
陈慧明							创建						2010.05.10

*****************************************************************************/

/*=======include standard header file======*/
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <assert.h>
#include <semaphore.h>

/*=======include platform header file======*/

#include "hifb.h"

/*=======include local header file======*/

#include "osd.h"


/*****************************const define*************************************/
#define OSD_DEBUG(x)			

#define OSD_INVALID_HANDLE		0xFFFFFFFF

#define OSD_INIT_CHECK(a, b)			do{\
											if(a == NULL)\
											{\
												b;\
											}\
										}while(0)

#define	OSD_USE_CLUT			1/*OSD 是否支持色板*/

#define	OSD_NUM	10			/*最大OSD 个数*/

#define	OSD_USE_THOS_MALLOC		1

#define	ENABLE_WAIT_LAST_BLIT	1

#define	OSD_DRAW_BITMAP	(OSD_DrawBitmap)
#define	OSD_SHOW_BITMAP	(OSD_ShowBitmap)

#define	OSD_WIDTH				1280
#define	OSD_HEIGHT				720

/*****************************struct define*************************************/
typedef struct
{
	U32						Flag;
	U32						bReference;
}TH_InstanceHeader_t;

typedef struct
{
	U32						Flag;
	U32						bReference;
	PAL_OSD_Bitmap_t		Bitmap;
	PAL_OSD_IO_Win_t		IOWin;
	U8						Transparency;/*0~~100, 0代表不透明，100代表全透明*/
	BOOL					bShow;
}OSD_Instance_t;

struct framebuffer {
	int	fbfd;
	uint32_t  phys_address;
	uint8_t  *mmap_address;
	uint32_t size;
	void *mem_mgr;
	int m_number_of_pages;
};

/*****************************gloal data define**********************************/
struct framebuffer fb, fb1;

static void (*OSD_CacheFree_p)(BOOL Force) = NULL;

static TH_Semaphore_t 		*OSD_Access_p = NULL;
static TH_Semaphore_t 		*BLIT_Access_p = NULL;
static TH_Semaphore_t 		*MEM_Access_p = NULL;
static OSD_Instance_t		OSD_Instance[OSD_NUM];
static U32				FlagCount = 0;

static PAL_OSD_Handle		OSD_OrderList[OSD_NUM];/*这张列表记录当前OSD 层叠加的顺序*/


PAL_OSD_Bitmap_t	OSD_DrawBitmap;/*OSD 用于后台合成的位图空间*/
PAL_OSD_Bitmap_t	OSD_ShowBitmap;
PAL_OSD_Bitmap_t	OSD_SppBitmap;/*用于播放图片及字幕*/
static PAL_OSD_Win_t		OSD_DirtyWin;/*修改过的窗口*/

struct
{
	PAL_OSD_ColorType_t ColorType;
	HIFB_COLOR_FMT_E		PlatColorType;
}OSD_ColorTypeTable[] ={
	{PAL_OSD_COLOR_TYPE_ARGB8888, HIFB_FMT_ARGB8888},
	{PAL_OSD_COLOR_TYPE_RGB888, HIFB_FMT_RGB888},
	{PAL_OSD_COLOR_TYPE_RGB565, HIFB_FMT_RGB565},
	{PAL_OSD_COLOR_TYPE_ARGB1555, HIFB_FMT_ARGB1555},
	{PAL_OSD_COLOR_TYPE_ARGB4444, HIFB_FMT_ARGB4444},
	{PAL_OSD_COLOR_TYPE_CLUT8, HIFB_FMT_8BPP},
	{PAL_OSD_COLOR_TYPE_CLUT4, HIFB_FMT_4BPP},
	{PAL_OSD_COLOR_TYPE_CLUT2, HIFB_FMT_2BPP},
	{PAL_OSD_COLOR_TYPE_CLUT1, HIFB_FMT_1BPP},
	{PAL_OSD_COLOR_TYPE_NUM, HIFB_FMT_ARGB8888}
};

/*****************************local function define*******************************/

int THOS_TaskDelay(unsigned int millisecond )
{
	unsigned int ms_loop;
	unsigned int us_loop;

	ms_loop = millisecond;

	while (ms_loop > 0)
	{
		us_loop = ms_loop <= 999 ? ms_loop : 999;

		if (usleep(us_loop * 1000 ) != 0)
		{
			return -1;
		}
		ms_loop -= us_loop;
	}

	return 0;
}

TH_Semaphore_t *THOS_SemaphoreCreate(const S32 InitialValue)
{
	sem_t	*Sem_p;
	int		ret;
	
	Sem_p = (sem_t *)malloc(sizeof(sem_t));
	
	ret = sem_init(Sem_p, 0, InitialValue);
	if( ret == -1 )
	{
		free(Sem_p);
		printf("Error Creating semaphore in THOS_SemaphoreCreate! errno = %d\n", errno);

		return NULL;
	}

	return ((TH_Semaphore_t *)(Sem_p));
}

/*******************************************************************************
函数名称	: THOS_SemaphoreDelete
函数功能	: 删除一个信号量

函数参数	: IN:		Semaphore_p		信号量指针

函数返回	: TH_Error_t				是否成功删除信号量

作者			|					修订					|	日期

陈慧明							创建						2010.06.02

*******************************************************************************/
int THOS_SemaphoreDelete(TH_Semaphore_t *Semaphore_p)
{
	int		ret;
	
    ret = sem_destroy((sem_t *)Semaphore_p);
    if (ret != 0)
    {
        switch(errno)
        {
            case EINVAL:
                printf("(0x%08x) returns EINVAL : this is not a valid semaphore", (U32)Semaphore_p);
                break;

            case EBUSY:
                printf("(0x%08x) eturns EBUSY  : this semaphore is locked by someone", (U32)Semaphore_p);
                break;

            default:
                printf("(0x%08x) eturns %d  : this semaphore is locked by someone", (U32)Semaphore_p, errno);
                break;
        }
    }
	
	free(Semaphore_p);
	
	return ret;
}

/*******************************************************************************
函数名称	: THOS_SemaphoreSignal
函数功能	: 释放信号量

函数参数	: IN:		Semaphore_p		信号量指针

函数返回	: 无

作者			|					修订					|	日期

陈慧明							创建						2010.06.02

*******************************************************************************/
void THOS_SemaphoreSignal(TH_Semaphore_t * Semaphore_p)
{
    sem_post((sem_t *)Semaphore_p);
}

/*******************************************************************************
函数名称	: THOS_SemaphoreWait
函数功能	: 等待信号量

函数参数	: IN:		Semaphore_p		信号量指针

函数返回	: TH_Error_t				是否成功

作者			|					修订					|	日期

陈慧明							创建						2010.06.02

*******************************************************************************/
int THOS_SemaphoreWait(TH_Semaphore_t * Semaphore_p)
{
    int ret = 0;

again:
    do
    {
        ret = sem_wait((sem_t *)Semaphore_p);
    } while((ret == -1) && (errno == EINTR));

	if(ret != 0)
	{
		printf("THOS_SemaphoreWait error\n");
		THOS_TaskDelay(10);
		goto again;
	}

	return ret;
}

/*******************************************************************************
函数名称	: THOS_SemaphoreWaitTimeOut
函数功能	: 等待超时信号量

函数参数	: IN:		Semaphore_p		信号量指针
				  IN:		TimeoutMs		等待超时时间，毫秒为单位

函数返回	: TH_Error_t				是否等到信号量

作者			|					修订					|	日期

陈慧明							创建						2010.06.02

*******************************************************************************/
int THOS_SemaphoreWaitTimeOut(TH_Semaphore_t * Semaphore_p, U32 TimeoutMs)
{
    int ret = 0;

	if(TimeoutMs == TH_TIMEOUT_INFINITY)
	{
        do
        {
           ret = sem_wait((sem_t *)Semaphore_p);
        } while((ret == -1) && (errno == EINTR));
	}
	else if(TimeoutMs == TH_TIMEOUT_IMMEDIATE)
	{
        do
        {
            ret = sem_trywait((sem_t *)Semaphore_p);
        } while((ret == -1) && (errno == EINTR));
	}
	else
	{
		struct timeval tv;
		struct timespec abstime;
		
		gettimeofday(&tv, NULL);
		abstime.tv_sec = tv.tv_sec;
		abstime.tv_nsec = (long)tv.tv_usec * 1000;
		
		/* add the delay to the current time */
		abstime.tv_sec	+= (time_t) (TimeoutMs / 1000) ;
		/* convert residue ( milli seconds)  to nano second */
		abstime.tv_nsec +=	(long)(TimeoutMs % 1000) * 1000000 ;
		
		if(abstime.tv_nsec > 999999999 )
		{
			abstime.tv_nsec -= 1000000000 ;
			abstime.tv_sec ++ ;
		}
		
		if ((abstime.tv_sec == 0) && (abstime.tv_nsec == 0))
		{
			printf("Semaphore_p: 0x%.8x, Unable to get proper time, will just try to get semaphore with immediate return", (U32)Semaphore_p);
			do
			{
				ret = sem_trywait((sem_t *)Semaphore_p);
			} while((ret == -1) && (errno == EINTR));
		}
		else
		{
			do
			{
				ret = sem_timedwait((sem_t *)Semaphore_p, &abstime);
			} while((ret == -1) && (errno == EINTR));
		}
	}

	return (ret);
}

void TH_OpenInstance(void *BaseHeader_p, 
					U32 BaseNum,
					U32 InstSize, 
					void *InstData_p, 
					U32 *Flag_p, 
					U32 *Handle_p)
{
	TH_InstanceHeader_t	*Header_p;
	S32 i;

	if(*Flag_p == 0)
	{
		(*Flag_p) ++;
	}
	Header_p = (TH_InstanceHeader_t *)BaseHeader_p;

	for(i=0; i<BaseNum; i++)
	{
		if(Header_p->Flag == 0)
		{
			if(InstData_p != NULL)
			{
				memcpy(Header_p, InstData_p, InstSize);
			}
			Header_p->Flag = *Flag_p;
			Header_p->bReference = FALSE;
			(*Flag_p) ++;
			*Handle_p = i;
			break;
		}
		else
		{
			Header_p = (TH_InstanceHeader_t *)(((U8 *)Header_p) + InstSize);
		}
	}
}

void TH_CloseInstance(void *InstHeader_p, U32 Flag)
{
	TH_InstanceHeader_t	*Header_p;
	
	Header_p = (TH_InstanceHeader_t *)InstHeader_p;

	if(Header_p->Flag == Flag)
	{
		Header_p->Flag = 0;
	}
}

OSD_Instance_t *OSD_ReferenceInstance(PAL_OSD_Handle Handle, U32 TimeOutMs)
{
	OSD_Instance_t *Inst_p = NULL;
	
	if(Handle >= OSD_NUM)
	{
		return Inst_p;
	}
	Inst_p = (OSD_Instance_t *)(&OSD_Instance[Handle]);
	return Inst_p;
}

void OSD_ReleaseInstance(PAL_OSD_Handle Handle)
{
	if(Handle >= OSD_NUM)
	{
		return ;
	}
}

void OSD_OpenInstance(OSD_Instance_t *Inst_p, PAL_OSD_Handle *Handle_p)
{
	TH_OpenInstance((void *)OSD_Instance, 
					OSD_NUM,
					sizeof(OSD_Instance_t), 
					(void *)Inst_p, 
					&FlagCount, 
					Handle_p);
}

void OSD_CloseInstance(PAL_OSD_Handle Handle, U32 Flag)
{
	if(Handle >= OSD_NUM)
	{
		return ;
	}
	TH_CloseInstance((void *)(&OSD_Instance[Handle]), Flag);
}

void OSD_InstanceInit(void)
{
	S32 i;
	OSD_Access_p = THOS_SemaphoreCreate(1);
	BLIT_Access_p = THOS_SemaphoreCreate(1);

	memset(OSD_Instance, 0, sizeof(OSD_Instance_t)*OSD_NUM);
	for(i=0; i<OSD_NUM; i++)
	{
		OSD_OrderList[i] = TH_INVALID_HANDLE;
	}
}

void OSD_InstanceTerm(void)
{
	S32 i;
	THOS_SemaphoreDelete(OSD_Access_p);
	THOS_SemaphoreDelete(BLIT_Access_p);
	THOS_SemaphoreDelete(MEM_Access_p);
	BLIT_Access_p = NULL;
	OSD_Access_p = NULL;
	for(i=0; i<OSD_NUM; i++)
	{
		OSD_OrderList[i] = TH_INVALID_HANDLE;
	}
}

HIFB_COLOR_FMT_E OSD_TransColorType2Platform(PAL_OSD_ColorType_t ColorType)
{
	S32						i;

	i = 0;
	while(OSD_ColorTypeTable[i].ColorType != PAL_OSD_COLOR_TYPE_NUM)
	{
		if(OSD_ColorTypeTable[i].ColorType == ColorType)
		{
			break;
		}
		i ++;
	}
	return OSD_ColorTypeTable[i].PlatColorType;
}

/*设置屏幕的修改窗口，该函数为在原来窗口基础上加上新的窗口*/
void OSD_SetDirtyWin(PAL_OSD_Win_t *DirtyWin_p)
{
	THOS_SemaphoreWait(OSD_Access_p);

	if(OSD_DirtyWin.Width == 0)
	{
		OSD_DirtyWin = *DirtyWin_p;
	}
	else
	{
		if(OSD_DirtyWin.LeftX > DirtyWin_p->LeftX)
		{
			OSD_DirtyWin.Width += (OSD_DirtyWin.LeftX - DirtyWin_p->LeftX);
			OSD_DirtyWin.LeftX = DirtyWin_p->LeftX;
		}
		if(OSD_DirtyWin.TopY > DirtyWin_p->TopY)
		{
			OSD_DirtyWin.Height += (OSD_DirtyWin.TopY - DirtyWin_p->TopY);
			OSD_DirtyWin.TopY = DirtyWin_p->TopY;
		}
		
		if(OSD_DirtyWin.LeftX+OSD_DirtyWin.Width < DirtyWin_p->LeftX+DirtyWin_p->Width)
		{
			OSD_DirtyWin.Width = DirtyWin_p->LeftX+DirtyWin_p->Width- OSD_DirtyWin.LeftX;
		}
		if(OSD_DirtyWin.TopY+OSD_DirtyWin.Height< DirtyWin_p->TopY+DirtyWin_p->Height)
		{
			OSD_DirtyWin.Height = DirtyWin_p->TopY+DirtyWin_p->Height- OSD_DirtyWin.TopY;
		}
	}
	THOS_SemaphoreSignal(OSD_Access_p);
}

/*计算两个窗口的重叠窗口*/
void OSD_ConflictWin(PAL_OSD_Win_t *Win1_p, PAL_OSD_Win_t *Win2_p, PAL_OSD_Win_t *ConflictWin_p)
{
	if(((Win1_p->LeftX + Win1_p->Width) <= Win2_p->LeftX) ||\
		((Win2_p->LeftX + Win2_p->Width) <= Win1_p->LeftX) ||\
		((Win1_p->TopY + Win1_p->Height) <= Win2_p->TopY) ||\
		((Win2_p->TopY + Win2_p->Height) <= Win1_p->TopY))
	{
		//printf("ddddd\n");
		memset(ConflictWin_p, 0, sizeof(PAL_OSD_Win_t));
		return ;
	}

	ConflictWin_p->LeftX = (Win1_p->LeftX > Win2_p->LeftX) ? Win1_p->LeftX : Win2_p->LeftX;
	ConflictWin_p->TopY = (Win1_p->TopY > Win2_p->TopY) ? Win1_p->TopY : Win2_p->TopY;
	if((Win1_p->LeftX + Win1_p->Width) > (Win2_p->LeftX + Win2_p->Width))
	{
		ConflictWin_p->Width = (Win2_p->LeftX + Win2_p->Width) - ConflictWin_p->LeftX;
	}
	else
	{
		ConflictWin_p->Width = (Win1_p->LeftX + Win1_p->Width) - ConflictWin_p->LeftX;
	}
	
	if((Win1_p->TopY + Win1_p->Height) > (Win2_p->TopY + Win2_p->Height))
	{
		ConflictWin_p->Height = (Win2_p->TopY + Win2_p->Height) - ConflictWin_p->TopY;
	}
	else
	{
		ConflictWin_p->Height = (Win1_p->TopY + Win1_p->Height) - ConflictWin_p->TopY;
	}
}

/*转换屏幕窗口到指定的OSD 的输入窗口*/
void OSD_ConvScreenWin2SrcWin(OSD_Instance_t *Inst_p, PAL_OSD_Win_t *ScreenWin_p, PAL_OSD_Win_t *SrcWin_p)
{
	SrcWin_p->LeftX = ScreenWin_p->LeftX - Inst_p->IOWin.OutputWin.LeftX + Inst_p->IOWin.InputWin.LeftX;
	SrcWin_p->TopY = ScreenWin_p->TopY - Inst_p->IOWin.OutputWin.TopY + Inst_p->IOWin.InputWin.TopY;
	SrcWin_p->Width = ScreenWin_p->Width;
	SrcWin_p->Height = ScreenWin_p->Height;
}

/*转换OSD 的坐标到屏幕窗口的从标*/
void OSD_ConvSrcWin2ScreenWin(OSD_Instance_t *Inst_p, PAL_OSD_Win_t *SrcWin_p, PAL_OSD_Win_t *ScreenWin_p)
{
	ScreenWin_p->LeftX = SrcWin_p->LeftX - Inst_p->IOWin.InputWin.LeftX + Inst_p->IOWin.OutputWin.LeftX;
	ScreenWin_p->TopY = SrcWin_p->TopY - Inst_p->IOWin.InputWin.TopY + Inst_p->IOWin.OutputWin.TopY;
	ScreenWin_p->Width = SrcWin_p->Width;
	ScreenWin_p->Height = SrcWin_p->Height;
}

/*添加OSD 到列表*/
void OSD_AddToList(PAL_OSD_Handle Handle)
{
	S32				i;

	THOS_SemaphoreWait(OSD_Access_p);
	for(i = 0; i<OSD_NUM; i++)
	{
		if(OSD_OrderList[i] == Handle)
		{
			break;
		}
		else if(OSD_OrderList[i] == TH_INVALID_HANDLE)
		{
			OSD_OrderList[i] = Handle;
			break;
		}
	}
	THOS_SemaphoreSignal(OSD_Access_p);
}

/*从OSD 列表删除一个OSD*/
void OSD_RemoveFromList(PAL_OSD_Handle Handle)
{
	S32				i;

	THOS_SemaphoreWait(OSD_Access_p);
	for(i = 0; i<OSD_NUM; i++)
	{
		if(OSD_OrderList[i] == Handle)
		{
			OSD_OrderList[i] = TH_INVALID_HANDLE;
			for(; i<(OSD_NUM-1); i++)
			{
				OSD_OrderList[i] = OSD_OrderList[i+1];
			}
			OSD_OrderList[OSD_NUM-1] = TH_INVALID_HANDLE;
			break;
		}
	}
	THOS_SemaphoreSignal(OSD_Access_p);
}

#if (OSD_USE_THOS_MALLOC)

#define ALLIAN_BYTE		8

void  *OSD_GetPhysicalAddr(void *ptr)
{
	if ((uint8_t *)ptr >= fb.mmap_address && (uint8_t *)ptr < fb.mmap_address + fb.size)
		return (ptr - (void *)(fb.mmap_address) + (void *)(fb.phys_address));

	return (void *)0;
}

//GxHwMallocObj	FbHwMem;

void FB_Init(void)
{
	void *Ptr;
	U32 BlockSize;

	THOS_SemaphoreWait(MEM_Access_p);

	Ptr = (void *)(fb.mmap_address);
	BlockSize = fb.size - (ALLIAN_BYTE<<1);
	*((U32 *)Ptr) = BlockSize;/*头标志*/
	Ptr += (BlockSize + ALLIAN_BYTE);
	*((U32 *)Ptr) = BlockSize;/*尾标志*/
	THOS_SemaphoreSignal(MEM_Access_p);
}

void *FB_Malloc(U32 Size, BOOL FromTop)
{
	void *Ptr, *Ret = NULL;
	U32 BlockSize, BlockUsed;


	THOS_SemaphoreWait(MEM_Access_p);
	Size = (Size+7) & 0xFFFFFFF8;/*eight byte*/
	if(FromTop)
	{
		Ptr = (void *)(fb.mmap_address);
		while(1)
		{
			BlockSize = *((U32 *)Ptr);
			BlockUsed = ((BlockSize & 0x80000000) != 0);
			BlockSize = BlockSize & 0x7FFFFFFF;
			if((BlockUsed == 0) && (BlockSize > (Size+(ALLIAN_BYTE<<1))))
			{
				Ret = Ptr + ALLIAN_BYTE;
				*((U32 *)Ptr) = (Size | 0x80000000);/*头标志*/
				Ptr += (Size + ALLIAN_BYTE);
				*((U32 *)Ptr) = Size;/*尾标志*/
				Ptr += ALLIAN_BYTE;
				/*next block*/
				BlockSize -= (Size+(ALLIAN_BYTE<<1));
				*((U32 *)Ptr) = BlockSize;/*头标志*/
				Ptr += (BlockSize + ALLIAN_BYTE);
				*((U32 *)Ptr) = BlockSize;/*尾标志*/
				break;
			}
			else
			{
				/*next block*/
				Ptr += (BlockSize + (ALLIAN_BYTE<<1));
				if((uint8_t *)Ptr >= (fb.mmap_address + fb.size))
				{
					break;
				}
			}
		}	
	}
	else
	{
		Ptr = fb.mmap_address + fb.size - ALLIAN_BYTE;
		while(1)
		{
			BlockSize = *((U32 *)Ptr);
			Ptr -= (BlockSize + ALLIAN_BYTE);
			BlockSize = *((U32 *)Ptr);
			BlockUsed = ((BlockSize & 0x80000000) != 0);
			BlockSize = BlockSize & 0x7FFFFFFF;
			if((BlockUsed == 0) && (BlockSize > (Size+(ALLIAN_BYTE<<1))))
			{
				BlockSize = BlockSize - Size - (ALLIAN_BYTE<<1);
				*((U32 *)Ptr) = BlockSize;/*头标志*/
				Ptr += (BlockSize + ALLIAN_BYTE);
				*((U32 *)Ptr) = BlockSize;/*尾标志*/

				Ptr += ALLIAN_BYTE;
				Ret = Ptr + ALLIAN_BYTE;
				
				*((U32 *)Ptr) = (Size | 0x80000000);/*头标志*/
				Ptr += (Size + ALLIAN_BYTE);
				*((U32 *)Ptr) = Size;/*尾标志*/
				break;
			}
			else
			{
				/*prev block*/
				Ptr -= ALLIAN_BYTE;
				if((uint8_t *)Ptr < fb.mmap_address)
				{
					break;
				}
			}
		}	
	}
	THOS_SemaphoreSignal(MEM_Access_p);

	return Ret;
}

void FB_Free(void *Ptr)
{
	U32 BlockSize;
	U32 NextBlockSize, NextBlockUsed;
	U32 PrevBlockSize, PrevBlockUsed;
	
	THOS_SemaphoreWait(MEM_Access_p);
	if((Ptr <= (void *)(fb.mmap_address)) || (Ptr >= (void *)(fb.mmap_address + fb.size)))
	{
		THOS_SemaphoreSignal(MEM_Access_p);
		printf("[OSD]: Fatal error!!! try to free wrong address\n");
		return ;
	}
	Ptr -= ALLIAN_BYTE;
	BlockSize = *((U32 *)Ptr);
	BlockSize = BlockSize & 0x7FFFFFFF;

	/*current block*/
	*((U32 *)Ptr) = BlockSize;
	
	/*check next block*/
	if(((uint8_t *)Ptr+BlockSize + (ALLIAN_BYTE<<1)) < (fb.mmap_address + fb.size))
	{
		NextBlockSize = *((U32 *)(Ptr+BlockSize + (ALLIAN_BYTE<<1)));
		NextBlockUsed = ((NextBlockSize & 0x80000000) != 0);
		NextBlockSize = NextBlockSize & 0x7FFFFFFF;
		if(NextBlockUsed == 0)
		{
			BlockSize = BlockSize + NextBlockSize + (ALLIAN_BYTE<<1);
			*((U32 *)Ptr) = BlockSize;/*头标志*/
			*((U32 *)(Ptr+BlockSize + ALLIAN_BYTE)) = BlockSize;/*尾标志*/
		}
	}
	/*check prev block*/
	if(((uint8_t *)Ptr-(ALLIAN_BYTE<<1)) >= fb.mmap_address)
	{
		PrevBlockSize = *((U32 *)(Ptr-ALLIAN_BYTE));
		Ptr -= (PrevBlockSize + (ALLIAN_BYTE<<1));
		PrevBlockSize = *((U32 *)(Ptr));
		PrevBlockUsed = ((PrevBlockSize & 0x80000000) != 0);
		PrevBlockSize = PrevBlockSize & 0x7FFFFFFF;
		if(PrevBlockUsed == 0)
		{
			BlockSize = BlockSize + PrevBlockSize + (ALLIAN_BYTE<<1);
			*((U32 *)Ptr) = BlockSize;/*头标志*/
			*((U32 *)(Ptr+BlockSize + ALLIAN_BYTE)) = BlockSize;/*尾标志*/
		}
	}
	THOS_SemaphoreSignal(MEM_Access_p);
}

void FB_Check(void)
{
	void *Ptr;
	U32 BlockSize, BlockUsed;
	
	Ptr = (void *)(fb.mmap_address);

	while(1)
	{
		BlockSize = *((U32 *)Ptr);
		BlockUsed = ((BlockSize & 0x80000000) != 0);
		BlockSize = BlockSize & 0x7FFFFFFF;
		printf("[OSD]: block:: %d 0x%08X 0x%08X\n", BlockUsed, (U32)Ptr, BlockSize);
		{
			/*next block*/
			Ptr += (BlockSize + (ALLIAN_BYTE<<1));
			if((uint8_t *)Ptr >= (fb.mmap_address + fb.size))
			{
				break;
			}
		}
	}
}

typedef struct
{
	int hw;
	void *phy;
}OSD_Private_t;

void  *OSD_AllocMem(PAL_OSD_Bitmap_t *Bitmap_p, BOOL ksurface, BOOL FromTop)
{
	OSD_Private_t      *pf;
	int ret = 0;

	OSD_DEBUG(printf("[GUI]OSD_AllocMem line = %d, %d\n", __LINE__, Bitmap_p->Size));
	if(Bitmap_p->Data_p == NULL)
	{
		Bitmap_p->Data_p = (void *)FB_Malloc(Bitmap_p->Size, FromTop);
	}
	if(Bitmap_p->Data_p == NULL)/*空间必须从fb 里分配*/
	{
		printf("[GUI]OSD_AllocMem no mem from fb, %d\n", Bitmap_p->Size);
		return NULL;
	}
	if(FromTop)
	{
		pf = malloc(sizeof(OSD_Private_t));
	}
	else
	{
		pf = malloc(sizeof(OSD_Private_t));
	}
	if(pf == NULL)
	{
		OSD_DEBUG(printf("[GUI]OSD_AllocMem failed\n"));
		return NULL;
	}
	memset(pf, 0, sizeof(OSD_Private_t));

	pf->hw        = 0;

	Bitmap_p->PlatformBitmap_p = pf;
	Bitmap_p->Private_p = pf;
	if(Bitmap_p->Data_p != NULL)
	{
		pf->phy = OSD_GetPhysicalAddr(Bitmap_p->Data_p);
		OSD_DEBUG(printf("[GUI]OSD_AllocMem line = %d, va = 0x%08X, pa = 0x%08X\n", __LINE__, Bitmap_p->Data_p, pf->phy));
	}

	return Bitmap_p->Data_p;
}

void  OSD_FreeMem(PAL_OSD_Bitmap_t *Bitmap_p)
{
	OSD_Private_t      *pf;
	
	pf = (OSD_Private_t *)(Bitmap_p->Private_p);
	if(pf == NULL)
	{
		return ;
	}
	
	if(Bitmap_p->Data_p)
	{
		FB_Free(Bitmap_p->Data_p);
	}
	free(Bitmap_p->Private_p);
	Bitmap_p->Private_p = NULL;
	Bitmap_p->PlatformBitmap_p = NULL;
	Bitmap_p->Data_p = NULL;
}

TH_Error_t PAL_VB_AllocMem(PAL_OSD_Vb_t *Vb)
{
	TH_Error_t Error = TH_NO_ERROR;
	
	if (Vb == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	Vb->Size = (Vb->Size+31) & 0xFFFFFFE0;/*32 byte*/



	return Error;
}


TH_Error_t  PAL_VB_FreeMem(PAL_OSD_Vb_t *Vb)
{
	TH_Error_t Error = TH_NO_ERROR;

	return Error;
}

#endif
TH_Error_t OSD_InitDrawBitmap(void)
{
	TH_Error_t Error = TH_NO_ERROR;

	memset(&OSD_DrawBitmap, 0, sizeof(OSD_DrawBitmap));

	OSD_DrawBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
	OSD_DrawBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_16TO9;
	OSD_DrawBitmap.Width = OSD_WIDTH;
	OSD_DrawBitmap.Height = OSD_HEIGHT;
	OSD_DrawBitmap.Pitch = (OSD_DrawBitmap.Width<<2);
	OSD_DrawBitmap.Size = OSD_DrawBitmap.Height * OSD_DrawBitmap.Pitch;
	
	if(OSD_AllocMem(&OSD_DrawBitmap, FALSE, TRUE) == NULL)
	{
		if(OSD_CacheFree_p)
		{
			(OSD_CacheFree_p)(FALSE);
		}
		if(OSD_AllocMem(&OSD_DrawBitmap, FALSE, TRUE) == NULL)
		{
			if(OSD_CacheFree_p)
			{
				(OSD_CacheFree_p)(TRUE);
			}
			if(OSD_AllocMem(&OSD_DrawBitmap, FALSE, TRUE) == NULL)
			{
				printf("[GUI]OSD_AllocMem failed\n");
				FB_Check();
				Error = TH_ERROR_NO_MEM;
			}
		}
	}
	return Error;
}

TH_Error_t OSD_TermDrawBitmap(void)
{
	TH_Error_t Error = TH_NO_ERROR;
	
	OSD_FreeMem(&OSD_DrawBitmap);
	memset(&OSD_DrawBitmap, 0, sizeof(OSD_DrawBitmap));
	
	return Error;
}

#define FBIO_ACCEL  0x23

enum
{
	blitAlphaTest=1,
	blitAlphaBlend=2,
	blitScale=4,
	blitKeepAspectRatio=8
};

static unsigned int displaylist[1024];
static int ptr = 0;

#define P(x, y) do { displaylist[ptr++] = x; displaylist[ptr++] = y; } while (0)
#define C(x) P(x, 0)

static int exec_list(void)
{
	int ret;
	struct
	{
		void *ptr;
		int len;
	} l;

	l.ptr = displaylist;
	l.len = ptr;
	ret = ioctl(fb.fbfd, FBIO_ACCEL, &l);
	ptr = 0;
	return ret;
}

static void fb_blit_fill(
		int dst_addr, int dst_width, int dst_height, int dst_stride,
		int x, int y, int width, int height,
		unsigned long color)
{
	C(0x43); // reset source
	C(0x53); // reset dest
	C(0x5b); // reset pattern
	C(0x67); // reset blend
	C(0x75); // reset output

	// clear dest surface
	P(0x0, 0);
	P(0x1, 0);
	P(0x2, 0);
	P(0x3, 0);
	P(0x4, 0);
	C(0x45);

	// clear src surface
	P(0x0, 0);
	P(0x1, 0);
	P(0x2, 0);
	P(0x3, 0);
	P(0x4, 0);
	C(0x5);

	P(0x2d, color);

	P(0x2e, x); // prepare output rect
	P(0x2f, y);
	P(0x30, width);
	P(0x31, height);
	C(0x6e); // set this rect as output rect

	P(0x0, dst_addr); // prepare output surface
	P(0x1, dst_stride);
	P(0x2, dst_width);
	P(0x3, dst_height);
	P(0x4, 0x7e48888);
	C(0x69); // set output surface

	P(0x6f, 0);
	P(0x70, 0);
	P(0x71, 2);
	P(0x72, 2);
	C(0x73); // select color keying

	C(0x77);  // do it

	exec_list();
}

static void fb_blit(
		int src_addr, int src_width, int src_height, int src_stride, int src_format,
		int dst_addr, int dst_width, int dst_height, int dst_stride,
		int src_x, int src_y, int width, int height,
		int dst_x, int dst_y, int dwidth, int dheight,
		int pal_addr, int flags)
{
	C(0x43); // reset source
	C(0x53); // reset dest
	C(0x5b);  // reset pattern
	C(0x67); // reset blend
	C(0x75); // reset output

	P(0x0, src_addr); // set source addr
	P(0x1, src_stride);  // set source pitch
	P(0x2, src_width); // source width
	P(0x3, src_height); // height
	switch (src_format)
	{
	case HIFB_FMT_ARGB8888:
		P(0x4, 0x7e48888); // format: ARGB 8888
		break;
	case HIFB_FMT_RGB888:
		P(0x4, 0x7e40888); // format: RGB 888
		break;
	case HIFB_FMT_8BPP:
		P(0x4, 0x12e40008); // indexed 8bit
		P(0x78, 256);
		P(0x79, pal_addr);
		P(0x7a, 0x7e48888);
		break;
	}

	C(0x5); // set source surface (based on last parameters)

	P(0x2e, src_x); // define  rect
	P(0x2f, src_y);
	P(0x30, width);
	P(0x31, height);

	C(0x32); // set this rect as source rect

	P(0x0, dst_addr); // prepare output surface
	P(0x1, dst_stride);
	P(0x2, dst_width);
	P(0x3, dst_height);
	P(0x4, 0x7e48888);

	C(0x69); // set output surface

	P(0x2e, dst_x); // prepare output rect
	P(0x2f, dst_y);
	P(0x30, dwidth);
	P(0x31, dheight);

	C(0x6e); // set this rect as output rect

	if (flags) P(0x80, flags); /* blend flags...  */

	C(0x77);  // do it

	exec_list();
}

static void fb_blit_ex(
		int src_addr, int src_width, int src_height, int src_stride, int src_format,
		int dst_addr, int dst_width, int dst_height, int dst_stride,
		int src_x, int src_y, int width, int height,
		int dst_x, int dst_y, int dwidth, int dheight,
		int pal_addr, int flags, int global_alpha)
{
	C(0x43); // reset source
	C(0x53); // reset dest
	C(0x5b);  // reset pattern
	P(0x67, global_alpha); // set blend value
	C(0x75); // reset output

	P(0x0, src_addr); // set source addr
	P(0x1, src_stride);  // set source pitch
	P(0x2, src_width); // source width
	P(0x3, src_height); // height
	switch (src_format)
	{
	case HIFB_FMT_ARGB8888:
		P(0x4, 0x7e48888); // format: ARGB 8888
		break;
	case HIFB_FMT_RGB888:
		P(0x4, 0x7e40888); // format: RGB 888
		break;
	case HIFB_FMT_8BPP:
		P(0x4, 0x12e40008); // indexed 8bit
		P(0x78, 256);
		P(0x79, pal_addr);
		P(0x7a, 0x7e48888);
		break;
	}

	C(0x5); // set source surface (based on last parameters)

	P(0x2e, src_x); // define  rect
	P(0x2f, src_y);
	P(0x30, width);
	P(0x31, height);

	C(0x32); // set this rect as source rect

	P(0x0, dst_addr); // prepare output surface
	P(0x1, dst_stride);
	P(0x2, dst_width);
	P(0x3, dst_height);
	P(0x4, 0x7e48888);

	C(0x69); // set output surface

	P(0x2e, dst_x); // prepare output rect
	P(0x2f, dst_y);
	P(0x30, dwidth);
	P(0x31, dheight);

	C(0x6e); // set this rect as output rect

	if (flags) P(0x80, flags); /* blend flags...  */

	C(0x77);  // do it

	exec_list();
}

TH_Error_t iOSD_Fill(PAL_OSD_Bitmap_t *DestBitmap_p, 
								PAL_OSD_Win_t *DestWin_p, 
								PAL_OSD_Color_t *Color_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Private_t      *dest_pf;
	unsigned long color;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));

	THOS_SemaphoreWait(BLIT_Access_p);
	dest_pf = DestBitmap_p->Private_p;

	color = (Color_p->Value.ARGB8888.Alpha<<24)|\
		(Color_p->Value.ARGB8888.R<<16)|\
		(Color_p->Value.ARGB8888.G<<8)|\
		(Color_p->Value.ARGB8888.B);
	fb_blit_fill(
			(int)dest_pf->phy, DestBitmap_p->Width, DestBitmap_p->Height, DestBitmap_p->Pitch,
			DestWin_p->LeftX, DestWin_p->TopY, DestWin_p->Width, DestWin_p->Height,
			color);

	THOS_SemaphoreSignal(BLIT_Access_p);

	return( Error );
}

/*****************************global function define*******************************/

/*******************************************************************************
函数名称	: PAL_OSD_Init
函数功能	: 初始化OSD 模块

函数参数	: 无

函数返回	: 是否初始化成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_Init(void)
{
	TH_Error_t Error = TH_NO_ERROR;

	OSD_InstanceInit();

	memset(&OSD_DirtyWin, 0, sizeof(OSD_DirtyWin));

	return ( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_Term
函数功能	: 结束OSD 模块

函数参数	: 无

函数返回	: 是否结束成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_Term(void)
{
	TH_Error_t Error = TH_NO_ERROR;
	int i;
	
	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));

	for(i = 0; i<OSD_NUM; i++)
	{
		if(OSD_OrderList[i] != TH_INVALID_HANDLE)
		{
			PAL_OSD_Close(OSD_OrderList[i]);
		}
	}

	OSD_TermDrawBitmap();
	OSD_InstanceTerm();
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_Open
函数功能	: 打开一个OSD 控制块

函数参数	: IN:		OpenParam_p		开OSD 控制块的参数
				  OUT:	Handle_p			输出控制块句柄

函数返回	: 是否打开成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_Open(PAL_OSD_OpenParam_t *OpenParam_p, PAL_OSD_Handle *Handle_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t	Inst;
	PAL_OSD_Color_t 			ClearColor;
	
	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	
	if(OpenParam_p->IOWin.InputWin.Width != OpenParam_p->IOWin.OutputWin.Width ||\
		OpenParam_p->IOWin.InputWin.Height != OpenParam_p->IOWin.OutputWin.Height)
	{
		return TH_ERROR_NOT_SUPPORT;
	}

	memset(&Inst.Bitmap, 0, sizeof(Inst.Bitmap));
	Inst.Bitmap.ColorType		= OpenParam_p->ColorType;
	Inst.Bitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_16TO9;
	Inst.Bitmap.Width			= OpenParam_p->Width;
	Inst.Bitmap.Height			= OpenParam_p->Height;
	Inst.Bitmap.Pitch			= Inst.Bitmap.Width<<2;
	Inst.Bitmap.Size			= Inst.Bitmap.Pitch*Inst.Bitmap.Height;

	if(OSD_AllocMem(&Inst.Bitmap, FALSE, FALSE) == NULL)
	{
		if(OSD_CacheFree_p)
		{
			(OSD_CacheFree_p)(FALSE);
			if(OSD_AllocMem(&Inst.Bitmap, FALSE, FALSE) == NULL)
			{
				(OSD_CacheFree_p)(TRUE);
				if(OSD_AllocMem(&Inst.Bitmap, FALSE, FALSE) == NULL)
				{
					FB_Check();
					return TH_ERROR_NO_MEM;
				}
			}
		}
	}

	Inst.bReference 		= FALSE;
	Inst.Transparency		=0;
	
	Inst.IOWin		= OpenParam_p->IOWin;
	if(Inst.IOWin.InputWin.LeftX + Inst.IOWin.InputWin.Width > OpenParam_p->Width)
	{
		Inst.IOWin.InputWin.LeftX = 0;
		Inst.IOWin.InputWin.Width = OpenParam_p->Width;
	}
	if(Inst.IOWin.InputWin.TopY + Inst.IOWin.InputWin.Height > OpenParam_p->Height)
	{
		Inst.IOWin.InputWin.TopY = 0;
		Inst.IOWin.InputWin.Height = OpenParam_p->Height;
	}
	if(Inst.IOWin.OutputWin.LeftX + Inst.IOWin.OutputWin.Width > OSD_WIDTH)
	{
		Inst.IOWin.OutputWin.LeftX = 0;
		Inst.IOWin.OutputWin.Width = OSD_WIDTH;
	}
	if(Inst.IOWin.OutputWin.TopY + Inst.IOWin.OutputWin.Height > OSD_HEIGHT)
	{
		Inst.IOWin.OutputWin.TopY = 0;
		Inst.IOWin.OutputWin.Height = OSD_HEIGHT;
	}
	Inst.bShow = TRUE;
	Inst.Transparency = 0;
//	OSD_SetDirtyWin(&(Inst.IOWin.OutputWin));

	OSD_OpenInstance(&Inst, Handle_p);

	OSD_AddToList(*Handle_p);
	
	if(OpenParam_p->ColorType == PAL_OSD_COLOR_TYPE_CLUT2 ||\
		OpenParam_p->ColorType == PAL_OSD_COLOR_TYPE_CLUT4 ||\
		OpenParam_p->ColorType == PAL_OSD_COLOR_TYPE_CLUT8)
	{
		ClearColor.Type = OpenParam_p->ColorType;
		ClearColor.Value.CLUT8 = OpenParam_p->ColorNum - 1;
	}
	else
	{
		ClearColor.Type = OpenParam_p->ColorType;
		memset(&(ClearColor.Value), 0, sizeof(ClearColor.Value));
	}
	
	PAL_OSD_Fill(*Handle_p, &(Inst.IOWin.InputWin), &ClearColor);
	return( Error );
}

TH_Error_t PAL_OSD_OpenNoShow(PAL_OSD_OpenParam_t *OpenParam_p, PAL_OSD_Handle *Handle_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t	Inst;
	PAL_OSD_Color_t 			ClearColor;
	
	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	
	if(OpenParam_p->IOWin.InputWin.Width != OpenParam_p->IOWin.OutputWin.Width ||\
		OpenParam_p->IOWin.InputWin.Height != OpenParam_p->IOWin.OutputWin.Height)
	{
		return TH_ERROR_NOT_SUPPORT;
	}

	memset(&Inst.Bitmap, 0, sizeof(Inst.Bitmap));
	Inst.Bitmap.ColorType		= OpenParam_p->ColorType;
	Inst.Bitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_16TO9;
	Inst.Bitmap.Width			= OpenParam_p->Width;
	Inst.Bitmap.Height			= OpenParam_p->Height;
	Inst.Bitmap.Pitch			= Inst.Bitmap.Width<<2;
	Inst.Bitmap.Size			= Inst.Bitmap.Pitch*Inst.Bitmap.Height;

	if(OSD_AllocMem(&Inst.Bitmap, FALSE, FALSE) == NULL)
	{
		if(OSD_CacheFree_p)
		{
			(OSD_CacheFree_p)(FALSE);
			if(OSD_AllocMem(&Inst.Bitmap, FALSE, FALSE) == NULL)
			{
				(OSD_CacheFree_p)(TRUE);
				if(OSD_AllocMem(&Inst.Bitmap, FALSE, FALSE) == NULL)
				{
					FB_Check();
					return TH_ERROR_NO_MEM;
				}
			}
		}
	}

	Inst.bReference 		= FALSE;
	Inst.Transparency		=0;
	
	Inst.IOWin		= OpenParam_p->IOWin;
	if(Inst.IOWin.InputWin.LeftX + Inst.IOWin.InputWin.Width > OpenParam_p->Width)
	{
		Inst.IOWin.InputWin.LeftX = 0;
		Inst.IOWin.InputWin.Width = OpenParam_p->Width;
	}
	if(Inst.IOWin.InputWin.TopY + Inst.IOWin.InputWin.Height > OpenParam_p->Height)
	{
		Inst.IOWin.InputWin.TopY = 0;
		Inst.IOWin.InputWin.Height = OpenParam_p->Height;
	}
	if(Inst.IOWin.OutputWin.LeftX + Inst.IOWin.OutputWin.Width > OSD_WIDTH)
	{
		Inst.IOWin.OutputWin.LeftX = 0;
		Inst.IOWin.OutputWin.Width = OSD_WIDTH;
	}
	if(Inst.IOWin.OutputWin.TopY + Inst.IOWin.OutputWin.Height > OSD_HEIGHT)
	{
		Inst.IOWin.OutputWin.TopY = 0;
		Inst.IOWin.OutputWin.Height = OSD_HEIGHT;
	}
	Inst.bShow = FALSE;
	Inst.Transparency = 0;
//	OSD_SetDirtyWin(&(Inst.IOWin.OutputWin));

	OSD_OpenInstance(&Inst, Handle_p);

	if(OpenParam_p->ColorType == PAL_OSD_COLOR_TYPE_CLUT2 ||\
		OpenParam_p->ColorType == PAL_OSD_COLOR_TYPE_CLUT4 ||\
		OpenParam_p->ColorType == PAL_OSD_COLOR_TYPE_CLUT8)
	{
		ClearColor.Type = OpenParam_p->ColorType;
		ClearColor.Value.CLUT8 = OpenParam_p->ColorNum - 1;
	}
	else
	{
		ClearColor.Type = OpenParam_p->ColorType;
		memset(&(ClearColor.Value), 0, sizeof(ClearColor.Value));
	}
	
	PAL_OSD_Fill(*Handle_p, &(Inst.IOWin.InputWin), &ClearColor);
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_Close
函数功能	: 关闭一个OSD

函数参数	: IN:		Handle		OSD 控制句柄

函数返回	: 是否关闭成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_Close(PAL_OSD_Handle Handle)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;
	U32 Flag;
	

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	
	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	OSD_FreeMem(&Inst_p->Bitmap);
	OSD_RemoveFromList(Handle);
	
	OSD_SetDirtyWin(&(Inst_p->IOWin.OutputWin));

	Flag = Inst_p->Flag;
	OSD_ReleaseInstance(Handle);
	OSD_CloseInstance(Handle, Flag);
	return( Error );
}

TH_Error_t PAL_OSD_CloseNoShow(PAL_OSD_Handle Handle)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;
	U32 Flag;
	

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	
	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	OSD_FreeMem(&Inst_p->Bitmap);

	Flag = Inst_p->Flag;
	OSD_ReleaseInstance(Handle);
	OSD_CloseInstance(Handle, Flag);
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_Show
函数功能	: 显示OSD

函数参数	: IN:		Handle		OSD 控制句柄

函数返回	: 是否显示成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_Show(PAL_OSD_Handle Handle)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;
	
	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	
	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	if(Inst_p->bShow == TRUE)
	{
		OSD_ReleaseInstance(Handle);
		return TH_NO_ERROR;
	}
	
	Inst_p->bShow = TRUE;
	
	OSD_SetDirtyWin(&(Inst_p->IOWin.OutputWin));

	OSD_ReleaseInstance(Handle);
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_Hide
函数功能	: 隐藏OSD

函数参数	: IN:		Handle		OSD 控制句柄

函数返回	: 是否隐藏成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_Hide(PAL_OSD_Handle Handle)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	
	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	if(Inst_p->bShow == FALSE)
	{
		OSD_ReleaseInstance(Handle);
		return TH_NO_ERROR;
	}
	
	Inst_p->bShow = FALSE;
	
	OSD_SetDirtyWin(&(Inst_p->IOWin.OutputWin));

	OSD_ReleaseInstance(Handle);
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_SetTransparency
函数功能	: 设置OSD 透明度

函数参数	: IN:		Handle			OSD 控制句柄(如果传入的是TH_INVALID_HANDLE，说明是只是设置图层的透明度)
				  IN:		TransPercent		透明度的百分比

函数返回	: 是否设置成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_SetTransparency(PAL_OSD_Handle Handle, U8 TransPercent)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));

	if(Handle == TH_INVALID_HANDLE)
	{
		HIFB_ALPHA_S fb_alpha;
		
		memset(&fb_alpha, 0, sizeof(fb_alpha));
		fb_alpha.bAlphaEnable = HI_TRUE;
		fb_alpha.bAlphaChannel = HI_TRUE;
		fb_alpha.u8GlobalAlpha = 0x00FF*(100 - TransPercent)/100;
		if (ioctl(fb.fbfd, FBIOPUT_ALPHA_HIFB, &fb_alpha)<0)
		{
			printf("[fb] FBIOPUT_ALPHA_HIFB failed\n");
		}
	}
	else
	{
		Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
		if(Inst_p == NULL)
		{
			return TH_ERROR_BAD_PARAM;
		}
		
		Inst_p->Transparency = TransPercent;
		
		if(Inst_p->bShow == TRUE)
		{
			OSD_SetDirtyWin(&(Inst_p->IOWin.OutputWin));
		}
		OSD_ReleaseInstance(Handle);
	}
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_SetOrder
函数功能	: 调整OSD 顺序

函数参数	: IN:		Handle		OSD 控制句柄
				  IN:		Order		顺序模式
				  IN:		ReferHandle	参考OSD
				  
函数返回	: 是否设置成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_SetOrder(PAL_OSD_Handle Handle, PAL_OSD_Order_t Order, PAL_OSD_Handle ReferHandle)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;
	S32				i, Pos;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	
	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}

	switch(Order)
	{
		case PAL_OSD_ORDER_FRONT:
			THOS_SemaphoreWait(OSD_Access_p);
			for(i = 0; i<OSD_NUM; i++)
			{
				if(OSD_OrderList[i] == Handle)
				{
					for(; i<(OSD_NUM-1); i++)
					{
						OSD_OrderList[i] = OSD_OrderList[i+1];
					}
					OSD_OrderList[OSD_NUM-1] = TH_INVALID_HANDLE;
					break;
				}
			}
			for(i = 0; i<OSD_NUM; i++)
			{
				if(OSD_OrderList[i] == TH_INVALID_HANDLE)
				{
					OSD_OrderList[i] = Handle;
					break;
				}
			}
			THOS_SemaphoreSignal(OSD_Access_p);
			OSD_SetDirtyWin(&(Inst_p->IOWin.OutputWin));
			break;
		case PAL_OSD_ORDER_BACK:
			THOS_SemaphoreWait(OSD_Access_p);
			for(i = 0; i<OSD_NUM; i++)
			{
				if(OSD_OrderList[i] == Handle)
				{
					for(; i<(OSD_NUM-1); i++)
					{
						OSD_OrderList[i] = OSD_OrderList[i+1];
					}
					OSD_OrderList[OSD_NUM-1] = TH_INVALID_HANDLE;
					break;
				}
			}
			for(i = OSD_NUM-1; i>0; i--)
			{
				OSD_OrderList[i] = OSD_OrderList[i-1];
			}
			OSD_OrderList[0] = Handle;
			THOS_SemaphoreSignal(OSD_Access_p);
			OSD_SetDirtyWin(&(Inst_p->IOWin.OutputWin));
			break;
		case PAL_OSD_ORDER_REF_BACK:
		case PAL_OSD_ORDER_REF_FRONT:
			OSD_SetDirtyWin(&(Inst_p->IOWin.OutputWin));
			Inst_p = OSD_ReferenceInstance(ReferHandle, TH_TIMEOUT_INFINITY);
			if(Inst_p == NULL)
			{
				OSD_ReleaseInstance(Handle);
				return TH_ERROR_BAD_PARAM;
			}
			
			THOS_SemaphoreWait(OSD_Access_p);
			/*断开原先在列表中的该点*/
			for(i = 0; i<OSD_NUM; i++)
			{
				if(OSD_OrderList[i] == Handle)
				{
					for(; i<(OSD_NUM-1); i++)
					{
						OSD_OrderList[i] = OSD_OrderList[i+1];
					}
					OSD_OrderList[OSD_NUM-1] = TH_INVALID_HANDLE;
					break;
				}
			}
			
			/*插入到相应位置*/
			for(i = 0; i<OSD_NUM; i++)
			{
				if(OSD_OrderList[i] == ReferHandle)
				{
					if(Order == PAL_OSD_ORDER_REF_FRONT)
					{
						i ++;
					}
					Pos = i;
					for(i = OSD_NUM-1; i>=Pos+1; i--)
					{
						OSD_OrderList[i] = OSD_OrderList[i-1];
					}
					OSD_OrderList[Pos] = Handle;
					break;
				}
			}
			THOS_SemaphoreSignal(OSD_Access_p);
			
			OSD_ReleaseInstance(ReferHandle);
			break;
	}
	
	OSD_ReleaseInstance(Handle);
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_SetIOWin
函数功能	: 设置OSD 输入输出窗口

函数参数	: IN:		Handle		OSD 控制句柄
				  IN:		IOWin_p		输入输出窗口参数

函数返回	: 是否设置成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_SetIOWin(PAL_OSD_Handle  Handle, PAL_OSD_IO_Win_t *IOWin_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));

	if(IOWin_p->InputWin.Width != IOWin_p->OutputWin.Width ||\
		IOWin_p->InputWin.Height != IOWin_p->OutputWin.Height)/*不支持输入输出窗口的缩放*/
	{
		return TH_ERROR_NOT_SUPPORT;
	}

	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	if(((IOWin_p->InputWin.LeftX + IOWin_p->InputWin.Width) > Inst_p->Bitmap.Width) ||\
		((IOWin_p->InputWin.TopY + IOWin_p->InputWin.Height) > Inst_p->Bitmap.Height) ||\
		((IOWin_p->OutputWin.LeftX + IOWin_p->OutputWin.Width) > OSD_WIDTH) ||\
		((IOWin_p->OutputWin.TopY + IOWin_p->OutputWin.Height) > OSD_HEIGHT))/*输入输出窗口超界*/
	{
		OSD_ReleaseInstance(Handle);
		return TH_ERROR_BAD_PARAM;
	}
	if(Inst_p->bShow == TRUE)
	{
		OSD_SetDirtyWin(&(Inst_p->IOWin.OutputWin));
		Inst_p->IOWin = *IOWin_p;
		OSD_SetDirtyWin(&(Inst_p->IOWin.OutputWin));
	}
	OSD_ReleaseInstance(Handle);
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_GetSrcBitmap
函数功能	: 获取OSD 源的位图结构，记得用完位图参数后要用
					PAL_OSD_PutSrcBitmap 来释放控制权，否则该OSD 将无法使用

函数参数	: IN:		Handle			OSD 控制句柄
				  OUT:	SrcBitmap_p		OSD 的源位图结构

函数返回	: 是否获取成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_GetSrcBitmap(PAL_OSD_Handle  Handle, PAL_OSD_Bitmap_t *SrcBitmap_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;

	if(SrcBitmap_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	if(Handle == PAL_OSD_SppHandle)
	{
		*SrcBitmap_p = OSD_SppBitmap;
		return( TH_NO_ERROR );
	}

	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	*SrcBitmap_p = Inst_p->Bitmap;
	
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_PutSrcBitmap
函数功能	: 释放OSD 控制权，与PAL_OSD_GetSrcBitmap 配套使用

函数参数	: IN:		Handle			OSD 控制句柄

函数返回	: 是否释放成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_PutSrcBitmap(PAL_OSD_Handle  Handle)
{
	TH_Error_t Error = TH_NO_ERROR;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	if(Handle == PAL_OSD_SppHandle)
	{
		return( TH_NO_ERROR );
	}
	
	OSD_ReleaseInstance(Handle);
	
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_Copy
函数功能	: 拷贝指定窗口的数据

函数参数	: IN:		DestBitmap_p			目标位图
				  IN:		DestPos_p			目标坐标
				  IN:		SrcBitmap_p			源位图
				  IN:		SrcWin_p				拷贝源窗口

函数返回	: 是否拷贝成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_Copy(PAL_OSD_Bitmap_t *DestBitmap_p, 
								PAL_OSD_Win_t *DestWin_p, 
								PAL_OSD_Bitmap_t *SrcBitmap_p,
								PAL_OSD_Win_t *SrcWin_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Private_t      *src_pf, *dest_pf;

	if(SrcWin_p->Width <= 0 || SrcWin_p->Height <= 0 || DestWin_p->Width <= 0 || DestWin_p->Height <= 0)
	{
		return TH_ERROR_BAD_PARAM;
	}

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));

	THOS_SemaphoreWait(BLIT_Access_p);
	src_pf = SrcBitmap_p->Private_p;
	dest_pf = DestBitmap_p->Private_p;
	
	fb_blit((int)src_pf->phy, SrcBitmap_p->Width, SrcBitmap_p->Height, SrcBitmap_p->Pitch, OSD_TransColorType2Platform(SrcBitmap_p->ColorType),
		(int)dest_pf->phy, DestBitmap_p->Width, DestBitmap_p->Height, DestBitmap_p->Pitch,
		SrcWin_p->LeftX, SrcWin_p->TopY, SrcWin_p->Width, SrcWin_p->Height,
		DestWin_p->LeftX, DestWin_p->TopY, DestWin_p->Width, DestWin_p->Height,
		0, 0);

	THOS_SemaphoreSignal(BLIT_Access_p);

	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_Put
函数功能	: 将位图数据放到指定的OSD 位置上

函数参数	: IN:		Handle			OSD 控制句柄
				  IN:		DestWin_p		位图放置的坐标
				  IN:		Bitmap_p			位图数据结构

函数返回	: 是否放置成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_Put(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *DestWin_p, PAL_OSD_Win_t *SrcWin_p, PAL_OSD_Bitmap_t *Bitmap_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Private_t      *src_pf, *dest_pf;
	OSD_Instance_t  *Inst_p;
	PAL_OSD_Win_t		ScreenWin;

	if(SrcWin_p->Width <= 0 || SrcWin_p->Height <= 0 || DestWin_p->Width <= 0 || DestWin_p->Height <= 0)
	{
		return TH_ERROR_BAD_PARAM;
	}

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	
	if(Handle == PAL_OSD_SppHandle)
	{
		THOS_SemaphoreWait(BLIT_Access_p);
		src_pf = Bitmap_p->Private_p;
		dest_pf = OSD_SppBitmap.Private_p;
		
		fb_blit((int)src_pf->phy, Bitmap_p->Width, Bitmap_p->Height, Bitmap_p->Pitch, OSD_TransColorType2Platform(Bitmap_p->ColorType),
			(int)dest_pf->phy, OSD_SppBitmap.Width, OSD_SppBitmap.Height, OSD_SppBitmap.Pitch,
			SrcWin_p->LeftX, SrcWin_p->TopY, SrcWin_p->Width, SrcWin_p->Height,
			DestWin_p->LeftX, DestWin_p->TopY, DestWin_p->Width, DestWin_p->Height,
			0, 0);

		THOS_SemaphoreSignal(BLIT_Access_p);
		return( TH_NO_ERROR );
	}

	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}

	THOS_SemaphoreWait(BLIT_Access_p);
	src_pf = Bitmap_p->Private_p;
	dest_pf = Inst_p->Bitmap.Private_p;
	
	fb_blit((int)src_pf->phy, Bitmap_p->Width, Bitmap_p->Height, Bitmap_p->Pitch, OSD_TransColorType2Platform(Bitmap_p->ColorType),
		(int)dest_pf->phy, Inst_p->Bitmap.Width, Inst_p->Bitmap.Height, Inst_p->Bitmap.Pitch,
		SrcWin_p->LeftX, SrcWin_p->TopY, SrcWin_p->Width, SrcWin_p->Height,
		DestWin_p->LeftX, DestWin_p->TopY, DestWin_p->Width, DestWin_p->Height,
		0, 0);

	THOS_SemaphoreSignal(BLIT_Access_p);

	if(Inst_p->bShow == TRUE)
	{
		OSD_ConflictWin(DestWin_p, &(Inst_p->IOWin.InputWin), &ScreenWin);
		
		OSD_ConvSrcWin2ScreenWin(Inst_p, &ScreenWin, &ScreenWin);
		
		OSD_SetDirtyWin(&ScreenWin);
	}
	
	OSD_ReleaseInstance(Handle);
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_CheckAlpha
函数功能	: 检查指定的OSD 区域是否为全透明数据

函数参数	: IN:		Handle			OSD 控制句柄
				  IN:		DestWin_p		需要检测区域
				  
函数返回	: 是否为全透明区域

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
BOOL PAL_OSD_CheckAlpha(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *DestWin_p)
{
	return FALSE;
}

/*******************************************************************************
函数名称	: PAL_OSD_PutAlpha
函数功能	: 将位图数据放到指定的OSD 位置上并进行透明度混合

函数参数	: IN:		Handle			OSD 控制句柄
				  IN:		DestWin_p		位图放置的坐标
				  IN:		Bitmap_p			位图数据结构
				  
函数返回	: 是否放置成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_PutAlpha(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *DestWin_p, PAL_OSD_Win_t *SrcWin_p, PAL_OSD_Bitmap_t *Bitmap_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Private_t      *src_pf, *dest_pf;
	OSD_Instance_t  *Inst_p;
	PAL_OSD_Win_t		ScreenWin;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));

	if(SrcWin_p->Width <= 0 || SrcWin_p->Height <= 0 || DestWin_p->Width <= 0 || DestWin_p->Height <= 0)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	if(Handle == PAL_OSD_SppHandle)
	{
		THOS_SemaphoreWait(BLIT_Access_p);
		src_pf = Bitmap_p->Private_p;
		dest_pf = OSD_SppBitmap.Private_p;
		
		fb_blit((int)src_pf->phy, Bitmap_p->Width, Bitmap_p->Height, Bitmap_p->Pitch, OSD_TransColorType2Platform(Bitmap_p->ColorType),
			(int)dest_pf->phy, OSD_SppBitmap.Width, OSD_SppBitmap.Height, OSD_SppBitmap.Pitch,
			SrcWin_p->LeftX, SrcWin_p->TopY, SrcWin_p->Width, SrcWin_p->Height,
			DestWin_p->LeftX, DestWin_p->TopY, DestWin_p->Width, DestWin_p->Height,
			0, blitAlphaBlend);

		THOS_SemaphoreSignal(BLIT_Access_p);
		return( TH_NO_ERROR );
	}

	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}

	THOS_SemaphoreWait(BLIT_Access_p);
	src_pf = Bitmap_p->Private_p;
	dest_pf = Inst_p->Bitmap.Private_p;
	
	fb_blit((int)src_pf->phy, Bitmap_p->Width, Bitmap_p->Height, Bitmap_p->Pitch, OSD_TransColorType2Platform(Bitmap_p->ColorType),
		(int)dest_pf->phy, Inst_p->Bitmap.Width, Inst_p->Bitmap.Height, Inst_p->Bitmap.Pitch,
		SrcWin_p->LeftX, SrcWin_p->TopY, SrcWin_p->Width, SrcWin_p->Height,
		DestWin_p->LeftX, DestWin_p->TopY, DestWin_p->Width, DestWin_p->Height,
		0, blitAlphaBlend);

	THOS_SemaphoreSignal(BLIT_Access_p);

	if(Inst_p->bShow == TRUE)
	{
		OSD_ConflictWin(DestWin_p, &(Inst_p->IOWin.InputWin), &ScreenWin);
		
		OSD_ConvSrcWin2ScreenWin(Inst_p, &ScreenWin, &ScreenWin);
		
		OSD_SetDirtyWin(&ScreenWin);
	}
	
	OSD_ReleaseInstance(Handle);
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_Fill
函数功能	: 将颜色数据填充到指定的OSD 位置上

函数参数	: IN:		Handle			OSD 控制句柄
				  IN:		Win_p			填充的坐标
				  IN:		Color_p			填充的颜色

函数返回	: 是否真充成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_FillAlpha(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *Win_p, PAL_OSD_Color_t *Color_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t	*Inst_p;
	PAL_OSD_Win_t		ScreenWin;
	OSD_Private_t	   *pf, *dest_pf;
	PAL_OSD_Bitmap_t	TempBitmap;
	unsigned long color;
	
	if(Win_p->Width <= 0 || Win_p->Height <= 0)
	{
		return TH_ERROR_BAD_PARAM;
	}

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	if(Handle == PAL_OSD_SppHandle)
	{
		memset(&TempBitmap, 0, sizeof(TempBitmap));
		TempBitmap.ColorType = OSD_SppBitmap.ColorType;
		TempBitmap.AspectRatio = OSD_SppBitmap.AspectRatio;
		TempBitmap.Width = Win_p->Width;
		TempBitmap.Height = Win_p->Height;
		if(PAL_OSD_AllocBitmap(&TempBitmap, NULL, FALSE) != TH_NO_ERROR)
		{
			return (TH_ERROR_NO_MEM);
		}
		THOS_SemaphoreWait(BLIT_Access_p);
		pf = TempBitmap.Private_p;
		color = (Color_p->Value.ARGB8888.Alpha<<24)|\
				(Color_p->Value.ARGB8888.R<<16)|\
				(Color_p->Value.ARGB8888.G<<8)|\
				(Color_p->Value.ARGB8888.B);
		fb_blit_fill(
				(int)pf->phy, TempBitmap.Width, TempBitmap.Height, TempBitmap.Pitch,
				0, 0, TempBitmap.Width, TempBitmap.Height,
				color);

		dest_pf = OSD_SppBitmap.Private_p;
		fb_blit((int)pf->phy, TempBitmap.Width, TempBitmap.Height, TempBitmap.Pitch, OSD_TransColorType2Platform(TempBitmap.ColorType),
			(int)dest_pf->phy, OSD_SppBitmap.Width, OSD_SppBitmap.Height, OSD_SppBitmap.Pitch,
			0, 0, TempBitmap.Width, TempBitmap.Height,
			Win_p->LeftX, Win_p->TopY, Win_p->Width, Win_p->Height,
			0, blitAlphaBlend);

		THOS_SemaphoreSignal(BLIT_Access_p);
		PAL_OSD_FreeBitmap(&TempBitmap);
		return( TH_NO_ERROR );
	}

	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		OSD_DEBUG(printf("[GUI]Filled Rectangle failed %d!\n", __LINE__));
		return TH_ERROR_BAD_PARAM;
	}

	memset(&TempBitmap, 0, sizeof(TempBitmap));
	TempBitmap.ColorType = Inst_p->Bitmap.ColorType;
	TempBitmap.AspectRatio = Inst_p->Bitmap.AspectRatio;
	TempBitmap.Width = Win_p->Width;
	TempBitmap.Height = Win_p->Height;
	if(PAL_OSD_AllocBitmap(&TempBitmap, NULL, FALSE) != TH_NO_ERROR)
	{
		OSD_ReleaseInstance(Handle);
		return (TH_ERROR_NO_MEM);
	}
	THOS_SemaphoreWait(BLIT_Access_p);
	pf = TempBitmap.Private_p;
	color = (Color_p->Value.ARGB8888.Alpha<<24)|\
			(Color_p->Value.ARGB8888.R<<16)|\
			(Color_p->Value.ARGB8888.G<<8)|\
			(Color_p->Value.ARGB8888.B);
	fb_blit_fill(
			(int)pf->phy, TempBitmap.Width, TempBitmap.Height, TempBitmap.Pitch,
			0, 0, TempBitmap.Width, TempBitmap.Height,
			color);

	dest_pf = Inst_p->Bitmap.Private_p;
	fb_blit((int)pf->phy, TempBitmap.Width, TempBitmap.Height, TempBitmap.Pitch, OSD_TransColorType2Platform(TempBitmap.ColorType),
		(int)dest_pf->phy, Inst_p->Bitmap.Width, Inst_p->Bitmap.Height, Inst_p->Bitmap.Pitch,
		0, 0, TempBitmap.Width, TempBitmap.Height,
		Win_p->LeftX, Win_p->TopY, Win_p->Width, Win_p->Height,
		0, blitAlphaBlend);
	
	THOS_SemaphoreSignal(BLIT_Access_p);
	
	PAL_OSD_FreeBitmap(&TempBitmap);

	if(Inst_p->bShow == TRUE)
	{
		OSD_ConflictWin(Win_p, &(Inst_p->IOWin.InputWin), &ScreenWin);
		
		OSD_ConvSrcWin2ScreenWin(Inst_p, &ScreenWin, &ScreenWin);
		
		OSD_SetDirtyWin(&ScreenWin);
	}
	
	OSD_ReleaseInstance(Handle);

	return( Error );
}

TH_Error_t PAL_OSD_Fill(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *Win_p, PAL_OSD_Color_t *Color_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;
	PAL_OSD_Win_t		ScreenWin;
	OSD_Private_t      *pf;
	unsigned long color;
	
	if(Win_p->Width <= 0 || Win_p->Height <= 0)
	{
		return TH_ERROR_BAD_PARAM;
	}

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	if(Color_p->Value.ARGB8888.Alpha != 0x00 &&\
		Color_p->Value.ARGB8888.Alpha != 0xFF)
	{
		return PAL_OSD_FillAlpha(Handle, Win_p, Color_p);
	}

	if(Handle == PAL_OSD_SppHandle)
	{
		THOS_SemaphoreWait(BLIT_Access_p);
		pf = OSD_SppBitmap.Private_p;

		color = (Color_p->Value.ARGB8888.Alpha<<24)|\
			(Color_p->Value.ARGB8888.R<<16)|\
			(Color_p->Value.ARGB8888.G<<8)|\
			(Color_p->Value.ARGB8888.B);
		fb_blit_fill(
				(int)pf->phy, OSD_SppBitmap.Width, OSD_SppBitmap.Height, OSD_SppBitmap.Pitch,
				Win_p->LeftX, Win_p->TopY, Win_p->Width, Win_p->Height,
				color);

		THOS_SemaphoreSignal(BLIT_Access_p);
		return( TH_NO_ERROR );
	}

	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		OSD_DEBUG(printf("[GUI]Filled Rectangle failed %d!\n", __LINE__));
		return TH_ERROR_BAD_PARAM;
	}

	THOS_SemaphoreWait(BLIT_Access_p);
	pf = Inst_p->Bitmap.Private_p;

	color = (Color_p->Value.ARGB8888.Alpha<<24)|\
		(Color_p->Value.ARGB8888.R<<16)|\
		(Color_p->Value.ARGB8888.G<<8)|\
		(Color_p->Value.ARGB8888.B);
	fb_blit_fill(
			(int)pf->phy, Inst_p->Bitmap.Width, Inst_p->Bitmap.Height, Inst_p->Bitmap.Pitch,
			Win_p->LeftX, Win_p->TopY, Win_p->Width, Win_p->Height,
			color);

	THOS_SemaphoreSignal(BLIT_Access_p);

	if(Inst_p->bShow == TRUE)
	{
		OSD_ConflictWin(Win_p, &(Inst_p->IOWin.InputWin), &ScreenWin);
		
		OSD_ConvSrcWin2ScreenWin(Inst_p, &ScreenWin, &ScreenWin);
		
		OSD_SetDirtyWin(&ScreenWin);
	}
	
	OSD_ReleaseInstance(Handle);

	return( Error );
}

TH_Error_t PAL_OSD_FillEx(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *Win_p, PAL_OSD_Color_t *Color_p, BOOL Copy)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;
	PAL_OSD_Win_t		ScreenWin;
	OSD_Private_t      *pf;
	unsigned long color;
	
	if(Win_p->Width <= 0 || Win_p->Height <= 0)
	{
		return TH_ERROR_BAD_PARAM;
	}

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	if(Copy == FALSE &&\
		Color_p->Value.ARGB8888.Alpha != 0x00 &&\
		Color_p->Value.ARGB8888.Alpha != 0xFF)
	{
		return PAL_OSD_FillAlpha(Handle, Win_p, Color_p);
	}
	
	if(Handle == PAL_OSD_SppHandle)
	{
		THOS_SemaphoreWait(BLIT_Access_p);
		pf = OSD_SppBitmap.Private_p;

		color = (Color_p->Value.ARGB8888.Alpha<<24)|\
			(Color_p->Value.ARGB8888.R<<16)|\
			(Color_p->Value.ARGB8888.G<<8)|\
			(Color_p->Value.ARGB8888.B);
		fb_blit_fill(
				(int)pf->phy, OSD_SppBitmap.Width, OSD_SppBitmap.Height, OSD_SppBitmap.Pitch,
				Win_p->LeftX, Win_p->TopY, Win_p->Width, Win_p->Height,
				color);

		THOS_SemaphoreSignal(BLIT_Access_p);
		return( TH_NO_ERROR );
	}

	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		OSD_DEBUG(printf("[GUI]Filled Rectangle failed %d!\n", __LINE__));
		return TH_ERROR_BAD_PARAM;
	}

	THOS_SemaphoreWait(BLIT_Access_p);
	pf = Inst_p->Bitmap.Private_p;

	color = (Color_p->Value.ARGB8888.Alpha<<24)|\
		(Color_p->Value.ARGB8888.R<<16)|\
		(Color_p->Value.ARGB8888.G<<8)|\
		(Color_p->Value.ARGB8888.B);
	fb_blit_fill(
			(int)pf->phy, Inst_p->Bitmap.Width, Inst_p->Bitmap.Height, Inst_p->Bitmap.Pitch,
			Win_p->LeftX, Win_p->TopY, Win_p->Width, Win_p->Height,
			color);

	THOS_SemaphoreSignal(BLIT_Access_p);

	if(Inst_p->bShow == TRUE)
	{
		OSD_ConflictWin(Win_p, &(Inst_p->IOWin.InputWin), &ScreenWin);
		
		OSD_ConvSrcWin2ScreenWin(Inst_p, &ScreenWin, &ScreenWin);
		
		OSD_SetDirtyWin(&ScreenWin);
	}
	
	OSD_ReleaseInstance(Handle);

	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_SetPalette
函数功能	: 设置指定OSD 的色板

函数参数	: IN:		Handle			OSD 控制句柄
				  IN:		Palette_p			色板数据
				  IN:		StartColorIndex	开始设置的色板索引
				  IN:		SetColorNum		需要设置的颜色个数

函数返回	: 是否设置成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_SetPalette(PAL_OSD_Handle  Handle, 
										PAL_OSD_Color_t *Palette_p, 
										U32 StartColorIndex, 
										U32 SetColorNum)
{
	TH_Error_t Error = TH_ERROR_NOT_SUPPORT;
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_GetPalette
函数功能	: 获取指定OSD 的色板

函数参数	: IN:		Handle			OSD 控制句柄
				  OUT:	Palette_p			获取的色板数据
				  IN:		StartColorIndex	开始获取的色板索引
				  IN:		GetColorNum		需要获取的颜色个数

函数返回	: 是否获取成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_GetPalette(PAL_OSD_Handle  Handle, 
										PAL_OSD_Color_t *Palette_p, 
										U32 StartColorIndex, 
										U32 GetColorNum)
{
	TH_Error_t Error = TH_ERROR_NOT_SUPPORT;
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_GetPlatformPalette
函数功能	: 获取指定OSD 的平台相关色板，此函数用于，
					当应用层有多个位图 使用同一张色板时，可以
					通过此函数来获取已经创建的平台色板，用于
					创建另一个位图结构 时使用，这样可以省掉一些空间.
					创建位图结构是调用PAL_OSD_AllocBitmap(PAL_OSD_Bitmap_t *Bitmap_p, void *PlatformPalette_p, BOOL Cache),
					第二个参数便是平台色板数据

函数参数	: IN:		Handle			OSD 控制句柄

函数返回	: 平台色板指针

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
void *PAL_OSD_GetPlatformPalette(PAL_OSD_Handle  Handle)
{
	OSD_Instance_t  *Inst_p;
	void *PlatformPalette_p;

	OSD_INIT_CHECK(OSD_Access_p, return (NULL));
	
	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return NULL;
	}
	
	PlatformPalette_p = Inst_p->Bitmap.PlatformPalette_p;

	OSD_ReleaseInstance(Handle);

	return( PlatformPalette_p );
}

/*******************************************************************************
函数名称	: PAL_OSD_SetDirtyWin
函数功能	: 设置OSD 的修改窗口

函数参数	: IN:		Handle		OSD 控制句柄
				  IN:		DirtyWin_p	改动的窗口

函数返回	: 是否设置成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_SetDirtyWin(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *DirtyWin_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;
	PAL_OSD_Win_t	ScreenWin;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));

	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	if(Inst_p->bShow == TRUE)
	{
		OSD_ConflictWin(DirtyWin_p, &(Inst_p->IOWin.InputWin), &ScreenWin);
		
		OSD_ConvSrcWin2ScreenWin(Inst_p, &ScreenWin, &ScreenWin);
		
		OSD_SetDirtyWin(&ScreenWin);
	}

	OSD_ReleaseInstance(Handle);
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_UpdateDisplay

函数功能	: 将更新后的OSD 显示到屏幕上

函数参数	: 无
				 

函数返回	: 无

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
void PAL_OSD_UpdateDisplay(void)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;
	S32		i;
	int ret = 0;
	PAL_OSD_Win_t		DirtyWin;
	PAL_OSD_Handle		OsdList[OSD_NUM];
	U32					ReferNum;
	PAL_OSD_Win_t		DestWin, SrcWin;
	OSD_Private_t      *pf, *src_pf;
	BOOL			FirstLayer = TRUE;
	
	OSD_INIT_CHECK(OSD_Access_p, return );
	
	if(OSD_DirtyWin.Width == 0)/*检查是否需要更新*/
	{
		printf("oooooooooooooooooooooo\n");
		return ;
	}

	/*锁定所有的OSD*/
	ReferNum = 0;
	for(i=0; i<OSD_NUM; i++)
	{
		OsdList[ReferNum] = OSD_OrderList[i];
		if(OsdList[ReferNum] == TH_INVALID_HANDLE)
		{
			continue;
		}
		Inst_p = OSD_ReferenceInstance(OsdList[ReferNum], TH_TIMEOUT_IMMEDIATE);
		if(Inst_p != NULL)
		{
			ReferNum ++;
		}
		else
		{
			goto release1;
			OsdList[ReferNum] = TH_INVALID_HANDLE;
		}
	}
	
	if(ReferNum == 0)
	{
		return;
	}
	OSD_DEBUG(printf("file = %s, line = %d\n", __FILE__, __LINE__));

	THOS_SemaphoreWait(OSD_Access_p);
	DirtyWin = OSD_DirtyWin;
	memset(&OSD_DirtyWin, 0, sizeof(OSD_DirtyWin));
	THOS_SemaphoreSignal(OSD_Access_p);
	if(DirtyWin.Width == 0)
	{
		goto release1;
	}

	THOS_SemaphoreWait(BLIT_Access_p);

	/*clear osd draw region */
	if(OSD_InitDrawBitmap() != TH_NO_ERROR)
	{
		goto release;
	}
	OSD_DEBUG(printf("file = %s, line = %d\n", __FILE__, __LINE__));
	pf = OSD_DRAW_BITMAP.Private_p;
	fb_blit_fill((int)pf->phy, OSD_WIDTH, OSD_HEIGHT, OSD_DRAW_BITMAP.Pitch,
			0, 0, OSD_WIDTH, OSD_HEIGHT,
			0);

	/*调用subt 提供的一个刷新字幕的接口函数
	SUBT_UpdateDisplay(0, &StDest.Rectangle, FALSE);*/

	/*copy all osd regions to draw region*/
	for(i=0; i<ReferNum; i++)
	{
		if(OsdList[i] >= OSD_NUM)
		{
			continue;
		}
		
		Inst_p = &(OSD_Instance[OsdList[i]]);
		if(Inst_p->Flag == 0 || Inst_p->bShow == FALSE)/*已经被关闭或禁止显示*/
		{
			continue;
		}
		
		OSD_ConflictWin(&DirtyWin, &(Inst_p->IOWin.OutputWin), &DestWin);/*计算屏幕更新窗口对应该OSD 的输出窗口*/

		if(DestWin.Width == 0 ||\
			DestWin.Height == 0)
		{
			continue;
		}

		OSD_ConvScreenWin2SrcWin(Inst_p, &DestWin, &SrcWin);/*将该OSD 的输出窗口转换成对应的输入窗口*/
		OSD_DEBUG(printf("file = %s, line = %d, [%d, %d, %d, %d]:[%d, %d, %d, %d]\n", __FILE__, __LINE__, SrcWin.LeftX, SrcWin.TopY, SrcWin.Width, SrcWin.Height, DestWin.LeftX, DestWin.TopY, DestWin.Width, DestWin.Height));
		src_pf = Inst_p->Bitmap.Private_p;
		if(FirstLayer)
		{
			FirstLayer = FALSE;
			fb_blit((int)src_pf->phy, Inst_p->Bitmap.Width, Inst_p->Bitmap.Height, Inst_p->Bitmap.Pitch, OSD_TransColorType2Platform(Inst_p->Bitmap.ColorType),
				(int)pf->phy, OSD_DRAW_BITMAP.Width, OSD_DRAW_BITMAP.Height, OSD_DRAW_BITMAP.Pitch,
				SrcWin.LeftX, SrcWin.TopY, SrcWin.Width, SrcWin.Height,
				DestWin.LeftX, DestWin.TopY, DestWin.Width, DestWin.Height,
				0, 0);
		}
		else
		{
			fb_blit_ex((int)src_pf->phy, Inst_p->Bitmap.Width, Inst_p->Bitmap.Height, Inst_p->Bitmap.Pitch, OSD_TransColorType2Platform(Inst_p->Bitmap.ColorType),
				(int)pf->phy, OSD_DRAW_BITMAP.Width, OSD_DRAW_BITMAP.Height, OSD_DRAW_BITMAP.Pitch,
				SrcWin.LeftX, SrcWin.TopY, SrcWin.Width, SrcWin.Height,
				DestWin.LeftX, DestWin.TopY, DestWin.Width, DestWin.Height,
				0, blitAlphaBlend, 0x00FF*(100 - Inst_p->Transparency)/100);
		}
	}
	OSD_DEBUG(printf("file = %s, line = %d\n", __FILE__, __LINE__));

	/*copy draw osd to show osd*/
	src_pf = OSD_DRAW_BITMAP.Private_p;
	pf = OSD_SHOW_BITMAP.Private_p;

	fb_blit((int)src_pf->phy, OSD_DRAW_BITMAP.Width, OSD_DRAW_BITMAP.Height, OSD_DRAW_BITMAP.Pitch, OSD_TransColorType2Platform(OSD_DRAW_BITMAP.ColorType),
		(int)pf->phy, OSD_SHOW_BITMAP.Width, OSD_SHOW_BITMAP.Height, OSD_SHOW_BITMAP.Pitch,
		DirtyWin.LeftX, DirtyWin.TopY, DirtyWin.Width, DirtyWin.Height,
		DirtyWin.LeftX, DirtyWin.TopY, DirtyWin.Width, DirtyWin.Height,
		0, 0);
	
	OSD_TermDrawBitmap();

release:
	THOS_SemaphoreSignal(BLIT_Access_p);
	OSD_DEBUG(printf("file = %s, line = %d\n", __FILE__, __LINE__));

release1:
	/*释放所有锁定的OSD*/
	for(i=0; i<ReferNum; i++)
	{
		OSD_ReleaseInstance(OsdList[i]);
	}
}

void PAL_OSD_ShowWin(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *DestWin_p, PAL_OSD_Win_t *SrcWin_p)
{
	PAL_OSD_Bitmap_t TempBitmap;

	PAL_OSD_GetSrcBitmap(Handle, &TempBitmap);
	PAL_OSD_Copy(&OSD_SHOW_BITMAP, DestWin_p, &TempBitmap, SrcWin_p);
	PAL_OSD_PutSrcBitmap(Handle);
}

/*******************************************************************************
函数名称	: PAL_OSD_AllocBitmap
函数功能	: 分配一个位图

函数参数	: IN/OUT:		Bitmap_p			位图数据
				  IN:				PlatformPalette_p	平台色板数据
				  IN:				Cache			是否是cache 空间
				 

函数返回	: 是否分配成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_AllocBitmap(PAL_OSD_Bitmap_t *Bitmap_p, void *PlatformPalette_p, BOOL Cache)
{
	TH_Error_t Error = TH_NO_ERROR;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	if(Bitmap_p == NULL || Bitmap_p->Private_p != NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}

	if(Bitmap_p->ColorType == PAL_OSD_COLOR_TYPE_RGB565 ||\
		Bitmap_p->ColorType == PAL_OSD_COLOR_TYPE_SIGNED_YCBCR888_422)
	{
		Bitmap_p->Pitch			= Bitmap_p->Width<<1;
	}
	else if(Bitmap_p->ColorType == PAL_OSD_COLOR_TYPE_RGB888)
	{
		Bitmap_p->Pitch			= Bitmap_p->Width*3;
	}
	else
	{
		Bitmap_p->Pitch			= Bitmap_p->Width<<2;
	}
	Bitmap_p->Pitch = (Bitmap_p->Pitch + 16 - 1) & (~(16 - 1));

	Bitmap_p->Size			= Bitmap_p->Pitch*Bitmap_p->Height;

	if(OSD_AllocMem(Bitmap_p, FALSE, TRUE) == NULL)
	{
		Error = TH_ERROR_NO_MEM;
	}
	if(Error == TH_ERROR_NO_MEM)
	{
		Error = TH_NO_ERROR;
		if(OSD_CacheFree_p)
		{
			(OSD_CacheFree_p)(FALSE);
			if(OSD_AllocMem(Bitmap_p, FALSE, TRUE) == NULL)
			{
				(OSD_CacheFree_p)(TRUE);
				if(OSD_AllocMem(Bitmap_p, FALSE, TRUE) == NULL)
				{
					FB_Check();
					Error = TH_ERROR_NO_MEM;
				}
			}
		}
	}
	return( Error );
}

TH_Error_t PAL_OSD_AllocBitmapFromVideo(PAL_OSD_Bitmap_t *Bitmap_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Private_t      *pf;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	if(Bitmap_p == NULL || Bitmap_p->Private_p != NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}

	if(Bitmap_p->ColorType == PAL_OSD_COLOR_TYPE_RGB565 ||\
		Bitmap_p->ColorType == PAL_OSD_COLOR_TYPE_SIGNED_YCBCR888_422)
	{
		Bitmap_p->Pitch			= Bitmap_p->Width<<1;
	}
	else if(Bitmap_p->ColorType == PAL_OSD_COLOR_TYPE_RGB888)
	{
		Bitmap_p->Pitch			= Bitmap_p->Width*3;
	}
	else
	{
		Bitmap_p->Pitch			= Bitmap_p->Width<<2;
	}
	Bitmap_p->Pitch = (Bitmap_p->Pitch + 16 - 1) & (~(16 - 1));

	Bitmap_p->Size			= Bitmap_p->Pitch*Bitmap_p->Height;
	
	pf = malloc(sizeof(OSD_Private_t));
	if(pf == NULL)
	{
		printf("[OSD]: malloc failed\n");
		return TH_ERROR_NO_MEM;
	}

	return( Error );
}

TH_Error_t PAL_OSD_SetCacheFree(void (*CacheFree_p)(BOOL Force))
{
	OSD_CacheFree_p = CacheFree_p;
	return TH_NO_ERROR;
}

/*******************************************************************************
函数名称	: PAL_OSD_FreeBitmap
函数功能	: 释放一个位图

函数参数	: IN/OUT:		Bitmap_p			位图数据
				 

函数返回	: 是否释放成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_FreeBitmap(PAL_OSD_Bitmap_t *Bitmap_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	
	if(Bitmap_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}

	OSD_FreeMem(Bitmap_p);
	memset(Bitmap_p, 0, sizeof(PAL_OSD_Bitmap_t));
	
	return( Error );
}

void *PAL_OSD_AllocMemory(U32 Size, BOOL FromTop)
{
	TH_Error_t Error = TH_NO_ERROR;
	void *ptr;
	
	OSD_INIT_CHECK(OSD_Access_p, return (NULL));
//	FB_Check();

	if((ptr = FB_Malloc(Size, FromTop)) == NULL)
	{
		Error = TH_ERROR_NO_MEM;
	}
	if(Error == TH_ERROR_NO_MEM)
	{
		Error = TH_NO_ERROR;
		if(OSD_CacheFree_p)
		{
			(OSD_CacheFree_p)(FALSE);
			if((ptr = FB_Malloc(Size, FromTop)) == NULL)
			{
				(OSD_CacheFree_p)(TRUE);
				if((ptr = FB_Malloc(Size, FromTop)) == NULL)
				{
					FB_Check();
				}
			}
		}
	}
	
//	FB_Check();
	return( ptr );
}

TH_Error_t PAL_OSD_FreeMemory(void *Ptr)
{
	TH_Error_t Error = TH_NO_ERROR;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	FB_Free(Ptr);
	return( Error );
}


/*******************************************************************************
函数名称	: PAL_OSD_GetMaxWin
函数功能	: 获取osd 的最大输出窗口

函数参数	: OUT:		OsdWin_p			osd 窗口
				 

函数返回	: 是否获取成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_GetMaxWin(PAL_OSD_Win_t *OsdWin_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	if(OsdWin_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	OsdWin_p->LeftX = 0;
	OsdWin_p->TopY= 0;
	OsdWin_p->Width= OSD_WIDTH;
	OsdWin_p->Height= OSD_HEIGHT;
	
	return( Error );
}

/*******************************************************************************
函数名称	: PAL_OSD_GetIOWin
函数功能	: 获取指定osd 的输入输出窗口

函数参数	: IN:			Handle			osd 控制句柄
				  OUT:		IOWin_p			输入输出窗口
				 

函数返回	: 是否获取成功

作者			|					修订					|	日期

陈慧明							创建						2010.05.10

*******************************************************************************/
TH_Error_t PAL_OSD_GetIOWin(PAL_OSD_Handle Handle, PAL_OSD_IO_Win_t *IOWin_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	OSD_Instance_t  *Inst_p;

	OSD_INIT_CHECK(OSD_Access_p, return (TH_ERROR_NO_INIT));
	
	if(IOWin_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	Inst_p = OSD_ReferenceInstance(Handle, TH_TIMEOUT_INFINITY);
	if(Inst_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	*IOWin_p = Inst_p->IOWin;

	OSD_ReleaseInstance(Handle);
	return( Error );
}

static int SDK_Osd2Init(void)
{
	int ret = 0;
	int fbFd;
	struct fb_var_screeninfo screeninfo;
	int available;
	int m_phys_mem;
	unsigned char *lfb;
	char *fb_path = "/dev/fb1";
	struct fb_fix_screeninfo fix;

	fbFd=open(fb_path, O_RDWR);
	if (fbFd<0)
	{
		printf("[fb] %s\n", fb_path);
		goto nolfb;
	}
	if (ioctl(fbFd, FBIOGET_VSCREENINFO, &screeninfo)<0)
	{
		printf("[fb] FBIOGET_VSCREENINFO\n");
		goto nolfb;
	}
	
	screeninfo.xres_virtual=screeninfo.xres=OSD_WIDTH;
	screeninfo.yres_virtual=screeninfo.yres=OSD_HEIGHT;
	screeninfo.height=0;
	screeninfo.width=0;
	screeninfo.xoffset=screeninfo.yoffset=0;
	screeninfo.bits_per_pixel=32;

	// ARGB 8888
	screeninfo.transp.offset = 24;
	screeninfo.transp.length = 8;
	screeninfo.red.offset = 16;
	screeninfo.red.length = 8;
	screeninfo.green.offset = 8;
	screeninfo.green.length = 8;
	screeninfo.blue.offset = 0;
	screeninfo.blue.length = 8;

	if (ioctl(fbFd, FBIOPUT_VSCREENINFO, &screeninfo)<0)
	{
		printf("[fb1] single buffering not available.\n");
	} else
		printf("[fb1] single buffering available!\n");

	ioctl(fbFd, FBIOGET_VSCREENINFO, &screeninfo);

	if (ioctl(fbFd, FBIOGET_FSCREENINFO, &fix)<0)
	{
		printf("[fb1] FBIOGET_FSCREENINFO\n");
		goto nolfb;
	}
	available = fix.smem_len;
	m_phys_mem = fix.smem_start;
	printf("[fb1] %s: %dk video mem\n", fb_path, available/1024);
	printf("[fb1] %dk video mem\n", available/1024);
	lfb=(unsigned char*)mmap(0, available, PROT_WRITE|PROT_READ, MAP_SHARED, fbFd, 0);
	if (!lfb)
	{
		printf("[fb1] mmap\n");
		goto nolfb;
	}
	
	memset(&OSD_SppBitmap, 0, sizeof(OSD_SppBitmap));
	OSD_SppBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
	OSD_SppBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_16TO9;
	OSD_SppBitmap.Width = OSD_WIDTH;
	OSD_SppBitmap.Height = OSD_HEIGHT;
	OSD_SppBitmap.Pitch = (OSD_SppBitmap.Width<<2);
	OSD_SppBitmap.Size = OSD_SppBitmap.Height * OSD_SppBitmap.Pitch;
	printf("[GUI]OSD_InitSppBitmap %d, %d, %d, %d\n", OSD_SppBitmap.Width, OSD_SppBitmap.Height, OSD_SppBitmap.Pitch, OSD_SppBitmap.Size);
	OSD_Private_t      *pf;

	pf = malloc(sizeof(OSD_Private_t));
	OSD_SppBitmap.PlatformBitmap_p = pf;
	OSD_SppBitmap.Private_p = pf;
	OSD_SppBitmap.Data_p = lfb;
	pf->phy = (void *)m_phys_mem;

	memset(OSD_SppBitmap.Data_p, 0x00, OSD_SppBitmap.Size);
	
	fb1.m_number_of_pages = screeninfo.yres_virtual / OSD_HEIGHT;
	printf("[fb1] %d page(s) available!\n", fb1.m_number_of_pages);

	fb1.phys_address = m_phys_mem;
	fb1.mmap_address = lfb;
	fb1.size = available;
	fb1.mem_mgr = (void *)fb1.mmap_address;
	fb1.fbfd = fbFd;

	printf("[%s] fb.phys_address = %08x,fb.size = %d\n", fb_path, fb1.phys_address,fb1.size);

	HIFB_ZORDER_E zorder = HIFB_ZORDER_MOVEBOTTOM;
	ioctl(fbFd, FBIOPUT_ZORDER, &zorder);
	return 0;
nolfb:
	if (fbFd >= 0)
	{
		close(fbFd);
		fbFd = -1;
	}
	printf("[fb1] framebuffer not available\n");

	return -1;
}

int SDK_OsdInit(void)
{
	int ret = 0;
	int fbFd;
	struct fb_var_screeninfo screeninfo;
	int available;
	int m_phys_mem;
	unsigned char *lfb;
	char *fb_path = "/dev/fb0";
	struct fb_fix_screeninfo fix;

	fbFd=open(fb_path, O_RDWR);
	if (fbFd<0)
	{
		printf("[fb] %s\n", fb_path);
		goto nolfb;
	}

	if (ioctl(fbFd, FBIOGET_VSCREENINFO, &screeninfo)<0)
	{
		printf("[fb] FBIOGET_VSCREENINFO\n");
		goto nolfb;
	}

	if (ioctl(fbFd, FBIOGET_FSCREENINFO, &fix)<0)
	{
		printf("[fb] FBIOGET_FSCREENINFO\n");
		goto nolfb;
	}
	available = fix.smem_len;
	m_phys_mem = fix.smem_start;
	printf("[fb] %s: %dk video mem\n", fb_path, available/1024);
	printf("[fb] %dk video mem\n", available/1024);
	lfb=(unsigned char*)mmap(0, available, PROT_WRITE|PROT_READ, MAP_SHARED, fbFd, 0);
	
	if (!lfb)
	{
		printf("[fb] mmap\n");
		goto nolfb;
	}
	
	screeninfo.xres_virtual=screeninfo.xres=OSD_WIDTH;
	screeninfo.yres_virtual=screeninfo.yres=OSD_HEIGHT;
	screeninfo.height=0;
	screeninfo.width=0;
	screeninfo.xoffset=screeninfo.yoffset=0;
	screeninfo.bits_per_pixel=32;

	// ARGB 8888
	screeninfo.transp.offset = 24;
	screeninfo.transp.length = 8;
	screeninfo.red.offset = 16;
	screeninfo.red.length = 8;
	screeninfo.green.offset = 8;
	screeninfo.green.length = 8;
	screeninfo.blue.offset = 0;
	screeninfo.blue.length = 8;

	if (ioctl(fbFd, FBIOPUT_VSCREENINFO, &screeninfo)<0)
	{
		printf("[fb] single buffering not available.\n");
	} else
		printf("[fb] single buffering available!\n");

	fb.m_number_of_pages = screeninfo.yres_virtual / OSD_HEIGHT;
	printf("[fb] %d page(s) available!\n", fb.m_number_of_pages);

	ioctl(fbFd, FBIOGET_VSCREENINFO, &screeninfo);
	memset(&OSD_ShowBitmap, 0, sizeof(OSD_ShowBitmap));

	OSD_ShowBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
	OSD_ShowBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_16TO9;
	OSD_ShowBitmap.Width = OSD_WIDTH;
	OSD_ShowBitmap.Height = OSD_HEIGHT;
	OSD_ShowBitmap.Pitch = (OSD_ShowBitmap.Width<<2);
	OSD_ShowBitmap.Size = OSD_ShowBitmap.Height * OSD_ShowBitmap.Pitch;
	printf("[GUI]OSD_InitDrawBitmap %d, %d, %d, %d\n", OSD_ShowBitmap.Width, OSD_ShowBitmap.Height, OSD_ShowBitmap.Pitch, OSD_ShowBitmap.Size);
	OSD_Private_t      *pf;

	pf = malloc(sizeof(OSD_Private_t));
	OSD_ShowBitmap.PlatformBitmap_p = pf;
	OSD_ShowBitmap.Private_p = pf;
	OSD_ShowBitmap.Data_p = lfb;
	pf->phy = (void *)m_phys_mem;

	memset(OSD_ShowBitmap.Data_p, 0, OSD_ShowBitmap.Size);

	fb.phys_address = m_phys_mem+OSD_ShowBitmap.Size;
	fb.mmap_address = lfb+OSD_ShowBitmap.Size;
	fb.size = available-OSD_ShowBitmap.Size;
	fb.mem_mgr = (void *)fb.mmap_address+OSD_ShowBitmap.Size;
	fb.fbfd = fbFd;
	
	MEM_Access_p = THOS_SemaphoreCreate(1);

	FB_Init();

	printf("fb.phys_address = %08x,fb.size = %d\n",fb.phys_address,fb.size);
/*
	unsigned long color;
	while(1)
	{
		color = 0xFFFF0000;
		fb_blit_fill(
				(int)pf->phy, OSD_WIDTH, OSD_HEIGHT, OSD_ShowBitmap.Pitch,
				0, 0, OSD_WIDTH, OSD_HEIGHT,
				color);
		sleep(5);
		color = 0xFF00FF00;
		fb_blit_fill(
				(int)pf->phy, OSD_WIDTH, OSD_HEIGHT, OSD_ShowBitmap.Pitch,
				0, 0, OSD_WIDTH, OSD_HEIGHT,
				color);
		sleep(5);
		color = 0xFF0000FF;
		fb_blit_fill(
				(int)pf->phy, OSD_WIDTH, OSD_HEIGHT, OSD_ShowBitmap.Pitch,
				0, 0, OSD_WIDTH, OSD_HEIGHT,
				color);
		sleep(5);
	}

	*/
	SDK_Osd2Init();
	return 0;
nolfb:
	if (fbFd >= 0)
	{
		close(fbFd);
		fbFd = -1;
	}
	printf("[fb] framebuffer not available\n");

	return -1;
}

static int SDK_Osd2Term(void)
{
	free(OSD_SppBitmap.Private_p);
	msync(fb1.mmap_address, fb1.size, MS_SYNC);
	if(munmap(fb1.mmap_address, fb1.size) < 0)
	{
		printf("unmap fb1 error!\n");
	}
	else
	{
		printf("unmap fb1 ok!\n");
	}
	close(fb1.fbfd);
}

void SDK_OsdTerm(void)
{
	SDK_Osd2Term();
	
	free(OSD_ShowBitmap.Private_p);
	msync(fb.mmap_address-OSD_ShowBitmap.Size, fb.size+OSD_ShowBitmap.Size, MS_SYNC);
	if(munmap(fb.mmap_address-OSD_ShowBitmap.Size, fb.size+OSD_ShowBitmap.Size) < 0)
	{
		printf("unmap fb error!\n");
	}
	else
	{
		printf("unmap fb ok!\n");
	}
	close(fb.fbfd);
	THOS_SemaphoreDelete(MEM_Access_p);
	MEM_Access_p = NULL;
}


/*end of osd.c*/


