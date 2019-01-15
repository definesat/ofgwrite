
/*****************************************************************************
文件名称	: gfx.c
版权所有	: Tech Home 2010-2015
文件功能	: 

作者			|					修订					|	日期
陈慧明							创建						2010.06.02

*****************************************************************************/
#define GFX_ENABLE_PNG				0
#define GFX_ENABLE_JPEG			0
#define GFX_ENABLE_GIF				0
#define GFX_ENABLE_LZO_BMP			0

/*=======include standard header file======*/
#include <stdio.h>
#include <string.h>


#if (GFX_ENABLE_PNG)
#include "png.h"
#endif
#if (GFX_ENABLE_JPEG)
#include "jpeglib.h"
#endif
#if (GFX_ENABLE_GIF)
#include "gif_lib.h"
#endif

#if (GFX_ENABLE_LZO_BMP)
#include "minilzo.h"
#endif

/*=======include pal header file======*/
#include "osd.h"


/*=======include local header file======*/
#include "i_gfx.h"

/*****************************const define*************************************/
#define GFX_MALLOC(size)				malloc(size)
#define GFX_FREE(ptr)				free(ptr)

#define GFX_ITALIC_PITCH			2/*固定为2 不能变*/
/*描边的宽度为2 是配合程序算法，如果要改变宽度，算法也要跟着改变*/
#define GFX_OUTLINE_WIDTH			2

#define GFX_GET_PIXEL_32(PTR)			(((*(PTR+3))<<24) |((*(PTR+2))<<16) |((*(PTR+1))<<8) |(*(PTR)))
#define GFX_GET_PIXEL_24(PTR)			(((*(PTR+2))<<16) |((*(PTR+1))<<8) |(*(PTR)))

#define GFX_MEMCPY_U32(dest, src, nb)		do{\
												S32 loop;\
												U32 *dest_p, *src_p;\
												dest_p = (U32 *)dest;\
												src_p = (U32 *)src;\
												for(loop=0; loop<nb; loop++)\
												{\
													*dest_p++ = *src_p++;\
												}\
											}while(0)

/*使能以下开关能提升带特效的写字速度，因为这种分配出来的空间是cache,
特效是需要大量的运算，因些cache 空间可能提升速度，对于没有特效的写
没有影响*/
#define GFX_USE_MALLOC		1

/*****************************struct define*************************************/

/*
Unicode 
UTF-8 

0000 - 007F 
0xxxxxxx 

0080 - 07FF 
110xxxxx 10xxxxxx 

0800 - FFFF 
1110xxxx 10xxxxxx 10xxxxxx 
*/
#if (GFX_ENABLE_GIF)
const short InterlacedOffset[] = { 0, 4, 2, 1 }; 
const short InterlacedJumps[] = { 8, 8, 4, 2 }; 
#define RGB8(r,g,b)	( (((b)>>3)<<10) | (((g)>>3)<<5) | ((r)>>3) )
#define GAMMA(x)	(x)

#endif

typedef struct
{
	U8 *Str_p;/*字符指针*/
	U32 Len;/*字节个数*/
	S32 CurrPos;/*当前字节索引*/
	U32 Count;/*字符的个数, 不是字节的个数*/
}GFX_Utf8_t;

/*****************************gloal data define**********************************/
static BOOL	GFX_EnableHwJpg = FALSE;

/*****************************local function define*******************************/
#define RLE_PART
#ifdef RLE_PART

#define MAX_COUNT_FOR_RLE							127
#define MAX_COUNT_FOR_RLE_2						32767

int TH_Rle_Init(void)
{
	return 0;
}

/*return : -1	表示长度不够; 0	表示连续相同; 1	表示不连续相同*/
int Rle_CheckContinue(const U8 *Data_p, U32 Len, U32 BytePerPixel)
{
	int i, threshold;
	const U8 *Temp_p;
	
	if(BytePerPixel > 1)
	{
		threshold = 2;
	}
	else
	{
		threshold = 3;
	}
	if(Len < (threshold*BytePerPixel))
	{
		return -1;
	}
	Temp_p = Data_p;

	for(i=0; i<threshold-1; i++)
	{
		if(memcmp(Temp_p, Temp_p+BytePerPixel, BytePerPixel) != 0)
		{
			return 1;
		}
		Temp_p += BytePerPixel;
	}
	return 0;
	
}

/*return : 0	表示连续两点相同; 1	 表示连续两点不同*/
int Rle_CheckSame(const U8 *Data_p, U32 Len, U32 BytePerPixel)
{
	if(Len < (2*BytePerPixel))
	{
		return -1;
	}
	if(memcmp(Data_p, Data_p+BytePerPixel, BytePerPixel) != 0)
	{
		return -1;
	}
	return 0;
}

/*获取连续相同的点的个数*/
int Rle_GetContinueCount(const U8 *Data_p, U32 Len, U32 BytePerPixel, int MaxCount)
{
	int count;
	
	count = 1;
	while(Rle_CheckSame(Data_p, Len, BytePerPixel) == 0)
	{
		count ++;
		Data_p += BytePerPixel;
		Len -= BytePerPixel;
		if(count >= MaxCount)
		{
			break;
		}
	}
	return count;
}

/*获取不连续相同的点的个数*/
int Rle_GetUncontinueCount(const U8 *Data_p, U32 Len, U32 BytePerPixel, int MaxCount)
{
	int count, ret;
	
	count = 0;
	while(1)
	{
		ret = Rle_CheckContinue(Data_p, Len, BytePerPixel);
		if(ret == 1)
		{
			count ++;
			Data_p += BytePerPixel;
			Len -= BytePerPixel;
			if(count >= MaxCount)
			{
				count = MaxCount;
				break;
			}
		}
		else if(ret == -1)
		{
			count += Len;
			if(count >= MaxCount)
			{
				count = MaxCount;
			}
			break;
		}
		else
		{
			break;
		}
	}
	return count;
}

int Rle_GetCompressSize(const U8 *Src_p, U32 SrcLen, U32 BytePerPixel, int MaxCount)
{
	const U8 *TempSrc_p;
	U32 TempDestLen, ContinueCount;
	int RestLen;
	
	TempSrc_p = Src_p;
	RestLen = SrcLen;

	TempDestLen = 0;
	
	ContinueCount = 0;
	
	if(BytePerPixel == 3 || BytePerPixel == 1)
	{
		while(RestLen >= (int)BytePerPixel)
		{
			if(Rle_CheckContinue(TempSrc_p, RestLen, BytePerPixel) == 0)
			{
				ContinueCount = Rle_GetContinueCount(TempSrc_p, RestLen, BytePerPixel, MaxCount);
				if(MaxCount > MAX_COUNT_FOR_RLE)
				{
					TempDestLen += (BytePerPixel + 2);
				}
				else
				{
					TempDestLen += (BytePerPixel + 1);
				}
			}
			else
			{
				ContinueCount = Rle_GetUncontinueCount(TempSrc_p, RestLen, BytePerPixel, MaxCount);
				if(MaxCount > MAX_COUNT_FOR_RLE)
				{
					TempDestLen += (ContinueCount*BytePerPixel + 2);
				}
				else
				{
					TempDestLen += (ContinueCount*BytePerPixel + 1);
				}
			}
			TempSrc_p += (ContinueCount*BytePerPixel);
			RestLen -= (ContinueCount*BytePerPixel);
		}
	}
	else
	{
		printf("cant support %d byte per pixel\n", BytePerPixel);
		return -1;
	}
	
	if(RestLen != 0)
	{
		return -1;
	}

	return TempDestLen;
}

int TH_Rle_Compress(U8 *Dest_p, U32 *DestLen_p, const U8 *Src_p, U32 SrcLen, U32 BytePerPixel, int MaxCount)
{
	U8 *TempDest_p;
	const U8 *TempSrc_p;
	U32 TempDestLen, ContinueCount;
	int RestLen;
	
	TempSrc_p = Src_p;
	RestLen = SrcLen;

	TempDest_p = Dest_p;
	TempDestLen = 0;
	
	ContinueCount = 0;
	
	if(BytePerPixel == 3 || BytePerPixel == 1)
	{
		while(RestLen >= (int)BytePerPixel)
		{
			if(Rle_CheckContinue(TempSrc_p, RestLen, BytePerPixel) == 0)
			{
				ContinueCount = Rle_GetContinueCount(TempSrc_p, RestLen, BytePerPixel, MaxCount);
				if(MaxCount > MAX_COUNT_FOR_RLE)
				{
					TempDestLen += (BytePerPixel + 2);
					if(TempDestLen > *DestLen_p)
					{
						return -1;
					}
					TempDest_p[0] = ContinueCount >> 8;
					TempDest_p[1] = (U8)ContinueCount;
					memcpy(TempDest_p+2, TempSrc_p, BytePerPixel);
					TempDest_p += (BytePerPixel + 2);
				}
				else
				{
					TempDestLen += (BytePerPixel + 1);
					if(TempDestLen > *DestLen_p)
					{
						return -1;
					}
					TempDest_p[0] = ContinueCount;
					memcpy(TempDest_p+1, TempSrc_p, BytePerPixel);
					TempDest_p += (BytePerPixel + 1);
				}
			}
			else
			{
				ContinueCount = Rle_GetUncontinueCount(TempSrc_p, RestLen, BytePerPixel, MaxCount);
				if(MaxCount > MAX_COUNT_FOR_RLE)
				{
					TempDestLen += (ContinueCount*BytePerPixel + 2);
					if(TempDestLen > *DestLen_p)
					{
						return -1;
					}
					TempDest_p[0] = (ContinueCount>>8)|0x80;
					TempDest_p[1] = (U8)ContinueCount;
					memcpy(TempDest_p+2, TempSrc_p, ContinueCount*BytePerPixel);
					TempDest_p += (ContinueCount*BytePerPixel + 2);
				}
				else
				{
					TempDestLen += (ContinueCount*BytePerPixel + 1);
					if(TempDestLen > *DestLen_p)
					{
						return -1;
					}
					TempDest_p[0] = ContinueCount|0x80;
					memcpy(TempDest_p+1, TempSrc_p, ContinueCount*BytePerPixel);
					TempDest_p += (ContinueCount*BytePerPixel + 1);
				}
			}
			TempSrc_p += (ContinueCount*BytePerPixel);
			RestLen -= (ContinueCount*BytePerPixel);
		}
	}
	else
	{
		printf("cant support %d byte per pixel\n", BytePerPixel);
		return -1;
	}
	
	if(RestLen != 0)
	{
		return -1;
	}
	printf("compressed %lu bytes into %lu bytes\n",
		(unsigned long) SrcLen, (unsigned long) TempDestLen);

	*DestLen_p = TempDestLen;
	return 0;
}

int TH_Rle_Decompress(U8 *Dest_p, U32 *DestLen_p, const U8 *Src_p, U32 SrcLen, U32 BytePerPixel, int MaxCount)
{
	int RestLen, TempDestLen, ContinueCount, i;
	const U8 *TempSrc_p;
	U8 *TempDest_p;

	RestLen = SrcLen;
	TempDestLen = 0;
	TempSrc_p = Src_p;
	TempDest_p = Dest_p;
	if(MaxCount > MAX_COUNT_FOR_RLE)
	{
		while(RestLen >= (int)(BytePerPixel + 2))
		{
			ContinueCount = TempSrc_p[0] & 0x7F;
			ContinueCount = (ContinueCount << 8) | TempSrc_p[1];
			TempDestLen += (ContinueCount*BytePerPixel);
			if(TempDestLen > (int)(*DestLen_p))
			{
				return -1;
			}
			if(TempSrc_p[0] & 0x80)
			{
				TempSrc_p += 2;
				RestLen -= 2;
				memcpy(TempDest_p, TempSrc_p, ContinueCount*BytePerPixel);
				TempDest_p += (ContinueCount*BytePerPixel);
				TempSrc_p += ContinueCount*BytePerPixel;
				RestLen -= ContinueCount*BytePerPixel;
			}
			else
			{
				TempSrc_p += 2;
				RestLen -= 2;
				for(i=0; i<ContinueCount; i++)
				{
					memcpy(TempDest_p, TempSrc_p, BytePerPixel);
					TempDest_p += BytePerPixel;
				}
				TempSrc_p += BytePerPixel;
				RestLen -= BytePerPixel;
			}
		}
	}
	else
	{
		while(RestLen >= (int)(BytePerPixel + 1))
		{
			ContinueCount = TempSrc_p[0] & 0x7F;
			TempDestLen += (ContinueCount*BytePerPixel);
			if(TempDestLen > (int)(*DestLen_p))
			{
				return -1;
			}
			if(TempSrc_p[0] & 0x80)
			{
				TempSrc_p ++;
				RestLen --;
				memcpy(TempDest_p, TempSrc_p, ContinueCount*BytePerPixel);
				TempDest_p += (ContinueCount*BytePerPixel);
				TempSrc_p += ContinueCount*BytePerPixel;
				RestLen -= ContinueCount*BytePerPixel;
			}
			else
			{
				TempSrc_p ++;
				RestLen --;
				for(i=0; i<ContinueCount; i++)
				{
					memcpy(TempDest_p, TempSrc_p, BytePerPixel);
					TempDest_p += BytePerPixel;
				}
				TempSrc_p += BytePerPixel;
				RestLen -= BytePerPixel;
			}
		}
	}
	if(RestLen != 0)
	{
		return -1;
	}
	printf("compressed %lu bytes into %lu bytes\n",
		(unsigned long) SrcLen, (unsigned long) TempDestLen);
	*DestLen_p = TempDestLen;
	return 0;
}

#endif

/********************************************************************
画一个没有压缩的ARGB 格式的位图带透明色
*********************************************************************/
TH_Error_t  GFX_DrawBmp_NONE_ARGB_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t OsdBitmap;
	U32 *SrcPtr_U32p, *DestPtr_U32p, *TempPtr_U32p, ColorU32, StartX, StartY, Width, Height, i, j, SrcPosH, SrcPosV;

	Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
	if(Error != TH_NO_ERROR)
	{
		return TH_INVALID_HANDLE;
	}
	
	if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888)
	{
		PAL_OSD_PutSrcBitmap(Handle);
		return TH_ERROR_NOT_SUPPORT;
	}
	
	ColorU32 = (BmpParams_p->KeyColor.ARGB8888.Alpha << 24) | (BmpParams_p->KeyColor.ARGB8888.R << 16) |\
				(BmpParams_p->KeyColor.ARGB8888.G << 8) | (BmpParams_p->KeyColor.ARGB8888.B);
//	ColorU32 = *((U32 *)(&BmpParams_p->KeyColor.ARGB8888));

	switch(BmpParams_p->PutType)
	{
		case MID_GFX_BMP_PUT_CENTER:
			StartX = ((BmpParams_p->DestWidth - BmpParams_p->SrcWidth) >> 1) + BmpParams_p->DestLeftX;
			StartY = ((BmpParams_p->DestHeight - BmpParams_p->SrcHeight) >> 1) + BmpParams_p->DestTopY;
			Width = BmpParams_p->SrcWidth;
			Height = BmpParams_p->SrcHeight;
			SrcPtr_U32p = (U32 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * (BmpParams_p->BmpWidth << 2)) + (BmpParams_p->SrcLeftX << 2));
			DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
			for(i=0; i<Height; i++)
			{
				for(j=0; j<Width; j++)
				{
					if((*SrcPtr_U32p) != ColorU32)
					{
						DestPtr_U32p[j] = *SrcPtr_U32p;
					}
					SrcPtr_U32p++;
				}
				DestPtr_U32p += OsdBitmap.Width;
			}
			break;
		case MID_GFX_BMP_PUT_TILE:
			StartX = BmpParams_p->DestLeftX;
			StartY = BmpParams_p->DestTopY;
			Width = BmpParams_p->DestWidth;
			Height = BmpParams_p->DestHeight;
			SrcPtr_U32p = (U32 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * (BmpParams_p->BmpWidth << 2)) + (BmpParams_p->SrcLeftX << 2));
			DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
			TempPtr_U32p = SrcPtr_U32p;
			SrcPosV = 0;
			for(i=0; i<Height; i++)
			{
				SrcPosH = 0;
				for(j=0; j<Width; j++)
				{
					if(TempPtr_U32p[SrcPosH] != ColorU32)
					{
						DestPtr_U32p[j] = TempPtr_U32p[SrcPosH];
					}
					SrcPosH ++;
					if(SrcPosH >= BmpParams_p->SrcWidth)
					{
						SrcPosH = 0;
					}
				}
				DestPtr_U32p += OsdBitmap.Width;/*目标数据向下移一行*/
				SrcPosV ++;
				if(SrcPosV >= BmpParams_p->SrcHeight)
				{
					TempPtr_U32p = SrcPtr_U32p;/*源数据回到第一行*/
					SrcPosV = 0;
				}
				else
				{
					TempPtr_U32p += (BmpParams_p->BmpWidth);/*源数据向下移一行*/
				}
			}
			break;
		case MID_GFX_BMP_PUT_ZOOM:
			Error = TH_ERROR_NOT_SUPPORT;
			break;
		default:
			Error = TH_ERROR_NOT_SUPPORT;
			break;
	}
	PAL_OSD_PutSrcBitmap(Handle);
	
	if(Error == TH_NO_ERROR)
	{
		PAL_OSD_Win_t DirtyWin;
		DirtyWin.LeftX = BmpParams_p->DestLeftX;
		DirtyWin.TopY = BmpParams_p->DestTopY;
		DirtyWin.Width = BmpParams_p->DestWidth;
		DirtyWin.Height = BmpParams_p->DestHeight;
		Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
	}

	return Error;
}

/********************************************************************
画一个没有压缩的ARGB 格式的位图
*********************************************************************/
TH_Error_t  GFX_DrawBmp_NONE_ARGB(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t OsdBitmap;
	U32 *SrcPtr_U32p, *DestPtr_U32p, *TempPtr_U32p, StartX, StartY, Width, Height, i, j, SrcPosH, SrcPosV;

	Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
	if(Error != TH_NO_ERROR)
	{
		return TH_INVALID_HANDLE;
	}
	
	if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888)
	{
		PAL_OSD_PutSrcBitmap(Handle);
		return TH_ERROR_NOT_SUPPORT;
	}
	
	switch(BmpParams_p->PutType)
	{
		case MID_GFX_BMP_PUT_CENTER:
			StartX = ((BmpParams_p->DestWidth - BmpParams_p->SrcWidth) >> 1) + BmpParams_p->DestLeftX;
			StartY = ((BmpParams_p->DestHeight - BmpParams_p->SrcHeight) >> 1) + BmpParams_p->DestTopY;
			Width = BmpParams_p->SrcWidth;
			Height = BmpParams_p->SrcHeight;
			SrcPtr_U32p = (U32 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * (BmpParams_p->BmpWidth << 2)) + (BmpParams_p->SrcLeftX << 2));
			DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
			for(i=0; i<Height; i++)
			{
				for(j=0; j<Width; j++)
				{
					DestPtr_U32p[j] = *SrcPtr_U32p++;
				}
				DestPtr_U32p += (OsdBitmap.Width);/*目标数据向下移一行*/
			}
			break;
		case MID_GFX_BMP_PUT_TILE:
			StartX = BmpParams_p->DestLeftX;
			StartY = BmpParams_p->DestTopY;
			Width = BmpParams_p->DestWidth;
			Height = BmpParams_p->DestHeight;
			SrcPtr_U32p = (U32 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * (BmpParams_p->BmpWidth << 2)) + (BmpParams_p->SrcLeftX << 2));
			DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
			TempPtr_U32p = SrcPtr_U32p;
			SrcPosV = 0;
			for(i=0; i<Height; i++)
			{
				SrcPosH = 0;
				for(j=0; j<Width; j++)
				{
					DestPtr_U32p[j] = TempPtr_U32p[SrcPosH];
					SrcPosH ++;
					if(SrcPosH >= BmpParams_p->SrcWidth)
					{
						SrcPosH = 0;
					}
				}
				DestPtr_U32p += OsdBitmap.Width;/*目标数据向下移一行*/
				SrcPosV ++;
				if(SrcPosV >= BmpParams_p->SrcHeight)
				{
					TempPtr_U32p = SrcPtr_U32p;/*源数据回到第一行*/
					SrcPosV = 0;
				}
				else
				{
					TempPtr_U32p += (BmpParams_p->BmpWidth);/*源数据向下移一行*/
				}
			}
			break;
		case MID_GFX_BMP_PUT_ZOOM:
			Error = TH_ERROR_NOT_SUPPORT;
			break;
		default:
			Error = TH_ERROR_NOT_SUPPORT;
			break;
	}
	PAL_OSD_PutSrcBitmap(Handle);
	
	if(Error == TH_NO_ERROR)
	{
		PAL_OSD_Win_t DirtyWin;
		DirtyWin.LeftX = BmpParams_p->DestLeftX;
		DirtyWin.TopY = BmpParams_p->DestTopY;
		DirtyWin.Width = BmpParams_p->DestWidth;
		DirtyWin.Height = BmpParams_p->DestHeight;
		Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
	}

	return Error;
}

/********************************************************************
画一个没有压缩的RGB 格式的位图带透明色
*********************************************************************/
TH_Error_t  GFX_DrawBmp_NONE_RGB_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t OsdBitmap;
	U32 *DestPtr_U32p, ColorU32, SrcColorU32, StartX, StartY, Width, Height, i, j, SrcPosH, SrcPosV, Pitch;
	U8 *SrcPtr_U8p, *TempPtr_U8p, *SrcLinePrt_U8p;

	Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
	if(Error != TH_NO_ERROR)
	{
		return TH_INVALID_HANDLE;
	}
	
	if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888)
	{
		PAL_OSD_PutSrcBitmap(Handle);
		return TH_ERROR_NOT_SUPPORT;
	}
	
	ColorU32 = (BmpParams_p->KeyColor.RGB888.R << 16) |\
				(BmpParams_p->KeyColor.RGB888.G << 8) | (BmpParams_p->KeyColor.RGB888.B);
	Pitch = BmpParams_p->BmpWidth * 3;
	switch(BmpParams_p->PutType)
	{
		case MID_GFX_BMP_PUT_CENTER:
			StartX = ((BmpParams_p->DestWidth - BmpParams_p->SrcWidth) >> 1) + BmpParams_p->DestLeftX;
			StartY = ((BmpParams_p->DestHeight - BmpParams_p->SrcHeight) >> 1) + BmpParams_p->DestTopY;
			Width = BmpParams_p->SrcWidth;
			Height = BmpParams_p->SrcHeight;
			SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + (BmpParams_p->SrcLeftX * 3));
			DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
			SrcLinePrt_U8p = SrcPtr_U8p;
			for(i=0; i<Height; i++)
			{
				TempPtr_U8p = SrcLinePrt_U8p;
				for(j=0; j<Width; j++)
				{
					SrcColorU32 = GFX_GET_PIXEL_24(TempPtr_U8p);
					if(SrcColorU32 != ColorU32)
					{
						DestPtr_U32p[j] = (SrcColorU32|(GFX_ALPHA_MAX<<24));
					}
					TempPtr_U8p += 3;
				}
				SrcLinePrt_U8p += Pitch;
				DestPtr_U32p += OsdBitmap.Width;
			}
			break;
		case MID_GFX_BMP_PUT_TILE:
			StartX = BmpParams_p->DestLeftX;
			StartY = BmpParams_p->DestTopY;
			Width = BmpParams_p->DestWidth;
			Height = BmpParams_p->DestHeight;
			SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + (BmpParams_p->SrcLeftX * 3));
			DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
			SrcLinePrt_U8p = SrcPtr_U8p;
			SrcPosV = 0;
			for(i=0; i<Height; i++)
			{
				TempPtr_U8p = SrcLinePrt_U8p;
				SrcPosH = 0;
				for(j=0; j<Width; j++)
				{
					SrcColorU32 = GFX_GET_PIXEL_24(TempPtr_U8p);
					if(SrcColorU32 != ColorU32)
					{
						DestPtr_U32p[j] = (SrcColorU32|(GFX_ALPHA_MAX<<24));
					}
					TempPtr_U8p += 3;
					SrcPosH ++;
					if(SrcPosH >= BmpParams_p->SrcWidth)
					{
						TempPtr_U8p = SrcLinePrt_U8p;
						SrcPosH = 0;
					}
				}
				DestPtr_U32p += OsdBitmap.Width;/*目标数据向下移一行*/
				SrcPosV ++;
				if(SrcPosV >= BmpParams_p->SrcHeight)
				{
					SrcLinePrt_U8p = SrcPtr_U8p;/*源数据回到第一行*/
					SrcPosV = 0;
				}
				else
				{
					SrcLinePrt_U8p += Pitch;/*源数据向下移一行*/
				}
			}
			break;
		case MID_GFX_BMP_PUT_ZOOM:
			Error = TH_ERROR_NOT_SUPPORT;
			break;
		default:
			Error = TH_ERROR_NOT_SUPPORT;
			break;
	}
	PAL_OSD_PutSrcBitmap(Handle);
	
	if(Error == TH_NO_ERROR)
	{
		PAL_OSD_Win_t DirtyWin;
		DirtyWin.LeftX = BmpParams_p->DestLeftX;
		DirtyWin.TopY = BmpParams_p->DestTopY;
		DirtyWin.Width = BmpParams_p->DestWidth;
		DirtyWin.Height = BmpParams_p->DestHeight;
		Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
	}

	return Error;
}

/********************************************************************
画一个没有压缩的RGB 格式的位图
*********************************************************************/
TH_Error_t  GFX_DrawBmp_NONE_RGB(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t OsdBitmap;
	U32 *DestPtr_U32p, StartX, StartY, Width, Height, i, j, SrcPosH, SrcPosV, Pitch;
	U8 *SrcPtr_U8p, *TempPtr_U8p, *SrcLinePrt_U8p;

	Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
	if(Error != TH_NO_ERROR)
	{
		return TH_INVALID_HANDLE;
	}
	
	if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888)
	{
		PAL_OSD_PutSrcBitmap(Handle);
		return TH_ERROR_NOT_SUPPORT;
	}
	
	Pitch = BmpParams_p->BmpWidth * 3;
	switch(BmpParams_p->PutType)
	{
		case MID_GFX_BMP_PUT_CENTER:
			StartX = ((BmpParams_p->DestWidth - BmpParams_p->SrcWidth) >> 1) + BmpParams_p->DestLeftX;
			StartY = ((BmpParams_p->DestHeight - BmpParams_p->SrcHeight) >> 1) + BmpParams_p->DestTopY;
			Width = BmpParams_p->SrcWidth;
			Height = BmpParams_p->SrcHeight;
			SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + (BmpParams_p->SrcLeftX * 3));
			DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
			SrcLinePrt_U8p = SrcPtr_U8p;
			for(i=0; i<Height; i++)
			{
				TempPtr_U8p = SrcLinePrt_U8p;
				for(j=0; j<Width; j++)
				{
					DestPtr_U32p[j] = GFX_GET_PIXEL_24(TempPtr_U8p)|(GFX_ALPHA_MAX<<24);
					TempPtr_U8p += 3;
				}
				SrcLinePrt_U8p += Pitch;
				DestPtr_U32p += OsdBitmap.Width;
			}
			break;
		case MID_GFX_BMP_PUT_TILE:
			StartX = BmpParams_p->DestLeftX;
			StartY = BmpParams_p->DestTopY;
			Width = BmpParams_p->DestWidth;
			Height = BmpParams_p->DestHeight;
			SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + (BmpParams_p->SrcLeftX * 3));
			DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
			SrcLinePrt_U8p = SrcPtr_U8p;
			SrcPosV = 0;
			for(i=0; i<Height; i++)
			{
				TempPtr_U8p = SrcLinePrt_U8p;
				SrcPosH = 0;
				for(j=0; j<Width; j++)
				{
					DestPtr_U32p[j] = GFX_GET_PIXEL_24(TempPtr_U8p)|(GFX_ALPHA_MAX<<24);
					TempPtr_U8p += 3;
					SrcPosH ++;
					if(SrcPosH >= BmpParams_p->SrcWidth)
					{
						TempPtr_U8p = SrcLinePrt_U8p;
						SrcPosH = 0;
					}
				}
				DestPtr_U32p += OsdBitmap.Width;/*目标数据向下移一行*/
				SrcPosV ++;
				if(SrcPosV >= BmpParams_p->SrcHeight)
				{
					SrcLinePrt_U8p = SrcPtr_U8p;/*源数据回到第一行*/
					SrcPosV = 0;
				}
				else
				{
					SrcLinePrt_U8p += Pitch;/*源数据向下移一行*/
				}
			}
			break;
		case MID_GFX_BMP_PUT_ZOOM:
			Error = TH_ERROR_NOT_SUPPORT;
			break;
		default:
			Error = TH_ERROR_NOT_SUPPORT;
			break;
	}
	PAL_OSD_PutSrcBitmap(Handle);
	
	if(Error == TH_NO_ERROR)
	{
		PAL_OSD_Win_t DirtyWin;
		DirtyWin.LeftX = BmpParams_p->DestLeftX;
		DirtyWin.TopY = BmpParams_p->DestTopY;
		DirtyWin.Width = BmpParams_p->DestWidth;
		DirtyWin.Height = BmpParams_p->DestHeight;
		Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
	}

	return Error;
}

/********************************************************************
画一个没有压缩的CLUT8 格式的位图带透明色
*********************************************************************/
TH_Error_t  GFX_DrawBmp_NONE_CLUT8_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t OsdBitmap;
	U32 *DestPtr_U32p, ColorU32, SrcColorU32, StartX, StartY, Width, Height, i, j, SrcPosH, SrcPosV, Pitch;
	U8 *SrcPtr_U8p, *DestPtr_U8p, *TempPtr_U8p;

	Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
	if(Error != TH_NO_ERROR)
	{
		return TH_INVALID_HANDLE;
	}
	
	if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888 &&\
		OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_CLUT8)
	{
		PAL_OSD_PutSrcBitmap(Handle);
		return TH_ERROR_NOT_SUPPORT;
	}
	
	ColorU32 = BmpParams_p->KeyColor.CLUT8;
	Pitch = BmpParams_p->BmpWidth;
	if(OsdBitmap.ColorType == PAL_OSD_COLOR_TYPE_ARGB8888)
	{
		switch(BmpParams_p->PutType)
		{
			case MID_GFX_BMP_PUT_CENTER:
				StartX = ((BmpParams_p->DestWidth - BmpParams_p->SrcWidth) >> 1) + BmpParams_p->DestLeftX;
				StartY = ((BmpParams_p->DestHeight - BmpParams_p->SrcHeight) >> 1) + BmpParams_p->DestTopY;
				Width = BmpParams_p->SrcWidth;
				Height = BmpParams_p->SrcHeight;
				SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + BmpParams_p->SrcLeftX);
				DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
				for(i=0; i<Height; i++)
				{
					for(j=0; j<Width; j++)
					{
						if(SrcPtr_U8p[j] != ColorU32)
						{
							DestPtr_U32p[j] = BmpParams_p->Palette_p[SrcPtr_U8p[j]].Alpha<<24|\
											BmpParams_p->Palette_p[SrcPtr_U8p[j]].R<<16|\
											BmpParams_p->Palette_p[SrcPtr_U8p[j]].G<<8|\
											BmpParams_p->Palette_p[SrcPtr_U8p[j]].B;
						}
					}
					SrcPtr_U8p += Pitch;
					DestPtr_U32p += OsdBitmap.Width;
				}
				break;
			case MID_GFX_BMP_PUT_TILE:
				StartX = BmpParams_p->DestLeftX;
				StartY = BmpParams_p->DestTopY;
				Width = BmpParams_p->DestWidth;
				Height = BmpParams_p->DestHeight;
				SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + BmpParams_p->SrcLeftX);
				DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
				TempPtr_U8p = SrcPtr_U8p;
				SrcPosV = 0;
				for(i=0; i<Height; i++)
				{
					SrcPosH = 0;
					for(j=0; j<Width; j++)
					{
						if(TempPtr_U8p[SrcPosH] != ColorU32)
						{
							DestPtr_U32p[j] = BmpParams_p->Palette_p[TempPtr_U8p[SrcPosH]].Alpha<<24|\
											BmpParams_p->Palette_p[TempPtr_U8p[SrcPosH]].R<<16|\
											BmpParams_p->Palette_p[TempPtr_U8p[SrcPosH]].G<<8|\
											BmpParams_p->Palette_p[TempPtr_U8p[SrcPosH]].B;
						}
						SrcPosH ++;
						if(SrcPosH >= BmpParams_p->SrcWidth)
						{
							SrcPosH = 0;
						}
					}
					DestPtr_U32p += OsdBitmap.Width;/*目标数据向下移一行*/
					SrcPosV ++;
					if(SrcPosV >= BmpParams_p->SrcHeight)
					{
						TempPtr_U8p = SrcPtr_U8p;/*源数据回到第一行*/
						SrcPosV = 0;
					}
					else
					{
						TempPtr_U8p += (BmpParams_p->BmpWidth);/*源数据向下移一行*/
					}
				}
				break;
			case MID_GFX_BMP_PUT_ZOOM:
				Error = TH_ERROR_NOT_SUPPORT;
				break;
			default:
				Error = TH_ERROR_NOT_SUPPORT;
				break;
		}
	}
	else
	{
		switch(BmpParams_p->PutType)
		{
			case MID_GFX_BMP_PUT_CENTER:
				StartX = ((BmpParams_p->DestWidth - BmpParams_p->SrcWidth) >> 1) + BmpParams_p->DestLeftX;
				StartY = ((BmpParams_p->DestHeight - BmpParams_p->SrcHeight) >> 1) + BmpParams_p->DestTopY;
				Width = BmpParams_p->SrcWidth;
				Height = BmpParams_p->SrcHeight;
				SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + BmpParams_p->SrcLeftX);
				DestPtr_U8p = (U8 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + StartX);
				for(i=0; i<Height; i++)
				{
					for(j=0; j<Width; j++)
					{
						SrcColorU32 = SrcPtr_U8p[j];
						if(SrcColorU32 != ColorU32)
						{
							DestPtr_U8p[j] = SrcColorU32;
						}
					}
					SrcPtr_U8p += Pitch;
					DestPtr_U8p += OsdBitmap.Pitch;
				}
				break;
			case MID_GFX_BMP_PUT_TILE:
				StartX = BmpParams_p->DestLeftX;
				StartY = BmpParams_p->DestTopY;
				Width = BmpParams_p->DestWidth;
				Height = BmpParams_p->DestHeight;
				SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + BmpParams_p->SrcLeftX);
				DestPtr_U8p = (U8 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + StartX);
				TempPtr_U8p = SrcPtr_U8p;
				SrcPosV = 0;
				for(i=0; i<Height; i++)
				{
					SrcPosH = 0;
					for(j=0; j<Width; j++)
					{
						if(TempPtr_U8p[SrcPosH] != ColorU32)
						{
							DestPtr_U8p[j] = TempPtr_U8p[SrcPosH];
						}
						SrcPosH ++;
						if(SrcPosH >= BmpParams_p->SrcWidth)
						{
							SrcPosH = 0;
						}
					}
					DestPtr_U8p += OsdBitmap.Pitch;/*目标数据向下移一行*/
					SrcPosV ++;
					if(SrcPosV >= BmpParams_p->SrcHeight)
					{
						TempPtr_U8p = SrcPtr_U8p;/*源数据回到第一行*/
						SrcPosV = 0;
					}
					else
					{
						TempPtr_U8p += (BmpParams_p->BmpWidth);/*源数据向下移一行*/
					}
				}
				break;
			case MID_GFX_BMP_PUT_ZOOM:
				Error = TH_ERROR_NOT_SUPPORT;
				break;
			default:
				Error = TH_ERROR_NOT_SUPPORT;
				break;
		}
	}
	
	PAL_OSD_PutSrcBitmap(Handle);
	
	if(Error == TH_NO_ERROR)
	{
		PAL_OSD_Win_t DirtyWin;
		DirtyWin.LeftX = BmpParams_p->DestLeftX;
		DirtyWin.TopY = BmpParams_p->DestTopY;
		DirtyWin.Width = BmpParams_p->DestWidth;
		DirtyWin.Height = BmpParams_p->DestHeight;
		Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
	}

	return Error;
}

