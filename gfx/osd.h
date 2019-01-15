
/*****************************************************************************
文件名称	: osd.h
版权所有	: Tech Home 2010-2015
文件功能	: osd header

作者			|					修订					|	日期
陈慧明							创建						2010.05.11

*****************************************************************************/

#ifndef __OSD_H__
#define __OSD_H__

/*=======include header file======*/



/* C++ support */
#if defined __cplusplus
    extern "C" {
#endif

/*****************************const define*************************************/
#if 1
#ifndef U8
typedef 	unsigned char			U8;
#endif

#ifndef S8
typedef 	signed char			S8;
#endif

#ifndef U16
typedef 	unsigned short int		U16;
#endif

#ifndef S16
typedef 	signed short int		S16;
#endif

#ifndef U32
typedef 	unsigned int			U32;
#endif
 
#ifndef S32
typedef 	signed int				S32;
#endif

#ifndef S64
typedef signed long long				S64;
#endif

#ifndef uint64_t
typedef 	unsigned long long			uint64_t;
#endif

#ifndef int64_t
typedef signed long long			int64_t;
#endif

#ifndef uint32_t
typedef unsigned int			uint32_t;
#endif

#ifndef int32_t
typedef int			int32_t;
#endif

#ifndef uint8_t
typedef unsigned char			uint8_t;
#endif

#ifndef BOOL
typedef unsigned char			BOOL;
#endif

#if !defined __cplusplus
#ifndef bool
typedef unsigned char			bool;
#endif
#endif

#ifndef TRUE
#define TRUE            (1)
#endif

#ifndef FALSE
#define FALSE           (0)
#endif

#ifndef true
#define true            (1)
#endif

#ifndef false
#define false           (0)
#endif

#endif

typedef unsigned int			TH_Semaphore_t;

#define TH_INVALID_HANDLE		0xFFFFFFFF	/*无效句柄*/

#define TH_TIMEOUT_INFINITY	0xFFFFFFFF	/*无限等待*/
#define TH_TIMEOUT_IMMEDIATE	0			/*立即检测不等待*/

#define PAL_OSD_SppHandle		0xFFFFFFFF

typedef enum
{
	TH_NO_ERROR,  				/*没有错误*/
	TH_ERROR_NO_MEM,			/*没有内存空间*/
	TH_ERROR_BAD_PARAM,		/*参数错误*/		
	TH_ERROR_TIME_OUT,		/*超时返回*/
	TH_ERROR_INVALID_HANDLE,	/*无效句柄*/
	TH_ERROR_BIOS_ERROR,		/*平台相关驱动执行出错*/
	TH_ERROR_NO_INIT,			/*没有初始化*/
	TH_ERROR_NO_FREE,			/*没有空闲设备*/
	TH_ERROR_TABLE_FULL,		/*表满*/
	TH_ERROR_LIST_ERROR,		/*列表出错*/
	TH_ERROR_NOT_SUPPORT,	/*功能不支持*/
	TH_ERROR_BUSY,				/*设备忙*/
	TH_ERROR_ALREADY_EXIST,	/*已经存在*/
	TH_ERROR_NO_EXIST,		/*不存在*/
	TH_ERROR_MAX = 0xFFFFFFFF
}TH_Error_t;

/*****************************struct define*************************************/
typedef U32			PAL_OSD_Handle;	/*OSD 控制句柄*/

typedef enum
{
	PAL_OSD_ORDER_BACK,				/*最底部*/
	PAL_OSD_ORDER_FRONT,			/*最顶部*/
	PAL_OSD_ORDER_REF_BACK,			/*参考底部*/
	PAL_OSD_ORDER_REF_FRONT		/*参考顶部*/
}PAL_OSD_Order_t;	/*层次序*/

typedef struct
{
	S32		LeftX;	/*左角X 轴*/
	S32		TopY;	/*顶部Y 轴*/
	S32		Width;	/*宽度*/
	S32		Height;	/*高度*/
}PAL_OSD_Win_t;	/*层窗口*/

typedef struct
{
	PAL_OSD_Win_t InputWin;		/*层输入窗口*/
	PAL_OSD_Win_t OutputWin;	/*层输出窗口*/
}PAL_OSD_IO_Win_t;	/*层输入输出窗口*/

typedef enum
{
	PAL_OSD_RESOLUTION_INVALID,
	PAL_OSD_RESOLUTION_1080I_60HZ,
	PAL_OSD_RESOLUTION_1080I_50HZ,
	
	PAL_OSD_RESOLUTION_720P_60HZ,
	PAL_OSD_RESOLUTION_720P_50HZ,
	
	PAL_OSD_RESOLUTION_576P_50HZ,
	PAL_OSD_RESOLUTION_576I_50HZ,
	
	PAL_OSD_RESOLUTION_480P_60HZ,
	PAL_OSD_RESOLUTION_480P_50HZ,
	PAL_OSD_RESOLUTION_480I_60HZ,

	PAL_OSD_RESOLUTION_1080P_60HZ,
	PAL_OSD_RESOLUTION_1080P_50HZ
}PAL_OSD_Resolution_t;/*分辩率*/

typedef enum
{
	PAL_OSD_ASPECT_RATIO_INVALID,
	PAL_OSD_ASPECT_RATIO_16TO9,	/*16 : 9*/
	PAL_OSD_ASPECT_RATIO_4TO3,	/*4 : 3*/
	PAL_OSD_ASPECT_RATIO_AUTO /*高清设置为16:9 标清节目为4:3*/
}PAL_OSD_AspectRatio_t;	/*显示比例*/

typedef enum
{
	PAL_OSD_AR_MODE_INVALID,
	PAL_OSD_AR_MODE_PAN_SCAN,
	PAL_OSD_AR_MODE_LETTER_BOX,
	PAL_OSD_AR_MODE_COMBINED,
	PAL_OSD_AR_MODE_IGNORE
}PAL_OSD_AspectRatioMode_t;	/*比例模式*/

typedef enum
{
	PAL_OSD_COLOR_TYPE_ARGB8888,
	PAL_OSD_COLOR_TYPE_RGB888,
	PAL_OSD_COLOR_TYPE_ARGB8565,
	PAL_OSD_COLOR_TYPE_RGB565,
	PAL_OSD_COLOR_TYPE_ARGB1555,
	PAL_OSD_COLOR_TYPE_ARGB4444,

	PAL_OSD_COLOR_TYPE_CLUT8,
	PAL_OSD_COLOR_TYPE_CLUT4,
	PAL_OSD_COLOR_TYPE_CLUT2,
	PAL_OSD_COLOR_TYPE_CLUT1,
	PAL_OSD_COLOR_TYPE_ACLUT88,
	PAL_OSD_COLOR_TYPE_ACLUT44,

	PAL_OSD_COLOR_TYPE_SIGNED_YCBCR888_444,
	PAL_OSD_COLOR_TYPE_UNSIGNED_YCBCR888_444,
	PAL_OSD_COLOR_TYPE_SIGNED_YCBCR888_422,
	PAL_OSD_COLOR_TYPE_UNSIGNED_YCBCR888_422,
	PAL_OSD_COLOR_TYPE_SIGNED_YCBCR888_420,
	PAL_OSD_COLOR_TYPE_UNSIGNED_YCBCR888_420,
	PAL_OSD_COLOR_TYPE_UNSIGNED_AYCBCR6888_444,
	PAL_OSD_COLOR_TYPE_SIGNED_AYCBCR8888,
	PAL_OSD_COLOR_TYPE_UNSIGNED_AYCBCR8888,

	PAL_OSD_COLOR_TYPE_ALPHA1,
	PAL_OSD_COLOR_TYPE_ALPHA4,
	PAL_OSD_COLOR_TYPE_ALPHA8,
	PAL_OSD_COLOR_TYPE_BYTE,

	PAL_OSD_COLOR_TYPE_ARGB8888_255,
	PAL_OSD_COLOR_TYPE_ARGB8565_255,
	PAL_OSD_COLOR_TYPE_ACLUT88_255,
	PAL_OSD_COLOR_TYPE_ALPHA8_255,
	PAL_OSD_COLOR_TYPE_NUM
}PAL_OSD_ColorType_t;	/*颜色类型*/

typedef struct
{
	U8 R;
	U8 G;
	U8 B;
}PAL_OSD_ColorRGB_t;	/*三基色*/

typedef struct
{
	U8 Y;
	S8 Cb;
	S8 Cr;
}PAL_OSD_ColorSignedYCbCr_t;	/*亮度，色差*/

typedef struct
{
	U8 Y;
	U8 Cb;
	U8 Cr;
}PAL_OSD_ColorUnsignedYCbCr_t;	/*亮度，色差*/

typedef struct
{
	U8 Alpha;
	U8 Y;
	S8 Cb;
	S8 Cr;
}PAL_OSD_ColorSignedAYCbCr_t;	/*透明度，亮度，色差*/

typedef struct
{
	U8 Alpha;
	U8 Y;
	U8 Cb;
	U8 Cr;
}PAL_OSD_ColorUnsignedAYCbCr_t;	/*透明度，亮度，色差*/

typedef struct
{
	U8 Alpha;
	U8 R;
	U8 G;
	U8 B;
}PAL_OSD_ColorARGB_t;	/*透明度，三基色*/

typedef struct
{
	U8 Alpha;
	U8 PaletteEntry;
}PAL_OSD_ColorACLUT_t;	/*透明度，色板索引*/

typedef union
{
	PAL_OSD_ColorARGB_t           ARGB8888;
	PAL_OSD_ColorRGB_t            RGB888;
	PAL_OSD_ColorARGB_t           ARGB8565;
	PAL_OSD_ColorRGB_t            RGB565;
	PAL_OSD_ColorARGB_t           ARGB1555;
	PAL_OSD_ColorARGB_t           ARGB4444;

	U8                            CLUT8;
	U8                            CLUT4;
	U8                            CLUT2;
	U8                            CLUT1;
	PAL_OSD_ColorACLUT_t          ACLUT88 ;
	PAL_OSD_ColorACLUT_t          ACLUT44 ;

	PAL_OSD_ColorSignedYCbCr_t    SignedYCbCr888_444;
	PAL_OSD_ColorUnsignedYCbCr_t  UnsignedYCbCr888_444;
	PAL_OSD_ColorSignedYCbCr_t    SignedYCbCr888_422;
	PAL_OSD_ColorUnsignedYCbCr_t  UnsignedYCbCr888_422;
	PAL_OSD_ColorSignedYCbCr_t    SignedYCbCr888_420;
	PAL_OSD_ColorUnsignedYCbCr_t  UnsignedYCbCr888_420;
	PAL_OSD_ColorUnsignedAYCbCr_t UnsignedAYCbCr6888_444;
	PAL_OSD_ColorSignedAYCbCr_t   SignedAYCbCr8888;
	PAL_OSD_ColorUnsignedAYCbCr_t UnsignedAYCbCr8888;

	U8                            ALPHA1;
	U8                            ALPHA4;
	U8                            ALPHA8;
	U8                            Byte;
}PAL_OSD_ColorValue_t;	/*颜色值*/

typedef struct
{
	PAL_OSD_ColorType_t            Type;	/*颜色类型*/
	PAL_OSD_ColorValue_t           Value;	/*颜色值*/
}PAL_OSD_Color_t;	/*颜色数据*/

typedef struct
{
	PAL_OSD_ColorType_t		ColorType;	/*颜色类型*/
	PAL_OSD_AspectRatio_t		AspectRatio;	/*显示比例*/
	U32						Width;	/*宽度*/
	U32						Height;	/*高度*/
	U32						Pitch;	/*深度，一行几个字节*/
	void*					Data_p;	/*数据指针*/
	U32						Size;	/*数据大小*/
	void*					Private_p;/*针对平台不同而保留的私有数据*/
	void*					PlatformBitmap_p;/*针对平台不同而保留的位图结构指针*/
	void*					PlatformPalette_p;/*针对平台不同而保留的色板结构指针*/
}PAL_OSD_Bitmap_t;	/*位图数据结构*/

typedef struct
{
	U32		LeftX;	/*左边X 轴*/
	U32		TopY;	/*顶部Y 轴*/
}PAL_OSD_Pos_t;	/*作标结构*/

typedef struct
{
	PAL_OSD_ColorType_t		ColorType;	/*颜色类型*/
	PAL_OSD_Color_t			*Palette_p;	/*色板数据*/
	U32						ColorNum;	/*色板颜色个数*/
	U32						Width;
	U32						Height;
	PAL_OSD_IO_Win_t		IOWin;
}PAL_OSD_OpenParam_t;	/*开OSD 的参数*/


typedef struct
{
	void*					Data_p;	/*数据指针*/
	U32						Size;	/*数据大小*/
	void*					Private_p;/*针对平台不同而保留的私有数据*/
}PAL_OSD_Vb_t;	/*视频内存空间*/

/*****************************gloal data define**********************************/

/*****************************function define*******************************/

TH_Error_t PAL_OSD_Init(void);
TH_Error_t PAL_OSD_Term(void);
TH_Error_t PAL_OSD_Open(PAL_OSD_OpenParam_t *OpenParam_p, PAL_OSD_Handle *Handle_p);
TH_Error_t PAL_OSD_Close(PAL_OSD_Handle Handle);
TH_Error_t PAL_OSD_OpenNoShow(PAL_OSD_OpenParam_t *OpenParam_p, PAL_OSD_Handle *Handle_p);
TH_Error_t PAL_OSD_CloseNoShow(PAL_OSD_Handle Handle);

TH_Error_t PAL_OSD_Show(PAL_OSD_Handle Handle);
TH_Error_t PAL_OSD_Hide(PAL_OSD_Handle Handle);
TH_Error_t PAL_OSD_SetTransparency(PAL_OSD_Handle Handle, U8 TransPercent);
TH_Error_t PAL_OSD_SetOrder(PAL_OSD_Handle Handle, PAL_OSD_Order_t Order, PAL_OSD_Handle ReferHandle);
TH_Error_t PAL_OSD_SetIOWin(PAL_OSD_Handle  Handle, PAL_OSD_IO_Win_t *IOWin_p);
TH_Error_t PAL_OSD_GetSrcBitmap(PAL_OSD_Handle  Handle, PAL_OSD_Bitmap_t *SrcBitmap_p);
TH_Error_t PAL_OSD_PutSrcBitmap(PAL_OSD_Handle  Handle);
TH_Error_t PAL_OSD_Copy(PAL_OSD_Bitmap_t *DestBitmap_p, 
								PAL_OSD_Win_t *DestWin_p, 
								PAL_OSD_Bitmap_t *SrcBitmap_p,
								PAL_OSD_Win_t *SrcWin_p);
TH_Error_t PAL_OSD_Put(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *DestWin_p, PAL_OSD_Win_t *SrcWin_p, PAL_OSD_Bitmap_t *Bitmap_p);
BOOL PAL_OSD_CheckAlpha(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *DestWin_p);
TH_Error_t PAL_OSD_PutAlpha(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *DestWin_p, PAL_OSD_Win_t *SrcWin_p, PAL_OSD_Bitmap_t *Bitmap_p);
TH_Error_t PAL_OSD_Fill(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *Win_p, PAL_OSD_Color_t *Color_p);
TH_Error_t PAL_OSD_FillEx(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *Win_p, PAL_OSD_Color_t *Color_p, BOOL Copy);
TH_Error_t PAL_OSD_SetPalette(PAL_OSD_Handle  Handle, 
										PAL_OSD_Color_t *Palette_p, 
										U32 StartColorIndex, 
										U32 SetColorNum);
TH_Error_t PAL_OSD_GetPalette(PAL_OSD_Handle  Handle, 
										PAL_OSD_Color_t *Palette_p, 
										U32 StartColorIndex, 
										U32 GetColorNum);
void *PAL_OSD_GetPlatformPalette(PAL_OSD_Handle  Handle);
TH_Error_t PAL_OSD_SetDirtyWin(PAL_OSD_Handle  Handle, PAL_OSD_Win_t *DirtyWin_p);
void PAL_OSD_UpdateDisplay(void);
TH_Error_t PAL_OSD_AllocBitmap(PAL_OSD_Bitmap_t *Bitmap_p, void *PlatformPalette_p, BOOL Cache);
TH_Error_t PAL_OSD_AllocBitmapFromVideo(PAL_OSD_Bitmap_t *Bitmap_p);

TH_Error_t PAL_OSD_SetCacheFree(void (*CacheFree_p)(BOOL Force));
TH_Error_t PAL_OSD_FreeBitmap(PAL_OSD_Bitmap_t *Bitmap_p);
TH_Error_t PAL_OSD_GetMaxWin(PAL_OSD_Win_t *OsdWin_p);
TH_Error_t PAL_OSD_GetIOWin(PAL_OSD_Handle Handle, PAL_OSD_IO_Win_t *IOWin_p);

PAL_OSD_Bitmap_t *PAL_OSD_HwJpegDecode(PAL_OSD_Bitmap_t *Bitmap_p, U8 *PicData_p, U32 PicDataLen);
PAL_OSD_Bitmap_t *PAL_OSD_HwJpegDecodeEx(PAL_OSD_Bitmap_t *Bitmap_p, U8 *PicData_p, U32 PicDataLen, U32 Width, U32 Heigth);

TH_Error_t  PAL_OSD_PlayPicture(PAL_OSD_Bitmap_t *Bitmap_p, PAL_OSD_Win_t *Win_p);
TH_Error_t  PAL_OSD_StopPicture(void);

TH_Error_t  PAL_OSD_PlayPicturePrepare(PAL_OSD_Win_t *Win_p);
TH_Error_t PAL_OSD_PlayPicturePart(PAL_OSD_Win_t *DestWin_p, 
										PAL_OSD_Bitmap_t *SrcBitmap_p,
										PAL_OSD_Win_t *SrcWin_p);
TH_Error_t  PAL_OSD_SetPictureWin(PAL_OSD_Win_t *Win_p);

TH_Error_t PAL_VB_AllocMem(PAL_OSD_Vb_t *Vb);
TH_Error_t  PAL_VB_FreeMem(PAL_OSD_Vb_t *Vb);

/* C++ support */
#if defined __cplusplus
}
#endif

#endif


