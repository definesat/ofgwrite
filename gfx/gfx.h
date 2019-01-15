
/*****************************************************************************
文件名称	: mid_gfx.h
版权所有	: Tech Home 2007-2010
文件功能	: gfx header

作者			|					修订					|	日期
陈慧明							创建						2007.04.24

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
	MID_GFX_OBJ_BMP,/*画位图, 指所有图片格式*/
	MID_GFX_OBJ_TEXT,/*文字*/
	MID_GFX_OBJ_GRAPH,/*图形, 三角形, 矩形等*/
	MID_GFX_OBJ_FADE
}MID_GFX_ObjType_t;/*画图对像类型*/

typedef enum
{
	MID_GFX_GRAPH_TRIANGLE,/*三角形*/
	MID_GFX_GRAPH_TRIANGLE_UP,/*向上等腰三角形*/
	MID_GFX_GRAPH_TRIANGLE_DOWN,/*向下等腰三角形*/
	MID_GFX_GRAPH_TRIANGLE_LEFT,/*向左等腰三角形*/
	MID_GFX_GRAPH_TRIANGLE_RIGHT,/*向右等腰三角形*/
	MID_GFX_GRAPH_RECTANGLE,/*矩形*/
	MID_GFX_GRAPH_CIRCLE,/*圆形*/
	MID_GFX_GRAPH_LINE/*直线*/
}MID_GFX_GraphType_t;/*图形类型*/

typedef enum
{
	MID_GFX_TEXT_NORMAL	= 0x00,/*无添加效果*/
	MID_GFX_TEXT_BOLD		= 0x01,/*加粗*/
	MID_GFX_TEXT_ITALIC	= 0x02,/*斜体*/
	MID_GFX_TEXT_OUTLINE	= 0x04,/*描边*/
	MID_GFX_TEXT_ZOOM	= 0x08,/*放大*/
	MID_GFX_TEXT_ELLIPSIS	= 0x10,/*自动加省略符"..."*/
	MID_GFX_TEXT_NEWLINE	= 0x20,/*自动换行*/
	MID_GFX_TEXT_X_OFFSET	= 0x40/*横向偏移*/
}MID_GFX_TextEffect_t;/*写字效果, 可以多种或在一起*/

typedef enum
{
	MID_GFX_TEXT_WRITE_L_TO_R = 0x00,/*从左往右*/
	MID_GFX_TEXT_WRITE_R_TO_L = 0x01/*从右往左*/
}MID_GFX_TextWriteType_t;/*写字方式*/

typedef enum
{
	MID_GFX_TEXT_PUT_CENTER	= 0x00,/*居中*/

	MID_GFX_TEXT_PUT_LEFT	= 0x01,/*居左*/
	MID_GFX_TEXT_PUT_RIGHT	= 0x02,/*居右*/
	
	MID_GFX_TEXT_PUT_TOP		= 0x10,/*居上*/
	MID_GFX_TEXT_PUT_BOT		= 0x20/*居下*/
}MID_GFX_TextPutType_t;/*文本放置方式, 可以多种或在一起*/

typedef enum
{
	MID_GFX_COMPRESS_NONE,
	MID_GFX_COMPRESS_RLE,
	MID_GFX_COMPRESS_LZO,
	MID_GFX_COMPRESS_RLE_2,
	MID_GFX_COMPRESS_PNG,
	MID_GFX_COMPRESS_JPEG,
	MID_GFX_COMPRESS_GIF
}MID_GFX_CompressType_t;/*图片数据压缩类型*/

typedef enum
{
	MID_GFX_BMP_PUT_CENTER,	/*位图居中放置*/
	MID_GFX_BMP_PUT_TILE,		/*位图平铺放置*/
	MID_GFX_BMP_PUT_ZOOM,	/*位图拉伸放置*/
	MID_GFX_BMP_PUT_RATIO_ZOOM
}MID_GFX_BmpPutType_t;/*位图放置类型*/

typedef struct
{
	/*对应于位图的坐标*/
	S32 SrcLeftX;
	S32 SrcTopY;
	S32 SrcWidth;/*对PNG 为0 时表示使用图片宽度*/
	S32 SrcHeight;/*对PNG 为0 时表示使用图片高度*/
	
	/*对应于OSD 显示的坐标*/
	S32 DestLeftX;
	S32 DestTopY;
	S32 DestWidth; /*对PNG 为0 时表示使用图片宽度*/
	S32 DestHeight; /*对PNG 为0 时表示使用图片高度*/
	
	/*位图的尺寸，对PNG 图无效*/
	S32 BmpWidth;
	S32 BmpHeight;
	
	
	U32	BmpDataRawSize;		/*位图原始数据大小，对PNG 图无效*/
	U32	BmpDataCompSize;		/*位图压缩后数据大小*/
	void *BmpData_p;			/*位图的数据指针*/
	MID_GFX_BmpPutType_t PutType;	/*放置的类型，对PNG 图无效*/
	PAL_OSD_ColorType_t ColorType;		/*颜色类型，对PNG 图无效*/
	PAL_OSD_ColorARGB_t *Palette_p;			/*色板数据，对PNG 图无效*/
	MID_GFX_CompressType_t CompressType;/*压缩类型*/
	BOOL EnableKeyColor;					/*是否使能透明色，对PNG 图无效*/
	PAL_OSD_ColorValue_t KeyColor;			/*透明色值，对PNG 图无效*/
	BOOL Mix;								/*是否使用混合算法*/


	BOOL ForceCache;/*强制cache*/
	void *Key;/*强制cache, 对应关键数据*/
}MID_GFX_PutBmp_t;/*放置位图的结构体*/

