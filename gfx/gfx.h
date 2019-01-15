
/*****************************************************************************
�ļ�����	: mid_gfx.h
��Ȩ����	: Tech Home 2007-2010
�ļ�����	: gfx header

����			|					�޶�					|	����
�»���							����						2007.04.24

*****************************************************************************/

#ifndef __MID_GFX_H__
#define __MID_GFX_H__

/*=======include standard header file======*/


/*=======include pal header file======*/
#include "osd.h"


/*=======include local header file======*/


/* C++ support */
#if defined __cplusplus
    extern "C" {
#endif

/*****************************const define*************************************/

/*****************************struct define*************************************/

typedef enum
{
	MID_GFX_OBJ_BMP,/*��λͼ, ָ����ͼƬ��ʽ*/
	MID_GFX_OBJ_TEXT,/*����*/
	MID_GFX_OBJ_GRAPH,/*ͼ��, ������, ���ε�*/
	MID_GFX_OBJ_FADE
}MID_GFX_ObjType_t;/*��ͼ��������*/

typedef enum
{
	MID_GFX_GRAPH_TRIANGLE,/*������*/
	MID_GFX_GRAPH_TRIANGLE_UP,/*���ϵ���������*/
	MID_GFX_GRAPH_TRIANGLE_DOWN,/*���µ���������*/
	MID_GFX_GRAPH_TRIANGLE_LEFT,/*�������������*/
	MID_GFX_GRAPH_TRIANGLE_RIGHT,/*���ҵ���������*/
	MID_GFX_GRAPH_RECTANGLE,/*����*/
	MID_GFX_GRAPH_CIRCLE,/*Բ��*/
	MID_GFX_GRAPH_LINE/*ֱ��*/
}MID_GFX_GraphType_t;/*ͼ������*/

typedef enum
{
	MID_GFX_TEXT_NORMAL	= 0x00,/*�����Ч��*/
	MID_GFX_TEXT_BOLD		= 0x01,/*�Ӵ�*/
	MID_GFX_TEXT_ITALIC	= 0x02,/*б��*/
	MID_GFX_TEXT_OUTLINE	= 0x04,/*���*/
	MID_GFX_TEXT_ZOOM	= 0x08,/*�Ŵ�*/
	MID_GFX_TEXT_ELLIPSIS	= 0x10,/*�Զ���ʡ�Է�"..."*/
	MID_GFX_TEXT_NEWLINE	= 0x20,/*�Զ�����*/
	MID_GFX_TEXT_X_OFFSET	= 0x40/*����ƫ��*/
}MID_GFX_TextEffect_t;/*д��Ч��, ���Զ��ֻ���һ��*/

typedef enum
{
	MID_GFX_TEXT_WRITE_L_TO_R = 0x00,/*��������*/
	MID_GFX_TEXT_WRITE_R_TO_L = 0x01/*��������*/
}MID_GFX_TextWriteType_t;/*д�ַ�ʽ*/

typedef enum
{
	MID_GFX_TEXT_PUT_CENTER	= 0x00,/*����*/

	MID_GFX_TEXT_PUT_LEFT	= 0x01,/*����*/
	MID_GFX_TEXT_PUT_RIGHT	= 0x02,/*����*/
	
	MID_GFX_TEXT_PUT_TOP		= 0x10,/*����*/
	MID_GFX_TEXT_PUT_BOT		= 0x20/*����*/
}MID_GFX_TextPutType_t;/*�ı����÷�ʽ, ���Զ��ֻ���һ��*/

typedef enum
{
	MID_GFX_COMPRESS_NONE,
	MID_GFX_COMPRESS_RLE,
	MID_GFX_COMPRESS_LZO,
	MID_GFX_COMPRESS_RLE_2,
	MID_GFX_COMPRESS_PNG,
	MID_GFX_COMPRESS_JPEG,
	MID_GFX_COMPRESS_GIF
}MID_GFX_CompressType_t;/*ͼƬ����ѹ������*/

typedef enum
{
	MID_GFX_BMP_PUT_CENTER,	/*λͼ���з���*/
	MID_GFX_BMP_PUT_TILE,		/*λͼƽ�̷���*/
	MID_GFX_BMP_PUT_ZOOM,	/*λͼ�������*/
	MID_GFX_BMP_PUT_RATIO_ZOOM
}MID_GFX_BmpPutType_t;/*λͼ��������*/

typedef struct
{
	/*��Ӧ��λͼ������*/
	S32 SrcLeftX;
	S32 SrcTopY;
	S32 SrcWidth;/*��PNG Ϊ0 ʱ��ʾʹ��ͼƬ���*/
	S32 SrcHeight;/*��PNG Ϊ0 ʱ��ʾʹ��ͼƬ�߶�*/
	
	/*��Ӧ��OSD ��ʾ������*/
	S32 DestLeftX;
	S32 DestTopY;
	S32 DestWidth; /*��PNG Ϊ0 ʱ��ʾʹ��ͼƬ���*/
	S32 DestHeight; /*��PNG Ϊ0 ʱ��ʾʹ��ͼƬ�߶�*/
	
	/*λͼ�ĳߴ磬��PNG ͼ��Ч*/
	S32 BmpWidth;
	S32 BmpHeight;
	
	
	U32	BmpDataRawSize;		/*λͼԭʼ���ݴ�С����PNG ͼ��Ч*/
	U32	BmpDataCompSize;		/*λͼѹ�������ݴ�С*/
	void *BmpData_p;			/*λͼ������ָ��*/
	MID_GFX_BmpPutType_t PutType;	/*���õ����ͣ���PNG ͼ��Ч*/
	PAL_OSD_ColorType_t ColorType;		/*��ɫ���ͣ���PNG ͼ��Ч*/
	PAL_OSD_ColorARGB_t *Palette_p;			/*ɫ�����ݣ���PNG ͼ��Ч*/
	MID_GFX_CompressType_t CompressType;/*ѹ������*/
	BOOL EnableKeyColor;					/*�Ƿ�ʹ��͸��ɫ����PNG ͼ��Ч*/
	PAL_OSD_ColorValue_t KeyColor;			/*͸��ɫֵ����PNG ͼ��Ч*/
	BOOL Mix;								/*�Ƿ�ʹ�û���㷨*/


	BOOL ForceCache;/*ǿ��cache*/
	void *Key;/*ǿ��cache, ��Ӧ�ؼ�����*/
}MID_GFX_PutBmp_t;/*����λͼ�Ľṹ��*/

typedef struct
{
	/*��Ӧ��OSD ��ʾ������*/
	S32 LeftX;
	S32 TopY;
	S32 Width;
	S32 Height;
	
	char *FontName_p;/*��������, Ӧ��Ҫ���������ʱ������һ��*/
	S32 FontSize;/*�ֺ�*/
	
	S32 XOffset;/*ˮƽ�ַ���ʼƫ��*/
	
	S32 HInterval;/*ˮƽ�ַ���ļ��*/
	S32 VInterval;/*��ֱ�м�ļ��*/
	U8 EffectType;/*�ı�Ч��MID_GFX_TextEffect_t*/
	U8 WriteType;/*д�ַ�ʽMID_GFX_TextWriteType_t*/
	U8 PutType;/*�ı����÷�ʽMID_GFX_TextPutType_t*/
	BOOL Force ;
	U8 *Str_p;/*�ַ�������*/
	PAL_OSD_Color_t TextColor;/*�ı���ɫ*/
	PAL_OSD_Color_t LineColor;/*���ʱ�ı�����ɫ*/
	BOOL Mix;				/*�Ƿ�ǿ��ʹ�û���㷨*/
}MID_GFX_PutText_t;/*�����ı��ṹ��*/

typedef struct
{
	MID_GFX_GraphType_t Type;/*ͼ������*/
	BOOL				JustFrame;/*�Ƿ�ֻ�ǻ��߿�*/
	S32					FrameLineWidth;/*�߿��ߴ�*/
	BOOL				Copy;/*�Ƿ���ÿ���, ����ʹ�û��*/
	union
	{
		struct
		{
			S32 PosX[3];
			S32 PosY[3];
			PAL_OSD_Color_t Color;
		}Triangle;/*����������*/
		
		struct
		{
			S32 LeftX;
			S32 TopY;
			S32 Width;
			S32 Height;
			PAL_OSD_Color_t Color;
		}RegularTriangle;/*����������*/

		struct
		{
			S32 LeftX;
			S32 TopY;
			S32 Width;
			S32 Height;
			PAL_OSD_Color_t Color;
		}Rectangle;/*����*/

		struct
		{
			S32 CenterX;
			S32 CenterY;
			S32 Radius;
			PAL_OSD_Color_t Color;
		}Circle;/*Բ*/

		struct
		{
			S32 PosX[2];
			S32 PosY[2];
			PAL_OSD_Color_t Color;
		}Line;/*ֱ��*/
	}Params;
}MID_GFX_PutGraph_t;/*����ͼ�νṹ��*/


typedef enum
{
	MID_GFX_FADE_FROM_LEFT,
	MID_GFX_FADE_FROM_RIGHT
}MID_GFX_FadeDirection_t;

typedef struct
{
	/*��Ӧ��λͼ������*/
	S32 LeftX;
	S32 TopY;
	S32 Width;
	S32 Height;

	S32	FadeSpace;
	
	U8	MinAlpha;
	U8	MaxAlpha;

	MID_GFX_FadeDirection_t	Direction;
}MID_GFX_Fade_t;

typedef struct
{
	MID_GFX_ObjType_t ObjType;/*��������*/
	union
	{
		MID_GFX_PutBmp_t Bmp;/*ͼƬ*/
		
		MID_GFX_PutText_t Text;/*�ı�*/
		
		MID_GFX_PutGraph_t Graph;/*ͼ��*/

		MID_GFX_Fade_t	Fade;
	}Obj;
}MID_GFX_Obj_t;/*ͼ�ζ���ṹ��*/

typedef struct _GFX_GIF_Image {
	int img_top;
	int img_left;
	int img_width;
	int img_height;
	int num_color;
	int enable_trans;
	unsigned int img_trans;
	unsigned int *img_pal;
	unsigned char *img_data;
	int	img_delay_ms;
	int	DisposalMethod;
	struct _GFX_GIF_Image *next;
}MID_GFX_GifImage;

typedef struct
{
	int screen_width;
	int screen_height;
	int bpp;
	int num_color;
	unsigned int *pal;
	MID_GFX_GifImage *each_image;
	MID_GFX_GifImage *cur_image;
}MID_GFX_GifParams_t;

/*==================================================
�ֿⲿ��
==================================================*/
typedef enum
{
	MID_GFX_FONT_TYPE_INVALID,/*��Ч����*/
	MID_GFX_FONT_TYPE_BDF,/*BDF ����*/
	MID_GFX_FONT_TYPE_BMP,/*��������*/
	MID_GFX_FONT_TYPE_TTF/*ʸ���ֿ�*/
}MID_GFX_FontType_t;/*�ֿ�����*/


typedef struct
{
	U8 BoundingBoxW;
	U8 BoundingBoxH;
	S8 BoundingBoxX;
	S8 BoundingBoxY;
	U8 Width;
	U8* GlyphData_p;
}MID_GFX_BdfGlyph_t;/*BDF �ֿ��ַ��ṹ��*/

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
}MID_GFX_BdfFontParams_t;/*BDF �ֿ�ṹ��*/