/********************************************************************
画一个没有压缩的CLUT8 格式的位图
*********************************************************************/
TH_Error_t  GFX_DrawBmp_NONE_CLUT8(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t OsdBitmap;
	U32 *DestPtr_U32p, StartX, StartY, Width, Height, i, j, SrcPosH, SrcPosV, Pitch;
	U8 *SrcPtr_U8p, *DestPtr_U8p, *TempPtr_U8p;

	Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
	if(Error != TH_NO_ERROR)
	{
		return TH_INVALID_HANDLE;
	}
	
	if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888 &&\
		OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_CLUT8)
	{
		PAL_OSD_PutSrcBitmap(Handle);
		return TH_ERROR_NOT_SUPPORT;
	}
	
	Pitch = BmpParams_p->BmpWidth;
	if(OsdBitmap.ColorType == PAL_OSD_COLOR_TYPE_ARGB8888)
	{
		switch(BmpParams_p->PutType)
		{
			case MID_GFX_BMP_PUT_CENTER:
				StartX = ((BmpParams_p->DestWidth - BmpParams_p->SrcWidth) >> 1) + BmpParams_p->DestLeftX;
				StartY = ((BmpParams_p->DestHeight - BmpParams_p->SrcHeight) >> 1) + BmpParams_p->DestTopY;
				Width = BmpParams_p->SrcWidth;
				Height = BmpParams_p->SrcHeight;
				SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + BmpParams_p->SrcLeftX);
				DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
				for(i=0; i<Height; i++)
				{
					for(j=0; j<Width; j++)
					{
						DestPtr_U32p[j] = BmpParams_p->Palette_p[SrcPtr_U8p[j]].Alpha<<24|\
										BmpParams_p->Palette_p[SrcPtr_U8p[j]].R<<16|\
										BmpParams_p->Palette_p[SrcPtr_U8p[j]].G<<8|\
										BmpParams_p->Palette_p[SrcPtr_U8p[j]].B;
					}
					SrcPtr_U8p += Pitch;
					DestPtr_U32p += OsdBitmap.Width;
				}
				break;
			case MID_GFX_BMP_PUT_TILE:
				StartX = BmpParams_p->DestLeftX;
				StartY = BmpParams_p->DestTopY;
				Width = BmpParams_p->DestWidth;
				Height = BmpParams_p->DestHeight;
				SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + BmpParams_p->SrcLeftX);
				DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + (StartX << 2));
				TempPtr_U8p = SrcPtr_U8p;
				SrcPosV = 0;
				for(i=0; i<Height; i++)
				{
					SrcPosH = 0;
					for(j=0; j<Width; j++)
					{
						DestPtr_U32p[j] = BmpParams_p->Palette_p[TempPtr_U8p[SrcPosH]].Alpha<<24|\
										BmpParams_p->Palette_p[TempPtr_U8p[SrcPosH]].R<<16|\
										BmpParams_p->Palette_p[TempPtr_U8p[SrcPosH]].G<<8|\
										BmpParams_p->Palette_p[TempPtr_U8p[SrcPosH]].B;
						SrcPosH ++;
						if(SrcPosH >= BmpParams_p->SrcWidth)
						{
							SrcPosH = 0;
						}
					}
					DestPtr_U32p += OsdBitmap.Width;/*目标数据向下移一行*/
					SrcPosV ++;
					if(SrcPosV >= BmpParams_p->SrcHeight)
					{
						TempPtr_U8p = SrcPtr_U8p;/*源数据回到第一行*/
						SrcPosV = 0;
					}
					else
					{
						TempPtr_U8p += (BmpParams_p->BmpWidth);/*源数据向下移一行*/
					}
				}
				break;
			case MID_GFX_BMP_PUT_ZOOM:
				Error = TH_ERROR_NOT_SUPPORT;
				break;
			default:
				Error = TH_ERROR_NOT_SUPPORT;
				break;
		}
	}
	else
	{
		switch(BmpParams_p->PutType)
		{
			case MID_GFX_BMP_PUT_CENTER:
				StartX = ((BmpParams_p->DestWidth - BmpParams_p->SrcWidth) >> 1) + BmpParams_p->DestLeftX;
				StartY = ((BmpParams_p->DestHeight - BmpParams_p->SrcHeight) >> 1) + BmpParams_p->DestTopY;
				Width = BmpParams_p->SrcWidth;
				Height = BmpParams_p->SrcHeight;
				SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + BmpParams_p->SrcLeftX);
				DestPtr_U8p = (U8 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + StartX);
				for(i=0; i<Height; i++)
				{
					for(j=0; j<Width; j++)
					{
						DestPtr_U8p[j] = SrcPtr_U8p[j];
					}
					SrcPtr_U8p += Pitch;
					DestPtr_U8p += OsdBitmap.Pitch;
				}
				break;
			case MID_GFX_BMP_PUT_TILE:
				StartX = BmpParams_p->DestLeftX;
				StartY = BmpParams_p->DestTopY;
				Width = BmpParams_p->DestWidth;
				Height = BmpParams_p->DestHeight;
				SrcPtr_U8p = (U8 *)(((U8 *)BmpParams_p->BmpData_p) + (BmpParams_p->SrcTopY * Pitch) + BmpParams_p->SrcLeftX);
				DestPtr_U8p = (U8 *)(((U8 *)OsdBitmap.Data_p) + (StartY * OsdBitmap.Pitch) + StartX);
				TempPtr_U8p = SrcPtr_U8p;
				SrcPosV = 0;
				for(i=0; i<Height; i++)
				{
					SrcPosH = 0;
					for(j=0; j<Width; j++)
					{
						DestPtr_U8p[j] = TempPtr_U8p[SrcPosH];
						SrcPosH ++;
						if(SrcPosH >= BmpParams_p->SrcWidth)
						{
							SrcPosH = 0;
						}
					}
					DestPtr_U8p += OsdBitmap.Pitch;/*目标数据向下移一行*/
					SrcPosV ++;
					if(SrcPosV >= BmpParams_p->SrcHeight)
					{
						TempPtr_U8p = SrcPtr_U8p;/*源数据回到第一行*/
						SrcPosV = 0;
					}
					else
					{
						TempPtr_U8p += (BmpParams_p->BmpWidth);/*源数据向下移一行*/
					}
				}
				break;
			case MID_GFX_BMP_PUT_ZOOM:
				Error = TH_ERROR_NOT_SUPPORT;
				break;
			default:
				Error = TH_ERROR_NOT_SUPPORT;
				break;
		}
	}
	
	PAL_OSD_PutSrcBitmap(Handle);
	
	if(Error == TH_NO_ERROR)
	{
		PAL_OSD_Win_t DirtyWin;
		DirtyWin.LeftX = BmpParams_p->DestLeftX;
		DirtyWin.TopY = BmpParams_p->DestTopY;
		DirtyWin.Width = BmpParams_p->DestWidth;
		DirtyWin.Height = BmpParams_p->DestHeight;
		Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
	}

	return Error;
}

#if (GFX_ENABLE_LZO_BMP)
/********************************************************************
画一个LZO 压缩的ARGB 格式的位图带透明色
*********************************************************************/
TH_Error_t  GFX_DrawBmp_LZO_ARGB_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;
	
	Error = lzo1x_decompress_safe((U8 *)(BmpParams_p->BmpData_p), (lzo_uint)(BmpParams_p->BmpDataCompSize), (U8 *)(TempBitmap.Data_p), (lzo_uint *)(&DestLen), NULL);
	if(Error != LZO_E_OK)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_ARGB_KeyColor(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

/********************************************************************
画一个LZO 压缩的ARGB 格式的位图
*********************************************************************/
TH_Error_t  GFX_DrawBmp_LZO_ARGB(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;
	
	Error = lzo1x_decompress_safe((U8 *)(BmpParams_p->BmpData_p), (lzo_uint)(BmpParams_p->BmpDataCompSize), (U8 *)(TempBitmap.Data_p), (lzo_uint *)(&DestLen), NULL);
	if(Error != LZO_E_OK)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_ARGB(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

/********************************************************************
画一个LZO 压缩的RGB 格式的位图带透明色
*********************************************************************/
TH_Error_t  GFX_DrawBmp_LZO_RGB_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_RGB888;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;
	
	Error = lzo1x_decompress_safe((U8 *)(BmpParams_p->BmpData_p), (lzo_uint)(BmpParams_p->BmpDataCompSize), (U8 *)(TempBitmap.Data_p), (lzo_uint *)(&DestLen), NULL);
	if(Error != LZO_E_OK)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_RGB_KeyColor(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

/********************************************************************
画一个LZO 压缩的RGB 格式的位图
*********************************************************************/
TH_Error_t  GFX_DrawBmp_LZO_RGB(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_RGB888;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;
	
	Error = lzo1x_decompress_safe((U8 *)(BmpParams_p->BmpData_p), (lzo_uint)(BmpParams_p->BmpDataCompSize), (U8 *)(TempBitmap.Data_p), (lzo_uint *)(&DestLen), NULL);
	if(Error != LZO_E_OK)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_RGB(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

/********************************************************************
画一个LZO 压缩的CLUT8 格式的位图带透明色
*********************************************************************/
TH_Error_t  GFX_DrawBmp_LZO_CLUT8_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_CLUT8;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;

	Error = lzo1x_decompress_safe((U8 *)(BmpParams_p->BmpData_p), (lzo_uint)(BmpParams_p->BmpDataCompSize), (U8 *)(TempBitmap.Data_p), (lzo_uint *)(&DestLen), NULL);
	if(Error != LZO_E_OK)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_CLUT8_KeyColor(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

/********************************************************************
画一个LZO 压缩的CLUT8 格式的位图
*********************************************************************/
TH_Error_t  GFX_DrawBmp_LZO_CLUT8(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_CLUT8;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;

	Error = lzo1x_decompress_safe((U8 *)(BmpParams_p->BmpData_p), (lzo_uint)(BmpParams_p->BmpDataCompSize), (U8 *)(TempBitmap.Data_p), (lzo_uint *)(&DestLen), NULL);
	if(Error != LZO_E_OK)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_CLUT8(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}
#endif

TH_Error_t  GFX_DrawBmp_RLE_ARGB_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;

	if(BmpParams_p->SrcWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->SrcHeight == BmpParams_p->BmpHeight &&\
		BmpParams_p->DestWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->DestHeight == BmpParams_p->BmpHeight)
	{
		PAL_OSD_Bitmap_t OsdBitmap;
		S32 i, j, DrawWidth, LeftWidth;
		U8 *SrcPtr_p, Count, Value;
		U32 *DestPtr_U32p, ValueU32, ColorU32;
		
		Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
		if(Error != TH_NO_ERROR)
		{
			return TH_INVALID_HANDLE;
		}
		
		if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NOT_SUPPORT;
		}

		SrcPtr_p = (U8 *)(BmpParams_p->BmpData_p);
		DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (BmpParams_p->DestTopY * OsdBitmap.Pitch) + (BmpParams_p->DestLeftX << 2));
		LeftWidth = BmpParams_p->BmpWidth;
		ColorU32 = BmpParams_p->KeyColor.ARGB8888.Alpha << 24 | BmpParams_p->KeyColor.ARGB8888.R << 16 |
					BmpParams_p->KeyColor.ARGB8888.G << 8 | BmpParams_p->KeyColor.ARGB8888.B;
		for(i=0; i<BmpParams_p->BmpHeight; )
		{
			Value = *SrcPtr_p++;
			if(Value & 0x80)
			{
				Count = (Value & 0x7F);
				while(Count)
				{
					if(Count >= LeftWidth)
					{
						DrawWidth = LeftWidth;
					}
					else
					{
						DrawWidth = Count;
					}

					for(j=0; j<DrawWidth; j++)
					{
						ValueU32 = GFX_GET_PIXEL_32(SrcPtr_p);
						SrcPtr_p += 4;
						if(ValueU32 != ColorU32)
						{
							*DestPtr_U32p = ValueU32;
						}
						DestPtr_U32p ++;
					}
					if(DrawWidth == LeftWidth)
					{
						DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
						i ++;
						LeftWidth = BmpParams_p->BmpWidth;
					}
					else
					{
						LeftWidth -= DrawWidth;
					}
					Count -= DrawWidth;
				}
			}
			else
			{
				Count = Value;
				ValueU32 = GFX_GET_PIXEL_32(SrcPtr_p);
				SrcPtr_p += 4;
				while(Count)
				{
					if(Count >= LeftWidth)
					{
						DrawWidth = LeftWidth;
					}
					else
					{
						DrawWidth = Count;
					}
					if(ValueU32 != ColorU32)
					{
						for(j=0; j<DrawWidth; j++)
						{
							*DestPtr_U32p++ = ValueU32;
						}
					}
					else
					{
						DestPtr_U32p += DrawWidth;
					}
					if(DrawWidth == LeftWidth)
					{
						DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
						i ++;
						LeftWidth = BmpParams_p->BmpWidth;
					}
					else
					{
						LeftWidth -= DrawWidth;
					}
					Count -= DrawWidth;
				}
			}
		}	
		PAL_OSD_PutSrcBitmap(Handle);
		if(Error == TH_NO_ERROR)
		{
			PAL_OSD_Win_t DirtyWin;
			DirtyWin.LeftX = BmpParams_p->DestLeftX;
			DirtyWin.TopY = BmpParams_p->DestTopY;
			DirtyWin.Width = BmpParams_p->DestWidth;
			DirtyWin.Height = BmpParams_p->DestHeight;
			Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
		}
	}
	else
	{
		MID_GFX_PutBmp_t TempBmpParams;
		PAL_OSD_Bitmap_t		TempBitmap;
		U32 DestLen;

		TempBitmap.Width = BmpParams_p->BmpWidth;
		TempBitmap.Height = BmpParams_p->BmpHeight;
		TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
		TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

		Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
		if(Error != TH_NO_ERROR)
		{
			return TH_ERROR_NO_MEM;
		}
		DestLen = TempBitmap.Size;
		if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 4, 127) != 0)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		if(DestLen != BmpParams_p->BmpDataRawSize)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		
		TempBmpParams = *BmpParams_p;
		TempBmpParams.BmpData_p = TempBitmap.Data_p;
		TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
		Error = GFX_DrawBmp_NONE_ARGB_KeyColor(Handle, &TempBmpParams);

		PAL_OSD_FreeBitmap(&TempBitmap);
	}
	return Error;
}

TH_Error_t  GFX_DrawBmp_RLE_ARGB(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	
	if(BmpParams_p->SrcWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->SrcHeight == BmpParams_p->BmpHeight &&\
		BmpParams_p->DestWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->DestHeight == BmpParams_p->BmpHeight)
	{
		PAL_OSD_Bitmap_t OsdBitmap;
		S32 i, j, DrawWidth, LeftWidth;
		U8 *SrcPtr_p, Count, Value;
		U32 *DestPtr_U32p, ValueU32;
		
		Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
		if(Error != TH_NO_ERROR)
		{
			return TH_INVALID_HANDLE;
		}
		
		if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NOT_SUPPORT;
		}

		SrcPtr_p = (U8 *)(BmpParams_p->BmpData_p);
		DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (BmpParams_p->DestTopY * OsdBitmap.Pitch) + (BmpParams_p->DestLeftX << 2));
		LeftWidth = BmpParams_p->BmpWidth;
		for(i=0; i<BmpParams_p->BmpHeight; )
		{
			Value = *SrcPtr_p++;
			if(Value & 0x80)
			{
				Count = (Value & 0x7F);
				while(Count)
				{
					if(Count >= LeftWidth)
					{
						DrawWidth = LeftWidth;
					}
					else
					{
						DrawWidth = Count;
					}

					for(j=0; j<DrawWidth; j++)
					{
						*DestPtr_U32p++ = GFX_GET_PIXEL_32(SrcPtr_p);
						SrcPtr_p += 4;
					}
					if(DrawWidth == LeftWidth)
					{
						DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
						i ++;
						LeftWidth = BmpParams_p->BmpWidth;
					}
					else
					{
						LeftWidth -= DrawWidth;
					}
					Count -= DrawWidth;
				}
			}
			else
			{
				Count = Value;
				ValueU32 = GFX_GET_PIXEL_32(SrcPtr_p);
				SrcPtr_p += 4;
				while(Count)
				{
					if(Count >= LeftWidth)
					{
						DrawWidth = LeftWidth;
					}
					else
					{
						DrawWidth = Count;
					}
					for(j=0; j<DrawWidth; j++)
					{
						*DestPtr_U32p++ = ValueU32;
					}
					if(DrawWidth == LeftWidth)
					{
						DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
						i ++;
						LeftWidth = BmpParams_p->BmpWidth;
					}
					else
					{
						LeftWidth -= DrawWidth;
					}
					Count -= DrawWidth;
				}
			}
		}	
		PAL_OSD_PutSrcBitmap(Handle);
		if(Error == TH_NO_ERROR)
		{
			PAL_OSD_Win_t DirtyWin;
			DirtyWin.LeftX = BmpParams_p->DestLeftX;
			DirtyWin.TopY = BmpParams_p->DestTopY;
			DirtyWin.Width = BmpParams_p->DestWidth;
			DirtyWin.Height = BmpParams_p->DestHeight;
			Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
		}
	}
	else
	{
		PAL_OSD_Bitmap_t TempBitmap;
		U32 DestLen;
		MID_GFX_PutBmp_t TempBmpParams;
		
		TempBitmap.Width = BmpParams_p->BmpWidth;
		TempBitmap.Height = BmpParams_p->BmpHeight;
		TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
		TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

		Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
		if(Error != TH_NO_ERROR)
		{
			return TH_ERROR_NO_MEM;
		}
		DestLen = TempBitmap.Size;
		if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 4, 127) != 0)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		if(DestLen != BmpParams_p->BmpDataRawSize)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		
		TempBmpParams = *BmpParams_p;
		TempBmpParams.BmpData_p = TempBitmap.Data_p;
		TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
		Error = GFX_DrawBmp_NONE_ARGB(Handle, &TempBmpParams);

		PAL_OSD_FreeBitmap(&TempBitmap);
	}
	return Error;
}

TH_Error_t  GFX_DrawBmp_RLE_RGB_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;

	if(BmpParams_p->SrcWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->SrcHeight == BmpParams_p->BmpHeight &&\
		BmpParams_p->DestWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->DestHeight == BmpParams_p->BmpHeight)
	{
		PAL_OSD_Bitmap_t OsdBitmap;
		S32 i, j, DrawWidth, LeftWidth;
		U8 *SrcPtr_p, Count, Value;
		U32 *DestPtr_U32p, ValueU32, ColorU32;
		
		Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
		if(Error != TH_NO_ERROR)
		{
			return TH_INVALID_HANDLE;
		}
		
		if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NOT_SUPPORT;
		}

		SrcPtr_p = (U8 *)(BmpParams_p->BmpData_p);
		DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (BmpParams_p->DestTopY * OsdBitmap.Pitch) + (BmpParams_p->DestLeftX << 2));
		LeftWidth = BmpParams_p->BmpWidth;
		ColorU32 = BmpParams_p->KeyColor.RGB888.R << 16 |
					BmpParams_p->KeyColor.RGB888.G << 8 | BmpParams_p->KeyColor.RGB888.B;
		for(i=0; i<BmpParams_p->BmpHeight; )
		{
			Value = *SrcPtr_p++;
			if(Value & 0x80)
			{
				Count = (Value & 0x7F);
				while(Count)
				{
					if(Count >= LeftWidth)
					{
						DrawWidth = LeftWidth;
					}
					else
					{
						DrawWidth = Count;
					}

					for(j=0; j<DrawWidth; j++)
					{
						ValueU32 = GFX_GET_PIXEL_24(SrcPtr_p);
						SrcPtr_p += 3;
						if(ValueU32 != ColorU32)
						{
							*DestPtr_U32p = ValueU32|(GFX_ALPHA_MAX<<24);
						}
						DestPtr_U32p ++;
					}
					if(DrawWidth == LeftWidth)
					{
						DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
						i ++;
						LeftWidth = BmpParams_p->BmpWidth;
					}
					else
					{
						LeftWidth -= DrawWidth;
					}
					Count -= DrawWidth;
				}
			}
			else
			{
				Count = Value;
				ValueU32 = GFX_GET_PIXEL_24(SrcPtr_p);
				SrcPtr_p += 3;
				while(Count)
				{
					if(Count >= LeftWidth)
					{
						DrawWidth = LeftWidth;
					}
					else
					{
						DrawWidth = Count;
					}
					if(ValueU32 != ColorU32)
					{
						ValueU32 |= (GFX_ALPHA_MAX<<24);
						for(j=0; j<DrawWidth; j++)
						{
							*DestPtr_U32p++ = ValueU32;
						}
					}
					else
					{
						DestPtr_U32p += DrawWidth;
					}
					if(DrawWidth == LeftWidth)
					{
						DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
						i ++;
						LeftWidth = BmpParams_p->BmpWidth;
					}
					else
					{
						LeftWidth -= DrawWidth;
					}
					Count -= DrawWidth;
				}
			}
		}	
		PAL_OSD_PutSrcBitmap(Handle);
		if(Error == TH_NO_ERROR)
		{
			PAL_OSD_Win_t DirtyWin;
			DirtyWin.LeftX = BmpParams_p->DestLeftX;
			DirtyWin.TopY = BmpParams_p->DestTopY;
			DirtyWin.Width = BmpParams_p->DestWidth;
			DirtyWin.Height = BmpParams_p->DestHeight;
			Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
		}
	}
	else
	{
		PAL_OSD_Bitmap_t TempBitmap;
		U32 DestLen;
		MID_GFX_PutBmp_t TempBmpParams;
		
		TempBitmap.Width = BmpParams_p->BmpWidth;
		TempBitmap.Height = BmpParams_p->BmpHeight;
		TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_RGB888;
		TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

		Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
		if(Error != TH_NO_ERROR)
		{
			return TH_ERROR_NO_MEM;
		}
		DestLen = TempBitmap.Size;
		if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 3, 127) != 0)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		if(DestLen != BmpParams_p->BmpDataRawSize)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		
		TempBmpParams = *BmpParams_p;
		TempBmpParams.BmpData_p = TempBitmap.Data_p;
		TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
		Error = GFX_DrawBmp_NONE_RGB_KeyColor(Handle, &TempBmpParams);

		PAL_OSD_FreeBitmap(&TempBitmap);
	}
	return Error;
}

TH_Error_t  GFX_DrawBmp_RLE_RGB(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;

	if(BmpParams_p->SrcWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->SrcHeight == BmpParams_p->BmpHeight &&\
		BmpParams_p->DestWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->DestHeight == BmpParams_p->BmpHeight)
	{
		PAL_OSD_Bitmap_t OsdBitmap;
		S32 i, j, DrawWidth, LeftWidth;
		U8 *SrcPtr_p, Count, Value;
		U32 *DestPtr_U32p, ValueU32;
		
		Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
		if(Error != TH_NO_ERROR)
		{
			return TH_INVALID_HANDLE;
		}
		
		if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NOT_SUPPORT;
		}

		SrcPtr_p = (U8 *)(BmpParams_p->BmpData_p);
		DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (BmpParams_p->DestTopY * OsdBitmap.Pitch) + (BmpParams_p->DestLeftX << 2));
		LeftWidth = BmpParams_p->BmpWidth;
		for(i=0; i<BmpParams_p->BmpHeight; )
		{
			Value = *SrcPtr_p++;
			if(Value & 0x80)
			{
				Count = (Value & 0x7F);
				while(Count)
				{
					if(Count >= LeftWidth)
					{
						DrawWidth = LeftWidth;
					}
					else
					{
						DrawWidth = Count;
					}

					for(j=0; j<DrawWidth; j++)
					{
						*DestPtr_U32p++ = GFX_GET_PIXEL_24(SrcPtr_p)|(GFX_ALPHA_MAX<<24);
						SrcPtr_p += 3;
					}
					if(DrawWidth == LeftWidth)
					{
						DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
						i ++;
						LeftWidth = BmpParams_p->BmpWidth;
					}
					else
					{
						LeftWidth -= DrawWidth;
					}
					Count -= DrawWidth;
				}
			}
			else
			{
				Count = Value;
				ValueU32 = GFX_GET_PIXEL_24(SrcPtr_p)|(GFX_ALPHA_MAX<<24);
				SrcPtr_p += 3;
				while(Count)
				{
					if(Count >= LeftWidth)
					{
						DrawWidth = LeftWidth;
					}
					else
					{
						DrawWidth = Count;
					}
					for(j=0; j<DrawWidth; j++)
					{
						*DestPtr_U32p++ = ValueU32;
					}
					if(DrawWidth == LeftWidth)
					{
						DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
						i ++;
						LeftWidth = BmpParams_p->BmpWidth;
					}
					else
					{
						LeftWidth -= DrawWidth;
					}
					Count -= DrawWidth;
				}
			}
		}	
		PAL_OSD_PutSrcBitmap(Handle);
		if(Error == TH_NO_ERROR)
		{
			PAL_OSD_Win_t DirtyWin;
			DirtyWin.LeftX = BmpParams_p->DestLeftX;
			DirtyWin.TopY = BmpParams_p->DestTopY;
			DirtyWin.Width = BmpParams_p->DestWidth;
			DirtyWin.Height = BmpParams_p->DestHeight;
			Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
		}
	}
	else
	{
		PAL_OSD_Bitmap_t TempBitmap;
		U32 DestLen;
		MID_GFX_PutBmp_t TempBmpParams;
		
		TempBitmap.Width = BmpParams_p->BmpWidth;
		TempBitmap.Height = BmpParams_p->BmpHeight;
		TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_RGB888;
		TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

		Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
		if(Error != TH_NO_ERROR)
		{
			return TH_ERROR_NO_MEM;
		}
		DestLen = TempBitmap.Size;
		if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 3, 127) != 0)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		if(DestLen != BmpParams_p->BmpDataRawSize)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		
		TempBmpParams = *BmpParams_p;
		TempBmpParams.BmpData_p = TempBitmap.Data_p;
		TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
		Error = GFX_DrawBmp_NONE_RGB(Handle, &TempBmpParams);

		PAL_OSD_FreeBitmap(&TempBitmap);
	}
	return Error;
}

TH_Error_t  GFX_DrawBmp_RLE_CLUT8_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	
	if(BmpParams_p->SrcWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->SrcHeight == BmpParams_p->BmpHeight &&\
		BmpParams_p->DestWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->DestHeight == BmpParams_p->BmpHeight)
	{
		PAL_OSD_Bitmap_t OsdBitmap;
		S32 i, j, DrawWidth, LeftWidth;
		U8 *SrcPtr_p, Count, Value;
		
		Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
		if(Error != TH_NO_ERROR)
		{
			return TH_INVALID_HANDLE;
		}
		
		if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888 &&\
			OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_CLUT8)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NOT_SUPPORT;
		}
		
		if(OsdBitmap.ColorType == PAL_OSD_COLOR_TYPE_ARGB8888)
		{
			U32 *DestPtr_U32p, ValueU32, ColorU32;
			
			SrcPtr_p = (U8 *)(BmpParams_p->BmpData_p);
			DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (BmpParams_p->DestTopY * OsdBitmap.Pitch) + (BmpParams_p->DestLeftX << 2));
			LeftWidth = BmpParams_p->BmpWidth;
			ColorU32 = BmpParams_p->KeyColor.CLUT8;
			for(i=0; i<BmpParams_p->BmpHeight; )
			{
				Value = *SrcPtr_p++;
				if(Value & 0x80)
				{
					Count = (Value & 0x7F);
					while(Count)
					{
						if(Count >= LeftWidth)
						{
							DrawWidth = LeftWidth;
						}
						else
						{
							DrawWidth = Count;
						}

						for(j=0; j<DrawWidth; j++)
						{
							if((*SrcPtr_p) != ColorU32)
							{
								*DestPtr_U32p = *((U32 *)&(BmpParams_p->Palette_p[*SrcPtr_p]));
							}
							DestPtr_U32p ++;
							SrcPtr_p ++;
						}
						if(DrawWidth == LeftWidth)
						{
							DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
							i ++;
							LeftWidth = BmpParams_p->BmpWidth;
						}
						else
						{
							LeftWidth -= DrawWidth;
						}
						Count -= DrawWidth;
					}
				}
				else
				{
					Count = Value;
					ValueU32 = *((U32 *)&(BmpParams_p->Palette_p[*SrcPtr_p]));
					SrcPtr_p ++;
					while(Count)
					{
						if(Count >= LeftWidth)
						{
							DrawWidth = LeftWidth;
						}
						else
						{
							DrawWidth = Count;
						}
						if(ValueU32 != ColorU32)
						{
							for(j=0; j<DrawWidth; j++)
							{
								*DestPtr_U32p++ = ValueU32;
							}
						}
						else
						{
							DestPtr_U32p += DrawWidth;
						}
						if(DrawWidth == LeftWidth)
						{
							DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
							i ++;
							LeftWidth = BmpParams_p->BmpWidth;
						}
						else
						{
							LeftWidth -= DrawWidth;
						}
						Count -= DrawWidth;
					}
				}
			}	
		}
		else
		{
			U8 *DestPtr_U8p;
			U32 ValueU32, ColorU32;
			
			SrcPtr_p = (U8 *)(BmpParams_p->BmpData_p);
			DestPtr_U8p = (U8 *)(((U8 *)OsdBitmap.Data_p) + (BmpParams_p->DestTopY * OsdBitmap.Pitch) + BmpParams_p->DestLeftX);
			LeftWidth = BmpParams_p->BmpWidth;
			ColorU32 = BmpParams_p->KeyColor.CLUT8;
			for(i=0; i<BmpParams_p->BmpHeight; )
			{
				Value = *SrcPtr_p++;
				if(Value & 0x80)
				{
					Count = (Value & 0x7F);
					while(Count)
					{
						if(Count >= LeftWidth)
						{
							DrawWidth = LeftWidth;
						}
						else
						{
							DrawWidth = Count;
						}

						for(j=0; j<DrawWidth; j++)
						{
							if((*SrcPtr_p) != ColorU32)
							{
								*DestPtr_U8p = *SrcPtr_p;
							}
							DestPtr_U8p ++;
							SrcPtr_p ++;
						}
						if(DrawWidth == LeftWidth)
						{
							DestPtr_U8p += (OsdBitmap.Pitch- BmpParams_p->BmpWidth);
							i ++;
							LeftWidth = BmpParams_p->BmpWidth;
						}
						else
						{
							LeftWidth -= DrawWidth;
						}
						Count -= DrawWidth;
					}
				}
				else
				{
					Count = Value;
					ValueU32 = *SrcPtr_p++;
					while(Count)
					{
						if(Count >= LeftWidth)
						{
							DrawWidth = LeftWidth;
						}
						else
						{
							DrawWidth = Count;
						}
						if(ValueU32 != ColorU32)
						{
							for(j=0; j<DrawWidth; j++)
							{
								*DestPtr_U8p++ = ValueU32;
							}
						}
						else
						{
							DestPtr_U8p += DrawWidth;
						}
						if(DrawWidth == LeftWidth)
						{
							DestPtr_U8p += (OsdBitmap.Pitch- BmpParams_p->BmpWidth);
							i ++;
							LeftWidth = BmpParams_p->BmpWidth;
						}
						else
						{
							LeftWidth -= DrawWidth;
						}
						Count -= DrawWidth;
					}
				}
			}	
		}
		PAL_OSD_PutSrcBitmap(Handle);
		if(Error == TH_NO_ERROR)
		{
			PAL_OSD_Win_t DirtyWin;
			DirtyWin.LeftX = BmpParams_p->DestLeftX;
			DirtyWin.TopY = BmpParams_p->DestTopY;
			DirtyWin.Width = BmpParams_p->DestWidth;
			DirtyWin.Height = BmpParams_p->DestHeight;
			Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
		}
	}
	else
	{
		PAL_OSD_Bitmap_t TempBitmap;
		U32 DestLen;
		MID_GFX_PutBmp_t TempBmpParams;
		
		TempBitmap.Width = BmpParams_p->BmpWidth;
		TempBitmap.Height = BmpParams_p->BmpHeight;
		TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_CLUT8;
		TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

		Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
		if(Error != TH_NO_ERROR)
		{
			return TH_ERROR_NO_MEM;
		}
		DestLen = TempBitmap.Size;
		if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 1, 127) != 0)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		if(DestLen != BmpParams_p->BmpDataRawSize)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		
		TempBmpParams = *BmpParams_p;
		TempBmpParams.BmpData_p = TempBitmap.Data_p;
		TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
		Error = GFX_DrawBmp_NONE_CLUT8_KeyColor(Handle, &TempBmpParams);

		PAL_OSD_FreeBitmap(&TempBitmap);
	}
	return Error;
}

