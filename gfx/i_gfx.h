
/*****************************************************************************
文件名称	: i_gfx.h
版权所有	: Tech Home 2010-2015
文件功能	: gfx internal header

作者			|					修订					|	日期
陈慧明							创建						2010.06.02

*****************************************************************************/

#ifndef __I_GFX_H__
#define __I_GFX_H__

/*=======include standard header file======*/

/*=======include pal header file======*/

/*=======include local header file======*/
#include "gfx.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_CACHE_H
#include FT_STROKER_H

/*****************************const define*************************************/
#define GFX_ENABLE_IDX_CACHE		1

#define GFX_ENABLE_TTF				1
#define GFX_USE_TTF_SBIT			1

#define GFX_USE_TTF_GRAY			1

#define GFX_ALPHA_MAX				0xFF

/*****************************struct define*************************************/

typedef struct
{
	S16 FontAscent;
	S16 FontDescent;
	U32 Num;
	U16 DefaltGlyphs;
	
	U16 StartUnicode;
	U16 EndUnicode;
	U16 *Unicode_p;/*Unicode 码与字符索引对应表*/
	
	MID_GFX_BdfGlyph_t* Glyph_p;/*字符BDF 数据表*/
}GFX_BdfFont_t;/*BDF 字库结构体*/

typedef struct
{
	FT_Face					Ft_face;/*矢量字库句柄*/
	FT_Int					CurrMapIndex;/*当前使用编码表索引, 这里我们只使用Unicode 表*/
	U8						*FontData_p;/*字库文件路径*/
	U32						FontDataSize;/*数据大小*/
}GFX_TtfFont_t;/*矢量字库结构体*/

typedef struct
{
	U8 *PixelData_p;/*点阵数据*/
	
	U8 *Width_p;/*每个字符宽度表*/

	U16 StartUnicode;/**/
	U16 EndUnicode;
	U16 *Unicode_p;/*Unicode 码与字符索引对应表*/
	
	U32 Num;
	
	U8 Width;/*字符最大宽度*/
	U8 Height;/*字符统一高度*/
}GFX_BmpFont_t;/*点阵字库*/


typedef struct
{
	char 					FontName[32];/*字库名称, 最大31 个字符*/
	U32						ReferCount;/*被引用计数, 一旦被引用就不能删除*/
	MID_GFX_FontType_t  		Type;/*字库类型*/
	union{
		GFX_TtfFont_t		TtfFont;/*矢量字库*/
		GFX_BdfFont_t	BdfFont;/*BDF 字库*/
		GFX_BmpFont_t	BmpFont;/*点阵字库*/
	}Font;
	S32 (*GetCharIndex_p)(U16 Unicode);/*特殊字库的Unicode 转索引函数指针*/
}GFX_Font_t;/*字库统一结构体*/

typedef struct
{
	GFX_Font_t *Font_p;/*所属字库指针*/
	S32	GlyphIdx;/*字符在字库中的索引*/
	U8	Width;/*字符宽度*/
	S16	Left;/*位图X 轴的位置，有可能为负数*/
	S16 BitmapW;/*位图的宽度*/
	S16	Ascent;/**/
	S16	Descent;
	U16	Unicode;/*Unicode 编码*/
	BOOL Arabic;/*是否阿拉伯字符*/
	U16 BaseCode;/*只用于阿拉伯*/
}GFX_CharData_t;/*一个字符的参数结构体*/

/*****************************gloal data define**********************************/

#if (GFX_ENABLE_TTF)
extern FT_Library			Ft_library;
extern FTC_Manager		Ft_CacheManager;
extern FTC_SBitCache		Ft_SBmpCache;
extern FTC_CMapCache		Ft_CMapCache;
extern FTC_ImageCache	Ft_ImageCache;
//extern FT_Stroker		Ft_stroker;
#endif

/*****************************function define*******************************/

GFX_Font_t *GFX_GetFont(char *FontName_p);
TH_Error_t GFX_PutFont(GFX_Font_t **Font_pp);
GFX_Font_t *GFX_LocateGlyph(GFX_Font_t *Font_p, U16 Unicode, S32 *GlyphIdx_p);
GFX_Font_t *GFX_LocateGlyphForSize(GFX_Font_t *Font_p, S32 FontSize, U16 Unicode,  BOOL MonoBmp,
								S32 *GlyphIdx_p, S32 *Width_p, S32 *Left_p, S32 *BitmapW_p, S32 *Ascent_p, S32 *Descent_p);
unsigned short GxGetPresentationForm(unsigned short PrePrev, unsigned short Char, unsigned short Next, unsigned short Prev, int* pIgnoreNext);
void GFX_PrepareArabicStr(GFX_CharData_t *StrData_p, GFX_Font_t *Font_p, MID_GFX_PutText_t *TextParams_p);

TH_Error_t GFX_FontInit(void);

#endif


