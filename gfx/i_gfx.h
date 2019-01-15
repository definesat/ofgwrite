
/*****************************************************************************
�ļ�����	: i_gfx.h
��Ȩ����	: Tech Home 2010-2015
�ļ�����	: gfx internal header

����			|					�޶�					|	����
�»���							����						2010.06.02

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
	U16 *Unicode_p;/*Unicode �����ַ�������Ӧ��*/
	
	MID_GFX_BdfGlyph_t* Glyph_p;/*�ַ�BDF ���ݱ�*/
}GFX_BdfFont_t;/*BDF �ֿ�ṹ��*/

typedef struct
{
	FT_Face					Ft_face;/*ʸ���ֿ���*/
	FT_Int					CurrMapIndex;/*��ǰʹ�ñ��������, ��������ֻʹ��Unicode ��*/
	U8						*FontData_p;/*�ֿ��ļ�·��*/
	U32						FontDataSize;/*���ݴ�С*/
}GFX_TtfFont_t;/*ʸ���ֿ�ṹ��*/

typedef struct
{
	U8 *PixelData_p;/*��������*/
	
	U8 *Width_p;/*ÿ���ַ���ȱ�*/

	U16 StartUnicode;/**/
	U16 EndUnicode;
	U16 *Unicode_p;/*Unicode �����ַ�������Ӧ��*/
	
	U32 Num;
	
	U8 Width;/*�ַ������*/
	U8 Height;/*�ַ�ͳһ�߶�*/
}GFX_BmpFont_t;/*�����ֿ�*/


typedef struct
{
	char 					FontName[32];/*�ֿ�����, ���31 ���ַ�*/
	U32						ReferCount;/*�����ü���, һ�������þͲ���ɾ��*/
	MID_GFX_FontType_t  		Type;/*�ֿ�����*/
	union{
		GFX_TtfFont_t		TtfFont;/*ʸ���ֿ�*/
		GFX_BdfFont_t	BdfFont;/*BDF �ֿ�*/
		GFX_BmpFont_t	BmpFont;/*�����ֿ�*/
	}Font;
	S32 (*GetCharIndex_p)(U16 Unicode);/*�����ֿ��Unicode ת��������ָ��*/
}GFX_Font_t;/*�ֿ�ͳһ�ṹ��*/

typedef struct
{
	GFX_Font_t *Font_p;/*�����ֿ�ָ��*/
	S32	GlyphIdx;/*�ַ����ֿ��е�����*/
	U8	Width;/*�ַ����*/
	S16	Left;/*λͼX ���λ�ã��п���Ϊ����*/
	S16 BitmapW;/*λͼ�Ŀ��*/
	S16	Ascent;/**/
	S16	Descent;
	U16	Unicode;/*Unicode ����*/
	BOOL Arabic;/*�Ƿ������ַ�*/
	U16 BaseCode;/*ֻ���ڰ�����*/
}GFX_CharData_t;/*һ���ַ��Ĳ����ṹ��*/

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