TH_Error_t  GFX_DrawBmp_RLE_CLUT8(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;

	if(BmpParams_p->SrcWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->SrcHeight == BmpParams_p->BmpHeight &&\
		BmpParams_p->DestWidth == BmpParams_p->BmpWidth &&\
		BmpParams_p->DestHeight == BmpParams_p->BmpHeight)
	{
		PAL_OSD_Bitmap_t OsdBitmap;
		S32 i, j, DrawWidth, LeftWidth;
		U8 *SrcPtr_p, Count, Value;
		
		Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
		if(Error != TH_NO_ERROR)
		{
			return TH_INVALID_HANDLE;
		}
		
		if(OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_ARGB8888 &&\
			OsdBitmap.ColorType != PAL_OSD_COLOR_TYPE_CLUT8)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NOT_SUPPORT;
		}
		
		if(OsdBitmap.ColorType == PAL_OSD_COLOR_TYPE_ARGB8888)
		{
			U32 *DestPtr_U32p, ValueU32;
			
			SrcPtr_p = (U8 *)(BmpParams_p->BmpData_p);
			DestPtr_U32p = (U32 *)(((U8 *)OsdBitmap.Data_p) + (BmpParams_p->DestTopY * OsdBitmap.Pitch) + (BmpParams_p->DestLeftX << 2));
			LeftWidth = BmpParams_p->BmpWidth;
			for(i=0; i<BmpParams_p->BmpHeight; )
			{
				Value = *SrcPtr_p++;
				if(Value & 0x80)
				{
					Count = (Value & 0x7F);
					while(Count)
					{
						if(Count >= LeftWidth)
						{
							DrawWidth = LeftWidth;
						}
						else
						{
							DrawWidth = Count;
						}

						for(j=0; j<DrawWidth; j++)
						{
							*DestPtr_U32p++ = *((U32 *)&(BmpParams_p->Palette_p[*SrcPtr_p++]));
						}
						if(DrawWidth == LeftWidth)
						{
							DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
							i ++;
							LeftWidth = BmpParams_p->BmpWidth;
						}
						else
						{
							LeftWidth -= DrawWidth;
						}
						Count -= DrawWidth;
					}
				}
				else
				{
					Count = Value;
					ValueU32 = *((U32 *)&(BmpParams_p->Palette_p[*SrcPtr_p++]));
					while(Count)
					{
						if(Count >= LeftWidth)
						{
							DrawWidth = LeftWidth;
						}
						else
						{
							DrawWidth = Count;
						}
						for(j=0; j<DrawWidth; j++)
						{
							*DestPtr_U32p++ = ValueU32;
						}
						if(DrawWidth == LeftWidth)
						{
							DestPtr_U32p += (OsdBitmap.Width- BmpParams_p->BmpWidth);
							i ++;
							LeftWidth = BmpParams_p->BmpWidth;
						}
						else
						{
							LeftWidth -= DrawWidth;
						}
						Count -= DrawWidth;
					}
				}
			}	
		}
		else
		{
			U8 *DestPtr_U8p;
			U32 ValueU32, ColorU32;
			
			SrcPtr_p = (U8 *)(BmpParams_p->BmpData_p);
			DestPtr_U8p = (U8 *)(((U8 *)OsdBitmap.Data_p) + (BmpParams_p->DestTopY * OsdBitmap.Pitch) + BmpParams_p->DestLeftX);
			LeftWidth = BmpParams_p->BmpWidth;
			ColorU32 = BmpParams_p->KeyColor.CLUT8;
			for(i=0; i<BmpParams_p->BmpHeight; )
			{
				Value = *SrcPtr_p++;
				if(Value & 0x80)
				{
					Count = (Value & 0x7F);
					while(Count)
					{
						if(Count >= LeftWidth)
						{
							DrawWidth = LeftWidth;
						}
						else
						{
							DrawWidth = Count;
						}

						for(j=0; j<DrawWidth; j++)
						{
							if((*SrcPtr_p) != ColorU32)
							{
								*DestPtr_U8p = *SrcPtr_p;
							}
							DestPtr_U8p ++;
							SrcPtr_p ++;
						}
						if(DrawWidth == LeftWidth)
						{
							DestPtr_U8p += (OsdBitmap.Pitch- BmpParams_p->BmpWidth);
							i ++;
							LeftWidth = BmpParams_p->BmpWidth;
						}
						else
						{
							LeftWidth -= DrawWidth;
						}
						Count -= DrawWidth;
					}
				}
				else
				{
					Count = Value;
					ValueU32 = *SrcPtr_p;
					SrcPtr_p ++;
					while(Count)
					{
						if(Count >= LeftWidth)
						{
							DrawWidth = LeftWidth;
						}
						else
						{
							DrawWidth = Count;
						}
						if(ValueU32 != ColorU32)
						{
							for(j=0; j<DrawWidth; j++)
							{
								*DestPtr_U8p++ = ValueU32;
							}
						}
						else
						{
							DestPtr_U8p += DrawWidth;
						}
						if(DrawWidth == LeftWidth)
						{
							DestPtr_U8p += (OsdBitmap.Pitch- BmpParams_p->BmpWidth);
							i ++;
							LeftWidth = BmpParams_p->BmpWidth;
						}
						else
						{
							LeftWidth -= DrawWidth;
						}
						Count -= DrawWidth;
					}
				}
			}	
		}
		PAL_OSD_PutSrcBitmap(Handle);
		if(Error == TH_NO_ERROR)
		{
			PAL_OSD_Win_t DirtyWin;
			DirtyWin.LeftX = BmpParams_p->DestLeftX;
			DirtyWin.TopY = BmpParams_p->DestTopY;
			DirtyWin.Width = BmpParams_p->DestWidth;
			DirtyWin.Height = BmpParams_p->DestHeight;
			Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
		}
	}
	else
	{
		PAL_OSD_Bitmap_t TempBitmap;
		U32 DestLen;
		MID_GFX_PutBmp_t TempBmpParams;
		
		TempBitmap.Width = BmpParams_p->BmpWidth;
		TempBitmap.Height = BmpParams_p->BmpHeight;
		TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_CLUT8;
		TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

		Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
		if(Error != TH_NO_ERROR)
		{
			return TH_ERROR_NO_MEM;
		}
		DestLen = TempBitmap.Size;
		if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 1, 127) != 0)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		if(DestLen != BmpParams_p->BmpDataRawSize)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			return TH_ERROR_BAD_PARAM;
		}
		
		TempBmpParams = *BmpParams_p;
		TempBmpParams.BmpData_p = TempBitmap.Data_p;
		TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
		Error = GFX_DrawBmp_NONE_CLUT8(Handle, &TempBmpParams);

		PAL_OSD_FreeBitmap(&TempBitmap);
	}
	return Error;
}


TH_Error_t  GFX_DrawBmp_RLE2_ARGB_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;
	if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 4, 32767) != 0)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_ARGB_KeyColor(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

TH_Error_t  GFX_DrawBmp_RLE2_ARGB(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;
	if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 4, 32767) != 0)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_ARGB(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

TH_Error_t  GFX_DrawBmp_RLE2_RGB_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_RGB888;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;
	if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 3, 32767) != 0)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_RGB_KeyColor(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

TH_Error_t  GFX_DrawBmp_RLE2_RGB(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_RGB888;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;
	if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 3, 32767) != 0)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_RGB(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

TH_Error_t  GFX_DrawBmp_RLE2_CLUT8_KeyColor(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_CLUT8;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;
	if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 1, 32767) != 0)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_CLUT8_KeyColor(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

TH_Error_t  GFX_DrawBmp_RLE2_CLUT8(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	U32 DestLen;
	MID_GFX_PutBmp_t TempBmpParams;
	
	TempBitmap.Width = BmpParams_p->BmpWidth;
	TempBitmap.Height = BmpParams_p->BmpHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_CLUT8;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;

	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, TRUE);
	if(Error != TH_NO_ERROR)
	{
		return TH_ERROR_NO_MEM;
	}
	DestLen = TempBitmap.Size;
	if(TH_Rle_Decompress((U8 *)(TempBitmap.Data_p), &DestLen, (const U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, 1, 32767) != 0)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	if(DestLen != BmpParams_p->BmpDataRawSize)
	{
		PAL_OSD_FreeBitmap(&TempBitmap);
		return TH_ERROR_BAD_PARAM;
	}
	
	TempBmpParams = *BmpParams_p;
	TempBmpParams.BmpData_p = TempBitmap.Data_p;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	Error = GFX_DrawBmp_NONE_CLUT8(Handle, &TempBmpParams);

	PAL_OSD_FreeBitmap(&TempBitmap);
	return Error;
}

#define GFX_ENABLE_PNG_CACHE		1/*是否使能png 缓存，使能png 缓存后会使得画图快很多*/
#if (TH_USE_OS21 && TH_STRONG_9610)
#define GFX_PNG_CACHE_SIZE			8388608
#else
#define GFX_PNG_CACHE_SIZE			16777216 /*4M bytes, png 最大缓存空间，如果空间不够将针优先释放占用空间最小的
										png 图片，因为小图片就算不缓存也较快*/
#endif
typedef struct
{
	U8 *PngData_p;
	U32	PngDataLen;
	U32 CurrPos;
}PngRead_t;

#if (GFX_ENABLE_PNG_CACHE)

typedef struct png_cache_s
{
	struct png_cache_s	*prev;
	struct png_cache_s	*next;
	
	PngRead_t			PngData;/*png 原始数据*/
	PAL_OSD_Bitmap_t	PngBitmap;/*png 对应的解析好的位图*/
	U32					ReferCount;
	U32					Priority;
	BOOL				Force;
}GFX_PngCache_t;

static GFX_PngCache_t		*PngCacheList_p;
static U32				PngCachedSize;
static TH_Semaphore_t		*PngCacheAccess_p;

void GFX_PngCacheInit(void)
{
	PngCacheList_p = NULL;
	PngCachedSize = 0;
	PngCacheAccess_p = THOS_SemaphoreCreate(1);
}

void GFX_PngCacheTerm(void)
{
	GFX_PngCache_t *CacheItem_p, *NextItem_p;
	
	THOS_SemaphoreWait(PngCacheAccess_p);
	CacheItem_p = PngCacheList_p;
	while(CacheItem_p)
	{
		NextItem_p = (GFX_PngCache_t *)(CacheItem_p->next);
		PAL_OSD_FreeBitmap(&(CacheItem_p->PngBitmap));
		GFX_FREE(CacheItem_p);
		CacheItem_p = NextItem_p;
	}
	PngCacheList_p = NULL;
	PngCachedSize = 0;
	THOS_SemaphoreSignal(PngCacheAccess_p);
	
	THOS_SemaphoreDelete(PngCacheAccess_p);
}

void GFX_PngCacheAdd(PngRead_t *PngData_p, PAL_OSD_Bitmap_t *PngBitmap_p, BOOL Force)
{
	GFX_PngCache_t *CacheItem_p, *MinItem_p;
#if 0
	if((PngBitmap_p->Width + PngBitmap_p->Height) <= 50)/*150+150*/
	{
		PAL_OSD_FreeBitmap(PngBitmap_p);/*small pic, no need to cache*/
		return ;
	}
#endif
	THOS_SemaphoreWait(PngCacheAccess_p);

check:
	if((PngCachedSize+PngBitmap_p->Size) > GFX_PNG_CACHE_SIZE)
	{
		//printf("png data more than max cache size %d %d\n", PngCachedSize, PngBitmap_p->Size);
		CacheItem_p = PngCacheList_p;
		if(CacheItem_p)
		{
			while(CacheItem_p->next)
			{
				CacheItem_p = CacheItem_p->next;
			}
		}
		MinItem_p = NULL;
		while(CacheItem_p)
		{
			if(MinItem_p == NULL)
			{
				if(CacheItem_p->ReferCount == 0)
				{
					MinItem_p = CacheItem_p;
				}
			}
			else
			{
				if((MinItem_p->PngBitmap.Width + MinItem_p->PngBitmap.Height) > (CacheItem_p->PngBitmap.Width + CacheItem_p->PngBitmap.Height) &&\
					CacheItem_p->ReferCount == 0)
				{
					MinItem_p = CacheItem_p;
				}
			}
			CacheItem_p = (GFX_PngCache_t *)(CacheItem_p->prev);
		}
		if(MinItem_p == NULL)
		{
			THOS_SemaphoreSignal(PngCacheAccess_p);
			PAL_OSD_FreeBitmap(PngBitmap_p);
			//printf("no free png cache mem\n");
			return ;
		}
		
		if(MinItem_p->prev == NULL)
		{
			PngCacheList_p = (GFX_PngCache_t *)(MinItem_p->next);
		}
		else
		{
			MinItem_p->prev->next = MinItem_p->next;
		}
		if(MinItem_p->next != NULL)
		{
			MinItem_p->next->prev = MinItem_p->prev;
		}
		
		PngCachedSize -= MinItem_p->PngBitmap.Size;
		PAL_OSD_FreeBitmap(&(MinItem_p->PngBitmap));
		GFX_FREE(MinItem_p);
		goto check;
	}
	CacheItem_p = GFX_MALLOC(sizeof(GFX_PngCache_t));
	if(CacheItem_p == NULL)
	{
		THOS_SemaphoreSignal(PngCacheAccess_p);
		return ;
	}
	
	CacheItem_p->prev = NULL;
	CacheItem_p->PngBitmap = *PngBitmap_p;
	CacheItem_p->PngData = *PngData_p;
	CacheItem_p->ReferCount = 0;
	CacheItem_p->Priority = 5;
	CacheItem_p->next = PngCacheList_p;
	CacheItem_p->Force = Force;
	if(CacheItem_p->next)
	{
		CacheItem_p->next->prev = CacheItem_p;
	}
	PngCacheList_p = CacheItem_p;
	
	PngCachedSize += PngBitmap_p->Size;
	
//	printf("cached png size %d\n", PngCachedSize);
	THOS_SemaphoreSignal(PngCacheAccess_p);
}

GFX_PngCache_t *GFX_PngCacheGet(PngRead_t *PngData_p)
{
	GFX_PngCache_t *CacheItem_p;
	
	THOS_SemaphoreWait(PngCacheAccess_p);

	CacheItem_p = PngCacheList_p;
	
	while(CacheItem_p)
	{
		if(CacheItem_p->PngData.PngData_p == PngData_p->PngData_p)
		{
			CacheItem_p->ReferCount ++;
			CacheItem_p->Priority ++;
			/*移动到队列头*/
			if(CacheItem_p->prev != NULL)
			{
				CacheItem_p->prev->next = CacheItem_p->next;
				if(CacheItem_p->next != NULL)
				{
					CacheItem_p->next->prev = CacheItem_p->prev;
				}
				CacheItem_p->prev = NULL;
				CacheItem_p->next = PngCacheList_p;
				CacheItem_p->next->prev = CacheItem_p;
				PngCacheList_p = CacheItem_p;
			}

			THOS_SemaphoreSignal(PngCacheAccess_p);
			return CacheItem_p;
		}
		CacheItem_p = (GFX_PngCache_t *)(CacheItem_p->next);
	}
	
	THOS_SemaphoreSignal(PngCacheAccess_p);
	return (GFX_PngCache_t *)NULL;
}

void GFX_PngCachePut(GFX_PngCache_t *PngCache_p)
{
	THOS_SemaphoreWait(PngCacheAccess_p);

	PngCache_p->ReferCount --;
	
	THOS_SemaphoreSignal(PngCacheAccess_p);
}

void GFX_PngCacheFree(BOOL Force)
{
	GFX_PngCache_t *CacheItem_p, *NextItem_p;

	printf("*********************************try to free all cache bmp\n");
	THOS_SemaphoreWait(PngCacheAccess_p);

	CacheItem_p = PngCacheList_p;
	
	while(CacheItem_p)
	{
		NextItem_p = (GFX_PngCache_t *)(CacheItem_p->next);
		if(CacheItem_p->ReferCount == 0)
		{
			if(CacheItem_p->Force == FALSE || Force == TRUE)
			{
				if(CacheItem_p->prev == NULL)
				{
					PngCacheList_p = (GFX_PngCache_t *)(CacheItem_p->next);
				}
				else
				{
					CacheItem_p->prev->next = CacheItem_p->next;
				}
				if(CacheItem_p->next != NULL)
				{
					CacheItem_p->next->prev = CacheItem_p->prev;
				}
				
				PngCachedSize -= CacheItem_p->PngBitmap.Size;
				PAL_OSD_FreeBitmap(&(CacheItem_p->PngBitmap));
				GFX_FREE(CacheItem_p);
			}
		}
		CacheItem_p = NextItem_p;
	}
	THOS_SemaphoreSignal(PngCacheAccess_p);
}

#endif

static void GFX_WinZoom(PAL_OSD_Win_t *src, PAL_OSD_Win_t *dest)
{
	int zoom_width, zoom_height;
	
	if(src->Width > dest->Width || src->Height > dest->Height)
	{
		zoom_width = src->Width;
		zoom_height = src->Height;
		if(src->Width > dest->Width)
		{
			zoom_width = dest->Width;
			zoom_height = src->Height * dest->Width / src->Width;
		}
		if(zoom_height > dest->Height)
		{
			zoom_width = zoom_width * dest->Height / zoom_height;
			zoom_height = dest->Height;
		}
		if(dest->Width > zoom_width)
		{
			dest->LeftX += ((dest->Width-zoom_width)>>1);
		}
		else if(dest->Width < zoom_width)
		{
			dest->LeftX -= ((zoom_width-dest->Width)>>1);
		}
		dest->Width = zoom_width;
		if(dest->Height > zoom_height)
		{
			dest->TopY += ((dest->Height-zoom_height)>>1);
		}
		else if(dest->Height < zoom_height)
		{
			dest->TopY -= ((zoom_height-dest->Height)>>1);
		}
		dest->Height = zoom_height;
	}
	else if(src->Width < dest->Width || src->Height < dest->Height)
	{
		zoom_width = dest->Width;
		zoom_height = src->Height * dest->Width / src->Width;
		if(zoom_height > dest->Height)
		{
			zoom_width = zoom_width * dest->Height / zoom_height;
			zoom_height = dest->Height;
		}
		if(dest->Width > zoom_width)
		{
			dest->LeftX += ((dest->Width-zoom_width)>>1);
		}
		else if(dest->Width < zoom_width)
		{
			dest->LeftX -= ((zoom_width-dest->Width)>>1);
		}
		dest->Width = zoom_width;
		if(dest->Height > zoom_height)
		{
			dest->TopY += ((dest->Height-zoom_height)>>1);
		}
		else if(dest->Height < zoom_height)
		{
			dest->TopY -= ((zoom_height-dest->Height)>>1);
		}
		dest->Height = zoom_height;
	}
}
#if (GFX_ENABLE_PNG)

static void qt_png_warning(png_structp png_ptr, png_const_charp message)
{
    printf("libpng warning: %s", message);
}

static void iod_read_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
	PngRead_t	*PngRead_p = (PngRead_t*)png_get_io_ptr(png_ptr);
	if(PngRead_p->CurrPos + length > PngRead_p->PngDataLen)
	{
		length = PngRead_p->PngDataLen - PngRead_p->CurrPos;
		memcpy(data, PngRead_p->PngData_p + PngRead_p->CurrPos, length);
		PngRead_p->CurrPos += length;
		png_error(png_ptr, "Read Error");
	}
	else
	{
		memcpy(data, PngRead_p->PngData_p + PngRead_p->CurrPos, length);
		PngRead_p->CurrPos += length;
	}
	return;
}

TH_Error_t  GFX_DrawBmp_Png(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t TempBitmap;
	PAL_OSD_Win_t DestWin, SrcWin;
#if (GFX_ENABLE_PNG_CACHE)
	GFX_PngCache_t *PngCache_p;
#endif
	
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;
	png_bytep* row_pointers = NULL;
	PngRead_t	PngRead;
	U32				pitch;
	void 			*temp_pointer;
	png_uint_32 width;
	png_uint_32 height;
	int bit_depth, y;
	int color_type;

	PngRead.PngData_p = (U8 *)(BmpParams_p->BmpData_p);
	PngRead.PngDataLen = BmpParams_p->BmpDataCompSize;
	PngRead.CurrPos = 0;
	if(BmpParams_p->ForceCache)
	{
		PngRead.PngData_p = (U8 *)(BmpParams_p->Key);
	}
#if (GFX_ENABLE_PNG_CACHE)
	PngCache_p = NULL;
	if(BmpParams_p->EnableKeyColor == FALSE || BmpParams_p->ForceCache)
	{
		PngCache_p = GFX_PngCacheGet(&PngRead);
	}
	if(PngCache_p == NULL)
#endif
	{
		PngRead.PngData_p = (U8 *)(BmpParams_p->BmpData_p);
		PngRead.PngDataLen = BmpParams_p->BmpDataCompSize;
		PngRead.CurrPos = 0;
		memset(&TempBitmap, 0, sizeof(TempBitmap));
		png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,0,0,0);
		if (!png_ptr)
		{
			printf("GFX_DrawBmp_Png %d\n", __LINE__);
			return TH_ERROR_BAD_PARAM;
		}

		png_set_error_fn(png_ptr, 0, 0, qt_png_warning);

		info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr)
		{
			png_destroy_read_struct(&png_ptr, 0, 0);
			printf("GFX_DrawBmp_Png %d\n", __LINE__);
			return TH_ERROR_BAD_PARAM;
		}

		end_info = png_create_info_struct(png_ptr);
		if (!end_info)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, 0);
			printf("GFX_DrawBmp_Png %d\n", __LINE__);
			return TH_ERROR_BAD_PARAM;
		}
		if (setjmp( png_jmpbuf(png_ptr) ))
		{
			if(TempBitmap.Data_p)
			{
				PAL_OSD_FreeBitmap(&TempBitmap);
			}
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			printf("GFX_DrawBmp_Png %d\n", __LINE__);
			return TH_ERROR_BAD_PARAM;
		}/**/
		png_set_read_fn(png_ptr, (void*)(&PngRead), iod_read_fn);
		png_read_info(png_ptr, info_ptr);
		
		png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
					0, 0, 0);
		if((width * height) > 921600)
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			printf("GFX_DrawBmp_Png %d too big\n", __LINE__);
			return TH_ERROR_NO_MEM;
		}
       //调色板格式的png图片，转化为RGB888的像素格式
        // force palette images to be expanded to 24-bit RGB
        // it may include alpha channel
        if (color_type == PNG_COLOR_TYPE_PALETTE)
        {
            png_set_palette_to_rgb(png_ptr);
        }
        //像素格式少于1字节长度的灰度图，将其转为每像素占1字节的像素格式
        // low-bit-depth grayscale images are to be expanded to 8 bits
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        {
            bit_depth = 8;
        //    png_set_gray_1_2_4_to_8(png_ptr);
            png_set_gray_to_rgb(png_ptr);
        }
        //将tRNS块数据信息扩展为完整的ALPHA通道信息 
        // expand any tRNS chunk data into a full alpha channel
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        {
            png_set_tRNS_to_alpha(png_ptr);
        }  
	#if 0
		if ( color_type == PNG_COLOR_TYPE_GRAY )
		{
			/*单色格式*/
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			return TH_ERROR_NOT_SUPPORT;
		}
		else if ( color_type == PNG_COLOR_TYPE_PALETTE
				&& png_get_valid(png_ptr, info_ptr, PNG_INFO_PLTE)
				&& info_ptr->num_palette <= 256 )
		{
			/*色板格式*/
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			return TH_ERROR_NOT_SUPPORT;
		} 
		else
	#endif
		{
			// 32-bit
			if ( bit_depth == 16 )
			{
				png_set_strip_16(png_ptr);
			}
			png_set_expand(png_ptr);

			if ( color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
				png_set_gray_to_rgb(png_ptr);
			
			TempBitmap.Width = width;
			TempBitmap.Height = height;
			TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
			TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;
			Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, FALSE);
			if(Error != TH_NO_ERROR)
			{
				png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
				printf("GFX_DrawBmp_Png %d %d\n", __LINE__, Error);
				return TH_ERROR_NO_MEM;
			}

			// Only add filler if no alpha, or we can get 5 channel data.
			if (!(color_type & PNG_COLOR_MASK_ALPHA)
			&& !png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
				png_set_filler(png_ptr, GFX_ALPHA_MAX,PNG_FILLER_AFTER);
			// We want 4 bytes, but it isn't an alpha channel
			}
			png_set_bgr(png_ptr);
		//	png_set_swap_alpha(png_ptr);
			png_read_update_info(png_ptr, info_ptr);
		}

		row_pointers = (png_bytep*)THOS_Malloc(sizeof(png_bytep) * height);
		if (row_pointers == NULL)
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
			png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
			printf("GFX_DrawBmp_Png %d %d\n", __LINE__, Error);
			return TH_ERROR_NO_MEM;
		}
		temp_pointer = TempBitmap.Data_p;
		pitch = TempBitmap.Pitch;

		for (y=0; y<height; y++) {
			row_pointers[y] = (png_bytep)temp_pointer;
			temp_pointer += pitch;
		}

		png_read_image(png_ptr, row_pointers);

		THOS_Free(row_pointers);
		png_read_end(png_ptr, end_info);
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		
		SrcWin.LeftX = 0;
		SrcWin.TopY = 0;
		SrcWin.Width = TempBitmap.Width;
		SrcWin.Height = TempBitmap.Height;
		switch(BmpParams_p->PutType)
		{
			case MID_GFX_BMP_PUT_TILE:
			case MID_GFX_BMP_PUT_CENTER:
				if(BmpParams_p->DestWidth== 0 || TempBitmap.Width >= BmpParams_p->DestWidth)
				{
					DestWin.LeftX = BmpParams_p->DestLeftX;
					if(BmpParams_p->DestWidth == 0)
					{
						BmpParams_p->DestWidth = TempBitmap.Width;
					}
					DestWin.Width = BmpParams_p->DestWidth;
				}
				else
				{
					DestWin.LeftX = BmpParams_p->DestLeftX + ((BmpParams_p->DestWidth - TempBitmap.Width) >> 1);
					DestWin.Width = TempBitmap.Width;
				}
				
				if(BmpParams_p->DestHeight== 0 || TempBitmap.Height >= BmpParams_p->DestHeight)
				{
					DestWin.TopY = BmpParams_p->DestTopY;
					if(BmpParams_p->DestHeight == 0)
					{
						BmpParams_p->DestHeight = TempBitmap.Height;
					}
					DestWin.Height = BmpParams_p->DestHeight;
				}
				else
				{
					DestWin.TopY = BmpParams_p->DestTopY+ ((BmpParams_p->DestHeight - TempBitmap.Height) >> 1);
					DestWin.Height = TempBitmap.Height;
				}
				break;
			case MID_GFX_BMP_PUT_RATIO_ZOOM:
				DestWin.LeftX = BmpParams_p->DestLeftX;
				DestWin.TopY = BmpParams_p->DestTopY;
				if(BmpParams_p->DestWidth== 0)
				{
					BmpParams_p->DestWidth = TempBitmap.Width;
				}
				if(BmpParams_p->DestHeight == 0)
				{
					BmpParams_p->DestHeight = TempBitmap.Height;
				}
				DestWin.Width = BmpParams_p->DestWidth;
				DestWin.Height = BmpParams_p->DestHeight;
				GFX_WinZoom(&SrcWin, &DestWin);
				break;
			default:
			case MID_GFX_BMP_PUT_ZOOM:
				DestWin.LeftX = BmpParams_p->DestLeftX;
				DestWin.TopY = BmpParams_p->DestTopY;
				if(BmpParams_p->DestWidth== 0)
				{
					BmpParams_p->DestWidth = TempBitmap.Width;
				}
				if(BmpParams_p->DestHeight == 0)
				{
					BmpParams_p->DestHeight = TempBitmap.Height;
				}
				DestWin.Width = BmpParams_p->DestWidth;
				DestWin.Height = BmpParams_p->DestHeight;
				break;
		}

		if(BmpParams_p->Mix && !PAL_OSD_CheckAlpha(Handle, &DestWin))
		{
			Error = PAL_OSD_PutAlpha(Handle, &DestWin, &SrcWin, &TempBitmap);
		}
		else
		{
			Error = PAL_OSD_Put(Handle, &DestWin, &SrcWin, &TempBitmap);
		}
	
		if(BmpParams_p->EnableKeyColor == FALSE || BmpParams_p->ForceCache)
		{
			if(BmpParams_p->ForceCache)
			{
				PngRead.PngData_p = (U8 *)(BmpParams_p->Key);
			}
		#if (GFX_ENABLE_PNG_CACHE)
			GFX_PngCacheAdd(&PngRead, &TempBitmap, FALSE);
		#else
			PAL_OSD_FreeBitmap(&TempBitmap);
		#endif
		}
		else
		{
			PAL_OSD_FreeBitmap(&TempBitmap);
		}
	}
#if (GFX_ENABLE_PNG_CACHE)
	else
	{
		SrcWin.LeftX = 0;
		SrcWin.TopY = 0;
		SrcWin.Width = PngCache_p->PngBitmap.Width;
		SrcWin.Height = PngCache_p->PngBitmap.Height;
		switch(BmpParams_p->PutType)
		{
			case MID_GFX_BMP_PUT_TILE:
			case MID_GFX_BMP_PUT_CENTER:
				if(BmpParams_p->DestWidth== 0 || PngCache_p->PngBitmap.Width >= BmpParams_p->DestWidth)
				{
					DestWin.LeftX = BmpParams_p->DestLeftX;
					if(BmpParams_p->DestWidth == 0)
					{
						BmpParams_p->DestWidth = PngCache_p->PngBitmap.Width;
					}
					DestWin.Width = BmpParams_p->DestWidth;
				}
				else
				{
					DestWin.LeftX = BmpParams_p->DestLeftX + ((BmpParams_p->DestWidth - PngCache_p->PngBitmap.Width) >> 1);
					DestWin.Width = PngCache_p->PngBitmap.Width;
				}
				
				if(BmpParams_p->DestHeight== 0 || PngCache_p->PngBitmap.Height >= BmpParams_p->DestHeight)
				{
					DestWin.TopY = BmpParams_p->DestTopY;
					if(BmpParams_p->DestHeight == 0)
					{
						BmpParams_p->DestHeight = PngCache_p->PngBitmap.Height;
					}
					DestWin.Height = BmpParams_p->DestHeight;
				}
				else
				{
					DestWin.TopY = BmpParams_p->DestTopY+ ((BmpParams_p->DestHeight - PngCache_p->PngBitmap.Height) >> 1);
					DestWin.Height = PngCache_p->PngBitmap.Height;
				}
				break;
			case MID_GFX_BMP_PUT_RATIO_ZOOM:
				DestWin.LeftX = BmpParams_p->DestLeftX;
				DestWin.TopY = BmpParams_p->DestTopY;
				if(BmpParams_p->DestWidth== 0)
				{
					BmpParams_p->DestWidth = PngCache_p->PngBitmap.Width;
				}
				if(BmpParams_p->DestHeight == 0)
				{
					BmpParams_p->DestHeight = PngCache_p->PngBitmap.Height;
				}
				DestWin.Width = BmpParams_p->DestWidth;
				DestWin.Height = BmpParams_p->DestHeight;
				GFX_WinZoom(&SrcWin, &DestWin);
				break;
			default:
			case MID_GFX_BMP_PUT_ZOOM:
				DestWin.LeftX = BmpParams_p->DestLeftX;
				DestWin.TopY = BmpParams_p->DestTopY;
				if(BmpParams_p->DestWidth== 0)
				{
					BmpParams_p->DestWidth = PngCache_p->PngBitmap.Width;
				}
				if(BmpParams_p->DestHeight == 0)
				{
					BmpParams_p->DestHeight = PngCache_p->PngBitmap.Height;
				}
				DestWin.Width = BmpParams_p->DestWidth;
				DestWin.Height = BmpParams_p->DestHeight;
				break;
		}

		if(BmpParams_p->Mix && !PAL_OSD_CheckAlpha(Handle, &DestWin))
		{
			Error = PAL_OSD_PutAlpha(Handle, &DestWin, &SrcWin, &(PngCache_p->PngBitmap));
		}
		else
		{
			Error = PAL_OSD_Put(Handle, &DestWin, &SrcWin, &(PngCache_p->PngBitmap));
		}

		GFX_PngCachePut(PngCache_p);
	}