typedef struct
{
	/*对应于OSD 显示的坐标*/
	S32 LeftX;
	S32 TopY;
	S32 Width;
	S32 Height;
	
	char *FontName_p;/*字体名称, 应该要跟添加字体时的名称一致*/
	S32 FontSize;/*字号*/
	
	S32 XOffset;/*水平字符起始偏移*/
	
	S32 HInterval;/*水平字符间的间隔*/
	S32 VInterval;/*垂直行间的间隔*/
	U8 EffectType;/*文本效果MID_GFX_TextEffect_t*/
	U8 WriteType;/*写字方式MID_GFX_TextWriteType_t*/
	U8 PutType;/*文本放置方式MID_GFX_TextPutType_t*/
	BOOL Force ;
	U8 *Str_p;/*字符串数据*/
	PAL_OSD_Color_t TextColor;/*文本颜色*/
	PAL_OSD_Color_t LineColor;/*描边时的边线颜色*/
	BOOL Mix;				/*是否强制使用混合算法*/
}MID_GFX_PutText_t;/*放置文本结构体*/

typedef struct
{
	MID_GFX_GraphType_t Type;/*图形类型*/
	BOOL				JustFrame;/*是否只是画边框*/
	S32					FrameLineWidth;/*边框线粗*/
	BOOL				Copy;/*是否采用拷贝, 否则使用混合*/
	union
	{
		struct
		{
			S32 PosX[3];
			S32 PosY[3];
			PAL_OSD_Color_t Color;
		}Triangle;/*任意三角形*/
		
		struct
		{
			S32 LeftX;
			S32 TopY;
			S32 Width;
			S32 Height;
			PAL_OSD_Color_t Color;
		}RegularTriangle;/*等腰三角形*/

		struct
		{
			S32 LeftX;
			S32 TopY;
			S32 Width;
			S32 Height;
			PAL_OSD_Color_t Color;
		}Rectangle;/*矩形*/

		struct
		{
			S32 CenterX;
			S32 CenterY;
			S32 Radius;
			PAL_OSD_Color_t Color;
		}Circle;/*圆*/

		struct
		{
			S32 PosX[2];
			S32 PosY[2];
			PAL_OSD_Color_t Color;
		}Line;/*直线*/
	}Params;
}MID_GFX_PutGraph_t;/*放置图形结构体*/


typedef enum
{
	MID_GFX_FADE_FROM_LEFT,
	MID_GFX_FADE_FROM_RIGHT
}MID_GFX_FadeDirection_t;

typedef struct
{
	/*对应于位图的坐标*/
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
	MID_GFX_ObjType_t ObjType;/*对像类型*/
	union
	{
		MID_GFX_PutBmp_t Bmp;/*图片*/
		
		MID_GFX_PutText_t Text;/*文本*/
		
		MID_GFX_PutGraph_t Graph;/*图形*/

		MID_GFX_Fade_t	Fade;
	}Obj;
}MID_GFX_Obj_t;/*图形对像结构体*/

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
字库部分
==================================================*/
typedef enum
{
	MID_GFX_FONT_TYPE_INVALID,/*无效类型*/
	MID_GFX_FONT_TYPE_BDF,/*BDF 类型*/
	MID_GFX_FONT_TYPE_BMP,/*点阵类型*/
	MID_GFX_FONT_TYPE_TTF/*矢量字库*/
}MID_GFX_FontType_t;/*字库类型*/


typedef struct
{
	U8 BoundingBoxW;
	U8 BoundingBoxH;
	S8 BoundingBoxX;
	S8 BoundingBoxY;
	U8 Width;
	U8* GlyphData_p;
}MID_GFX_BdfGlyph_t;/*BDF 字库字符结构体*/

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
}MID_GFX_BdfFontParams_t;/*BDF 字库结构体*/

typedef struct
{
	U8						*FontData_p;/*字库数据*/
	U32						FontDataSize;/*数据大小*/
}MID_GFX_TtfFontParams_t;/*矢量字库结构体*/

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
}MID_GFX_BmpFontParams_t;/*点阵字库*/

typedef struct
{
	MID_GFX_FontType_t  Type;/*字库类型*/
	union{
		MID_GFX_TtfFontParams_t		TtfFontParams;/*矢量字库*/
		MID_GFX_BdfFontParams_t		BdfFontParams;/*BDF 字库*/
		MID_GFX_BmpFontParams_t		BmpFontParams;/*点阵字库*/
	}Font;
	S32 (*GetCharIndex_p)(U16 Unicode);/*特殊字库的Unicode 转索引函数指针*/
}MID_GFX_FontParams_t;/*字库统一参数结构体*/

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