typedef struct
{
	U8						*FontData_p;/*�ֿ�����*/
	U32						FontDataSize;/*���ݴ�С*/
}MID_GFX_TtfFontParams_t;/*ʸ���ֿ�ṹ��*/

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
}MID_GFX_BmpFontParams_t;/*�����ֿ�*/

typedef struct
{
	MID_GFX_FontType_t  Type;/*�ֿ�����*/
	union{
		MID_GFX_TtfFontParams_t		TtfFontParams;/*ʸ���ֿ�*/
		MID_GFX_BdfFontParams_t		BdfFontParams;/*BDF �ֿ�*/
		MID_GFX_BmpFontParams_t		BmpFontParams;/*�����ֿ�*/
	}Font;
	S32 (*GetCharIndex_p)(U16 Unicode);/*�����ֿ��Unicode ת��������ָ��*/
}MID_GFX_FontParams_t;/*�ֿ�ͳһ�����ṹ��*/

/*****************************gloal data define**********************************/

/*****************************function define*******************************/

TH_Error_t MID_GFX_Init(void);
TH_Error_t MID_GFX_Term(void);
TH_Error_t MID_GFX_Draw(PAL_OSD_Handle  Handle, MID_GFX_Obj_t *ObjParams_p);
TH_Error_t MID_GFX_GetTextSize(MID_GFX_PutText_t *PutText_p);
U32  MID_GFX_GetTextPageNum(MID_GFX_PutText_t *PutText_p);
S32  MID_GFX_GetTextNextPageOffset(MID_GFX_PutText_t *PutText_p);

TH_Error_t MID_GFX_SetDefaultFont(char *FontName_p, MID_GFX_FontParams_t *FontParams_p);
TH_Error_t MID_GFX_AddFont(char *FontName_p, MID_GFX_FontParams_t *FontParams_p);
TH_Error_t MID_GFX_DelFont(char *FontName_p);

void  MID_GFX_CleanBmpCache(void *BmpData_p);
void  MID_GFX_CleanAllBmpCache(void);

MID_GFX_GifParams_t *MID_GFX_GifLoad(const char *GifData_p, const int GifSize);
void MID_GFX_GifFree(MID_GFX_GifParams_t **GifParams_pp);
void MID_GFX_SwitchHwJpeg(BOOL Switch);

/* C++ support */
#if defined __cplusplus
}
#endif

#endif
/*end of mid_gfx.h*/