#endif
	return Error;
}
#endif

#if (GFX_ENABLE_JPEG)

typedef struct
{
	U8 *Data_p;
	U32 DataLen;
	U32 CurrPos;
}JPEG_DATA_T;

typedef struct
{
	struct jpeg_source_mgr src_mgr;
	JPEG_DATA_T	 jpeg_data;
	JOCTET buffer[10];
}JPEG_SOURCE_MGR_T;

typedef struct
{
    struct jpeg_error_mgr error_mgr;
    jmp_buf setjmp_buffer;
}JPEG_ERROR_MRG_T;

static
void jpeg_init_source(j_decompress_ptr temp)
{
}

static
boolean jpeg_fill_input_buffer(j_decompress_ptr cinfo)
{
	int num_read, rest;
	JPEG_SOURCE_MGR_T* src = (JPEG_SOURCE_MGR_T *)(cinfo->src);
	JPEG_DATA_T* dev = &(src->jpeg_data);
	src->src_mgr.next_input_byte = dev->Data_p+dev->CurrPos;
	rest = dev->DataLen - dev->CurrPos;
	num_read = rest;
	/*
	if(rest > 4096)
	{
		memcpy(src->buffer, dev->Data_p+dev->CurrPos, 4096);
		num_read = 4096;
	}
	else
	{
		memcpy(src->buffer, dev->Data_p+dev->CurrPos, rest);
		num_read = rest;
	}*/
	dev->CurrPos += num_read;
	if ( num_read <= 0 )
	{
		// Insert a fake EOI marker - as per jpeglib recommendation
		src->buffer[0] = (JOCTET) 0xFF;
		src->buffer[1] = (JOCTET) JPEG_EOI;
		src->src_mgr.next_input_byte = src->buffer;
		src->src_mgr.bytes_in_buffer = 2;
	}
	else
	{
		src->src_mgr.bytes_in_buffer = num_read;
	}
	return TRUE;
}

static
void jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    JPEG_SOURCE_MGR_T* src = (JPEG_SOURCE_MGR_T*)(cinfo->src);

    // `dumb' implementation from jpeglib

    /* Just a dumb implementation for now.  Could use fseek() except
     * it doesn't work on pipes.  Not clear that being smart is worth
     * any trouble anyway --- large skips are infrequent.
     */
    if (num_bytes > 0) {
	while (num_bytes > (long) src->src_mgr.bytes_in_buffer) {
	    num_bytes -= (long) src->src_mgr.bytes_in_buffer;
	    (void) jpeg_fill_input_buffer(cinfo);
	    /* note we assume that qt_fill_input_buffer will never return FALSE,
	    * so suspension need not be handled.
	    */
	}
	src->src_mgr.next_input_byte += (size_t) num_bytes;
	src->src_mgr.bytes_in_buffer -= (size_t) num_bytes;
    }
}

static
void jpeg_term_source(j_decompress_ptr temp)
{
}

static
void jpeg_source_mgr_init(JPEG_SOURCE_MGR_T* iioptr, U8 *JpegData_p, U32 JpegDataLen)
{
	iioptr->jpeg_data.Data_p = JpegData_p;
	iioptr->jpeg_data.DataLen = JpegDataLen;
	iioptr->jpeg_data.CurrPos = 0;
	iioptr->src_mgr.init_source = jpeg_init_source;
	iioptr->src_mgr.fill_input_buffer = jpeg_fill_input_buffer;
	iioptr->src_mgr.skip_input_data = jpeg_skip_input_data;
	iioptr->src_mgr.resync_to_restart = jpeg_resync_to_restart;
	iioptr->src_mgr.term_source = jpeg_term_source;
	iioptr->src_mgr.bytes_in_buffer = 0;
	iioptr->src_mgr.next_input_byte = iioptr->buffer;
}

static
void jpeg_error_exit (j_common_ptr cinfo)
{
    JPEG_ERROR_MRG_T* myerr = (JPEG_ERROR_MRG_T *) (cinfo->err);
    char buffer[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, buffer);
    printf("%s\n", buffer);
    longjmp(myerr->setjmp_buffer, 1);
}

TH_Error_t  GFX_DrawBmp_Jpeg(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	struct jpeg_decompress_struct cinfo;

	JPEG_SOURCE_MGR_T iod_src;
	JPEG_ERROR_MRG_T jerr;
	JSAMPARRAY				Buffer;
//	JDIMENSION				Buffer_height;
//	JSAMPROW				RowBuffer;
	PAL_OSD_Bitmap_t 		TempBitmap;
	BOOL					Success = FALSE;
#if (GFX_ENABLE_PNG_CACHE)
	GFX_PngCache_t *PngCache_p;
#endif
	PngRead_t	PngRead;
	PAL_OSD_Win_t DestWin, SrcWin;

	PngRead.PngData_p = (U8 *)(BmpParams_p->BmpData_p);
	PngRead.PngDataLen = BmpParams_p->BmpDataCompSize;
	PngRead.CurrPos = 0;
	if(BmpParams_p->ForceCache)
	{
		PngRead.PngData_p = (U8 *)(BmpParams_p->Key);
	}
#if (GFX_ENABLE_PNG_CACHE)
	PngCache_p = NULL;
	if(BmpParams_p->EnableKeyColor == FALSE || BmpParams_p->ForceCache)
	{
		PngCache_p = GFX_PngCacheGet(&PngRead);
	}
	if(PngCache_p == NULL)
#endif
	{
		memset(&TempBitmap, 0, sizeof(TempBitmap));
	#if 1/*disable this because video mem can not use when playing*/
		if(GFX_EnableHwJpg)
		{
			PAL_OSD_Bitmap_t 		VBTempBitmap;
			memset(&VBTempBitmap, 0, sizeof(VBTempBitmap));
		#if 0
			if(((BmpParams_p->DestWidth == 0 &&\
				BmpParams_p->DestHeight == 0) ||\
				BmpParams_p->PutType == MID_GFX_BMP_PUT_RATIO_ZOOM) &&\
				cinfo.image_width < 1280 && cinfo.image_height < 720)
			{
				if(PAL_OSD_HwJpegDecode(&VBTempBitmap, (U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize) != NULL)
				{
					if(BmpParams_p->EnableKeyColor == FALSE || BmpParams_p->ForceCache)
					{
						SrcWin.LeftX = 0;
						SrcWin.TopY = 0;
						SrcWin.Width = VBTempBitmap.Width;
						SrcWin.Height = VBTempBitmap.Height;
						TempBitmap.Width = VBTempBitmap.Width;
						TempBitmap.Height = VBTempBitmap.Height;
						TempBitmap.ColorType = VBTempBitmap.ColorType;
						TempBitmap.AspectRatio = VBTempBitmap.AspectRatio;
						Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, FALSE);
						if(Error != TH_NO_ERROR)
						{
							BmpParams_p->EnableKeyColor = TRUE;
							BmpParams_p->ForceCache = FALSE;
							TempBitmap = VBTempBitmap;
						}
						else
						{
							PAL_OSD_Copy(&TempBitmap, &SrcWin, &VBTempBitmap, &SrcWin);
							PAL_OSD_FreeBitmap(&VBTempBitmap);
						}
					}
					else
					{
						TempBitmap = VBTempBitmap;
					}
					Success = TRUE;
					goto Success_draw;
				}
				else
				{
					printf("[GFX]: FAILED: PAL_OSD_HwJpegDecode:\n");
				}
			}
			else
			{
				if(PAL_OSD_HwJpegDecodeEx(&VBTempBitmap, (U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize, BmpParams_p->DestWidth, BmpParams_p->DestHeight) != NULL)
				{
					if(BmpParams_p->EnableKeyColor == FALSE || BmpParams_p->ForceCache)
					{
						SrcWin.LeftX = 0;
						SrcWin.TopY = 0;
						SrcWin.Width = VBTempBitmap.Width;
						SrcWin.Height = VBTempBitmap.Height;
						TempBitmap.Width = VBTempBitmap.Width;
						TempBitmap.Height = VBTempBitmap.Height;
						TempBitmap.ColorType = VBTempBitmap.ColorType;
						TempBitmap.AspectRatio = VBTempBitmap.AspectRatio;
						Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, FALSE);
						if(Error != TH_NO_ERROR)
						{
							BmpParams_p->EnableKeyColor = TRUE;
							BmpParams_p->ForceCache = FALSE;
							TempBitmap = VBTempBitmap;
						}
						else
						{
							PAL_OSD_Copy(&TempBitmap, &SrcWin, &VBTempBitmap, &SrcWin);
							PAL_OSD_FreeBitmap(&VBTempBitmap);
						}
					}
					else
					{
						TempBitmap = VBTempBitmap;
					}

					Success = TRUE;
					goto Success_draw;
				}
				else
				{
					printf("[GFX]: FAILED: PAL_OSD_HwJpegDecodeEx:\n");
				}
			}
		#endif
		}
	#endif
		jpeg_source_mgr_init(&iod_src, (U8 *)(BmpParams_p->BmpData_p), BmpParams_p->BmpDataCompSize);
		
		cinfo.err = jpeg_std_error(&(jerr.error_mgr));
		jerr.error_mgr.error_exit = jpeg_error_exit;
		
		memset(&TempBitmap, 0, sizeof(TempBitmap));

		if (!setjmp(jerr.setjmp_buffer))
		{
			unsigned int i;
			
			jpeg_create_decompress(&cinfo);
			
			cinfo.src = &(iod_src.src_mgr);
			
			(void) jpeg_read_header(&cinfo, TRUE);
			cinfo.dct_method = JDCT_IFAST;
			cinfo.out_color_space = JCS_EXT_BGR;
			cinfo.scale_num = 1;
			if((BmpParams_p->EnableKeyColor == FALSE ||\
				BmpParams_p->DestWidth == 0 ||\
				BmpParams_p->DestHeight == 0 ||\
				BmpParams_p->PutType == MID_GFX_BMP_PUT_RATIO_ZOOM) &&\
				cinfo.image_width <= 1280 && cinfo.image_height <= 720)
			{
				cinfo.scale_denom = 1;
			}
			else
			{
				if(cinfo.image_width >= BmpParams_p->DestWidth*8)
				{
					cinfo.scale_denom = 8;
				}
				else if(cinfo.image_width >= BmpParams_p->DestWidth*4)
				{
					cinfo.scale_denom = 4;
				}
				else if(cinfo.image_width >= BmpParams_p->DestWidth*2)
				{
					cinfo.scale_denom = 2;
				}
				else
				{
					cinfo.scale_denom = 1;
				}

				if(cinfo.image_height >= BmpParams_p->DestHeight*8)
				{
					if(cinfo.scale_denom < 8)
					{
						cinfo.scale_denom = 8;
					}
				}
				else if(cinfo.image_height >= BmpParams_p->DestHeight*4)
				{
					if(cinfo.scale_denom < 4)
					{
						cinfo.scale_denom = 4;
					}
				}
				else if(cinfo.image_height >= BmpParams_p->DestHeight*2)
				{
					if(cinfo.scale_denom < 2)
					{
						cinfo.scale_denom = 2;
					}
				}
				else
				{
					if(cinfo.scale_denom < 1)
					{
						cinfo.scale_denom = 1;
					}
				}
				
				printf("jpeg draw, height :%d, scale:%d\n", cinfo.image_height,cinfo.scale_denom);
			}
			
	//		cinfo.dither_mode = JDITHER_NONE;
	//		cinfo.do_fancy_upsampling = FALSE;
	//		cinfo.two_pass_quantize = FALSE;
			(void) jpeg_start_decompress(&cinfo);
			if ( cinfo.output_components == 3)
			{
				TempBitmap.Width = cinfo.output_width;
				TempBitmap.Height = cinfo.output_height;
				TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_RGB888;
				TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;
				Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, FALSE);
				if(Error == TH_NO_ERROR)
				{
					Success = TRUE;
					printf("pic width : %d\n", TempBitmap.Width);
					printf("pic height : %d\n", TempBitmap.Height);
					printf("pic Pitch : %d\n", TempBitmap.Pitch);
					printf("pic ColorType : %d\n", TempBitmap.ColorType);
				}
			}
			else if (cinfo.output_components == 4)
			{
				TempBitmap.Width = cinfo.output_width;
				TempBitmap.Height = cinfo.output_height;
				TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
				TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;
				Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, FALSE);
				if(Error == TH_NO_ERROR)
				{
					Success = TRUE;
					printf("pic width : %d\n", TempBitmap.Width);
					printf("pic height : %d\n", TempBitmap.Height);
					printf("pic Pitch : %d\n", TempBitmap.Pitch);
					printf("pic ColorType : %d\n", TempBitmap.ColorType);
				}
			}
			else if ( cinfo.output_components == 1 )
			{
				printf("Unsupported format\n");
			}
			else
			{
				printf("Unsupported format\n");
				// Unsupported format
			}
			if (Success)
			{
				//void *Temp;
				TH_Clock_t Time;
				JSAMPARRAY row_buffer;	/* Output row buffer */
				int row_stride;		/* physical row width in output buffer */

			 /*	U32   bmpFile_p=0;            datafile descriptor         
				U32   offset = 0;
				U8 BmpHeader[0x36]={0x42, 0x4d, 0x36, 0xfc, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
									0x00, 0x00, 0xd0, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
									0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
									0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; */
				/* Open and Read file into memory 
				bmpFile_p = fopen("radio.bmp", "wb");
				fwrite(BmpHeader, 1, 0x36, bmpFile_p);
				offset = 0x36 + 575 * Bitmap.Pitch;*/
				Time = THOS_TimeNow();
				Buffer = (JSAMPARRAY)THOS_Malloc(sizeof(JSAMPROW) * cinfo.output_height);
			//	RowBuffer = (JSAMPROW)THOS_Malloc(TempBitmap.Pitch);
				Buffer[0] = TempBitmap.Data_p;
				for(i=1; i<cinfo.output_height; i++)
				{
					Buffer[i] = Buffer[i-1] + TempBitmap.Pitch;
				}
				
				/* JSAMPLEs per row in output buffer */
				row_stride = cinfo.output_width * cinfo.output_components;
				/* Make a one-row-high sample array that will go away when done with image */
				row_buffer = (*cinfo.mem->alloc_sarray)
					((j_common_ptr) & cinfo, JPOOL_IMAGE, row_stride, 1);
				while (cinfo.output_scanline < cinfo.output_height)
				{
				//	Temp = Buffer[cinfo.output_scanline];
				/*	jpeg_read_scanlines(&cinfo,
											&(Buffer[cinfo.output_scanline]),
											cinfo.output_height - cinfo.output_scanline);
				*/
					(void)jpeg_read_scanlines(&cinfo, row_buffer, 1);
					/* Assume put_scanline_someplace wants a pointer and sample count. */

					memcpy(Buffer[cinfo.output_scanline-1],
							row_buffer[0],
							row_stride);

				//	memcpy(Temp, RowBuffer, TempBitmap.Pitch);
				//	STAVMEM_CopyBlock1D(RowBuffer, Temp, Bitmap.Pitch);
				/*	fseek(bmpFile_p,offset,0);
					fwrite(RowBuffer, 1, (size_t) Bitmap.Pitch, bmpFile_p);
					offset -= Bitmap.Pitch;*/
				}
				
				Time = THOS_TimeMinus(THOS_TimeNow(), Time);
			//	fclose(bmpFile_p);

				(void) jpeg_finish_decompress(&cinfo);
			
			//	THOS_Free(RowBuffer);
				THOS_Free(Buffer);
				printf("waste time %lld\n", Time/(THOS_GetClocksPerSecond()/1000));
			}
		}
		jpeg_destroy_decompress(&cinfo);
Success_draw:
		if(Success)
		{
			SrcWin.LeftX = 0;
			SrcWin.TopY = 0;
			SrcWin.Width = TempBitmap.Width;
			SrcWin.Height = TempBitmap.Height;
			switch(BmpParams_p->PutType)
			{
				case MID_GFX_BMP_PUT_TILE:
				case MID_GFX_BMP_PUT_CENTER:
					if(BmpParams_p->DestWidth== 0 || TempBitmap.Width >= BmpParams_p->DestWidth)
					{
						DestWin.LeftX = BmpParams_p->DestLeftX;
						if(BmpParams_p->DestWidth == 0)
						{
							BmpParams_p->DestWidth = TempBitmap.Width;
						}
						DestWin.Width = BmpParams_p->DestWidth;
					}
					else
					{
						DestWin.LeftX = BmpParams_p->DestLeftX + ((BmpParams_p->DestWidth - TempBitmap.Width) >> 1);
						DestWin.Width = TempBitmap.Width;
					}
					
					if(BmpParams_p->DestHeight== 0 || TempBitmap.Height >= BmpParams_p->DestHeight)
					{
						DestWin.TopY = BmpParams_p->DestTopY;
						if(BmpParams_p->DestHeight == 0)
						{
							BmpParams_p->DestHeight = TempBitmap.Height;
						}
						DestWin.Height = BmpParams_p->DestHeight;
					}
					else
					{
						DestWin.TopY = BmpParams_p->DestTopY+ ((BmpParams_p->DestHeight - TempBitmap.Height) >> 1);
						DestWin.Height = TempBitmap.Height;
					}
					break;
				case MID_GFX_BMP_PUT_RATIO_ZOOM:
					DestWin.LeftX = BmpParams_p->DestLeftX;
					DestWin.TopY = BmpParams_p->DestTopY;
					if(BmpParams_p->DestWidth== 0)
					{
						BmpParams_p->DestWidth = TempBitmap.Width;
					}
					if(BmpParams_p->DestHeight == 0)
					{
						BmpParams_p->DestHeight = TempBitmap.Height;
					}
					DestWin.Width = BmpParams_p->DestWidth;
					DestWin.Height = BmpParams_p->DestHeight;
					GFX_WinZoom(&SrcWin, &DestWin);
					break;
				default:
				case MID_GFX_BMP_PUT_ZOOM:
					DestWin.LeftX = BmpParams_p->DestLeftX;
					DestWin.TopY = BmpParams_p->DestTopY;
					if(BmpParams_p->DestWidth== 0)
					{
						BmpParams_p->DestWidth = TempBitmap.Width;
					}
					if(BmpParams_p->DestHeight == 0)
					{
						BmpParams_p->DestHeight = TempBitmap.Height;
					}
					DestWin.Width = BmpParams_p->DestWidth;
					DestWin.Height = BmpParams_p->DestHeight;
					break;
			}
			Error = PAL_OSD_Put(Handle, &DestWin, &SrcWin, &TempBitmap);
			
			if(BmpParams_p->EnableKeyColor == FALSE || BmpParams_p->ForceCache)
			{
			#if (GFX_ENABLE_PNG_CACHE)
				GFX_PngCacheAdd(&PngRead, &TempBitmap, TRUE);
			#else
				PAL_OSD_FreeBitmap(&TempBitmap);
			#endif
			}
			else
			{
				PAL_OSD_FreeBitmap(&TempBitmap);
			}
		}
	}
#if (GFX_ENABLE_PNG_CACHE)
	else
	{
		SrcWin.LeftX = 0;
		SrcWin.TopY = 0;
		SrcWin.Width = PngCache_p->PngBitmap.Width;
		SrcWin.Height = PngCache_p->PngBitmap.Height;
		switch(BmpParams_p->PutType)
		{
			case MID_GFX_BMP_PUT_TILE:
			case MID_GFX_BMP_PUT_CENTER:
				if(BmpParams_p->DestWidth== 0 || PngCache_p->PngBitmap.Width >= BmpParams_p->DestWidth)
				{
					DestWin.LeftX = BmpParams_p->DestLeftX;
					if(BmpParams_p->DestWidth == 0)
					{
						BmpParams_p->DestWidth = PngCache_p->PngBitmap.Width;
					}
					DestWin.Width = BmpParams_p->DestWidth;
				}
				else
				{
					DestWin.LeftX = BmpParams_p->DestLeftX + ((BmpParams_p->DestWidth - PngCache_p->PngBitmap.Width) >> 1);
					DestWin.Width = PngCache_p->PngBitmap.Width;
				}
				
				if(BmpParams_p->DestHeight== 0 || PngCache_p->PngBitmap.Height >= BmpParams_p->DestHeight)
				{
					DestWin.TopY = BmpParams_p->DestTopY;
					if(BmpParams_p->DestHeight == 0)
					{
						BmpParams_p->DestHeight = PngCache_p->PngBitmap.Height;
					}
					DestWin.Height = BmpParams_p->DestHeight;
				}
				else
				{
					DestWin.TopY = BmpParams_p->DestTopY+ ((BmpParams_p->DestHeight - PngCache_p->PngBitmap.Height) >> 1);
					DestWin.Height = PngCache_p->PngBitmap.Height;
				}
				break;
			case MID_GFX_BMP_PUT_RATIO_ZOOM:
				DestWin.LeftX = BmpParams_p->DestLeftX;
				DestWin.TopY = BmpParams_p->DestTopY;
				if(BmpParams_p->DestWidth== 0)
				{
					BmpParams_p->DestWidth = PngCache_p->PngBitmap.Width;
				}
				if(BmpParams_p->DestHeight == 0)
				{
					BmpParams_p->DestHeight = PngCache_p->PngBitmap.Height;
				}
				DestWin.Width = BmpParams_p->DestWidth;
				DestWin.Height = BmpParams_p->DestHeight;
				GFX_WinZoom(&SrcWin, &DestWin);
				break;
			default:
			case MID_GFX_BMP_PUT_ZOOM:
				DestWin.LeftX = BmpParams_p->DestLeftX;
				DestWin.TopY = BmpParams_p->DestTopY;
				if(BmpParams_p->DestWidth== 0)
				{
					BmpParams_p->DestWidth = PngCache_p->PngBitmap.Width;
				}
				if(BmpParams_p->DestHeight == 0)
				{
					BmpParams_p->DestHeight = PngCache_p->PngBitmap.Height;
				}
				DestWin.Width = BmpParams_p->DestWidth;
				DestWin.Height = BmpParams_p->DestHeight;
				break;
		}
		Error = PAL_OSD_Put(Handle, &DestWin, &SrcWin, &(PngCache_p->PngBitmap));

		GFX_PngCachePut(PngCache_p);
	}
#endif

	return Error;
}
#endif

#if (GFX_ENABLE_GIF)
#define GifByteType	ByteType
#define GifRowType	RowType
#define GifPixelType	PixelType
#define GIF_ERROR	ERROR

GifFileType *DGifOpenFileName(char *FileName);
int DGifGetRecordType(GifFileType * GifFile, GifRecordType * Type);
int DGifGetImageDesc(GifFileType * GifFile);
int DGifGetLine(GifFileType * GifFile, PixelType * Line, int LineLen);
int DGifGetExtension(GifFileType * GifFile, int *ExtCode, ByteType ** Extension);
int DGifGetExtensionNext(GifFileType * GifFile, ByteType ** Extension);
int DGifCloseFile(GifFileType * GifFile);
/*
int readFunc(GifFileType* GifFile, GifByteType* buf, int count)
{
    char* ptr = GifFile->UserData;
    memcpy(buf, ptr, count);
    GifFile->UserData = ptr + count;
    return count;
}

*/
TH_Error_t  GFX_DrawBmp_Gif(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	S32	i, j, Row, Col, Width, Height, ExtCode, Count;
    GifRecordType RecordType;
    GifByteType *Extension;
    GifFileType *GifFile;
    GifColorType *ColorMap;
    PAL_OSD_Bitmap_t 		TempBitmap;
	PAL_OSD_Win_t DestWin, SrcWin;
	GifRowType *ScreenBuffer;
	GifRowType GifRow;
	GifColorType *ColorMapEntry;
	U8 *BufferP, *Temp_p;
	
	memset(&TempBitmap, 0, sizeof(TempBitmap));

	if(BmpParams_p->BmpDataCompSize == 0)
	{
	    GifFile = DGifOpenFileName(BmpParams_p->BmpData_p);
		if (GifFile == NULL)
		{
			PrintGifError();
			return FALSE;
		}
	}
	else
	{
		GifFile = gif_header_parse((char *)(BmpParams_p->BmpData_p));
		if(NULL == GifFile)
		{
			return FALSE;
		}
	}

	ScreenBuffer = (GifRowType *) THOS_Malloc(GifFile->SHeight * sizeof(GifRowType *));
	if (ScreenBuffer == NULL)
	{
		printf("Allocate Memory Wrong!!\n");
		Error = TH_ERROR_BIOS_ERROR;
		goto xit1;
	}
	
	ScreenBuffer[0] = (GifRowType) THOS_Malloc(GifFile->SWidth * sizeof(GifPixelType));
	if (ScreenBuffer[0] == NULL)
	{
		printf("Allocate Memory Wrong!!\n");
		THOS_Free(ScreenBuffer);
		Error = TH_ERROR_BIOS_ERROR;
		goto xit1;
	}
	
	for (i = 0; i < GifFile->SWidth; i++)
	{
		ScreenBuffer[0][i] = GifFile->SBackGroundColor;
	}

	for (i = 1; i < GifFile->SHeight; i++) 
	{
		ScreenBuffer[i] = (GifRowType) THOS_Malloc(GifFile->SWidth * sizeof(GifPixelType));
		if (ScreenBuffer[i] == NULL)
		{
			printf("Allocate Memory Wrong!!\n");
			for (j = 0;j < i;j++)
			{
				THOS_Free(ScreenBuffer[j]);
			}
			THOS_Free(ScreenBuffer);
			Error = TH_ERROR_BIOS_ERROR;
			goto xit1;
		}

		memcpy(ScreenBuffer[i], ScreenBuffer[0], GifFile->SWidth * sizeof(GifPixelType));
	}
	
	do 
	 {
		 if (DGifGetRecordType(GifFile, &RecordType) == GIF_ERROR) 
		 {
			 PrintGifError();
			 Error = TH_ERROR_BIOS_ERROR;
			 goto xit;
		 }
		 switch (RecordType) 
		 {
			 case IMAGE_DESC_RECORD_TYPE:
				 if (DGifGetImageDesc(GifFile) == GIF_ERROR) 
				 {
					 PrintGifError();
					 Error = TH_ERROR_BIOS_ERROR;
			 		 goto xit;
				 }
		 
				 Row = GifFile->ITop;
				 Col = GifFile->ILeft;
				 Width = GifFile->IWidth;
				 Height = GifFile->IHeight;

				 if (GifFile->ILeft + GifFile->IWidth > GifFile->SWidth ||
						GifFile->ITop + GifFile->IHeight > GifFile->SHeight) 
				 {
					 printf("Pic Size Or Position Wrong!!\n");
					 Error = TH_ERROR_BIOS_ERROR;
			 		 goto xit;
				 }
				 if (GifFile->IInterlace) 
				 {
					 for (Count = i = 0; i < 4; i++)
					 for (j = Row + InterlacedOffset[i]; j < Row + Height;
								  j += InterlacedJumps[i]) 
					  {
						 if (DGifGetLine(GifFile, &ScreenBuffer[j][Col],Width) == GIF_ERROR) 
						 {
							 PrintGifError();
							 Error = TH_ERROR_BIOS_ERROR;
							 goto xit;
						 }
					 }
				 }
				 else 
				 {
					 for (i = 0; i < Height; i++) 
					 {
						 if (DGifGetLine(GifFile, &ScreenBuffer[Row++][Col],Width) == GIF_ERROR) 
						 {
							 PrintGifError();
							 Error = TH_ERROR_BIOS_ERROR;
							 goto xit;
						 }
					 }
				 }
				 break;
			 case EXTENSION_RECORD_TYPE:
				 if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR) 
				 {
					 PrintGifError();
					 Error = TH_ERROR_BIOS_ERROR;
			 		 goto xit;
				 }
				 while (Extension != NULL) 
				 {
					 if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR) 
					 {
						 PrintGifError();
						 Error = TH_ERROR_BIOS_ERROR;
				 		 goto xit;
					 }
				 }
				 break;
			 case TERMINATE_RECORD_TYPE:
				 break;
			 default:
				 break;
		 }
	 } while (RecordType != TERMINATE_RECORD_TYPE);

    ColorMap = (GifFile->IColorMap ? GifFile->IColorMap : GifFile->SColorMap);
    if (ColorMap == NULL) 
    {
        printf("ColorMap Wrong!!\n");
		Error = TH_ERROR_BIOS_ERROR;
		goto xit;
    }
	
	TempBitmap.Width = GifFile->SWidth;
	TempBitmap.Height = GifFile->SHeight;
	TempBitmap.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
	TempBitmap.AspectRatio = PAL_OSD_ASPECT_RATIO_4TO3;
	Error = PAL_OSD_AllocBitmap(&TempBitmap, NULL, FALSE);
	if(Error == TH_NO_ERROR)
	{
		printf("pic Swidth : %d\n", TempBitmap.Width);
		printf("pic Sheight : %d\n", TempBitmap.Height);
		printf("pic Pitch : %d\n", TempBitmap.Pitch);
		printf("pic ColorType : %d\n", TempBitmap.ColorType);
	}
	
	Temp_p = (U8 *)TempBitmap.Data_p;

    for (i = 0; i < GifFile->SHeight; i++) 
    {
	    GifRow = ScreenBuffer[i];
	    BufferP = Temp_p;
	    for (j = 0; j < GifFile->SWidth; j++) 
	    {
		    ColorMapEntry = &(ColorMap[GifRow[j]]);
		    *BufferP++ = ColorMapEntry->Blue;
		    *BufferP++ = ColorMapEntry->Green;
		    *BufferP++ = ColorMapEntry->Red;
			*BufferP++ = 0xFF;
	    }
		
		Temp_p += TempBitmap.Pitch;
    }

	DestWin.LeftX = BmpParams_p->DestLeftX;
	DestWin.TopY = BmpParams_p->DestTopY;
	if(BmpParams_p->DestWidth== 0)
	{
		BmpParams_p->DestWidth = TempBitmap.Width;
	}
	if(BmpParams_p->DestHeight == 0)
	{
		BmpParams_p->DestHeight = TempBitmap.Height;
	}
	
	DestWin.Width = BmpParams_p->DestWidth;
	DestWin.Height = BmpParams_p->DestHeight;
	SrcWin.LeftX = 0;
	SrcWin.TopY = 0;
	SrcWin.Width = TempBitmap.Width;
	SrcWin.Height = TempBitmap.Height;
	Error = PAL_OSD_Put(Handle, &DestWin, &SrcWin, &TempBitmap);
	PAL_OSD_FreeBitmap(&TempBitmap);
	
xit:
	for (j = 0;j < GifFile->SHeight;j++)
	{
		THOS_Free(ScreenBuffer[j]);
	}
	
	THOS_Free(ScreenBuffer);
	
xit1:
	
	DGifCloseFile(GifFile);
	return Error;
	
}

