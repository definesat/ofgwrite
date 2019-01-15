
/*****************************************************************************
�ļ�����	: osd.h
��Ȩ����	: Tech Home 2010-2015
�ļ�����	: osd header

����			|					�޶�					|	����
�»���							����						2010.05.11

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

#define TH_INVALID_HANDLE		0xFFFFFFFF	/*��Ч���*/

#define TH_TIMEOUT_INFINITY	0xFFFFFFFF	/*���޵ȴ�*/
#define TH_TIMEOUT_IMMEDIATE	0			/*������ⲻ�ȴ�*/

#define PAL_OSD_SppHandle		0xFFFFFFFF

typedef enum
{
	TH_NO_ERROR,  				/*û�д���*/
	TH_ERROR_NO_MEM,			/*û���ڴ�ռ�*/
	TH_ERROR_BAD_PARAM,		/*��������*/		
	TH_ERROR_TIME_OUT,		/*��ʱ����*/
	TH_ERROR_INVALID_HANDLE,	/*��Ч���*/
	TH_ERROR_BIOS_ERROR,		/*ƽ̨�������ִ�г���*/
	TH_ERROR_NO_INIT,			/*û�г�ʼ��*/
	TH_ERROR_NO_FREE,			/*û�п����豸*/
	TH_ERROR_TABLE_FULL,		/*����*/
	TH_ERROR_LIST_ERROR,		/*�б����*/
	TH_ERROR_NOT_SUPPORT,	/*���ܲ�֧��*/
	TH_ERROR_BUSY,				/*�豸æ*/
	TH_ERROR_ALREADY_EXIST,	/*�Ѿ�����*/
	TH_ERROR_NO_EXIST,		/*������*/
	TH_ERROR_MAX = 0xFFFFFFFF
}TH_Error_t;

/*****************************struct define*************************************/
typedef U32			PAL_OSD_Handle;	/*OSD ���ƾ��*/

typedef enum
{
	PAL_OSD_ORDER_BACK,				/*��ײ�*/
	PAL_OSD_ORDER_FRONT,			/*���*/
	PAL_OSD_ORDER_REF_BACK,			/*�ο��ײ�*/
	PAL_OSD_ORDER_REF_FRONT		/*�ο�����*/
}PAL_OSD_Order_t;	/*�����*/

typedef struct
{
	S32		LeftX;	/*���X ��*/
	S32		TopY;	/*����Y ��*/
	S32		Width;	/*���*/
	S32		Height;	/*�߶�*/
}PAL_OSD_Win_t;	/*�㴰��*/

typedef struct
{
	PAL_OSD_Win_t InputWin;		/*�����봰��*/
	PAL_OSD_Win_t OutputWin;	/*���������*/
}PAL_OSD_IO_Win_t;	/*�������������*/

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
}PAL_OSD_Resolution_t;/*�ֱ���*/

typedef enum
{
	PAL_OSD_ASPECT_RATIO_INVALID,
	PAL_OSD_ASPECT_RATIO_16TO9,	/*16 : 9*/
	PAL_OSD_ASPECT_RATIO_4TO3,	/*4 : 3*/
	PAL_OSD_ASPECT_RATIO_AUTO /*��������Ϊ16:9 �����ĿΪ4:3*/
}PAL_OSD_AspectRatio_t;	/*��ʾ����*/

typedef enum
{
	PAL_OSD_AR_MODE_INVALID,
	PAL_OSD_AR_MODE_PAN_SCAN,
	PAL_OSD_AR_MODE_LETTER_BOX,
	PAL_OSD_AR_MODE_COMBINED,
	PAL_OSD_AR_MODE_IGNORE
}PAL_OSD_AspectRatioMode_t;	/*����ģʽ*/

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
}PAL_OSD_ColorType_t;	/*��ɫ����*/

typedef struct
{
	U8 R;
	U8 G;
	U8 B;
}PAL_OSD_ColorRGB_t;	/*����ɫ*/

typedef struct
{
	U8 Y;
	S8 Cb;
	S8 Cr;
}PAL_OSD_ColorSignedYCbCr_t;	/*���ȣ�ɫ��*/

typedef struct
{
	U8 Y;
	U8 Cb;
	U8 Cr;
}PAL_OSD_ColorUnsignedYCbCr_t;	/*���ȣ�ɫ��*/

typedef struct
{
	U8 Alpha;
	U8 Y;
	S8 Cb;
	S8 Cr;
}PAL_OSD_ColorSignedAYCbCr_t;	/*͸���ȣ����ȣ�ɫ��*/

typedef struct
{
	U8 Alpha;
	U8 Y;
	U8 Cb;
	U8 Cr;
}PAL_OSD_ColorUnsignedAYCbCr_t;	/*͸���ȣ����ȣ�ɫ��*/

typedef struct
{
	U8 Alpha;
	U8 R;
	U8 G;
	U8 B;
}PAL_OSD_ColorARGB_t;	/*͸���ȣ�����ɫ*/

typedef struct
{
	U8 Alpha;
	U8 PaletteEntry;
}PAL_OSD_ColorACLUT_t;	/*͸���ȣ�ɫ������*/

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
}PAL_OSD_ColorValue_t;	/*��ɫֵ*/

typedef struct
{
	PAL_OSD_ColorType_t            Type;	/*��ɫ����*/
	PAL_OSD_ColorValue_t           Value;	/*��ɫֵ*/
}PAL_OSD_Color_t;	/*��ɫ����*/

typedef struct
{
	PAL_OSD_ColorType_t		ColorType;	/*��ɫ����*/
	PAL_OSD_AspectRatio_t		AspectRatio;	/*��ʾ����*/
	U32						Width;	/*���*/
	U32						Height;	/*�߶�*/
	U32						Pitch;	/*��ȣ�һ�м����ֽ�*/
	void*					Data_p;	/*����ָ��*/
	U32						Size;	/*���ݴ�С*/
	void*					Private_p;/*���ƽ̨��ͬ��������˽������*/
	void*					PlatformBitmap_p;/*���ƽ̨��ͬ��������λͼ�ṹָ��*/
	void*					PlatformPalette_p;/*���ƽ̨��ͬ��������ɫ��ṹָ��*/
}PAL_OSD_Bitmap_t;	/*λͼ���ݽṹ*/

typedef struct
{
	U32		LeftX;	/*���X ��*/
	U32		TopY;	/*����Y ��*/
}PAL_OSD_Pos_t;	/*����ṹ*/

typedef struct
{
	PAL_OSD_ColorType_t		ColorType;	/*��ɫ����*/
	PAL_OSD_Color_t			*Palette_p;	/*ɫ������*/
	U32						ColorNum;	/*ɫ����ɫ����*/
	U32						Width;
	U32						Height;
	PAL_OSD_IO_Win_t		IOWin;
}PAL_OSD_OpenParam_t;	/*��OSD �Ĳ���*/


typedef struct
{
	void*					Data_p;	/*����ָ��*/
	U32						Size;	/*���ݴ�С*/
	void*					Private_p;/*���ƽ̨��ͬ��������˽������*/
}PAL_OSD_Vb_t;	/*��Ƶ�ڴ�ռ�*/

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