MID_GFX_GifParams_t *MID_GFX_GifLoad(const char *GifData_p, const int GifSize)
{
	MID_GFX_GifParams_t *GifParams_p;
    GifFileType *GifFile;
	int i, j, Len = 0;
	U32 nClut = 0, nTransColor = 0, enabeltran = 0, img_delay_ms = 0;
	U32 flagTransColorFlag = 0, UserInput = 0, DisposalMethod = 1;
	MID_GFX_GifImage *pGifImag = NULL, *pPreImg = NULL, *pNewImg = NULL;
	ByteType *Extension = NULL;
	int ExtCode = 0;
	unsigned char *pData = NULL;
	GifColorType *pClut = NULL;
	GifRecordType RecordType;

	GifParams_p = (MID_GFX_GifParams_t *) THOS_Malloc(sizeof(MID_GFX_GifParams_t));
	if(NULL == GifParams_p)
	{
		return NULL;
	}
	
	memset(GifParams_p, 0, sizeof(MID_GFX_GifParams_t));

	if(GifSize == 0)
	{
	    GifFile = DGifOpenFileName(GifData_p);
		if (GifFile == NULL)
		{
			PrintGifError();
			GFX_FREE(GifParams_p);
			return NULL;
		}
	}
	else
	{
		GifFile = gif_header_parse((char *)GifData_p);
		if(NULL == GifFile)
		{
			GFX_FREE(GifParams_p);
			return NULL;
		}
	}

	/*Basic arguments configure*/
	GifParams_p->bpp		   = GifFile->SBitsPerPixel;
	GifParams_p->screen_width  = GifFile->SWidth;
	GifParams_p->screen_height = GifFile->SHeight;

	/*Global color look-up table*/
	if(GifFile->SColorMap)
	{
		Len = 1 << GifFile->SBitsPerPixel;
		GifParams_p->num_color = Len;

		GifParams_p->pal = (unsigned int *)THOS_Malloc(GifParams_p->num_color * sizeof(unsigned int));
		for(i = 0; i < Len; i += 4) {
			for(j = 0; j < 4 && j < Len; j++) {
				nClut = ((GifFile->SColorMap[i + j].Red) << 16) |
					((GifFile->SColorMap[i + j].Green) << 8) |
					((GifFile->SColorMap[i + j].Blue));

				GifParams_p->pal[i + j] = nClut;
			}
		}
	}

	do {
		if(GifGetRecordType(GifFile, &RecordType) == ERROR) {
			goto err;
		}

		switch (RecordType) {
			case IMAGE_DESC_RECORD_TYPE:
				pNewImg = (MID_GFX_GifImage *)THOS_Malloc(sizeof(MID_GFX_GifImage));
				if(NULL == pNewImg) {
					goto err;
				}
				memset(pNewImg, 0, sizeof(MID_GFX_GifImage));
/*
				if(GifParams_p->each_image) {
					pNewImg->img_trans = nTransColor;
					pNewImg->enable_trans = enabeltran;
					pNewImg->img_delay_ms = img_delay_ms;
					nTransColor = 0;
					enabeltran = 0;
					printf("img_trans 0x%08X\n", pNewImg->img_trans);
				}
				else
				{
					printf("get new frame\n");
				}
				nTransColor = 0;
				img_delay_ms = 0;
				*/

				pPreImg = pGifImag = GifParams_p->each_image;
				while(pGifImag) {
					pPreImg = pGifImag;
					pGifImag = pGifImag->next;
				}
				if(pPreImg)
					pPreImg->next = pNewImg;
				else
					GifParams_p->each_image = pNewImg;
				
				pNewImg->img_trans = nTransColor;
				pNewImg->enable_trans = enabeltran;
				pNewImg->img_delay_ms = img_delay_ms;
				pNewImg->DisposalMethod = DisposalMethod;

				nTransColor = 0;
				enabeltran = 0;
				img_delay_ms = 0;
				DisposalMethod = 1;
				if(ERROR == GifGetImageDesc(GifFile)) {
					goto err;
				}
				/*Parse local palette in each image*/
				if(GifFile->IColorMap) {
					Len = 1 << GifFile->IBitsPerPixel;

					pNewImg->num_color = Len;
					if(pNewImg->num_color) {
						pNewImg->img_pal = (unsigned int *)THOS_Malloc(sizeof(unsigned int) * Len);
						if(NULL == pNewImg->img_pal) {
							goto err;
						}
						memset(pNewImg->img_pal, 0, sizeof(unsigned int) * Len);

						for (i = 0; i < Len; i+=4) {
							for (j = 0; j < 4 && j < Len; j++) {
								nClut = ((GifFile->IColorMap[i + j].Red) << 16) |
									((GifFile->IColorMap[i + j].Green) << 8) |
									((GifFile->IColorMap[i + j].Blue));

								pNewImg->img_pal[i + j] = nClut;
							}
						}
					}
				}

				/*Parse pixel information in each image*/
				pNewImg->img_left = GifFile->ILeft;
				pNewImg->img_top = GifFile->ITop;
				pNewImg->img_width = GifFile->IWidth;
				pNewImg->img_height = GifFile->IHeight;
				pNewImg->img_data = (unsigned char *)THOS_Malloc(GifFile->IWidth *
						GifFile->IHeight *
						sizeof(unsigned char));
				if(NULL == pNewImg->img_data) {
						goto err;
				}

				memset(pNewImg->img_data,
						0,
						GifFile->IWidth * GifFile->IHeight * sizeof(unsigned char));

				for(i = 0; i < GifFile->IHeight; i++) {
					pData = pNewImg->img_data + i * GifFile->IWidth;
					if(GifGetLine(GifFile, pData, GifFile->IWidth) == ERROR) {
						goto err;
					}
				}
				break;
			case EXTENSION_RECORD_TYPE:
				if (GifGetExtension(GifFile, &ExtCode, &Extension) == ERROR) {
						goto err;
				}
				if(ExtCode == 0xF9) {
					
					flagTransColorFlag  =  Extension[1] & 0x01;
					UserInput 			= (Extension[1] >> 1) & 0x01;
					DisposalMethod      = (Extension[1] >> 2) & 0x07;
					
					//printf("Extension[1] : %x , TransColorFlag : 0x%x, UserInput : 0x%x, DisposalMethod: 0x%x\n",
					//			Extension[1], flagTransColorFlag, UserInput, DisposalMethod);

					if((Extension[1] & 0x01) == 1)  //Transport Color Flag
					{
						if(GifFile->IColorMap)/*I think here is bug, because here the pal is for prev image not for next*/
							pClut = GifFile->IColorMap;
						else
							pClut = GifFile->SColorMap;
						nClut = ((pClut[Extension[4]].Red) << 16) |
								((pClut[Extension[4]].Green) << 8) |
								(pClut[Extension[4]].Blue);
						nTransColor = Extension[4];//Extension[4];//nClut;
						enabeltran = 1;
					}
					
					img_delay_ms = ((Extension[3]<<8)|Extension[2])*10;
					//printf("Control Extension : 0x%x : 0x%x : 0x%x : 0x%x : 0x%x\n",
					//			Extension[0], Extension[1], Extension[2], Extension[3], Extension[4]);
				}

				while (Extension != NULL) 
				{
					if (GifGetExtensionNext(GifFile, &Extension) == ERROR) {
						goto err;
					}
				}
				break;
				
			case TERMINATE_RECORD_TYPE:
				printf("TERMINATE_RECORD_TYPE\n");
				break;
			default:			 /* Should be traps by GxDGifGetRecordType */
				break;
		}
	}while(RecordType != TERMINATE_RECORD_TYPE);
	
	DGifCloseFile(GifFile);
	return GifParams_p;

err:
	pGifImag = GifParams_p->each_image;
	while(pGifImag)
	{
		pPreImg = pGifImag->next;
		THOS_Free(pGifImag->img_pal);
		THOS_Free(pGifImag->img_data);
		THOS_Free(pGifImag);
		pGifImag = pPreImg;
	}
	
	if(GifParams_p->pal)
	{
		THOS_Free(GifParams_p->pal);
	}
	
	THOS_Free(GifParams_p);
	
	DGifCloseFile(GifFile);
	return NULL;
}

void MID_GFX_GifFree(MID_GFX_GifParams_t **GifParams_pp)
{
	MID_GFX_GifImage *pGifImag, *pNextImg;

	pGifImag = (*GifParams_pp)->each_image;
	while(pGifImag)
	{
		pNextImg = pGifImag->next;
		THOS_Free(pGifImag->img_pal);
		THOS_Free(pGifImag->img_data);
		THOS_Free(pGifImag);
		pGifImag = pNextImg;
	}
	
	if((*GifParams_pp)->pal)
	{
		THOS_Free((*GifParams_pp)->pal);
	}
	
	THOS_Free((*GifParams_pp));
	(*GifParams_pp) = NULL;
}

#endif

TH_Error_t  GFX_DrawBmp(PAL_OSD_Handle  Handle, MID_GFX_PutBmp_t *BmpParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	
	switch(BmpParams_p->CompressType)
	{
		case MID_GFX_COMPRESS_RLE:
			switch(BmpParams_p->ColorType)
			{
				case PAL_OSD_COLOR_TYPE_ARGB8888:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_RLE_ARGB_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_RLE_ARGB(Handle, BmpParams_p);
					}
					break;
				case PAL_OSD_COLOR_TYPE_RGB888:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_RLE_RGB_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_RLE_RGB(Handle, BmpParams_p);
					}
					break;
				case PAL_OSD_COLOR_TYPE_CLUT8:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_RLE_CLUT8_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_RLE_CLUT8(Handle, BmpParams_p);
					}
					break;
				default :
					Error = TH_ERROR_NOT_SUPPORT;
					break;
			}
			break;
	#if (GFX_ENABLE_LZO_BMP)
		case MID_GFX_COMPRESS_LZO:
			switch(BmpParams_p->ColorType)
			{
				case PAL_OSD_COLOR_TYPE_ARGB8888:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_LZO_ARGB_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_LZO_ARGB(Handle, BmpParams_p);
					}
					break;
				case PAL_OSD_COLOR_TYPE_RGB888:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_LZO_RGB_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_LZO_RGB(Handle, BmpParams_p);
					}
					break;
				case PAL_OSD_COLOR_TYPE_CLUT8:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_LZO_CLUT8_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_LZO_CLUT8(Handle, BmpParams_p);
					}
					break;
				default :
					Error = TH_ERROR_NOT_SUPPORT;
					break;
			}
			break;
	#endif
		case MID_GFX_COMPRESS_RLE_2:
			switch(BmpParams_p->ColorType)
			{
				case PAL_OSD_COLOR_TYPE_ARGB8888:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_RLE2_ARGB_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_RLE2_ARGB(Handle, BmpParams_p);
					}
					break;
				case PAL_OSD_COLOR_TYPE_RGB888:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_RLE2_RGB_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_RLE2_RGB(Handle, BmpParams_p);
					}
					break;
				case PAL_OSD_COLOR_TYPE_CLUT8:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_RLE2_CLUT8_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_RLE2_CLUT8(Handle, BmpParams_p);
					}
					break;
				default :
					Error = TH_ERROR_NOT_SUPPORT;
					break;
			}
			break;
		#if (GFX_ENABLE_PNG)
		case MID_GFX_COMPRESS_PNG:
			Error = GFX_DrawBmp_Png(Handle, BmpParams_p);
			break;
		#endif
		#if (GFX_ENABLE_JPEG)
		case MID_GFX_COMPRESS_JPEG:
			Error = GFX_DrawBmp_Jpeg(Handle, BmpParams_p);
			break;
		#endif
	#if 0 //(GFX_ENABLE_GIF)
		case MID_GFX_COMPRESS_GIF:
			Error = GFX_DrawBmp_Gif(Handle, BmpParams_p);
			break;
	#endif
		default:
			switch(BmpParams_p->ColorType)
			{
				case PAL_OSD_COLOR_TYPE_ARGB8888:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_NONE_ARGB_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_NONE_ARGB(Handle, BmpParams_p);
					}
					break;
				case PAL_OSD_COLOR_TYPE_RGB888:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_NONE_RGB_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_NONE_RGB(Handle, BmpParams_p);
					}
					break;
				case PAL_OSD_COLOR_TYPE_CLUT8:
					if(BmpParams_p->EnableKeyColor)
					{
						Error = GFX_DrawBmp_NONE_CLUT8_KeyColor(Handle, BmpParams_p);
					}
					else
					{
						Error = GFX_DrawBmp_NONE_CLUT8(Handle, BmpParams_p);
					}
					break;
				default :
					Error = TH_ERROR_NOT_SUPPORT;
					break;
			}
			break;
	}

	return Error;
}

/********************************************************************
建造UTF-8 字符串管理结构
*********************************************************************/
GFX_Utf8_t *GFX_ConstructUtf8(U8 *Utf8Str_p)
{
	int i;
	GFX_Utf8_t *Utf8_p;
	Utf8_p = GFX_MALLOC(sizeof(GFX_Utf8_t));
	if(Utf8_p == NULL)
	{
		return Utf8_p;
	}
	Utf8_p->Str_p = Utf8Str_p;
	Utf8_p->Len = strlen((char *)Utf8Str_p);
	Utf8_p->CurrPos = 0;
	Utf8_p->Count  = 0;
	for(i=0; i<Utf8_p->Len; )
	{
		if(Utf8_p->Str_p[i] < 0x80)
		{
			if(Utf8_p->Str_p[i] == 0x0D)
			{
				if(i+1 < Utf8_p->Len)
				{
					if(Utf8_p->Str_p[i+1] == 0x0A)
					{
						i += 2;
						Utf8_p->Count ++;
						continue;
					}
				}
			}
				
			if(Utf8_p->Str_p[i] < 0x20 && Utf8_p->Str_p[i] != 0x0A)
			{
				i ++;
				continue;
			}
			i ++;
			Utf8_p->Count ++;
		}
		else if(Utf8_p->Str_p[i] < 0xE0)
		{
			i += 2;
			Utf8_p->Count ++;
		}
		else
		{
			i += 3;
			Utf8_p->Count ++;
		}
	}
	return Utf8_p;
}

/********************************************************************
释放UTF-8 字符串管理结构
*********************************************************************/
void GFX_DestructUtf8(GFX_Utf8_t **Utf8_pp)
{
	if(Utf8_pp == NULL || (*Utf8_pp) == NULL)
	{
		return ;
	}
	GFX_FREE(*Utf8_pp);
	*Utf8_pp = NULL;
}

/********************************************************************
读取UTF-8 的当前字符的Unicode, 并且根据Step 来决定是否
将当前字符定位到下一个字符
*********************************************************************/
U16 GFX_Utf8Read(GFX_Utf8_t *Utf8_p, BOOL Step)
{
	U16 Unicode;

again:
	if(Utf8_p->CurrPos >= Utf8_p->Len)
	{
		return 0;
	}
	if(Utf8_p->Str_p[Utf8_p->CurrPos] == 0x00)
	{
		Unicode = Utf8_p->Str_p[Utf8_p->CurrPos];
		if(Step)
		{
			Utf8_p->CurrPos ++;
		}
		return Unicode;
	}

	if(Utf8_p->Str_p[Utf8_p->CurrPos] == 0x0D)
	{
		if((Utf8_p->CurrPos+1) >= Utf8_p->Len)
		{
			return 0;
		}
		Unicode = 0x0A;
		if(Step)
		{
			Utf8_p->CurrPos += 2;
		}
	}
	else if(Utf8_p->Str_p[Utf8_p->CurrPos] < 0x80)
	{
		if(Utf8_p->Str_p[Utf8_p->CurrPos] < 0x20 && Utf8_p->Str_p[Utf8_p->CurrPos] != 0x0A)
		{
			Utf8_p->CurrPos ++;
			goto again;
		}
		Unicode = Utf8_p->Str_p[Utf8_p->CurrPos];
		if(Step)
		{
			Utf8_p->CurrPos ++;
		}
	}
	else if(Utf8_p->Str_p[Utf8_p->CurrPos] < 0xE0)
	{
		if((Utf8_p->CurrPos+1) >= Utf8_p->Len)
		{
			return 0;
		}
		Unicode = ((Utf8_p->Str_p[Utf8_p->CurrPos] & 0x1F) << 6) | (Utf8_p->Str_p[Utf8_p->CurrPos+1] & 0x3F);
		if(Step)
		{
			Utf8_p->CurrPos += 2;
		}
	}
	else
	{
		if((Utf8_p->CurrPos+2) >= Utf8_p->Len)
		{
			return 0;
		}
		Unicode = ((Utf8_p->Str_p[Utf8_p->CurrPos] & 0x0F) << 12) |\
					((Utf8_p->Str_p[Utf8_p->CurrPos+1] & 0x3F) << 6) |\
					(Utf8_p->Str_p[Utf8_p->CurrPos+2] & 0x3F);
		if(Step)
		{
			Utf8_p->CurrPos += 3;
		}
	}
	return Unicode;
}

/********************************************************************
读取UTF-8 的当前字符的前一个字符的Unicode, 
并且根据Step 来决定是否将当前字符定位到前一个字符
*********************************************************************/
U16 GFX_Utf8Prev(GFX_Utf8_t *Utf8_p, BOOL Step)
{
	U16 Unicode;
	
again:
	if(Utf8_p->CurrPos <= 0)
	{
		return 0;
	}
	
	if(Utf8_p->Str_p[Utf8_p->CurrPos-1] == 0x00)
	{
		Unicode = Utf8_p->Str_p[Utf8_p->CurrPos-1];
		if(Step)
		{
			Utf8_p->CurrPos --;
		}
		return Unicode;
	}

	if((Utf8_p->CurrPos > 1) &&\
		Utf8_p->Str_p[Utf8_p->CurrPos-2] == 0x0D)
	{
		Unicode = 0x0A;
		if(Step)
		{
			Utf8_p->CurrPos -= 2;
		}
	}
	else if(Utf8_p->Str_p[Utf8_p->CurrPos-1] < 0x80)/*one byte*/
	{
		if(Utf8_p->Str_p[Utf8_p->CurrPos-1] < 0x20 && Utf8_p->Str_p[Utf8_p->CurrPos-1] != 0x0A)
		{
			Utf8_p->CurrPos --;
			goto again;
		}
		Unicode = Utf8_p->Str_p[Utf8_p->CurrPos-1];
		if(Step)
		{
			Utf8_p->CurrPos --;
		}
	}
	else
	{
		if(Utf8_p->CurrPos <= 1)
		{
			return 0;
		}
		if(Utf8_p->Str_p[Utf8_p->CurrPos-2] >= 0xC0)/*two bytes*/
		{
			Unicode = ((Utf8_p->Str_p[Utf8_p->CurrPos-2] & 0x1F) << 6) | (Utf8_p->Str_p[Utf8_p->CurrPos-1] & 0x3F);
			if(Step)
			{
				Utf8_p->CurrPos -= 2;
			}
		}
		else/*three bytes*/
		{
			if(Utf8_p->CurrPos <= 2)
			{
				return 0;
			}
			Unicode = ((Utf8_p->Str_p[Utf8_p->CurrPos-3] & 0x0F) << 12) |\
						((Utf8_p->Str_p[Utf8_p->CurrPos-2] & 0x3F) << 6) |\
						(Utf8_p->Str_p[Utf8_p->CurrPos-1] & 0x3F);
			if(Step)
			{
				Utf8_p->CurrPos -= 3;
			}
		}
	}
	
	return Unicode;
}

TH_Error_t GFX_GetTextSize(GFX_Utf8_t *Utf8_p, GFX_Font_t *Font_p, MID_GFX_PutText_t *TextParams_p, S32 StartChar, S32 EndChar, 
								S32 *Width_p, S32 *Height_p, S32 *MaxAscent_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	S32 count, CharIndex, Width, Height, CharWidth, CharAscent, CharDescent, MaxAscent, MaxDescent, CharLeft, CharBitW;
	U16 Unicode;
	BOOL MonoBmp = FALSE;
	S32 ExtWidth;
	
	count = StartChar;
	Width = 0;
	Height = 0;
	MaxAscent = 0;
	MaxDescent = 0;
	
	Utf8_p->CurrPos = count;
	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
		MonoBmp = TRUE;
	}
	ExtWidth = 0;
	while(count < EndChar)
	{
		Unicode = GFX_Utf8Read(Utf8_p, TRUE);
		if(Unicode == 0)
		{
			break;
		}
		count = Utf8_p->CurrPos;
		Width -= ExtWidth;
		ExtWidth = 0;
		GFX_LocateGlyphForSize(Font_p, TextParams_p->FontSize, Unicode, MonoBmp, &CharIndex, &CharWidth, &CharLeft, &CharBitW, &CharAscent, &CharDescent);
		if(CharIndex != -1 && CharIndex != 0)
		{
			Width += (CharWidth + TextParams_p->HInterval);
			if(count == StartChar)
			{
				if(CharLeft < 0)
				{
					Width += (-CharLeft);
				}
			}
			if(CharLeft + CharBitW > CharWidth)
			{
				ExtWidth = (CharLeft + CharBitW - CharWidth);
				Width += ExtWidth;
			}
			if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
			{
				Width += 1;
			}
			
			if(MaxAscent < CharAscent)
				MaxAscent = CharAscent;
			if(MaxDescent < CharDescent)
				MaxDescent = CharDescent;
		}
	}

	if(MaxDescent > 0)
	{
		Height = MaxAscent + MaxDescent;
	}
	else
	{
		Height = MaxAscent;
	}
	*MaxAscent_p = MaxAscent;
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)
	{
//		Width += (Height / GFX_ITALIC_PITCH);
		Width += (Height >> 1);
	}
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
//		Width += (GFX_OUTLINE_WIDTH * 2);
//		Height += (GFX_OUTLINE_WIDTH * 2);
		Width += (GFX_OUTLINE_WIDTH << 1);
		Height += (GFX_OUTLINE_WIDTH << 1);
	}

	if(Width_p != NULL)
	{
		*Width_p = Width;
	}
	if(Height_p != NULL)
	{
		*Height_p = Height;
	}
	return Error;
}

BOOL GFX_CheckBreakCode(U16 Unicode)
{
	if(Unicode == 0x20 || Unicode == 0xA0 || Unicode == 0x0D || Unicode == 0x0A)
	{
		return TRUE;
	}
/*	if(Unicode > 0x0700)
	{
		return TRUE;
	}*/
	if(Unicode >= 0x4E00 && Unicode <= 0xA000)
	{
		return TRUE;
	}
	return FALSE;
}

#if (GFX_ENABLE_TTF)
#if (GFX_USE_TTF_SBIT)
TH_Error_t  GFX_GRAY_PutTtfText(PAL_OSD_Bitmap_t *Bitmap_p, GFX_TtfFont_t *TtfFont_p, S32 FontSize, S32 GlyphIndex, S32 LeftX, S32 BaseY, U32 TextColor, MID_GFX_PutText_t *TextParams_p, U16 Unicode, BOOL bArabic)
{
	TH_Error_t Error = TH_NO_ERROR;
	U8 Mask, BoxWidth, BoxHeight;
	U8 *SrcPtr_p, *DestPtr_p, *LinePtr_p;
	U32 *DestPtr_U32p;
	U32 i, j, Temp;
	S32 NewX, NewY;
	
	FTC_FaceID		FaceId;
	FTC_ScalerRec	Scaler;
	FTC_Node	Node = NULL;
	FTC_SBit		Sbit = NULL;
	
	FaceId = (FTC_FaceID)TtfFont_p;
	Scaler.face_id	= FaceId;
	Scaler.width=FontSize;
	Scaler.height=FontSize;
	Scaler.pixel=1;
	Scaler.x_res=0;
	Scaler.y_res=0;
	
	FTC_SBitCache_LookupScaler(Ft_SBmpCache, &Scaler, FT_LOAD_TARGET_NORMAL, GlyphIndex, &Sbit, &Node);
	if(Node == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
	{
		if(bArabic)
		{
		//	LeftX += Sbit->xadvance;
		//	LeftX -= Sbit->width;
		}
	}

	TextColor &= 0x00FFFFFF;
//	DumpSBitStruct(Sbit);

	if(Sbit->format == 2)
	{
		switch(Bitmap_p->ColorType)
		{
			case PAL_OSD_COLOR_TYPE_ARGB8888:
				BoxWidth = Sbit->width;
				BoxHeight = Sbit->height;
				if(bArabic)
				{
					NewX = LeftX + Sbit->left;
				}
				else
				{
					NewX = LeftX + Sbit->left;
				}
				NewY = BaseY - Sbit->top;
				if(NewX < 0)
				{
					printf("get error param x\n");
					NewX = 0;
				}
				if(NewY < 0)
				{
					printf("get error param y\n");
					NewY = 0;
				}
				DestPtr_U32p = (U32 *)(((U8 *)(Bitmap_p->Data_p)) + (NewY * Bitmap_p->Pitch) + (NewX << 2));
				SrcPtr_p = Sbit->buffer;
				LinePtr_p = SrcPtr_p;
				for( i = 0; i <BoxHeight; i++ )
				{
					for( j = 0; j < BoxWidth; j++ )
					{
						if((*SrcPtr_p) != 0)
						{
						#if (GFX_ALPHA_MAX == 0x80)
							Temp = ((*SrcPtr_p)>>1)&0x000000FF;
							Temp += 3;
							if(Temp > 0x80)
							{
								Temp = 0x80;
							}
						#else
							Temp = (*SrcPtr_p)&0x000000FF;
						#endif
							Temp = Temp << 24;
							if((DestPtr_U32p[j] & 0xFF000000) < Temp)
							{
								DestPtr_U32p[j] = TextColor|Temp;
							}
						//	printf("%02X ", Temp);
						}
						else
						{
						//	printf("00 ");
						}
						SrcPtr_p ++;
					}
				//	printf("\n");
					LinePtr_p = LinePtr_p + Sbit->pitch;
					SrcPtr_p = LinePtr_p;
					DestPtr_U32p = (U32 *)(((void *)DestPtr_U32p)+Bitmap_p->Pitch);
				}
				break;
			default :
				Error = TH_ERROR_NOT_SUPPORT;
				break;
		}
	}
	else
	{
		TextColor |= (GFX_ALPHA_MAX<<24);
		switch(Bitmap_p->ColorType)
		{
			case PAL_OSD_COLOR_TYPE_CLUT8:
				BoxWidth = Sbit->width;
				BoxHeight = Sbit->height;
				if(bArabic)
				{
					NewX = LeftX - Sbit->left;
				}
				else
				{
					NewX = LeftX + Sbit->left;
				}
				NewY = BaseY - Sbit->top;
				if(NewX < 0)
				{
					printf("get error param x\n");
					NewX = 0;
				}
				if(NewY < 0)
				{
					printf("get error param y\n");
					NewY = 0;
				}

				DestPtr_p = (U8 *)(((U8 *)Bitmap_p->Data_p) + (NewY * Bitmap_p->Pitch) + NewX);
				SrcPtr_p = Sbit->buffer;
				LinePtr_p = SrcPtr_p;
				for( i = 0; i <BoxHeight; i++ )
				{
					Mask = 0x80;
					for( j = 0; j < BoxWidth; j++ )
					{
						if(((*SrcPtr_p) & Mask) != 0)
						{
							*(DestPtr_p+j) = (U8)TextColor;
						}
						
						if(Mask == 0x01)
						{
							Mask = 0x80;
							SrcPtr_p ++;
						}
						else
						{
							Mask >>= 1;
						}
					}
					LinePtr_p = LinePtr_p + Sbit->pitch;
					SrcPtr_p = LinePtr_p;
					DestPtr_p += Bitmap_p->Pitch;
				}
				break;
			case PAL_OSD_COLOR_TYPE_ARGB8888:
				BoxWidth = Sbit->width;
				BoxHeight = Sbit->height;
				if(bArabic)
				{
					NewX = LeftX - Sbit->left;
				}
				else
				{
					NewX = LeftX + Sbit->left;
				}
				NewY = BaseY - Sbit->top;
				if(NewX < 0)
				{
					printf("get error param x\n");
					NewX = 0;
				}
				if(NewY < 0)
				{
					printf("get error param y\n");
					NewY = 0;
				}
				DestPtr_U32p = (U32 *)(((U8 *)(Bitmap_p->Data_p)) + (NewY * Bitmap_p->Pitch) + (NewX << 2));
				SrcPtr_p = Sbit->buffer;
				LinePtr_p = SrcPtr_p;
				for( i = 0; i <BoxHeight; i++ )
				{
					Mask = 0x80;
					for( j = 0; j < BoxWidth; j++ )
					{
						if(((*SrcPtr_p) & Mask) != 0)
						{
							DestPtr_U32p[j] = TextColor;
						}
						
						if(Mask == 0x01)
						{
							Mask = 0x80;
							SrcPtr_p ++;
						}
						else
						{
							Mask >>= 1;
						}
					}
					LinePtr_p = LinePtr_p + Sbit->pitch;
					SrcPtr_p = LinePtr_p;
					DestPtr_U32p = (U32 *)(((void *)DestPtr_U32p)+Bitmap_p->Pitch);
				}
				break;
			default :
				Error = TH_ERROR_NOT_SUPPORT;
				break;
		}
	}
	
	FTC_Node_Unref(Node, Ft_CacheManager);

	return Error;
}
TH_Error_t  GFX_MONO_PutTtfText(PAL_OSD_Bitmap_t *Bitmap_p, GFX_TtfFont_t *TtfFont_p, S32 FontSize, S32 GlyphIndex, S32 LeftX, S32 BaseY, U32 TextColor, MID_GFX_PutText_t *TextParams_p, U16 Unicode, BOOL bArabic)
{
	TH_Error_t Error = TH_NO_ERROR;
	U8 Mask, BoxWidth, BoxHeight;
	U8 *SrcPtr_p, *DestPtr_p, *LinePtr_p;
	U32 *DestPtr_U32p;
	U32 i, j;
	S32 NewX, NewY;
	
	FTC_FaceID		FaceId;
	FTC_ScalerRec	Scaler;
	FTC_Node	Node = NULL;
	FTC_SBit		Sbit = NULL;

	FaceId = (FTC_FaceID)TtfFont_p;
	Scaler.face_id	= FaceId;
	Scaler.width=FontSize;
	Scaler.height=FontSize;
	Scaler.pixel=1;
	Scaler.x_res=0;
	Scaler.y_res=0;
	
	FTC_SBitCache_LookupScaler(Ft_SBmpCache, &Scaler, FT_LOAD_TARGET_MONO, GlyphIndex, &Sbit, &Node);
	if(Node == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
	{
		if(bArabic)
		{
		//	LeftX += Sbit->xadvance;
		//	LeftX -= Sbit->width;
		}
	}
	TextColor |= (GFX_ALPHA_MAX<<24);

	switch(Bitmap_p->ColorType)
	{
		case PAL_OSD_COLOR_TYPE_CLUT8:
			BoxWidth = Sbit->width;
			BoxHeight = Sbit->height;
			if(bArabic)
			{
				NewX = LeftX + Sbit->left;
			}
			else
			{
				NewX = LeftX + Sbit->left;
			}
			NewY = BaseY - Sbit->top;
			if(NewX < 0)
			{
				printf("get error param x\n");
				NewX = 0;
			}
			if(NewY < 0)
			{
				printf("get error param y\n");
				NewY = 0;
			}

			DestPtr_p = (U8 *)(((U8 *)Bitmap_p->Data_p) + (NewY * Bitmap_p->Pitch) + NewX);
			SrcPtr_p = Sbit->buffer;
			LinePtr_p = SrcPtr_p;
			for( i = 0; i <BoxHeight; i++ )
			{
				Mask = 0x80;
				for( j = 0; j < BoxWidth; j++ )
				{
					if(((*SrcPtr_p) & Mask) != 0)
					{
						*(DestPtr_p+j) = (U8)TextColor;
					}
					
					if(Mask == 0x01)
					{
						Mask = 0x80;
						SrcPtr_p ++;
					}
					else
					{
						Mask >>= 1;
					}
				}
				LinePtr_p = LinePtr_p + Sbit->pitch;
				SrcPtr_p = LinePtr_p;
				DestPtr_p += Bitmap_p->Pitch;
			}
			break;
		case PAL_OSD_COLOR_TYPE_ARGB8888:
			BoxWidth = Sbit->width;
			BoxHeight = Sbit->height;
			if(bArabic)
			{
				NewX = LeftX - Sbit->left;
			}
			else
			{
				NewX = LeftX + Sbit->left;
			}
			NewY = BaseY - Sbit->top;
			if(NewX < 0)
			{
				printf("get error param x\n");
				NewX = 0;
			}
			if(NewY < 0)
			{
				printf("get error param y\n");
				NewY = 0;
			}
			DestPtr_U32p = (U32 *)(((U8 *)(Bitmap_p->Data_p)) + (NewY * Bitmap_p->Pitch) + (NewX << 2));
			SrcPtr_p = Sbit->buffer;
			LinePtr_p = SrcPtr_p;
			for( i = 0; i <BoxHeight; i++ )
			{
				Mask = 0x80;
				for( j = 0; j < BoxWidth; j++ )
				{
					if(((*SrcPtr_p) & Mask) != 0)
					{
						DestPtr_U32p[j] = TextColor;
					}
					
					if(Mask == 0x01)
					{
						Mask = 0x80;
						SrcPtr_p ++;
					}
					else
					{
						Mask >>= 1;
					}
				}
				LinePtr_p = LinePtr_p + Sbit->pitch;
				SrcPtr_p = LinePtr_p;
				DestPtr_U32p = (U32 *)(((void *)DestPtr_U32p)+Bitmap_p->Pitch);
			}
			break;
		default :
			Error = TH_ERROR_NOT_SUPPORT;
			break;
	}
	FTC_Node_Unref(Node, Ft_CacheManager);

	return Error;
}
#else
TH_Error_t  GFX_PutTtfText(PAL_OSD_Bitmap_t *Bitmap_p, GFX_TtfFont_t *TtfFont_p, S32 FontSize, S32 GlyphIndex, S32 LeftX, S32 BaseY, U32 TextColor)
{
	TH_Error_t Error = TH_NO_ERROR;
	U8 Mask, BoxWidth, BoxHeight;
	U8 *SrcPtr_p, *DestPtr_p, *LinePtr_p;
	U32 *DestPtr_U32p;
	U32 i, j;
	S32 NewX, NewY;
	
	FTC_FaceID		FaceId;
	FTC_ScalerRec	Scaler;
	FTC_Node	Node = NULL;
//	FTC_SBit		Sbit;
	FT_Glyph		Glyph;
	FT_BitmapGlyph  Glyph_bitmap;
	BOOL		NeedDone;

	FaceId = (FTC_FaceID)TtfFont_p;
	Scaler.face_id	= FaceId;
	Scaler.width=FontSize;
	Scaler.height=FontSize;
	Scaler.pixel=1;
	Scaler.x_res=0;
	Scaler.y_res=0;

//	FTC_SBitCache_LookupScaler(Ft_SBmpCache, &Scaler, FT_LOAD_MONOCHROME, index, &Sbit, &Node);
	FTC_ImageCache_LookupScaler( Ft_ImageCache,
								&Scaler,
								FT_LOAD_DEFAULT|FT_LOAD_RENDER,
								GlyphIndex,
								&Glyph,
								&Node );
	if(Node == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	if (Glyph->format != FT_GLYPH_FORMAT_BITMAP)
	{
		FT_Glyph_To_Bitmap(&Glyph, FT_RENDER_MODE_MONO, NULL, 0);
		Glyph_bitmap = (FT_BitmapGlyph)Glyph;
		NeedDone = TRUE;
	}
	else
	{
		Glyph_bitmap = (FT_BitmapGlyph)Glyph;
		NeedDone = FALSE;
	}

	
	switch(Bitmap_p->ColorType)
	{
		case PAL_OSD_COLOR_TYPE_CLUT8:
			BoxWidth = Glyph_bitmap->bitmap.width;
			BoxHeight = Glyph_bitmap->bitmap.rows;
			NewX = LeftX + Glyph_bitmap->left;
			NewY = BaseY - Glyph_bitmap->top;

			DestPtr_p = (U8 *)(((U8 *)Bitmap_p->Data_p) + (NewY * Bitmap_p->Pitch) + NewX);
			SrcPtr_p = Glyph_bitmap->bitmap.buffer;
			LinePtr_p = SrcPtr_p;
			for( i = 0; i <BoxHeight; i++ )
			{
				Mask = 0x80;
				for( j = 0; j < BoxWidth; j++ )
				{
					if(((*SrcPtr_p) & Mask) != 0)
					{
						*(DestPtr_p+j) = (U8)TextColor;
					}
					
					if(Mask == 0x01)
					{
						Mask = 0x80;
						SrcPtr_p ++;
					}
					else
					{
						Mask >>= 1;
					}
				}
				SrcPtr_p = LinePtr_p + Glyph_bitmap->bitmap.pitch;
				DestPtr_p += Bitmap_p->Pitch;
			}
			break;
		case PAL_OSD_COLOR_TYPE_ARGB8888:
			break;
			BoxWidth = Glyph_bitmap->bitmap.width;
			BoxHeight = Glyph_bitmap->bitmap.rows;
			NewX = LeftX + Glyph_bitmap->left;
			NewY = BaseY - Glyph_bitmap->top;

			DestPtr_U32p = (U32 *)(((U8 *)Bitmap_p->Data_p) + (NewY * Bitmap_p->Pitch) + (NewX << 2));
			SrcPtr_p = Glyph_bitmap->bitmap.buffer;
			LinePtr_p = SrcPtr_p;
			for( i = 0; i <BoxHeight; i++ )
			{
				Mask = 0x80;
				for( j = 0; j < BoxWidth; j++ )
				{
					if(((*SrcPtr_p) & Mask) != 0)
					{
						*(DestPtr_U32p+j) = TextColor;
					}
					
					if(Mask == 0x01)
					{
						Mask = 0x80;
						SrcPtr_p ++;
					}
					else
					{
						Mask >>= 1;
					}
				}
				SrcPtr_p = LinePtr_p + Glyph_bitmap->bitmap.pitch;
				DestPtr_U32p = (U32 *)(((void *)DestPtr_U32p)+Bitmap_p->Pitch);
			}
			break;
		default :
			Error = TH_ERROR_NOT_SUPPORT;
			break;
	}
	if(NeedDone)
	{
		FT_Done_Glyph(Glyph);
	}
	FTC_Node_Unref(Node, Ft_CacheManager);

	return Error;
}
#endif
#endif

TH_Error_t  GFX_PutText(PAL_OSD_Bitmap_t *Bitmap_p, GFX_Font_t *Font_p, S32 GlyphIndex, BOOL MonoBmp, S32 LeftX, S32 BaseY, S32 FontSize, U32 TextColor, MID_GFX_PutText_t *TextParams_p, GFX_CharData_t *StrData_p)
{
	TH_Error_t Error = TH_NO_ERROR;
#if (GFX_ENABLE_TTF)
	if(Font_p->Type == MID_GFX_FONT_TYPE_TTF)
	{
	#if (GFX_USE_TTF_SBIT)
		if(MonoBmp)
		{
			Error = GFX_MONO_PutTtfText(Bitmap_p, &(Font_p->Font.TtfFont), FontSize, GlyphIndex, LeftX, BaseY, TextColor, TextParams_p, StrData_p->Unicode, StrData_p->Arabic);
		}
		else
		{
			Error = GFX_GRAY_PutTtfText(Bitmap_p, &(Font_p->Font.TtfFont), FontSize, GlyphIndex, LeftX, BaseY, TextColor, TextParams_p, StrData_p->Unicode, StrData_p->Arabic);
		}
	#else
		Error = GFX_PutTtfText(Bitmap_p, &(Font_p->Font.TtfFont), FontSize, GlyphIndex, LeftX, BaseY, TextColor);
	#endif
	}
	else
#endif
	{
		Error = TH_ERROR_NOT_SUPPORT;
	}
	return Error;
}

TH_Error_t  GFX_BoldText(PAL_OSD_Bitmap_t *SrcBitmap_p, PAL_OSD_Bitmap_t *DestBitmap_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	return Error;
}

TH_Error_t  GFX_ZoomText(PAL_OSD_Bitmap_t *SrcBitmap_p, PAL_OSD_Bitmap_t *DestBitmap_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	return Error;
}

TH_Error_t  GFX_OutlineText(PAL_OSD_Bitmap_t *SrcBitmap_p, PAL_OSD_Bitmap_t *DestBitmap_p, PAL_OSD_Color_t *TextColor_p, PAL_OSD_Color_t *LineColor_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	U32 i, j, k, z;
	U8 *SrcPtr_p, *DestPrt_p, *TempPtr_p;
	U32 *SrcPtr_U32p, *DestPrt_U32p, *TempPtr_U32p, TextColorU32, LineColorU32;
	
	switch(SrcBitmap_p->ColorType)
	{
		case PAL_OSD_COLOR_TYPE_CLUT8:
		case PAL_OSD_COLOR_TYPE_ARGB8888:
			break;
		default :
			return TH_ERROR_NOT_SUPPORT;
	}

	switch(SrcBitmap_p->ColorType)
	{
		case PAL_OSD_COLOR_TYPE_CLUT8:
			SrcPtr_p = (U8 *)(SrcBitmap_p->Data_p);
			DestPrt_p = (U8 *)(DestBitmap_p->Data_p);
			for(i=0; i<SrcBitmap_p->Height; i++)
			{
				for(j=0; j<SrcBitmap_p->Width; j++)
				{
					if((*SrcPtr_p) == TextColor_p->Value.CLUT8)
					{
						TempPtr_p = DestPrt_p;
						DestPrt_p[(DestBitmap_p->Pitch<<1)+GFX_OUTLINE_WIDTH] = TextColor_p->Value.CLUT8;
						for(k=0; k<=4; k+=2)
						{
							for(z=0; z<=4; z+=2)
							{
								if(TempPtr_p[z] == 0)
								{
									TempPtr_p[z] = LineColor_p->Value.CLUT8;
								}
							}
							TempPtr_p += DestBitmap_p->Pitch;
						}
					}
					DestPrt_p ++;
					SrcPtr_p ++;
				}
				DestPrt_p += (GFX_OUTLINE_WIDTH << 1);
			}
			break;
		case PAL_OSD_COLOR_TYPE_ARGB8888:
			TextColorU32 = (TextColor_p->Value.ARGB8888.Alpha << 24) | (TextColor_p->Value.ARGB8888.R << 16) |
						(TextColor_p->Value.ARGB8888.G << 8) | (TextColor_p->Value.ARGB8888.B);
//			TextColorU32 = *((U32 *)(&TextColor_p->Value.ARGB8888));
			LineColorU32 = (LineColor_p->Value.ARGB8888.Alpha << 24) | (LineColor_p->Value.ARGB8888.R << 16) |
						(LineColor_p->Value.ARGB8888.G << 8) | (LineColor_p->Value.ARGB8888.B);
//			LineColorU32 = *((U32 *)(&LineColor_p->Value.ARGB8888));
			SrcPtr_U32p = (U32 *)(SrcBitmap_p->Data_p);
			DestPrt_U32p = (U32 *)(DestBitmap_p->Data_p);
			DestPrt_U32p = DestPrt_U32p + (DestBitmap_p->Width<<1) + GFX_OUTLINE_WIDTH;
			
			for(i=0; i<SrcBitmap_p->Height; i++)
			{
				for(j=0; j<SrcBitmap_p->Width; j++)
				{
					if((*SrcPtr_U32p) != 0)
					{
						*DestPrt_U32p = TextColorU32;
						TempPtr_U32p = DestPrt_U32p - (DestBitmap_p->Width<<1) - GFX_OUTLINE_WIDTH;
			//			for(k=0; k<=4; k+=2)
						for(k=0; k<=4; k+=1)
						{
				//			for(z=0; z<=4; z+=2)
							for(z=0; z<=4; z+=1)
							{
								if(TempPtr_U32p[z] == 0)
								{
									TempPtr_U32p[z] = LineColorU32;
								}
							}
				//			TempPtr_U32p += (DestBitmap_p->Width << 1);
							TempPtr_U32p += (DestBitmap_p->Width);
						}
					}
					DestPrt_U32p ++;
					SrcPtr_U32p ++;
				}
				DestPrt_U32p += (GFX_OUTLINE_WIDTH << 1);
			}
			break;
		default:
			break;
	}

	return Error;
}
											
TH_Error_t  GFX_ItalicText(PAL_OSD_Bitmap_t *SrcBitmap_p, PAL_OSD_Bitmap_t *DestBitmap_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	S32 i;
	U8 *SrcData_p, *DestData_p;

	SrcData_p = (U8 *)SrcBitmap_p->Data_p;
	DestData_p = (U8 *)DestBitmap_p->Data_p;
	
	switch(SrcBitmap_p->ColorType)
	{
		case PAL_OSD_COLOR_TYPE_CLUT8:
			for(i=0; i<SrcBitmap_p->Height; i++)
			{
				memcpy((void *)(((U8 *)DestData_p) + ((SrcBitmap_p->Height - i - 1)>>1)), SrcData_p, SrcBitmap_p->Pitch);
				DestData_p += DestBitmap_p->Pitch;
				SrcData_p += SrcBitmap_p->Pitch;
			}
			break;
		case PAL_OSD_COLOR_TYPE_ARGB8888:
			for(i=0; i<SrcBitmap_p->Height; i++)
			{
				GFX_MEMCPY_U32((((U8 *)DestData_p) + (((SrcBitmap_p->Height - i - 1)>>1)<<2)), SrcData_p, SrcBitmap_p->Width);
		//		memcpy((void *)(((U8 *)DestData_p) + (((SrcBitmap_p->Height - i - 1)>>1)<<2)), SrcData_p, SrcBitmap_p->Pitch);
				DestData_p += DestBitmap_p->Pitch;
				SrcData_p += SrcBitmap_p->Pitch;
			}
			break;
		default :
			return TH_ERROR_NOT_SUPPORT;
	}
	
	
	return Error;
}

#if (GFX_ENABLE_IDX_CACHE)
TH_Error_t GFX_GetTextSizeUseIdxCache(MID_GFX_PutText_t *TextParams_p, 
											S32 StartChar, 
											S32 EndChar, 
											GFX_CharData_t *StrData_p, 
											S32 *Width_p, 
											S32 *Heigth_p, 
											S32 *MaxAscent_p, 
											BOOL bEllipsis, 
											GFX_CharData_t *EllipsisData_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	S32 count, Width, Height, CharWidth, MaxAscent, MaxDescent;
	GFX_Font_t *Font_p;
	
	count = StartChar;
	Width = 0;
	Height = 0;
	MaxAscent = 0;
	MaxDescent = 0;
	




	while((StrData_p[count].Unicode != 0x00) && count < EndChar)
	{

		Font_p = StrData_p[count].Font_p;
		CharWidth = StrData_p[count].Width;
		if(MaxAscent < StrData_p[count].Ascent)
		{
			MaxAscent = StrData_p[count].Ascent;
		}
		if(MaxDescent < StrData_p[count].Descent)
		{
			MaxDescent = StrData_p[count].Descent;
		}
		
		count ++;
		
		if(Font_p != NULL)
		{
			Width += (CharWidth + TextParams_p->HInterval);

			if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
			{
				Width += 1;
			}
		}
	}
	
	if(Width > 0)
	{
		if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
		{
			
			
			if(StrData_p[StartChar].Left + StrData_p[StartChar].BitmapW > StrData_p[StartChar].Width)/*如果第一个字符要向前缩进*/
			{
				Width += ((StrData_p[StartChar].Left + StrData_p[StartChar].BitmapW) - StrData_p[StartChar].Width);
			//	printf("get special line %d\n", Width);
			}
			
			if(StrData_p[count-1].Left < 0)/*如果最后一个字符是向后扩展*/
			{
				Width += (-StrData_p[count-1].Left);
			}
			
		}
		else
		{
		
			if(StrData_p[StartChar].Left < 0)/*如果第一个字符要向前缩进*/
			{
				Width += (-StrData_p[StartChar].Left);
			//	printf("get special line %d\n", Width);
			}
			
			if(StrData_p[count-1].Left + StrData_p[count-1].BitmapW > StrData_p[count-1].Width)/*如果最后一个字符是向后扩展*/
			{
				Width += ((StrData_p[count-1].Left + StrData_p[count-1].BitmapW) - StrData_p[count-1].Width);
			}
		}
	}

	if(bEllipsis && EllipsisData_p)/*算上省略符的宽*/
	{
		if(EllipsisData_p->Font_p)
		{
			CharWidth = EllipsisData_p->Width;
			if(MaxAscent < EllipsisData_p->Ascent)
			{
				MaxAscent = EllipsisData_p->Ascent;
			}
			if(MaxDescent < EllipsisData_p->Descent)
			{
				MaxDescent = EllipsisData_p->Descent;
			}
			Width += ((CharWidth + TextParams_p->HInterval)*3);
			if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
			{
				Width += 3;
			}
		}
	}
	
	if(MaxDescent > 0)
	{
		Height = MaxAscent + MaxDescent;
	}
	else
	{
		Height = MaxAscent;
	}
	*MaxAscent_p = MaxAscent;
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)
	{
//		Width += (Height / GFX_ITALIC_PITCH);
		Width += (Height >> 1);
	}
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
//		Width += (GFX_OUTLINE_WIDTH * 2);
//		Height += (GFX_OUTLINE_WIDTH * 2);
		Width += (GFX_OUTLINE_WIDTH << 1);
		Height += (GFX_OUTLINE_WIDTH << 1);
	}

	if(Width_p != NULL)
	{
		*Width_p = Width;
	}
	if(Heigth_p != NULL)
	{
		*Heigth_p = Height;
	}
	return Error;
}

TH_Error_t  GFX_PutOneLineText(PAL_OSD_Handle  Handle, 
									MID_GFX_PutText_t *TextParams_p, 
									S32 StartChar, 
									S32 EndChar, 
									GFX_CharData_t *StrData_p, 
									S32 YOffset, 
									BOOL bEllipsis, 
									GFX_CharData_t *EllipsisData_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	S32 count, CharIndex, Width, Height, CharWidth, XOffset, MaxAscent, prev;
	U8 EffectType;
	PAL_OSD_Bitmap_t SrcBitmap, DestBitmap;
	PAL_OSD_Win_t	DestWin, SrcWin;
	PAL_OSD_Color_t 	KeyColor;
	PAL_OSD_Bitmap_t *Bitmap_p, OsdBitmap;
	MID_GFX_PutBmp_t TempBmpParams;
	U32 TextColor;
	GFX_Font_t 	*Font_p;
	BOOL MonoBmp = FALSE;
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
		MonoBmp = TRUE;
	}

	Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
	if(Error != TH_NO_ERROR)
	{
		return Error;
	}
	
	switch(TextParams_p->TextColor.Type)
	{
		case PAL_OSD_COLOR_TYPE_CLUT8:
			TextColor = TextParams_p->TextColor.Value.CLUT8;
			break;
		case PAL_OSD_COLOR_TYPE_ARGB8888:
			TextColor = (TextParams_p->TextColor.Value.ARGB8888.Alpha << 24) | (TextParams_p->TextColor.Value.ARGB8888.R << 16) |\
						(TextParams_p->TextColor.Value.ARGB8888.G) << 8 | (TextParams_p->TextColor.Value.ARGB8888.B);
			break;
		default :
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NOT_SUPPORT;
	}
	
	Bitmap_p = &OsdBitmap;
	
	count = StartChar;
	EffectType = TextParams_p->EffectType;
	TextParams_p->EffectType = MID_GFX_TEXT_NORMAL;
	GFX_GetTextSizeUseIdxCache(TextParams_p, StartChar, EndChar, StrData_p, &Width, &Height, &MaxAscent, bEllipsis, EllipsisData_p);
	TextParams_p->EffectType = EffectType;
#if (!GFX_USE_TTF_GRAY)
	if((TextParams_p->EffectType & (~(MID_GFX_TEXT_NEWLINE|MID_GFX_TEXT_ELLIPSIS))) != 0)
#endif
	{
		memset(&SrcBitmap, 0, sizeof(SrcBitmap));
		SrcBitmap.Width = Width;
		SrcBitmap.Height = Height;
		SrcBitmap.ColorType = Bitmap_p->ColorType;
		SrcBitmap.AspectRatio = Bitmap_p->AspectRatio;
		if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
		{
			SrcBitmap.Pitch = ((Bitmap_p->Pitch * 8/Bitmap_p->Width) * Width + 7) / 8;
			SrcBitmap.Size = SrcBitmap.Pitch * Height;
			SrcBitmap.Data_p = (void *)GFX_MALLOC(SrcBitmap.Size);
			if(SrcBitmap.Data_p == NULL)
			{
				PAL_OSD_PutSrcBitmap(Handle);
				return TH_ERROR_NO_MEM;
			}
		}
		else
		{
			Error = PAL_OSD_AllocBitmap(&SrcBitmap, Bitmap_p->PlatformPalette_p, TRUE);
			if(Error != TH_NO_ERROR)
			{
				PAL_OSD_PutSrcBitmap(Handle);
				return TH_ERROR_NO_MEM;
			}
		}
		memset(SrcBitmap.Data_p, 0, SrcBitmap.Size);
	}
#if (!GFX_USE_TTF_GRAY)
	if((TextParams_p->EffectType & (~(MID_GFX_TEXT_NEWLINE|MID_GFX_TEXT_ELLIPSIS))) == 0)
	{
		if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
		{
			if(TextParams_p->PutType & MID_GFX_TEXT_PUT_LEFT)
			{
				XOffset = Width;
			}
			else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_RIGHT)
			{
				XOffset = TextParams_p->Width;
			}
			else
			{
//				XOffset = TextParams_p->Width - ((TextParams_p->Width - Width) / 2);
				XOffset = TextParams_p->Width - ((TextParams_p->Width - Width) >> 1);
			}
		}
		else
		{
			if(TextParams_p->PutType & MID_GFX_TEXT_PUT_LEFT)
			{
				XOffset = 0;
			}
			else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_RIGHT)
			{
				XOffset = TextParams_p->Width - Width;
			}
			else
			{
//				XOffset = (TextParams_p->Width - Width) / 2;
				XOffset = (TextParams_p->Width - Width) >> 1;
			}
		}
	}
	else
#endif
	{
		if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
		{
			XOffset = Width;
			if(StrData_p[StartChar].Left + StrData_p[StartChar].BitmapW > StrData_p[StartChar].Width)/*如果第一个字符要向前缩进*/
			{
				XOffset -= ((StrData_p[StartChar].Left + StrData_p[StartChar].BitmapW) - StrData_p[StartChar].Width);
			}
		}
		else
		{
			XOffset = 0;
			if(StrData_p[StartChar].Left < 0)/*第一个字符要往前缩进*/
			{
				XOffset += (-StrData_p[StartChar].Left);
			}
		}
	}
	prev = -1;
	while((StrData_p[count].Unicode != 0x00) && count < EndChar)
	{
		Font_p = StrData_p[count].Font_p;
		CharWidth = StrData_p[count].Width;
		CharIndex = StrData_p[count].GlyphIdx;
		
		if(Font_p != NULL && CharIndex != -1 && CharIndex != 0)
		{
			if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
			{
				if(StrData_p[count].Unicode >= 0x590 && StrData_p[count].Unicode <= 0x5c7 &&\
					StrData_p[count].Unicode != 0x5be && StrData_p[count].Unicode != 0x5c0 &&\
					StrData_p[count].Unicode != 0x5c6)
				{
					if(prev != -1)
					{
						XOffset += StrData_p[prev].Width;
						XOffset -= ((StrData_p[prev].Width - StrData_p[count].Width)>>1);
					}
					XOffset -= CharWidth;
				}
				else
				{
					XOffset -= CharWidth;
				}
			}
		#if (!GFX_USE_TTF_GRAY)
			if((TextParams_p->EffectType & (~(MID_GFX_TEXT_NEWLINE|MID_GFX_TEXT_ELLIPSIS))) == 0)/*没有任何其它效果*/
			{
				GFX_PutText(Bitmap_p, Font_p, CharIndex, MonoBmp, XOffset+TextParams_p->LeftX, YOffset+TextParams_p->TopY+MaxAscent, TextParams_p->FontSize, TextColor, TextParams_p, &(StrData_p[count]));
			}
			else
		#endif
			{
				GFX_PutText(&SrcBitmap, Font_p, CharIndex, MonoBmp, XOffset, MaxAscent, TextParams_p->FontSize, TextColor, TextParams_p, &(StrData_p[count]));
			}
			
			if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
			{
				if(StrData_p[count].Unicode >= 0x590 && StrData_p[count].Unicode <= 0x5c7 &&\
					StrData_p[count].Unicode != 0x5be && StrData_p[count].Unicode != 0x5c0 &&\
					StrData_p[count].Unicode != 0x5c6)
				{
					XOffset += CharWidth;
					if(prev != -1)
					{
						XOffset += ((StrData_p[prev].Width - StrData_p[count].Width)>>1);
						XOffset -= StrData_p[prev].Width;
					}
				}
				XOffset -= TextParams_p->HInterval;
			}
			else
			{
				XOffset += (CharWidth + TextParams_p->HInterval);
			}
		}
		prev = count;
		count ++;
	}
	if(bEllipsis && EllipsisData_p)/*画上省略符*/
	{
		Font_p = EllipsisData_p->Font_p;
		CharWidth = EllipsisData_p->Width;
		CharIndex = EllipsisData_p->GlyphIdx;
		if(Font_p != NULL && CharIndex != -1 && CharIndex != 0)
		{
			for(count=0; count<3; count++)
			{
				if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
				{
					XOffset -= CharWidth;
				}
			#if (!GFX_USE_TTF_GRAY)
				if((TextParams_p->EffectType & (~(MID_GFX_TEXT_NEWLINE|MID_GFX_TEXT_ELLIPSIS))) == 0)/*没有任何其它效果*/
				{
					GFX_PutText(Bitmap_p, Font_p, CharIndex, MonoBmp, XOffset+TextParams_p->LeftX, YOffset+TextParams_p->TopY+MaxAscent, TextParams_p->FontSize, TextColor, TextParams_p, &(StrData_p[count]));
				}
				else
			#endif
				{
					GFX_PutText(&SrcBitmap, Font_p, CharIndex, MonoBmp, XOffset, MaxAscent, TextParams_p->FontSize, TextColor, TextParams_p, &(StrData_p[count]));
				}
				
				if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
				{
					XOffset -= TextParams_p->HInterval;
				}
				else
				{
					XOffset += (CharWidth + TextParams_p->HInterval);
				}
			}
		}
	}
#if (!GFX_USE_TTF_GRAY)
	if((TextParams_p->EffectType & (~(MID_GFX_TEXT_NEWLINE|MID_GFX_TEXT_ELLIPSIS))) == 0)
	{
		PAL_OSD_PutSrcBitmap(Handle);
		return Error;
	}
#endif
	if(TextParams_p->EffectType & MID_GFX_TEXT_ZOOM)
	{
//		OSD_ZoomText(&SrcBitmap, &DestBitmap);
	}
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
	{
//		OSD_BoldText(&SrcBitmap, &DestBitmap);
	}
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)
	{
//		Width += (Height / GFX_ITALIC_PITCH);
		Width += (Height >> 1);
		memset(&DestBitmap, 0, sizeof(DestBitmap));
		DestBitmap.Width = Width;
		DestBitmap.Height = Height;
		DestBitmap.ColorType = Bitmap_p->ColorType;
		DestBitmap.AspectRatio = Bitmap_p->AspectRatio;
		if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
		{
			DestBitmap.Pitch = ((Bitmap_p->Pitch * 8/Bitmap_p->Width) * Width + 7) / 8;
			DestBitmap.Size = DestBitmap.Pitch * Height;
			DestBitmap.Data_p = (void *)GFX_MALLOC(DestBitmap.Size);
			if(DestBitmap.Data_p == NULL)
			{
				PAL_OSD_PutSrcBitmap(Handle);
				return TH_ERROR_NO_MEM;
			}
			memset(DestBitmap.Data_p, 0, DestBitmap.Size);
			GFX_ItalicText(&SrcBitmap, &DestBitmap);
			
			GFX_FREE(SrcBitmap.Data_p);
		}
		else
		{
			Error = PAL_OSD_AllocBitmap(&DestBitmap, Bitmap_p->PlatformPalette_p, TRUE);
			if(Error != TH_NO_ERROR)
			{
				PAL_OSD_PutSrcBitmap(Handle);
				return TH_ERROR_NO_MEM;
			}
			memset(DestBitmap.Data_p, 0, DestBitmap.Size);
			GFX_ItalicText(&SrcBitmap, &DestBitmap);
			PAL_OSD_FreeBitmap(&SrcBitmap);
		}
		SrcBitmap = DestBitmap;
		DestBitmap.Data_p = NULL;
	}

	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
//		Width += (GFX_OUTLINE_WIDTH * 2);
//		Height += (GFX_OUTLINE_WIDTH * 2);
		Width += (GFX_OUTLINE_WIDTH << 1);
		Height += (GFX_OUTLINE_WIDTH << 1);
		memset(&DestBitmap, 0, sizeof(DestBitmap));
		DestBitmap.Width = Width;
		DestBitmap.Height = Height;
		DestBitmap.ColorType = Bitmap_p->ColorType;
		DestBitmap.AspectRatio = Bitmap_p->AspectRatio;
		if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
		{
			DestBitmap.Pitch = ((Bitmap_p->Pitch * 8/Bitmap_p->Width) * Width + 7) / 8;
			DestBitmap.Size = DestBitmap.Pitch * Height;
			DestBitmap.Data_p = (void *)GFX_MALLOC(DestBitmap.Size);
			if(DestBitmap.Data_p == NULL)
			{
				PAL_OSD_PutSrcBitmap(Handle);
				return TH_ERROR_NO_MEM;
			}
			memset(DestBitmap.Data_p, 0, DestBitmap.Size);
			GFX_OutlineText(&SrcBitmap, &DestBitmap, &(TextParams_p->TextColor), &(TextParams_p->LineColor));

			GFX_FREE(SrcBitmap.Data_p);
		}
		else
		{
			Error = PAL_OSD_AllocBitmap(&DestBitmap, Bitmap_p->PlatformPalette_p, TRUE);
			if(Error != TH_NO_ERROR)
			{
				PAL_OSD_PutSrcBitmap(Handle);
				return TH_ERROR_NO_MEM;
			}
			memset(DestBitmap.Data_p, 0, DestBitmap.Size);
			GFX_OutlineText(&SrcBitmap, &DestBitmap, &(TextParams_p->TextColor), &(TextParams_p->LineColor));
			PAL_OSD_FreeBitmap(&SrcBitmap);
		}
		SrcBitmap = DestBitmap;
		DestBitmap.Data_p = NULL;
	}

	if(TextParams_p->PutType & MID_GFX_TEXT_PUT_LEFT)
	{
		DestWin.LeftX = TextParams_p->LeftX;
	}
	else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_RIGHT)
	{
		DestWin.LeftX = TextParams_p->LeftX + TextParams_p->Width - Width;
	}
	else
	{
//		Pos.LeftX = TextParams_p->LeftX + (TextParams_p->Width - Width) / 2;
		DestWin.LeftX = TextParams_p->LeftX + ((TextParams_p->Width - Width) >> 1);
	}
#if (TH_ADSYS_SUPPORT)
	if(TextParams_p->PutType & MID_GFX_TEXT_NEWLINE)
	{
		DestWin.TopY = YOffset + TextParams_p->TopY;
	}
	else
	{
		DestWin.TopY = YOffset + TextParams_p->TopY + (TextParams_p->FontSize-MaxAscent)-4;
	}
#else
	DestWin.TopY = YOffset + TextParams_p->TopY;
#endif
	DestWin.Width = SrcBitmap.Width;
	DestWin.Height = SrcBitmap.Height;
	memset(&KeyColor, 0, sizeof(PAL_OSD_Color_t));
	KeyColor.Type = Bitmap_p->ColorType;
	PAL_OSD_PutSrcBitmap(Handle);
#if (GFX_USE_TTF_GRAY)
	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
		TempBmpParams.SrcLeftX = TextParams_p->XOffset;
		TempBmpParams.SrcTopY = 0;
		TempBmpParams.SrcWidth = DestWin.Width-TextParams_p->XOffset;
		TempBmpParams.SrcHeight = DestWin.Height;
		TempBmpParams.DestLeftX = DestWin.LeftX;
		TempBmpParams.DestTopY = DestWin.TopY;
		TempBmpParams.DestWidth = DestWin.Width-TextParams_p->XOffset;
		TempBmpParams.DestHeight = DestWin.Height;
		TempBmpParams.BmpWidth = DestWin.Width;
		TempBmpParams.BmpHeight = DestWin.Height;
		TempBmpParams.BmpData_p = SrcBitmap.Data_p;
		TempBmpParams.PutType = MID_GFX_BMP_PUT_CENTER;
		TempBmpParams.ColorType = SrcBitmap.ColorType;
		TempBmpParams.Palette_p = NULL;
		TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
		TempBmpParams.EnableKeyColor = TRUE;
		TempBmpParams.KeyColor = KeyColor.Value;
		Error = GFX_DrawBmp(Handle, &TempBmpParams);
	}
	else
	{
		SrcWin.LeftX = TextParams_p->XOffset;
		SrcWin.TopY = 0;
		SrcWin.Width = SrcBitmap.Width-TextParams_p->XOffset;
		SrcWin.Height = SrcBitmap.Height;
		DestWin.Width = SrcWin.Width;
		if(TextParams_p->Mix && !PAL_OSD_CheckAlpha(Handle, &DestWin))
		{
			Error = PAL_OSD_PutAlpha(Handle, &DestWin, &SrcWin, &SrcBitmap);
		}
		else
		{
			Error = PAL_OSD_Put(Handle, &DestWin, &SrcWin, &SrcBitmap);
		}
	}
#else
	TempBmpParams.SrcLeftX = TextParams_p->XOffset;
	TempBmpParams.SrcTopY = 0;
	TempBmpParams.SrcWidth = DestWin.Width-TextParams_p->XOffset;
	TempBmpParams.SrcHeight = DestWin.Height;
	TempBmpParams.DestLeftX = DestWin.LeftX;
	TempBmpParams.DestTopY = DestWin.TopY;
	TempBmpParams.DestWidth = DestWin.Width-TextParams_p->XOffset;
	TempBmpParams.DestHeight = DestWin.Height;
	TempBmpParams.BmpWidth = DestWin.Width;
	TempBmpParams.BmpHeight = DestWin.Height;
	TempBmpParams.BmpData_p = SrcBitmap.Data_p;
	TempBmpParams.PutType = MID_GFX_BMP_PUT_CENTER;
	TempBmpParams.ColorType = SrcBitmap.ColorType;
	TempBmpParams.Palette_p = NULL;
	TempBmpParams.CompressType = MID_GFX_COMPRESS_NONE;
	TempBmpParams.EnableKeyColor = TRUE;
	TempBmpParams.KeyColor = KeyColor.Value;

	Error = GFX_DrawBmp(Handle, &TempBmpParams);
#endif
	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
		GFX_FREE(SrcBitmap.Data_p);
	}
	else
	{
		PAL_OSD_FreeBitmap(&SrcBitmap);
	}
	return Error;
}

BOOL  GFX_RenderText(PAL_OSD_Handle  Handle, 
								MID_GFX_PutText_t *TextParams_p, 
								S32 StartChar, 
								GFX_CharData_t *StrData_p, 
								S32 *YOffset_p)
{
	S32 Prev_count, count, CharIndex, Width, Height, CharWidth, MaxAscent, MaxDescent, StopChar, NextStop, CharLeft;
	U16 Unicode;
	BOOL bGetPoint, bLastLine, MonoBmp = FALSE, bForceNewLine = FALSE;
	S32	XOffset;

	count = StartChar;
	Width = 0;
	Height = 0;
	StopChar = StartChar;
	MaxAscent = 0;
	MaxDescent = 0;
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_X_OFFSET)
	{
		TextParams_p->EffectType &= (~MID_GFX_TEXT_NEWLINE);
	}
	else
	{
		TextParams_p->XOffset = 0;
	}

	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
//		Width += (GFX_OUTLINE_WIDTH * 2);
		Width += (GFX_OUTLINE_WIDTH << 1);
	}
	
	bGetPoint = FALSE;
	NextStop = -1;
	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
		MonoBmp = TRUE;
	}
	XOffset = TextParams_p->XOffset;

	while(StrData_p[count].Unicode != 0x00)
	{
		Prev_count = count;
		
		bForceNewLine = FALSE;
		Unicode = StrData_p[count].Unicode;
		if(GFX_CheckBreakCode(StrData_p[count].BaseCode))
		{
			StopChar = count;
			NextStop = count + 1;
			bGetPoint = TRUE;
			if(StrData_p[count].BaseCode == 0x0A)
			{
				StopChar ++;
				bForceNewLine = TRUE;
				TextParams_p->EffectType |= MID_GFX_TEXT_NEWLINE;

			//	printf("new line\n");
			}
		}
		else
		{
			NextStop = -1;
		}
		
		CharWidth = StrData_p[count].Width;
		if(MaxAscent < StrData_p[count].Ascent)
		{
			MaxAscent = StrData_p[count].Ascent;
		}
		if(MaxDescent < StrData_p[count].Descent)
		{
			MaxDescent = StrData_p[count].Descent;
		}

		if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
		{
			if(count == StartChar)/*如果第一个字符是向前缩进时是不能向前缩进的*/
			{
				if(StrData_p[count].Left + StrData_p[count].BitmapW > StrData_p[count].Width)
				{
					CharWidth += ((StrData_p[count].Left + StrData_p[count].BitmapW) - StrData_p[count].Width);
				}
			}
			
			if(StrData_p[count].Left < 0)/*如果最后一个字符是向后扩展*/
			{
				CharWidth += (-StrData_p[count].Left);
			}
		}
		else
		{
			if(count == StartChar)/*如果第一个字符是向前缩进时是不能向前缩进的*/
			{
				if(StrData_p[count].Left < 0)
				{
					CharWidth += (-StrData_p[count].Left);
	//				printf("get special char %d\n", CharWidth);
				}
			}
			
			if(StrData_p[count].Left + StrData_p[count].BitmapW > StrData_p[count].Width)/*如果最后一个字符是向后扩展*/
			{
				CharWidth += ((StrData_p[count].Left + StrData_p[count].BitmapW) - StrData_p[count].Width);
			}
		}
		CharIndex = StrData_p[count].GlyphIdx;
		
		count ++;
		
		if(StrData_p[Prev_count].Font_p != NULL && CharIndex != -1)
		{
			if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
			{
				CharWidth += 1;
			}
			if(MaxDescent > 0)
			{
				Height = MaxAscent + MaxDescent;
//				printf("line %d, get height %d\n", __LINE__, Height);
			}
			else
			{
				Height = MaxAscent;
//				printf("line %d, get height %d\n", __LINE__, Height);
			}
			if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
			{
				Height += (GFX_OUTLINE_WIDTH << 1);
			}
			
			Width += CharWidth;
			if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)
			{
//				Width += (Height / GFX_ITALIC_PITCH);
				Width += (Height >> 1);
			}
			
			if(TextParams_p->EffectType & MID_GFX_TEXT_X_OFFSET)
			{
				if(Width < XOffset)
				{
					StartChar = count;
					XOffset -= Width;
					Width = 0;
				}
				else
				{
					TextParams_p->XOffset = XOffset;
				}
			}

			if(Width > TextParams_p->Width || bForceNewLine)
			{
				Width -= CharWidth;
				if((TextParams_p->EffectType & MID_GFX_TEXT_NEWLINE) || bForceNewLine)
				{
					*YOffset_p = (*YOffset_p) + Height;
					
					if(*YOffset_p > TextParams_p->Height)
					{
						*YOffset_p = (*YOffset_p) - Height - TextParams_p->VInterval;
						if(TextParams_p->PutType & MID_GFX_TEXT_PUT_TOP)
						{
							/*nothing to do*/
						}
						else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_BOT)
						{
							*YOffset_p = TextParams_p->Height;
						}
						else
						{
					//		*YOffset_p = (*YOffset_p) + ((TextParams_p->Height - (*YOffset_p)) / 2);
							*YOffset_p = (*YOffset_p) + ((TextParams_p->Height - (*YOffset_p)) >> 1);
						}
						return TRUE;
					}
					
					*YOffset_p = (*YOffset_p) + TextParams_p->VInterval;
					if(bGetPoint == FALSE)
					{
						StopChar = Prev_count;
					}
					if(bForceNewLine)
					{
						count = StopChar;
					}
					bLastLine = GFX_RenderText(Handle, TextParams_p, StopChar, StrData_p, YOffset_p);
					/*GFX_RenderText 返回的YOffset_p 是在本行的底部，所以画本行之前必须减去本行的高度*/
					*YOffset_p = (*YOffset_p) - Height;
					if((bLastLine == TRUE) && (TextParams_p->EffectType & MID_GFX_TEXT_ELLIPSIS))/*加省略符"..."*/
					{
						GFX_CharData_t EllipsisData;
						StopChar = Prev_count;
						EllipsisData.Font_p = GFX_LocateGlyphForSize(StrData_p[0].Font_p, TextParams_p->FontSize, '.', MonoBmp,
																	&EllipsisData.GlyphIdx, &CharWidth, &CharLeft, NULL, &MaxAscent, &MaxDescent);
						
						EllipsisData.Width = CharWidth;
						EllipsisData.Ascent = MaxAscent;
						EllipsisData.Descent = MaxDescent;
						EllipsisData.Unicode = '.';
						/*加上省略符带效果宽度*/
						Width += ((CharWidth + TextParams_p->HInterval)*3);
						if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
						{
							Width += 3;
						}
						while(Width > TextParams_p->Width)/*加上省略符后超出宽度, 减掉一些字符*/
						{
							StopChar --;
							if(StopChar <= StartChar)
							{
								break;
							}
							Width -= StrData_p[StopChar].Width;
						}
						if(StopChar > StartChar)
						{
							GFX_PutOneLineText(Handle, TextParams_p, StartChar, StopChar, StrData_p, (*YOffset_p), TRUE, &EllipsisData);
						}
					}
					else
					{
						if(bGetPoint == FALSE)
						{
							StopChar = Prev_count;
						}
						GFX_PutOneLineText(Handle, TextParams_p, StartChar, StopChar, StrData_p, (*YOffset_p), FALSE, NULL);
					}
					*YOffset_p = (*YOffset_p) - TextParams_p->VInterval;
					return FALSE;
				}
				else
				{
					if(TextParams_p->PutType & MID_GFX_TEXT_PUT_TOP)
					{
						*YOffset_p = 0;
					}
					else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_BOT)
					{
						*YOffset_p = TextParams_p->Height - Height;
					}
					else
					{
				//		*YOffset_p = (TextParams_p->Height - Height) / 2;
					#if (TH_ADSYS_SUPPORT)
						*YOffset_p = (TextParams_p->Height - TextParams_p->FontSize) >> 1;
					#else
						*YOffset_p = (TextParams_p->Height - Height) >> 1;
					#endif
					}
					if(TextParams_p->EffectType & MID_GFX_TEXT_ELLIPSIS)/*加省略符"..."*/
					{
						GFX_CharData_t EllipsisData;
						StopChar = Prev_count;
						EllipsisData.Font_p = GFX_LocateGlyphForSize(StrData_p[0].Font_p, TextParams_p->FontSize, '.', MonoBmp,
																	&EllipsisData.GlyphIdx, &CharWidth, &CharLeft, NULL, &MaxAscent, &MaxDescent);
						
						EllipsisData.Width = CharWidth;
						EllipsisData.Ascent = MaxAscent;
						EllipsisData.Descent = MaxDescent;
						EllipsisData.Unicode = '.';
						/*加上省略符带效果宽度*/
						Width += ((CharWidth + TextParams_p->HInterval)*3);
						if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
						{
							Width += 3;
						}
						while(Width > TextParams_p->Width)/*加上省略符后超出宽度, 减掉一些字符*/
						{
							StopChar --;
							if(StopChar <= StartChar)
							{
								break;
							}
							Width -= StrData_p[StopChar].Width;
						}
						if(StopChar > StartChar)
						{
							GFX_PutOneLineText(Handle, TextParams_p, StartChar, StopChar, StrData_p, (*YOffset_p), TRUE, &EllipsisData);
						}
					}
					else
					{
						StopChar = Prev_count;
						if(TextParams_p->EffectType & MID_GFX_TEXT_X_OFFSET)/*fixed when add x offset will lost some end chars*/
						{
							StopChar = count;
						}
						GFX_PutOneLineText(Handle, TextParams_p, StartChar, StopChar, StrData_p, (*YOffset_p), FALSE, NULL);
					}
					return FALSE;
				}
			}
			else
			{
				if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)
				{
			//		Width -= (Height / GFX_ITALIC_PITCH);
					Width -= (Height >> 1);
				}
				if(StrData_p[Prev_count].Left + StrData_p[Prev_count].BitmapW > StrData_p[Prev_count].Width)/*减掉上面加上的向后扩展*/
				{
					Width -= ((StrData_p[Prev_count].Left + StrData_p[Prev_count].BitmapW) - StrData_p[Prev_count].Width);
				}

				Width += TextParams_p->HInterval;
				if(bGetPoint == TRUE)
				{
					if(NextStop != -1)
					{
						StopChar = NextStop;
					}
				}
			}
		}
		else
		{
			bGetPoint = TRUE;
			StopChar = count;
		}
	}
	/*以下部分不用加省略符, 能到达结束符说明宽度够*/
	if(TextParams_p->EffectType & MID_GFX_TEXT_NEWLINE)
	{
		*YOffset_p = (*YOffset_p) + Height;
		
		if(*YOffset_p > TextParams_p->Height)
		{
			*YOffset_p = (*YOffset_p) - Height - TextParams_p->VInterval;
			if(TextParams_p->PutType & MID_GFX_TEXT_PUT_TOP)
			{
				/*nothing to do*/
			}
			else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_BOT)
			{
				*YOffset_p = TextParams_p->Height;
			}
			else
			{
		//		*YOffset_p = (*YOffset_p) + ((TextParams_p->Height - (*YOffset_p)) / 2);
				*YOffset_p = (*YOffset_p) + ((TextParams_p->Height - (*YOffset_p)) >> 1);
			}
			return TRUE;
		}
		
		if(TextParams_p->PutType & MID_GFX_TEXT_PUT_TOP)
		{
			/*nothing to do*/
		}
		else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_BOT)
		{
			*YOffset_p = TextParams_p->Height;
		}
		else
		{
	//		*YOffset_p = (*YOffset_p) + ((TextParams_p->Height - (*YOffset_p)) / 2);
			*YOffset_p = (*YOffset_p) + ((TextParams_p->Height - (*YOffset_p)) >> 1);
		}

		*YOffset_p = (*YOffset_p) - Height;
		GFX_PutOneLineText(Handle, TextParams_p, StartChar, count, StrData_p, (*YOffset_p), FALSE, NULL);
		*YOffset_p = (*YOffset_p) - TextParams_p->VInterval;
	}
	else
	{
		if(TextParams_p->PutType & MID_GFX_TEXT_PUT_TOP)
		{
			*YOffset_p = 0;
		}
		else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_BOT)
		{
			*YOffset_p = TextParams_p->Height - Height;
		}
		else
		{
	//		*YOffset_p = (TextParams_p->Height - Height) / 2;
		#if (TH_ADSYS_SUPPORT)
			*YOffset_p = (TextParams_p->Height - TextParams_p->FontSize) >> 1;
		#else
			*YOffset_p = (TextParams_p->Height - Height) >> 1;
		#endif
	//		printf("TextParams_p->Height = %d, Height = %d\n", TextParams_p->Height, Height);
		}
		GFX_PutOneLineText(Handle, TextParams_p, StartChar, count, StrData_p, (*YOffset_p), FALSE, NULL);
	}
	
	return FALSE;
}

GFX_CharData_t *GFX_ConstructStrData(GFX_Font_t *Font_p, GFX_Utf8_t *Utf8_p, MID_GFX_PutText_t *TextParams_p)
{
	GFX_CharData_t *StrData_p;
	GFX_CharData_t *TempStrData_p;
	U16 Unicode;
	S32 CharIdx, CharWidth, CharBitWidth, CharAscent, CharDescent, CharLeft;
	BOOL MonoBmp = FALSE;
	
	StrData_p = (GFX_CharData_t *)GFX_MALLOC((Utf8_p->Count+1) * sizeof(GFX_CharData_t));
	if(StrData_p == NULL)
	{
		return StrData_p;
	}
	TempStrData_p = StrData_p;
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
		MonoBmp = TRUE;
	}
	while((Unicode = GFX_Utf8Read(Utf8_p, TRUE)))
	{
		TempStrData_p->BaseCode = Unicode;
		if(Unicode >= 0x590 && Unicode <= 0x6FF)
		{
			TempStrData_p->Arabic = TRUE;
			TempStrData_p->Font_p = NULL;
			CharIdx = 0;
			CharWidth = 0;
			CharLeft = 0;
			CharBitWidth = 0;
			CharAscent = 0;
			CharDescent = 0;
			TextParams_p->WriteType = MID_GFX_TEXT_WRITE_R_TO_L;
			if(TextParams_p->Force == FALSE)
			{
				if(TextParams_p->PutType & MID_GFX_TEXT_PUT_LEFT)
				{
					TextParams_p->PutType &= (~MID_GFX_TEXT_PUT_LEFT);
					TextParams_p->PutType |= MID_GFX_TEXT_PUT_RIGHT;
				}
			}
		}
		else
		{
			TempStrData_p->Arabic = FALSE;
			TempStrData_p->Font_p = GFX_LocateGlyphForSize(Font_p, TextParams_p->FontSize, Unicode, MonoBmp,
															&CharIdx, &CharWidth, &CharLeft, &CharBitWidth, &CharAscent, &CharDescent);
		}
		if(CharIdx != -1 )
		{
			TempStrData_p->GlyphIdx = CharIdx;
			TempStrData_p->Width = CharWidth;
			TempStrData_p->Left = CharLeft;
			TempStrData_p->BitmapW = CharBitWidth;
			TempStrData_p->Ascent = CharAscent;
			TempStrData_p->Descent = CharDescent;
			TempStrData_p->Unicode = Unicode;
			TempStrData_p ++;
		}
	}
	TempStrData_p->Unicode = 0;
	TempStrData_p->BaseCode = 0;
	return StrData_p;
}

void GFX_DestructStrData(GFX_CharData_t **StrData_p)
{
	if(StrData_p == NULL || (*StrData_p) == NULL)
	{
		return ;
	}
	GFX_FREE(*StrData_p);
	*StrData_p = NULL;
}

TH_Error_t  GFX_RenderTextForSize(MID_GFX_PutText_t *TextParams_p, 
										S32 StartChar, 
										GFX_CharData_t *StrData_p, 
										S32 *Height_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	S32 Prev_count, count, CharIndex, Width, Height, CharWidth, MaxAscent, MaxDescent, StopChar, NextStop;
	U16 Unicode;
	BOOL bGetPoint, bFirstChar, bForceNewLine;

	count = StartChar;
	
	if(StrData_p[count].Unicode == 0x00)
	{
		return Error;
	}
	Width = 0;
	Height = 0;
	StopChar = StartChar;
	MaxAscent = 0;
	MaxDescent = 0;

	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
//		Width += (GFX_OUTLINE_WIDTH * 2);
		Width += (GFX_OUTLINE_WIDTH << 1);
	}
	
	bGetPoint = FALSE;
	bFirstChar = TRUE;
	NextStop = -1;
	while(StrData_p[count].Unicode != 0x00)
	{
		Prev_count = count;
		bForceNewLine = FALSE;
		Unicode = StrData_p[count].Unicode;
		if(GFX_CheckBreakCode(StrData_p[count].BaseCode))
		{
			StopChar = count;
			NextStop = count + 1;
			bGetPoint = TRUE;
			if(StrData_p[count].BaseCode == 0x0A)
			{
				StopChar ++;
				bForceNewLine = TRUE;
			}
		}
		else
		{
			NextStop = -1;
		}
		
		CharWidth = StrData_p[count].Width;
		if(MaxAscent < StrData_p[count].Ascent)
		{
			MaxAscent = StrData_p[count].Ascent;
		}
		if(MaxDescent < StrData_p[count].Descent)
		{
			MaxDescent = StrData_p[count].Descent;
		}

		CharIndex = StrData_p[count].GlyphIdx;
		if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
		{
		
			if(bFirstChar)/*如果第一个字符是向前缩进时是不能向前缩进的*/
			{
				if(StrData_p[count].Left + StrData_p[count].BitmapW > StrData_p[count].Width)
				{
					CharWidth += ((StrData_p[count].Left + StrData_p[count].BitmapW) - StrData_p[count].Width);
				}
				bFirstChar = FALSE;
			}
			
			if(StrData_p[count].Left < 0)/*如果最后一个字符是向后扩展*/
			{
				CharWidth += (-StrData_p[count].Left);
			}
		}
		else
		{
			if(bFirstChar)
			{
				if(StrData_p[count].Left < 0)
				{
					CharWidth += (-StrData_p[count].Left);
				}
				bFirstChar = FALSE;
			}
			
			if(StrData_p[count].Left + StrData_p[count].BitmapW > StrData_p[count].Width)/*如果最后一个字符是向后扩展*/
			{
				CharWidth += ((StrData_p[count].Left + StrData_p[count].BitmapW) - StrData_p[count].Width);
			}
		}
		count ++;
		
		if(StrData_p[Prev_count].Font_p != NULL && CharIndex != -1)
		{
			if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
			{
				CharWidth += 1;
			}
			if(MaxDescent > 0)
			{
				Height = MaxAscent + MaxDescent;
			}
			else
			{
				Height = MaxAscent;
			}
			if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
			{
				Height += (GFX_OUTLINE_WIDTH << 1);
			}

			Width += CharWidth;
			if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)
			{
//				Width += (Height / GFX_ITALIC_PITCH);
				Width += (Height >> 1);
			}

			if(Width > TextParams_p->Width || bForceNewLine)/*进行换行*/
			{
				bFirstChar = TRUE;
				Width -= CharWidth;
				(*Height_p) += Height;
				
				(*Height_p) += TextParams_p->VInterval;

				if(bGetPoint)
				{
					count = StopChar;
				}
				else
				{
					count = Prev_count;
				}

				/*宽度重新开始计算*/
				bGetPoint = FALSE;
				Width = 0;
				Height = 0;
				StopChar = count;
				MaxAscent = 0;
				MaxDescent = 0;
				if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
				{
			//		Width += (GFX_OUTLINE_WIDTH * 2);
					Width += (GFX_OUTLINE_WIDTH << 1);
				}
			}
			else
			{
				if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)/*如果还没有换行不能加上斜体的宽度*/
				{
			//		Width -= (Height / GFX_ITALIC_PITCH);
					Width -= (Height >> 1);
				}
				if(StrData_p[Prev_count].Left + StrData_p[Prev_count].BitmapW > StrData_p[Prev_count].Width)/*减掉上面加上的向后扩展*/
				{
					Width -= ((StrData_p[Prev_count].Left + StrData_p[Prev_count].BitmapW) - StrData_p[Prev_count].Width);
				}

				Width += TextParams_p->HInterval;
				if(bGetPoint == TRUE)
				{
					if(NextStop != -1)
					{
						StopChar = NextStop;
					}
				}
			}
		}
		else
		{
			bGetPoint = TRUE;
			StopChar = count;
		}
	}
	
	(*Height_p) += Height;
	
	return Error;
}

#else

TH_Error_t  GFX_PutOneLineText(PAL_OSD_Handle  Handle, GFX_Utf8_t *Utf8_p, GFX_Font_t *Font_p, MID_GFX_PutText_t *TextParams_p, S32 StartChar, S32 EndChar, S32 YOffset)
{
	TH_Error_t Error = TH_NO_ERROR;
	U8 DoubleLen;
	S32 count, CharIndex, Width, Height, CharWidth, CharAscent, CharDescent, XOffset, MaxAscent;
	U16 Unicode;
	U8 EffectType;
	PAL_OSD_Bitmap_t SrcBitmap, DestBitmap;
	PAL_OSD_Pos_t	Pos;
	PAL_OSD_Color_t 	KeyColor;
	PAL_OSD_Bitmap_t *Bitmap_p, OsdBitmap;
	U32 TextColor;
	GFX_Font_t 	*AactualFont_p;
	
	Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
	if(Error != TH_NO_ERROR)
	{
		return Error;
	}

	switch(TextParams_p->TextColor.Type)
	{
		case PAL_OSD_COLOR_TYPE_CLUT8:
			TextColor = TextParams_p->TextColor.Value.CLUT8;
			break;
		case PAL_OSD_COLOR_TYPE_ARGB8888:
			TextColor = TextParams_p->TextColor.Value.ARGB8888.Alpha << 24 | TextParams_p->TextColor.Value.ARGB8888.R << 16 |
						TextParams_p->TextColor.Value.ARGB8888.G << 8 | TextParams_p->TextColor.Value.ARGB8888.B;
			break;
		default :
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NOT_SUPPORT;
	}

	Bitmap_p = &OsdBitmap;
	
	count = StartChar;
	EffectType = TextParams_p->EffectType;
	TextParams_p->EffectType = MID_GFX_TEXT_NORMAL;
	GFX_GetTextSize(Utf8_p, Font_p, TextParams_p, StartChar, EndChar, &Width, &Height, &MaxAscent);
	TextParams_p->EffectType = EffectType;
	if((TextParams_p->EffectType & (~(MID_GFX_TEXT_NEWLINE|MID_GFX_TEXT_ELLIPSIS))) != 0)
	{
		SrcBitmap.Width = Width;
		SrcBitmap.Height = Height;
		SrcBitmap.ColorType = Bitmap_p->ColorType;
		SrcBitmap.AspectRatio = Bitmap_p->AspectRatio;
	#if 0
		SrcBitmap.Pitch = ((Bitmap_p->Pitch * 8/Bitmap_p->Width) * Width + 7) / 8;
		SrcBitmap.Size = SrcBitmap.Pitch * Height;
		SrcBitmap.Data_p = (void *)GFX_MALLOC(SrcBitmap.Size);
		if(SrcBitmap.Data_p == NULL)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NO_MEM;
		}
	#else
		Error = PAL_OSD_AllocBitmap(&SrcBitmap, Bitmap_p->PlatformPalette_p, FALSE);
		if(Error != TH_NO_ERROR)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NO_MEM;
		}
	#endif
		memset(SrcBitmap.Data_p, 0xFF, SrcBitmap.Size);
	}
	if((TextParams_p->EffectType & (~(MID_GFX_TEXT_NEWLINE|MID_GFX_TEXT_ELLIPSIS))) == 0)
	{
		if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
		{
			if(TextParams_p->PutType & MID_GFX_TEXT_PUT_LEFT)
			{
				XOffset = Width;
			}
			else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_RIGHT)
			{
				XOffset = TextParams_p->Width;
			}
			else
			{
//				XOffset = TextParams_p->Width - ((TextParams_p->Width - Width) / 2);
				XOffset = TextParams_p->Width - ((TextParams_p->Width - Width) >> 1);
			}
		}
		else
		{
			if(TextParams_p->PutType & MID_GFX_TEXT_PUT_LEFT)
			{
				XOffset = 0;
			}
			else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_RIGHT)
			{
				XOffset = TextParams_p->Width - Width;
			}
			else
			{
//				XOffset = (TextParams_p->Width - Width) / 2;
				XOffset = (TextParams_p->Width - Width) >> 1;
			}
		}
	}
	else
	{
		if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
		{
			XOffset = Width;
		}
		else
		{
			XOffset = 0;
		}
	}
	Utf8_p->CurrPos = count;
	while(count < EndChar)
	{
		Unicode = GFX_Utf8Read(Utf8_p, TRUE);
		if(Unicode == 0)
		{
			break;
		}
		count = Utf8_p->CurrPos;
		
		AactualFont_p = GFX_LocateGlyphForSize(Font_p, TextParams_p->FontSize, Unicode, &CharIndex, &CharWidth, &CharAscent, &CharDescent);

		if(CharIndex != -1)
		{
			if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
			{
				XOffset -= CharWidth;
			}

			if((TextParams_p->EffectType & (~(MID_GFX_TEXT_NEWLINE|MID_GFX_TEXT_ELLIPSIS))) == 0)
			{
				GFX_PutText(Bitmap_p, AactualFont_p, CharIndex, XOffset+TextParams_p->LeftX, YOffset+TextParams_p->TopY+MaxAscent, TextColor, TextParams_p, Unicode);
			}
			else
			{
				GFX_PutText(&SrcBitmap, AactualFont_p, CharIndex, XOffset, MaxAscent, TextColor, TextParams_p, Unicode);
			}
			
			if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
			{
				XOffset -= TextParams_p->HInterval;
			}
			else
			{
				XOffset += (CharWidth + TextParams_p->HInterval);
			}
		}
	}
	
	if((TextParams_p->EffectType & (~(MID_GFX_TEXT_NEWLINE|MID_GFX_TEXT_ELLIPSIS))) == 0)
	{
		PAL_OSD_PutSrcBitmap(Handle);
		return Error;
	}
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_ZOOM)
	{
//		OSD_ZoomText(&SrcBitmap, &DestBitmap);
	}
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
	{
//		OSD_BoldText(&SrcBitmap, &DestBitmap);
	}
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)
	{
//		Width += (Height / GFX_ITALIC_PITCH);
		Width += (Height >> 1);

		DestBitmap.Width = Width;
		DestBitmap.Height = Height;
		DestBitmap.ColorType = Bitmap_p->ColorType;
		DestBitmap.AspectRatio = Bitmap_p->AspectRatio;
	#if 0
		DestBitmap.Pitch = ((Bitmap_p->Pitch * 8/Bitmap_p->Width) * Width + 7) / 8;
		DestBitmap.Size = DestBitmap.Pitch * Height;
		DestBitmap.Data_p = (void *)GFX_MALLOC(DestBitmap.Size);
		if(DestBitmap.Data_p == NULL)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NO_MEM;
		}
		memset(DestBitmap.Data_p, 0xFF, DestBitmap.Size);
		OSD_ItalicText(&SrcBitmap, &DestBitmap);
		
		GFX_FREE(SrcBitmap.Data_p);
	#else
		Error = PAL_OSD_AllocBitmap(&DestBitmap, Bitmap_p->PlatformPalette_p, FALSE);
		if(Error != TH_NO_ERROR)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NO_MEM;
		}
		memset(DestBitmap.Data_p, 0xFF, DestBitmap.Size);
		GFX_ItalicText(&SrcBitmap, &DestBitmap);
		PAL_OSD_FreeBitmap(&SrcBitmap);
	#endif
		SrcBitmap = DestBitmap;
		DestBitmap.Data_p = NULL;
	}

	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
//		Width += (GFX_OUTLINE_WIDTH * 2);
//		Height += (GFX_OUTLINE_WIDTH * 2);
		Width += (GFX_OUTLINE_WIDTH << 1);
		Height += (GFX_OUTLINE_WIDTH << 1);
		DestBitmap.Width = Width;
		DestBitmap.Height = Height;
		DestBitmap.ColorType = Bitmap_p->ColorType;
		DestBitmap.AspectRatio = Bitmap_p->AspectRatio;
	#if 0
		DestBitmap.Pitch = ((Bitmap_p->Pitch * 8/Bitmap_p->Width) * Width + 7) / 8;
		DestBitmap.Size = DestBitmap.Pitch * Height;
		DestBitmap.Data_p = (void *)GFX_MALLOC(DestBitmap.Size);
		if(DestBitmap.Data_p == NULL)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NO_MEM;
		}
		memset(DestBitmap.Data_p, 0xFF, DestBitmap.Size);
		OSD_OutlineText(&SrcBitmap, &DestBitmap, &(TextParams_p->TextColor), &(TextParams_p->LineColor));

		GFX_FREE(SrcBitmap.Data_p);
	#else
		Error = PAL_OSD_AllocBitmap(&DestBitmap, Bitmap_p->PlatformPalette_p, FALSE);
		if(Error != TH_NO_ERROR)
		{
			PAL_OSD_PutSrcBitmap(Handle);
			return TH_ERROR_NO_MEM;
		}
		memset(DestBitmap.Data_p, 0xFF, DestBitmap.Size);
		GFX_OutlineText(&SrcBitmap, &DestBitmap, &(TextParams_p->TextColor), &(TextParams_p->LineColor));
		PAL_OSD_FreeBitmap(&SrcBitmap);
	#endif
		SrcBitmap = DestBitmap;
		DestBitmap.Data_p = NULL;
	}
	
	if(TextParams_p->PutType & MID_GFX_TEXT_PUT_LEFT)
	{
		Pos.LeftX = TextParams_p->LeftX;
	}
	else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_RIGHT)
	{
		Pos.LeftX = TextParams_p->LeftX + TextParams_p->Width - Width;
	}
	else
	{
//		Pos.LeftX = TextParams_p->LeftX + (TextParams_p->Width - Width) / 2;
		Pos.LeftX = TextParams_p->LeftX + ((TextParams_p->Width - Width) >> 1);
	}
	Pos.TopY = YOffset + TextParams_p->TopY;
	
	memset(&KeyColor, 0xFF, sizeof(PAL_OSD_Color_t));
	KeyColor.Type = Bitmap_p->ColorType;
	PAL_OSD_PutSrcBitmap(Handle);
	
	PAL_OSD_PutKeyColor(Handle, &Pos, &SrcBitmap, &KeyColor);
#if 0
	GFX_FREE(SrcBitmap.Data_p);
#else
	PAL_OSD_FreeBitmap(&SrcBitmap);
#endif
	return Error;
}

TH_Error_t  GFX_RenderText(PAL_OSD_Handle  Handle, GFX_Utf8_t *Utf8_p, GFX_Font_t *Font_p, MID_GFX_PutText_t *TextParams_p, S32 StartChar, S32 *YOffset_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	U8 DoubleLen, NextDoubleLen;
	S32 Prev_count, count, CharIndex, Width, Height, CharWidth, CharAscent, CharDescent, MaxAscent, MaxDescent, StopChar;
	U16 Unicode;
	BOOL Double, NextDouble, bGetPoint, bStartDouble;
	
	count = StartChar;
	Width = 0;
	Height = 0;
	StopChar = StartChar;
	MaxAscent = 0;
	MaxDescent = 0;
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
		Width += (GFX_OUTLINE_WIDTH * 2);
	}
	
	Utf8_p->CurrPos = count;

	bGetPoint = FALSE;
	while((Unicode = GFX_Utf8Read(Utf8_p, TRUE)))
	{
		bGetPoint = FALSE;
		Prev_count = count;
		if(Unicode > 0x0700 || Unicode == 0x20)
		{
			StopChar = count;
			bGetPoint = TRUE;
		}
		count = Utf8_p->CurrPos;
		
		GFX_LocateGlyphForSize(Font_p, TextParams_p->FontSize, Unicode, &CharIndex, &CharWidth, &CharAscent, &CharDescent);

		if(CharIndex != -1)
		{
			if(MaxAscent < CharAscent)
			{
				MaxAscent = CharAscent;
			}
			if(MaxDescent < CharDescent)
			{
				MaxDescent = CharDescent;
			}
			if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
			{
				CharWidth += 1;
			}
			if(MaxDescent > 0)
			{
				Height = MaxAscent + MaxDescent;
			}
			else
			{
				Height = MaxAscent;
			}
			
			Width += CharWidth;
			if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)
			{
		//		Width += (Height / GFX_ITALIC_PITCH);
				Width += (Height >> 1);
			}

			if(Width > TextParams_p->Width)
			{
				Width -= CharWidth;
				if(TextParams_p->EffectType & MID_GFX_TEXT_NEWLINE)
				{
					*YOffset_p = (*YOffset_p) + Height;
					
					if(*YOffset_p > TextParams_p->Height)
					{
						*YOffset_p = (*YOffset_p) - Height - TextParams_p->VInterval;
						if(TextParams_p->PutType & MID_GFX_TEXT_PUT_TOP)
						{
							/*nothing to do*/
						}
						else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_BOT)
						{
							*YOffset_p = TextParams_p->Height;
						}
						else
						{
							*YOffset_p = (*YOffset_p) + ((TextParams_p->Height - (*YOffset_p)) / 2);
						}
						return Error;
					}
					
					*YOffset_p = (*YOffset_p) + TextParams_p->VInterval;
					GFX_RenderText(Handle, Utf8_p, Font_p, TextParams_p, StopChar, YOffset_p);
					
					*YOffset_p = (*YOffset_p) - Height;
					GFX_PutOneLineText(Handle, Utf8_p, Font_p, TextParams_p, StartChar, StopChar, (*YOffset_p));
					*YOffset_p = (*YOffset_p) - TextParams_p->VInterval;
					return Error;
				}
				else
				{
					if(TextParams_p->PutType & MID_GFX_TEXT_PUT_TOP)
					{
						*YOffset_p = 0;
					}
					else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_BOT)
					{
						*YOffset_p = TextParams_p->Height - Height;
					}
					else
					{
						*YOffset_p = (TextParams_p->Height - Height) / 2;
					}
					GFX_PutOneLineText(Handle, Utf8_p, Font_p, TextParams_p, StartChar, Prev_count, (*YOffset_p));
					return Error;
				}
			}
			else
			{
				if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)
				{
			//		Width -= (Height / GFX_ITALIC_PITCH);
					Width -= (Height >> 1);
				}
				Width += TextParams_p->HInterval;
				if(bGetPoint == TRUE)
				{
					StopChar = count;
				}
			}
		}
		else
		{
			if(bGetPoint == TRUE)
			{
				StopChar = count;
			}
		}
	}
	
	if(TextParams_p->EffectType & MID_GFX_TEXT_NEWLINE)
	{
		*YOffset_p = (*YOffset_p) + Height;
		
		if(*YOffset_p > TextParams_p->Height)
		{
			*YOffset_p = (*YOffset_p) - Height - TextParams_p->VInterval;
			if(TextParams_p->PutType & MID_GFX_TEXT_PUT_TOP)
			{
				/*nothing to do*/
			}
			else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_BOT)
			{
				*YOffset_p = TextParams_p->Height;
			}
			else
			{
				*YOffset_p = (*YOffset_p) + ((TextParams_p->Height - (*YOffset_p)) / 2);
			}
			return Error;
		}
		
		if(TextParams_p->PutType & MID_GFX_TEXT_PUT_TOP)
		{
			/*nothing to do*/
		}
		else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_BOT)
		{
			*YOffset_p = TextParams_p->Height;
		}
		else
		{
			*YOffset_p = (*YOffset_p) + ((TextParams_p->Height - (*YOffset_p)) / 2);
		}

		*YOffset_p = (*YOffset_p) - Height;
		GFX_PutOneLineText(Handle, Utf8_p, Font_p, TextParams_p, StartChar, count, (*YOffset_p));
		*YOffset_p = (*YOffset_p) - TextParams_p->VInterval;
	}
	else
	{
		if(TextParams_p->PutType & MID_GFX_TEXT_PUT_TOP)
		{
			*YOffset_p = 0;
		}
		else if(TextParams_p->PutType & MID_GFX_TEXT_PUT_BOT)
		{
			*YOffset_p = TextParams_p->Height - Height;
		}
		else
		{
			*YOffset_p = (TextParams_p->Height - Height) / 2;
		}
		GFX_PutOneLineText(Handle, Utf8_p, Font_p, TextParams_p, StartChar, count, (*YOffset_p));
	}
	
	return Error;
}

#endif

S32  GFX_RenderTextForNextPageOffset(GFX_Utf8_t	*Utf8_p,
													GFX_Font_t *Font_p,
													MID_GFX_PutText_t *TextParams_p)
{
	S32 Prev_count, count, CharIndex, Width, Height, CharWidth, CharAscent, CharDescent, MaxAscent, MaxDescent, StopChar, NextStop, CharLeft;
	U16 Unicode, PrevUnicode;
	BOOL bGetPoint, bFirstChar, MonoBmp = FALSE, bForceNewLine;
	S32 PageOffset, TotalHeight, CharBitW, ExtWidth;
	
	count = Utf8_p->CurrPos;
	Width = 0;
	Height = 0;
	StopChar = count;
	MaxAscent = 0;
	MaxDescent = 0;

	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
//		Width += (GFX_OUTLINE_WIDTH * 2);
		Width += (GFX_OUTLINE_WIDTH << 1);
	}
	if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
	{
		MonoBmp = TRUE;
	}

	bGetPoint = FALSE;
	bFirstChar = TRUE;
	PageOffset = count;
	TotalHeight = 0;
	NextStop = -1;
	PrevUnicode = 0;
	while((Unicode = GFX_Utf8Read(Utf8_p, TRUE)))
	{
		Prev_count = count;
		if(Unicode >= 0x600 && Unicode <= 0x6FF)/*arabic char*/
		{
			S32 TempPos = Utf8_p->CurrPos;
			U16 PrePrev = 0, Next = 0;
			int  IgnoreNext;
			
			Next = GFX_Utf8Read(Utf8_p, TRUE);
			if(Next != 0)
			{
				PrePrev = GFX_Utf8Read(Utf8_p, FALSE);
			}
			Utf8_p->CurrPos = TempPos;
			
			Unicode = GxGetPresentationForm(PrePrev, Unicode, Next, PrevUnicode, &IgnoreNext);
			TextParams_p->WriteType |= MID_GFX_TEXT_WRITE_R_TO_L;
		}
		PrevUnicode = Unicode;
		bForceNewLine = FALSE;
		if(GFX_CheckBreakCode(Unicode))
		{
			StopChar = count;
			NextStop = count + 1;
			bGetPoint = TRUE;
			if(Unicode == 0x0A)
			{
				StopChar ++;
				bForceNewLine = TRUE;
			}
		}
		else
		{
			NextStop = -1;
		}
		count = Utf8_p->CurrPos;
		
		GFX_LocateGlyphForSize(Font_p, TextParams_p->FontSize, Unicode, MonoBmp,
								&CharIndex, &CharWidth, &CharLeft, &CharBitW, &CharAscent, &CharDescent);
		if(CharIndex != -1)
		{
			if(MaxAscent < CharAscent)
			{
				MaxAscent = CharAscent;
			}
			if(MaxDescent < CharDescent)
			{
				MaxDescent = CharDescent;
			}
			
			if(TextParams_p->EffectType & MID_GFX_TEXT_BOLD)
			{
				CharWidth += 1;
			}
			if(TextParams_p->WriteType & MID_GFX_TEXT_WRITE_R_TO_L)
			{
				if(bFirstChar)
				{
					if(CharLeft + CharBitW > CharWidth)/*如果第一个字符要向前缩进*/
					{
						CharWidth += (CharLeft + CharBitW - CharWidth);
					}
					bFirstChar = FALSE;
				}
				ExtWidth = 0;
				if(CharLeft < 0)/*如果最后一个字符是向后扩展*/
				{
					ExtWidth = (-CharLeft);
					CharWidth += ExtWidth;
				}
			}
			else
			{
				if(bFirstChar)
				{
					if(CharLeft < 0)
					{
						CharWidth += (-CharLeft);
					}
					bFirstChar = FALSE;
				}
				ExtWidth = 0;
				if(CharLeft + CharBitW > CharWidth)/*最后一个字符的扩展宽度*/
				{
					ExtWidth = (CharLeft + CharBitW - CharWidth);
					CharWidth += ExtWidth;
				}
			}
			if(MaxDescent > 0)
			{
				Height = MaxAscent + MaxDescent;
			}
			else
			{
				Height = MaxAscent;
			}
			if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
			{
				Height += (GFX_OUTLINE_WIDTH << 1);
			}
			
			Width += CharWidth;
			if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)
			{
//				Width += (Height / GFX_ITALIC_PITCH);
				Width += (Height >> 1);
			}

			if((Width > TextParams_p->Width) || bForceNewLine)/*进行换行*/
			{
				bFirstChar = TRUE;
				Width -= CharWidth;
				TotalHeight += Height;
				if(TotalHeight > TextParams_p->Height)/*超出高度*/
				{
					return PageOffset;/*返回当前行的第一个字符位置*/
				}
				TotalHeight += TextParams_p->VInterval;
				if(bGetPoint)
				{
					count = StopChar;
				}
				else
				{
					count = Prev_count;
				}
				if(TotalHeight >= TextParams_p->Height)/*超出高度*/
				{
					return count;/*返回下一行的第一个字符位置*/
				}

				/*宽度重新开始计算*/
				PageOffset = count;/*将每一行开始的位置当做页偏移*/
				Utf8_p->CurrPos = count;
				bGetPoint = FALSE;
				Width = 0;
				Height = 0;
				StopChar = count;
				MaxAscent = 0;
				MaxDescent = 0;
				if(TextParams_p->EffectType & MID_GFX_TEXT_OUTLINE)
				{
			//		Width += (GFX_OUTLINE_WIDTH * 2);
					Width += (GFX_OUTLINE_WIDTH << 1);
				}
			}
			else
			{
				if(TextParams_p->EffectType & MID_GFX_TEXT_ITALIC)/*如果还没有换行不能加上斜体的宽度*/
				{
			//		Width -= (Height / GFX_ITALIC_PITCH);
					Width -= (Height >> 1);
				}
				Width -= ExtWidth;
				
				Width += TextParams_p->HInterval;
				if(bGetPoint == TRUE)
				{
					if(NextStop != -1)
					{
						StopChar = NextStop;
					}
				}
			}
		}
		else
		{
			bGetPoint = TRUE;
			StopChar = count;
		}
	}
	
	return 0;/*没有换行或换页*/
}

TH_Error_t  GFX_DrawText(PAL_OSD_Handle  Handle, MID_GFX_PutText_t *TextParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	S32 Y_Offset = 0;
	GFX_Font_t	*Font_p;
	GFX_Utf8_t	*Utf8_p;
	
	if(TextParams_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	if(TextParams_p->Str_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	if(TextParams_p->Str_p[0] == 0)
	{
		return Error;
	}
	Font_p = GFX_GetFont(TextParams_p->FontName_p);
	Utf8_p = GFX_ConstructUtf8(TextParams_p->Str_p);

#if (GFX_ENABLE_IDX_CACHE)
	{
		GFX_CharData_t *StrData_p;
		StrData_p = GFX_ConstructStrData(Font_p, Utf8_p, TextParams_p);
		if(StrData_p != NULL)
		{
			if(TextParams_p->WriteType == MID_GFX_TEXT_WRITE_R_TO_L)
			{
				GFX_PrepareArabicStr(StrData_p, Font_p, TextParams_p);
			}
			Error = GFX_RenderText(Handle, TextParams_p, 0, StrData_p, &Y_Offset);
			GFX_DestructStrData(&StrData_p);
		}
	}
#else
	Error = GFX_RenderText(Handle, Font_p, TextParams_p, 0, FALSE, 0, &Y_Offset);
#endif

	GFX_DestructUtf8(&Utf8_p);
	GFX_PutFont(&Font_p);
	
	if(Error == TH_NO_ERROR)
	{
		PAL_OSD_Win_t DirtyWin;
		DirtyWin.LeftX = TextParams_p->LeftX;
		DirtyWin.TopY = TextParams_p->TopY;
		DirtyWin.Width = TextParams_p->Width;
		DirtyWin.Height = TextParams_p->Height;
		Error = PAL_OSD_SetDirtyWin(Handle, &DirtyWin);
	}

	return Error;
}

TH_Error_t  GFX_DrawTriangle(PAL_OSD_Handle  Handle, MID_GFX_PutGraph_t *GraphParams_p)
{
	S32 temp, widthmax, width, posx, x1, x2, x3, y1, y2, y3;
	S32 loop;
	PAL_OSD_Win_t RectWin;

	x1 = GraphParams_p->Params.Triangle.PosX[0];
	x2 = GraphParams_p->Params.Triangle.PosX[1];
	x3 = GraphParams_p->Params.Triangle.PosX[2];
	y1 = GraphParams_p->Params.Triangle.PosY[0];
	y2 = GraphParams_p->Params.Triangle.PosY[1];
	y3 = GraphParams_p->Params.Triangle.PosY[2];
      if( y1 > y2 )
       {
       	temp = x1; x1 = x2; x2 = temp;
		temp = y1; y1 = y2; y2 = temp;
       }
	 if( y1 > y3 )
	 {
	 	temp = x1; x1 = x3; x3 = temp;
		temp = y1; y1 = y3; y3 = temp;
	 }
	  if( y2 > y3 )
	 {
	 	temp = x2; x2 = x3; x3 = temp;
		temp = y2; y2 = y3; y3 = temp;
	 }
	temp = x1 + ( x3 - x1) * (y2-y1) / (y3 - y1 );
	if( x2 > temp )
		widthmax = x2 - temp;
	else 
		widthmax = temp - x2;
	for(loop = y1+1 ; loop <y2;loop++)
	{
		width = widthmax *(loop - y1)/(y2 -y1);
		posx = x1 + ( loop-y1) * (x2 - x1)/(y2 - y1);
		RectWin.LeftX = posx;
		RectWin.TopY = loop;
		RectWin.Width = width;
		RectWin.Height = 1;
		if(width == 0)
			continue ;
		PAL_OSD_Fill(Handle, &RectWin, &(GraphParams_p->Params.Triangle.Color));
	}
	for(loop = y2; loop <y3;loop++)
	{
		width = widthmax * (y3 -loop) /(y3 -y2);
		posx = x2 + (loop - y2) * (x3 - x2)/(y3 - y2);
		RectWin.LeftX = posx;
		RectWin.TopY = loop;
		RectWin.Width = width;
		RectWin.Height = 1; 
		if(width == 0)
			continue ;
		PAL_OSD_Fill(Handle, &RectWin, &(GraphParams_p->Params.Triangle.Color));
	}
	
	return TH_NO_ERROR;	
}

TH_Error_t  GFX_DrawTriangleUp(PAL_OSD_Handle  Handle, MID_GFX_PutGraph_t *GraphParams_p)
{
	S32 width, loop, PositionX, PosX, PosY, Width, Height;
	PAL_OSD_Win_t RectWin;
	
	PosX = GraphParams_p->Params.RegularTriangle.LeftX;
	PosY = GraphParams_p->Params.RegularTriangle.TopY;
	Width = GraphParams_p->Params.RegularTriangle.Width;
	Height = GraphParams_p->Params.RegularTriangle.Height;
	for(loop = PosY+1; loop <= PosY+ Height; loop ++ )
	{
		PositionX = PosX + (Height + PosY - loop) * Width /(Height << 1);
		width = Width * ( loop -PosY )/Height;
		RectWin.LeftX = PositionX;
		RectWin.TopY = loop;
		RectWin.Width = width;
		RectWin.Height = 1;
		if(width == 0)
			continue ;
		PAL_OSD_Fill(Handle, &RectWin, &(GraphParams_p->Params.RegularTriangle.Color));
	}

	return TH_NO_ERROR;	
}

TH_Error_t  GFX_DrawTriangleDown(PAL_OSD_Handle  Handle, MID_GFX_PutGraph_t *GraphParams_p)
{
	S32 width, loop, PositionX, PosX, PosY, Width, Height;
	PAL_OSD_Win_t RectWin;
	
	PosX = GraphParams_p->Params.RegularTriangle.LeftX;
	PosY = GraphParams_p->Params.RegularTriangle.TopY;
	Width = GraphParams_p->Params.RegularTriangle.Width;
	Height = GraphParams_p->Params.RegularTriangle.Height;
	for(loop = PosY+1; loop <= PosY+ Height; loop ++ )
	{
		PositionX = PosX + (loop - PosY ) * Width /(Height << 1);
		width = Width * ( Height - loop +PosY )/Height;
		RectWin.LeftX = PositionX;
		RectWin.TopY = loop;
		RectWin.Width = width;
		RectWin.Height = 1;
		if(width == 0)
			continue ;
		PAL_OSD_Fill(Handle, &RectWin, &(GraphParams_p->Params.RegularTriangle.Color));
	}

	return TH_NO_ERROR;	
}

TH_Error_t  GFX_DrawTriangleLeft(PAL_OSD_Handle  Handle, MID_GFX_PutGraph_t *GraphParams_p)
{
	S32 height,  loop, PositionY, PosX, PosY, Width, Height;
	PAL_OSD_Win_t RectWin;

	PosX = GraphParams_p->Params.RegularTriangle.LeftX;
	PosY = GraphParams_p->Params.RegularTriangle.TopY;
	Width = GraphParams_p->Params.RegularTriangle.Width;
	Height = GraphParams_p->Params.RegularTriangle.Height;
	for(loop = PosX+1; loop <= PosX + Width; loop ++ )
	{
		PositionY = PosY + ( Width +PosX - loop) * Height /(Width << 1);
		height = Height * ( loop -PosX )/Width;
		RectWin.LeftX = loop;
		RectWin.TopY = PositionY;
		RectWin.Width = 1;
		RectWin.Height = height;
		if(height == 0)
			continue ;
		PAL_OSD_Fill(Handle, &RectWin, &(GraphParams_p->Params.RegularTriangle.Color));
	}

	return TH_NO_ERROR;	
}

TH_Error_t  GFX_DrawTriangleRight(PAL_OSD_Handle  Handle, MID_GFX_PutGraph_t *GraphParams_p)
{
	S32 height, loop, PositionY, PosX, PosY, Width, Height;
	PAL_OSD_Win_t RectWin;
	
	PosX = GraphParams_p->Params.RegularTriangle.LeftX;
	PosY = GraphParams_p->Params.RegularTriangle.TopY;
	Width = GraphParams_p->Params.RegularTriangle.Width;
	Height = GraphParams_p->Params.RegularTriangle.Height;
	for(loop = PosX+1; loop <= PosX + Width; loop ++ )
	{
		PositionY = PosY + (loop - PosX ) * Height /(Width << 1);
		height = Height * ( Width -loop + PosX )/Width;
		RectWin.LeftX = loop;
		RectWin.TopY = PositionY;
		RectWin.Width = 1;
		RectWin.Height = height;
		if(height == 0)
			continue ;
		PAL_OSD_Fill(Handle, &RectWin, &(GraphParams_p->Params.RegularTriangle.Color));
	}

	return TH_NO_ERROR;	
}

TH_Error_t  GFX_DrawGraph(PAL_OSD_Handle  Handle, MID_GFX_PutGraph_t *GraphParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Win_t RectWin;

	switch(GraphParams_p->Type)
	{
		case MID_GFX_GRAPH_RECTANGLE:
			if(GraphParams_p->JustFrame)
			{
				RectWin.LeftX = GraphParams_p->Params.Rectangle.LeftX;
				RectWin.TopY = GraphParams_p->Params.Rectangle.TopY;
				RectWin.Width = GraphParams_p->Params.Rectangle.Width;
				RectWin.Height = GraphParams_p->FrameLineWidth;
				Error = PAL_OSD_Fill(Handle, &RectWin, &(GraphParams_p->Params.Rectangle.Color));
				
				RectWin.LeftX = GraphParams_p->Params.Rectangle.LeftX;
				RectWin.TopY = GraphParams_p->Params.Rectangle.TopY+GraphParams_p->Params.Rectangle.Height-GraphParams_p->FrameLineWidth;
				RectWin.Width = GraphParams_p->Params.Rectangle.Width;
				RectWin.Height = GraphParams_p->FrameLineWidth;
				Error = PAL_OSD_Fill(Handle, &RectWin, &(GraphParams_p->Params.Rectangle.Color));
				
				RectWin.LeftX = GraphParams_p->Params.Rectangle.LeftX;
				RectWin.TopY = GraphParams_p->Params.Rectangle.TopY;
				RectWin.Width = GraphParams_p->FrameLineWidth;
				RectWin.Height = GraphParams_p->Params.Rectangle.Height;
				Error = PAL_OSD_Fill(Handle, &RectWin, &(GraphParams_p->Params.Rectangle.Color));
				
				RectWin.LeftX = GraphParams_p->Params.Rectangle.LeftX+GraphParams_p->Params.Rectangle.Width-GraphParams_p->FrameLineWidth;
				RectWin.TopY = GraphParams_p->Params.Rectangle.TopY;
				RectWin.Width = GraphParams_p->FrameLineWidth;
				RectWin.Height = GraphParams_p->Params.Rectangle.Height;
				Error = PAL_OSD_Fill(Handle, &RectWin, &(GraphParams_p->Params.Rectangle.Color));
			}
			else
			{
				RectWin.LeftX = GraphParams_p->Params.Rectangle.LeftX;
				RectWin.TopY = GraphParams_p->Params.Rectangle.TopY;
				RectWin.Width = GraphParams_p->Params.Rectangle.Width;
				RectWin.Height = GraphParams_p->Params.Rectangle.Height;
				//GraphParams_p->Copy = FALSE;
				Error = PAL_OSD_FillEx(Handle, &RectWin, &(GraphParams_p->Params.Rectangle.Color), GraphParams_p->Copy);
			}
			break;
		case MID_GFX_GRAPH_TRIANGLE:
			Error = GFX_DrawTriangle(Handle, GraphParams_p);
			break;
		case MID_GFX_GRAPH_TRIANGLE_UP:
			Error = GFX_DrawTriangleUp(Handle, GraphParams_p);
			break;
		case MID_GFX_GRAPH_TRIANGLE_DOWN:
			Error = GFX_DrawTriangleDown(Handle, GraphParams_p);
			break;
		case MID_GFX_GRAPH_TRIANGLE_LEFT:
			Error = GFX_DrawTriangleLeft(Handle, GraphParams_p);
			break;
		case MID_GFX_GRAPH_TRIANGLE_RIGHT:
			Error = GFX_DrawTriangleRight(Handle, GraphParams_p);
			break;
		case MID_GFX_GRAPH_CIRCLE:
		case MID_GFX_GRAPH_LINE:
		default:
			Error = TH_ERROR_NOT_SUPPORT;
			break;
	}
	
	return Error;
}

TH_Error_t  GFX_DrawFade(PAL_OSD_Handle  Handle, MID_GFX_Fade_t *FadeParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	PAL_OSD_Bitmap_t *Bitmap_p, OsdBitmap;
	U32 *DestPtr_U32p;
	int i, j;
	U32	Alpha, SrcAlpha;
	
	Error = PAL_OSD_GetSrcBitmap(Handle, &OsdBitmap);
	if(Error != TH_NO_ERROR)
	{
		return Error;
	}

	Bitmap_p = &OsdBitmap;
	
	DestPtr_U32p = (U32 *)(((U8 *)(Bitmap_p->Data_p)) + (FadeParams_p->TopY * Bitmap_p->Pitch) + (FadeParams_p->LeftX << 2));

	if(FadeParams_p->Direction == MID_GFX_FADE_FROM_LEFT)
	{
		for( i = 0; i <FadeParams_p->Height; i++ )
		{
			Alpha = FadeParams_p->MaxAlpha;
			for( j = 0; j < (FadeParams_p->Width-FadeParams_p->FadeSpace); j++ )
			{
				SrcAlpha = ((DestPtr_U32p[j]) >> 24);
				if(SrcAlpha > Alpha)
				{
					DestPtr_U32p[j] = ((DestPtr_U32p[j] & 0x00FFFFFF) | (Alpha << 24));
				}
			}
			
			for( ; j < FadeParams_p->Width; j++ )
			{
				SrcAlpha = ((DestPtr_U32p[j]) >> 24);
				if(SrcAlpha > Alpha)
				{
					DestPtr_U32p[j] = ((DestPtr_U32p[j] & 0x00FFFFFF) | (Alpha << 24));
				}
				Alpha -= ((Alpha - FadeParams_p->MinAlpha)/(FadeParams_p->Width-j));
			}
			
			DestPtr_U32p += Bitmap_p->Width;
		}
	}
	else
	{
		for( i = 0; i <FadeParams_p->Height; i++ )
		{
			Alpha = FadeParams_p->MaxAlpha;
			for( j = FadeParams_p->Width-1; j >= FadeParams_p->FadeSpace; j-- )
			{
				SrcAlpha = ((DestPtr_U32p[j]) >> 24);
				if(SrcAlpha > Alpha)
				{
					DestPtr_U32p[j] = ((DestPtr_U32p[j] & 0x00FFFFFF) | (Alpha << 24));
				}
			}
			
			for( ; j >= 0; j-- )
			{
				SrcAlpha = ((DestPtr_U32p[j]) >> 24);
				if(SrcAlpha > Alpha)
				{
					DestPtr_U32p[j] = ((DestPtr_U32p[j] & 0x00FFFFFF) | (Alpha << 24));
				}
				if(j)
				{
					Alpha -= ((Alpha-FadeParams_p->MinAlpha)/(j));
				}
			}
			
			DestPtr_U32p += Bitmap_p->Width;
		}
	}

	PAL_OSD_PutSrcBitmap(Handle);
	return Error;
}

/*****************************global function define*******************************/

/*******************************************************************************
函数名称	: MID_GFX_Init

函数功能	: GFX 初始化

函数参数	: 无
				 

函数返回	: 是否初始化成功

作者			|					修订					|	日期

陈慧明							创建						2007.04.25

*******************************************************************************/
TH_Error_t  MID_GFX_Init(void)
{
	TH_Error_t	Error = TH_NO_ERROR;
	
	Error = GFX_FontInit();
#if (GFX_ENABLE_PNG_CACHE)
	GFX_PngCacheInit();
	PAL_OSD_SetCacheFree(GFX_PngCacheFree);
#endif
	return Error;
}

/*******************************************************************************
函数名称	: MID_GFX_Term

函数功能	: GFX 结束

函数参数	: 无
				 

函数返回	: 是否结束成功

作者			|					修订					|	日期

陈慧明							创建						2007.04.25

*******************************************************************************/
TH_Error_t  MID_GFX_Term(void)
{
	TH_Error_t	Error = TH_NO_ERROR;
#if (GFX_ENABLE_PNG_CACHE)
	GFX_PngCacheTerm();
#endif
	return Error;
}

/*******************************************************************************
函数名称	: MID_GFX_Draw
函数功能	: 画指定的图形

函数参数	: IN:		Handle			OSD 控制句柄
				  IN:		ObjParams_p		画图控制结构
				 

函数返回	: 是否成功

作者			|					修订					|	日期

陈慧明							创建						2007.04.25

*******************************************************************************/
TH_Error_t  MID_GFX_Draw(PAL_OSD_Handle  Handle, MID_GFX_Obj_t *ObjParams_p)
{
	TH_Error_t Error = TH_NO_ERROR;

	if(ObjParams_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	
	switch(ObjParams_p->ObjType)
	{
		case MID_GFX_OBJ_BMP:
			Error = GFX_DrawBmp(Handle, &(ObjParams_p->Obj.Bmp));
			break;
		case MID_GFX_OBJ_TEXT:
			Error = GFX_DrawText(Handle, &(ObjParams_p->Obj.Text));
			break;
		case MID_GFX_OBJ_GRAPH:
			Error = GFX_DrawGraph(Handle, &(ObjParams_p->Obj.Graph));
			break;
		case MID_GFX_OBJ_FADE:
			Error = GFX_DrawFade(Handle, &(ObjParams_p->Obj.Fade));
			break;
		default:
			Error = TH_ERROR_NOT_SUPPORT;
			break;
	}

	return Error;
}

/*******************************************************************************
函数名称	: MID_GFX_GetTextSize
函数功能	: 获取指定的字符串的尺寸

函数参数	: IN/OUT:		PutText_p			字符串的显示参数，
								如果指定的宽度为负数则获取宽度和一行的高度,
								如果指定的高度为负数并且宽度有效则获取限定宽度时的高度
				 

函数返回	: 是否成功

作者			|					修订					|	日期

陈慧明							创建						2007.04.25

*******************************************************************************/
TH_Error_t  MID_GFX_GetTextSize(MID_GFX_PutText_t *PutText_p)
{
	TH_Error_t Error = TH_NO_ERROR;
	GFX_Font_t	*Font_p;
	GFX_Utf8_t	*Utf8_p;
	GFX_CharData_t *StrData_p;
	S32 Width, Height, MaxAscent;
	
	if(PutText_p == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	Font_p = GFX_GetFont(PutText_p->FontName_p);

	Utf8_p = GFX_ConstructUtf8(PutText_p->Str_p);
	if(Utf8_p == NULL)
	{
		GFX_PutFont(&Font_p);
		return TH_ERROR_NO_MEM;
	}

	StrData_p = GFX_ConstructStrData(Font_p, Utf8_p, PutText_p);
	if(StrData_p == NULL)
	{
		GFX_DestructUtf8(&Utf8_p);
		GFX_PutFont(&Font_p);
		return TH_ERROR_NO_MEM;
	}
	if(PutText_p->WriteType == MID_GFX_TEXT_WRITE_R_TO_L)
	{
		GFX_PrepareArabicStr(StrData_p, Font_p, PutText_p);
	}

	if(PutText_p->Width < 0)
	{
		
//		printf("fun=%s,line=%d,The PutText_p->Width=%d\n",__FUNCTION__,__LINE__,PutText_p->Width);
		Width = 0;
		Height = 0;
		
		GFX_GetTextSizeUseIdxCache(PutText_p, 0, Utf8_p->Count, StrData_p, &Width, &Height, &MaxAscent, FALSE, NULL);
		PutText_p->Width = Width;
		PutText_p->Height = Height;
		
	}
	else
	{
	
		Height = 0;
		GFX_RenderTextForSize(PutText_p, 0, StrData_p, &Height);
		PutText_p->Height = Height;
		
	}
	
	GFX_DestructStrData(&StrData_p);
	GFX_DestructUtf8(&Utf8_p);
	GFX_PutFont(&Font_p);

	return Error;
}

/*******************************************************************************
函数名称	: MID_GFX_GetTextPageNum
函数功能	: 获取指定的字符串在指定尺寸下的页数

函数参数	: IN:		PutText_p			字符串的显示参数
				 

函数返回	: 页数

作者			|					修订					|	日期

陈慧明							创建						2007.04.25

*******************************************************************************/
U32  MID_GFX_GetTextPageNum(MID_GFX_PutText_t *PutText_p)
{
	GFX_Font_t	*Font_p;
	GFX_Utf8_t	*Utf8_p;
	GFX_CharData_t *StrData_p;
	S32 Height, PageNum;
	
	if(PutText_p == NULL)
	{
		return 0;
	}
	if(PutText_p->Width < 0)
	{
		return 0;
	}

	if ( NULL == PutText_p->Str_p ) {
		return 0;
	}
	
	Font_p = GFX_GetFont(PutText_p->FontName_p);
	Utf8_p = GFX_ConstructUtf8(PutText_p->Str_p);
	if(Utf8_p == NULL)
	{
		GFX_PutFont(&Font_p);
		return 0;
	}

	StrData_p = GFX_ConstructStrData(Font_p, Utf8_p, PutText_p);
	if(StrData_p == NULL)
	{
		GFX_DestructUtf8(&Utf8_p);
		GFX_PutFont(&Font_p);
		return 0;
	}
	
	Height = 0;
	GFX_RenderTextForSize(PutText_p, 0, StrData_p, &Height);/*获取限定宽度下的高度*/
	PageNum = Height / PutText_p->Height;
	if(Height % PutText_p->Height)
	{
		PageNum ++;
	}
	
	GFX_DestructStrData(&StrData_p);
	GFX_DestructUtf8(&Utf8_p);
	GFX_PutFont(&Font_p);

	return PageNum;
}

/*******************************************************************************
函数名称	: MID_GFX_GetTextNextPageOffset
函数功能	: 获取指定的字符串在指定尺寸下的下一页的起始字符的偏移

函数参数	: IN:		PutText_p			字符串的显示参数
				 

函数返回	: 下一页的起始字符的偏移

作者			|					修订					|	日期

陈慧明							创建						2007.04.25

*******************************************************************************/
S32  MID_GFX_GetTextNextPageOffset(MID_GFX_PutText_t *PutText_p)
{
	GFX_Font_t	*Font_p;
	GFX_Utf8_t	*Utf8_p;
	S32			Offset;
	
	Font_p = GFX_GetFont(PutText_p->FontName_p);
	Utf8_p = GFX_ConstructUtf8(PutText_p->Str_p);
	if(Utf8_p == NULL)
	{
		GFX_PutFont(&Font_p);
		return -1;
	}

	Offset = GFX_RenderTextForNextPageOffset(Utf8_p, Font_p, PutText_p);
	
	GFX_DestructUtf8(&Utf8_p);
	GFX_PutFont(&Font_p);
	
	return Offset;
}


void  MID_GFX_CleanBmpCache(void *BmpData_p)
{
#if (GFX_ENABLE_PNG_CACHE)
	GFX_PngCache_t *CacheItem_p, *NextItem_p;

again:
	THOS_SemaphoreWait(PngCacheAccess_p);
	CacheItem_p = PngCacheList_p;
	
	while(CacheItem_p)
	{
		NextItem_p = (GFX_PngCache_t *)(CacheItem_p->next);
		if(CacheItem_p->PngData.PngData_p == BmpData_p)
		{
			if(CacheItem_p->ReferCount == 0)
			{
				if(CacheItem_p->prev == NULL)
				{
					PngCacheList_p = (GFX_PngCache_t *)(CacheItem_p->next);
				}
				else
				{
					CacheItem_p->prev->next = CacheItem_p->next;
				}
				if(CacheItem_p->next != NULL)
				{
					CacheItem_p->next->prev = CacheItem_p->prev;
				}
				
				PngCachedSize -= CacheItem_p->PngBitmap.Size;
				PAL_OSD_FreeBitmap(&(CacheItem_p->PngBitmap));
				GFX_FREE(CacheItem_p);
			}
			else
			{
				THOS_SemaphoreSignal(PngCacheAccess_p);
				THOS_TaskDelay(10);
				goto again;
			}
			break;
		}
			
		CacheItem_p = NextItem_p;
	}
	THOS_SemaphoreSignal(PngCacheAccess_p);
#endif
}

void  MID_GFX_CleanAllBmpCache(void)
{
#if (GFX_ENABLE_PNG_CACHE)
	GFX_PngCacheFree(TRUE);
#endif
}

void MID_GFX_SwitchHwJpeg(BOOL Switch)
{
	GFX_EnableHwJpg = Switch;
}

/*end of gfx.c*/


