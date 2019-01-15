
/*****************************************************************************
文件名称	: font.c
版权所有	: Tech Home 2010-2015
文件功能	: 

作者			|					修订					|	日期
陈慧明							创建						2010.06.02

*****************************************************************************/

/*=======include standard header file======*/
#include <stdio.h>
#include <string.h>

/*=======include pal header file======*/
#include "osd.h"


/*=======include local header file======*/
#include "i_gfx.h"


/*****************************const define*************************************/
#define GFX_USE_MEM_FONT		1

#define GFX_MAX_FONT_NUM		10

#define GFX_FONT_ACCESS_LOCK_INIT()		GFX_FontAccess_p = THOS_SemaphoreCreate(1)
#define GFX_FONT_ACCESS_LOCK()				THOS_SemaphoreWait(GFX_FontAccess_p)
#define GFX_FONT_ACCESS_UNLOCK()			THOS_SemaphoreSignal(GFX_FontAccess_p)
#define GFX_FONT_ACCESS_LOCK_TERM()		THOS_SemaphoreDelete(GFX_FontAccess_p)

/*****************************struct define*************************************/



/*****************************gloal data define**********************************/

static TH_Semaphore_t		*GFX_FontAccess_p = NULL;
static GFX_Font_t			GFX_Font[GFX_MAX_FONT_NUM];/*字库结构体, 第一个只用于放默认字库,
														并且默认字库不允许删除*/
#if (GFX_ENABLE_TTF)
FT_Library		Ft_library;
FTC_Manager		Ft_CacheManager;
FTC_SBitCache	Ft_SBmpCache;
FTC_CMapCache	Ft_CMapCache;
FTC_ImageCache	Ft_ImageCache;
//FT_Stroker		Ft_stroker;
#endif

/*****************************local function define*******************************/

FT_Error GFX_TtfNewFace( FTC_FaceID  face_id,FT_Library  library,FT_Pointer  request_data,FT_Face*    aface )
{
	GFX_TtfFont_t  *TtfFont_p;
	TtfFont_p = (GFX_TtfFont_t *)face_id;
	printf("GFX_TtfNewFace\n");
#if (GFX_USE_MEM_FONT)
	return FT_New_Memory_Face(library, TtfFont_p->FontData_p, TtfFont_p->FontDataSize, 0, aface ); /* create face object */
#else
	return FT_New_Face( library, (char *)(TtfFont_p->FontData_p), 0, aface );/* create face object */
#endif
}

GFX_Font_t *GFX_GetFont(char *FontName_p)
{
	int i;
	GFX_Font_t *Font_p = NULL;

	GFX_FONT_ACCESS_LOCK();
	for(i=0; i<GFX_MAX_FONT_NUM; i++)
	{
		if((GFX_Font[i].Type != MID_GFX_FONT_TYPE_INVALID) &&\
			(strcmp(GFX_Font[i].FontName, FontName_p) == 0))
		{
			Font_p = &(GFX_Font[i]);
			Font_p->ReferCount ++;
			break;
		}
	}
	GFX_FONT_ACCESS_UNLOCK();

	return Font_p;
}

TH_Error_t GFX_PutFont(GFX_Font_t **Font_pp)
{
	if(Font_pp == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	if((*Font_pp) == NULL)
	{
		return TH_ERROR_BAD_PARAM;
	}
	GFX_FONT_ACCESS_LOCK();
	(*Font_pp)->ReferCount --;
	GFX_FONT_ACCESS_UNLOCK();

	return TH_NO_ERROR;
}

GFX_Font_t *GFX_LocateGlyph(GFX_Font_t *Font_p, U16 Unicode, S32 *GlyphIdx_p)
{
	if(Font_p == NULL)
	{
		Font_p = &(GFX_Font[0]);
	}
	*GlyphIdx_p = -1;
	switch(Font_p->Type)
	{
		case MID_GFX_FONT_TYPE_TTF:
			{
				FTC_FaceID		FaceId;
				FT_UInt     index;
				
				FaceId = (FTC_FaceID)(&(Font_p->Font.TtfFont));

				index=FTC_CMapCache_Lookup( Ft_CMapCache, FaceId, Font_p->Font.TtfFont.CurrMapIndex, Unicode);
				if(index != 0)
				{
					*GlyphIdx_p = (S32)index;
				}
				else if(Font_p != (&(GFX_Font[0])) && strcmp(Font_p->FontName, "Aramian"))
				{
					return GFX_LocateGlyph(&(GFX_Font[0]), Unicode, GlyphIdx_p);
				}
				else if(strcmp(Font_p->FontName, "Aramian"))
				{
					GFX_Font_t *armian_font, *tmp_font;
					armian_font = GFX_GetFont("Aramian");
					if(armian_font)
					{
						tmp_font = GFX_LocateGlyph(armian_font, Unicode, GlyphIdx_p);
						GFX_PutFont(&armian_font);
						return tmp_font;
					}
				}
			}
			break;
		case MID_GFX_FONT_TYPE_BDF:
		case MID_GFX_FONT_TYPE_BMP:
		default:
			break;
	}
	return Font_p;
}

#if (GFX_USE_TTF_SBIT)
void DumpSBitStruct(FTC_SBit SBit)
{
	printf("width = %d\n", SBit->width);
	printf("height = %d\n", SBit->height);
	printf("left = %d\n", SBit->left);
	printf("top = %d\n", SBit->top);
	printf("format = %d\n", SBit->format);
	printf("pitch = %d\n", SBit->pitch);
	printf("max_grays = %d\n", SBit->max_grays);
	printf("xadvance = %d\n", SBit->xadvance);
	printf("yadvance = %d\n", SBit->yadvance);
}

GFX_Font_t *GFX_LocateGlyphForSize(GFX_Font_t *Font_p, S32 FontSize, U16 Unicode,  BOOL MonoBmp,
								S32 *GlyphIdx_p, S32 *Width_p, S32 *Left_p, S32 *BitmapW_p, S32 *Ascent_p, S32 *Descent_p)
{
	if(Unicode == 0x0A)
	{
	//	Unicode = 0x20;
		if(Font_p == NULL)
		{
			Font_p = &(GFX_Font[0]);
		}
		*GlyphIdx_p = 0;
		*Width_p = 0;
		*Left_p = 0;
		*BitmapW_p = 0;
		*Ascent_p = 0;
		*Descent_p = 0;
		return Font_p;
	}
	
	if(Font_p == NULL)
	{
		Font_p = &(GFX_Font[0]);
	}
	*GlyphIdx_p = -1;
	if(Font_p->Type == MID_GFX_FONT_TYPE_TTF)
	{
		FTC_FaceID		FaceId;
		FTC_ScalerRec	Scaler;
		FT_UInt		index;
		FTC_Node	Node = NULL;
		FTC_SBit		Sbit = NULL;
		
		FaceId = (FTC_FaceID)(&(Font_p->Font.TtfFont));

		index=FTC_CMapCache_Lookup( Ft_CMapCache, FaceId, Font_p->Font.TtfFont.CurrMapIndex, Unicode);
		if(index == 0)
		{
			if(Font_p != (&(GFX_Font[0])) && strcmp(Font_p->FontName, "Aramian"))
			{
				return GFX_LocateGlyphForSize(&(GFX_Font[0]), FontSize, Unicode, MonoBmp,
												GlyphIdx_p, Width_p, Left_p, BitmapW_p, Ascent_p, Descent_p);
			}
			else if(strcmp(Font_p->FontName, "Aramian"))
			{
				GFX_Font_t *armian_font, *tmp_font;
				armian_font = GFX_GetFont("Aramian");
				if(armian_font)
				{
					tmp_font = GFX_LocateGlyphForSize(armian_font, FontSize, Unicode, MonoBmp,
											GlyphIdx_p, Width_p, Left_p, BitmapW_p, Ascent_p, Descent_p);
					GFX_PutFont(&armian_font);
					return tmp_font;
				}
			}

			return Font_p;
		}
		Scaler.face_id	= FaceId;
		Scaler.width=FontSize;
		Scaler.height=FontSize;
		Scaler.pixel=1;
		Scaler.x_res=0;
		Scaler.y_res=0;
	#if GFX_USE_TTF_GRAY
		if(MonoBmp)
		{
			FTC_SBitCache_LookupScaler(Ft_SBmpCache, &Scaler, FT_LOAD_TARGET_MONO, index, &Sbit, &Node);
		}
		else
		{
			FTC_SBitCache_LookupScaler(Ft_SBmpCache, &Scaler, FT_LOAD_TARGET_NORMAL, index, &Sbit, &Node);
		}
	#else
		FTC_SBitCache_LookupScaler(Ft_SBmpCache, &Scaler, FT_LOAD_TARGET_MONO, index, &Sbit, &Node);
	#endif
		if(Node != NULL)
		{
			*Left_p = Sbit->left;
			*Width_p = Sbit->xadvance;
			if(BitmapW_p)
			{
				*BitmapW_p = Sbit->width;
			}
			/* 这段代码用于处理后面一个字符和前面一个字符粘连的问题,
			但是根据标准应该是不用下面的处理，可能是我们使用的这个
			了库的问题，特别是Ti会粘连.因此暂时不使用这段
			
			if(Unicode >= 0x600 && Unicode <= 0x6FF)
			{
				if(Sbit->width > Sbit->xadvance)
				{
					*Width_p = Sbit->width;
				}
			}*/
			
			*Ascent_p = Sbit->top;
			*Descent_p = Sbit->height-Sbit->top;
			FTC_Node_Unref(Node, Ft_CacheManager);
		}
		*GlyphIdx_p = (S32)index;
	}
	return Font_p;
}

#else
GFX_Font_t *GFX_LocateGlyphForSize(GFX_Font_t *Font_p, S32 FontSize, U16 Unicode,  BOOL MonoBmp,
								S32 *GlyphIdx_p, S32 *Width_p, S32 *Left_p, S32 *Ascent_p, S32 *Descent_p)
{
	if(Font_p == NULL)
	{
		Font_p = &(GFX_Font[0]);
	}
	*GlyphIdx_p = -1;
	if(Font_p->Type == MID_GFX_FONT_TYPE_TTF)
	{
		FTC_FaceID		FaceId;
		FTC_ScalerRec	Scaler;
		FT_UInt		index;
		FTC_Node	Node = NULL;
	//	FTC_SBit		Sbit;
		FT_Glyph		Glyph;
		FT_BitmapGlyph  Glyph_bitmap;
		
		FaceId = (FTC_FaceID)(&(Font_p->Font.TtfFont));

		index=FTC_CMapCache_Lookup( Ft_CMapCache, FaceId, Font_p->Font.TtfFont.CurrMapIndex, Unicode);
		if(index == 0)
		{
			if(Font_p != (&(GFX_Font[0])))
			{
				return GFX_LocateGlyphForSize(&(GFX_Font[0]), FontSize, Unicode, MonoBmp,
												GlyphIdx_p, Width_p, Left_p, Ascent_p, Descent_p);
			}
			return Font_p;
		}
		Scaler.face_id	= FaceId;
		Scaler.width=FontSize;
		Scaler.height=FontSize;
		Scaler.pixel=1;
		Scaler.x_res=0;
		Scaler.y_res=0;
	//	FTC_SBitCache_LookupScaler(Ft_SBmpCache, &Scaler, FT_LOAD_MONOCHROME, index, &Sbit, &Node);
		FTC_ImageCache_LookupScaler( Ft_ImageCache,
									&Scaler,
									FT_LOAD_DEFAULT,
									index,
									&Glyph,
									&Node );
		if(Node != NULL)
		{
			if (Glyph->format != FT_GLYPH_FORMAT_BITMAP)
			{
				FT_Glyph_To_Bitmap(&Glyph, FT_RENDER_MODE_NORMAL, NULL, 0);
				Glyph_bitmap = (FT_BitmapGlyph)Glyph;
				*Width_p = (Glyph_bitmap->root.advance.x)>>16;
				*Ascent_p = Glyph_bitmap->top;
				*Descent_p = Glyph_bitmap->bitmap.rows-Glyph_bitmap->top;
				FT_Done_Glyph(Glyph);
				printf("Line %d,  width %d %d %d %d\n", __LINE__, *Width_p, 
					Glyph_bitmap->root.advance.x, 
					Glyph_bitmap->top,
					Glyph_bitmap->bitmap.rows);
			}
			else
			{
				Glyph_bitmap = (FT_BitmapGlyph)Glyph;
				*Width_p = (Glyph_bitmap->root.advance.x)>>16;
				*Ascent_p = Glyph_bitmap->top;
				*Descent_p = Glyph_bitmap->bitmap.rows-Glyph_bitmap->top;
				printf("Line %d,  width %d %d %d %d\n", __LINE__, *Width_p, 
					Glyph_bitmap->root.advance.x, 
					Glyph_bitmap->top,
					Glyph_bitmap->bitmap.rows);
			}
			FTC_Node_Unref(Node, Ft_CacheManager);
		}
		else
		{
			printf("can't get char node\n");
		}
		*GlyphIdx_p = (S32)index;
	}
	return Font_p;
}
#endif

TH_Error_t GFX_FontInit(void)
{
	FT_Error		Ft_Error;
	
	GFX_FONT_ACCESS_LOCK_INIT();
	GFX_FONT_ACCESS_LOCK();
	memset(GFX_Font, 0, sizeof(GFX_Font));
	
#if (GFX_ENABLE_TTF)
	Ft_Error = FT_Init_FreeType( &Ft_library );
	if(Ft_Error)
	{
		GFX_FONT_ACCESS_UNLOCK();
		return TH_ERROR_BIOS_ERROR;
	}

	Ft_Error = FTC_Manager_New( Ft_library,
					                   10,
					                   10,
					                   1024*1024,
					                   GFX_TtfNewFace,
					                   NULL,
					                   &Ft_CacheManager );
	if(Ft_Error)
	{
		GFX_FONT_ACCESS_UNLOCK();
		return TH_ERROR_BIOS_ERROR;
	}
	/*cache for small bitbmp*/
	Ft_Error = FTC_SBitCache_New( Ft_CacheManager,&Ft_SBmpCache);
	if(Ft_Error)
	{
		GFX_FONT_ACCESS_UNLOCK();
		return TH_ERROR_BIOS_ERROR;
	}
	/*cache for char code map*/
	Ft_Error = FTC_CMapCache_New( Ft_CacheManager,&Ft_CMapCache);
	if(Ft_Error)
	{
		GFX_FONT_ACCESS_UNLOCK();
		return TH_ERROR_BIOS_ERROR;
	}
	/*cache for glyph*/
	Ft_Error = FTC_ImageCache_New( Ft_CacheManager,&Ft_ImageCache);
	if(Ft_Error)
	{
		GFX_FONT_ACCESS_UNLOCK();
		return TH_ERROR_BIOS_ERROR;
	}
#if 0
       Ft_Error = FT_Stroker_New(Ft_library, &Ft_stroker);
	if(Ft_Error)
	{
		GFX_FONT_ACCESS_UNLOCK();
		return TH_ERROR_BIOS_ERROR;
	}
	
       FT_Stroker_Set(Ft_stroker,
		                       3 << 6,
		                       FT_STROKER_LINECAP_ROUND,
		                       FT_STROKER_LINEJOIN_ROUND,
		                       0);
#endif
#endif

	GFX_FONT_ACCESS_UNLOCK();
	return TH_NO_ERROR;
}


/*****************************global function define*******************************/

/*******************************************************************************
函数名称	: MID_GFX_SetDefaultFont

函数功能	: 设置默认字库, 写字时如果指定的字库没有相
					应的字符, 就会使用默认字库

函数参数	: IN:		FontName_p			字库名称
				  IN:		FontParams_p			字库参数

函数返回	: 是否设置成功

作者			|					修订					|	日期

陈慧明							创建						2010.06.02

*******************************************************************************/
TH_Error_t MID_GFX_SetDefaultFont(char *FontName_p, MID_GFX_FontParams_t *FontParams_p)
{
	int i;

	GFX_FONT_ACCESS_LOCK();

	if(GFX_Font[0].Type != MID_GFX_FONT_TYPE_INVALID)
	{
		GFX_FONT_ACCESS_UNLOCK();
		return TH_ERROR_ALREADY_EXIST;
	}
	
	for(i=1; i<GFX_MAX_FONT_NUM; i++)
	{
		if((GFX_Font[i].Type != MID_GFX_FONT_TYPE_INVALID) &&\
			(strcmp(GFX_Font[i].FontName, FontName_p) == 0))
		{
			GFX_FONT_ACCESS_UNLOCK();
			return TH_ERROR_ALREADY_EXIST;
		}
	}

	switch(FontParams_p->Type)
	{
		case MID_GFX_FONT_TYPE_TTF:
			{
				FT_Error		Ft_Error;
				FTC_FaceID		FaceId;
				FT_Int		i;
			#if (GFX_USE_MEM_FONT)
				GFX_Font[0].Font.TtfFont.FontData_p = FontParams_p->Font.TtfFontParams.FontData_p;
			#else
				GFX_Font[0].Font.TtfFont.FontData_p = THOS_MallocBot(strlen((char *)(FontParams_p->Font.TtfFontParams.FontData_p))+4);
				strcpy((char *)(GFX_Font[0].Font.TtfFont.FontData_p), (char *)(FontParams_p->Font.TtfFontParams.FontData_p));
			#endif
				GFX_Font[0].Font.TtfFont.FontDataSize = FontParams_p->Font.TtfFontParams.FontDataSize;
				FaceId = (FTC_FaceID)(&(GFX_Font[0].Font.TtfFont));
				
				Ft_Error = FTC_Manager_LookupFace( Ft_CacheManager, FaceId, &(GFX_Font[0].Font.TtfFont.Ft_face));
				if(Ft_Error)
				{
					GFX_FONT_ACCESS_UNLOCK();
					return TH_ERROR_BIOS_ERROR;
				}
				for(i=0;i<GFX_Font[0].Font.TtfFont.Ft_face->num_charmaps;i++)
				{
					if(GFX_Font[0].Font.TtfFont.Ft_face->charmaps[i]->encoding == FT_ENCODING_UNICODE)
					{
						GFX_Font[0].Font.TtfFont.CurrMapIndex = i;
						break;
					}
				}
			}
			break;
		default:
			GFX_FONT_ACCESS_UNLOCK();
			return TH_ERROR_NOT_SUPPORT;
			break;
	}
	GFX_Font[0].Type = FontParams_p->Type;
	GFX_Font[0].ReferCount = 0;
	strncpy(GFX_Font[0].FontName, FontName_p, sizeof(GFX_Font[0].FontName)-1);
	GFX_Font[0].FontName[sizeof(GFX_Font[0].FontName)-1] = 0;
	
	GFX_Font[0].GetCharIndex_p = FontParams_p->GetCharIndex_p;
	GFX_FONT_ACCESS_UNLOCK();
	return TH_NO_ERROR;
}

/*******************************************************************************
函数名称	: MID_GFX_AddFont

函数功能	: 添加一个字库

函数参数	: IN:		FontName_p			字库名称
				  IN:		FontParams_p			字库参数

函数返回	: 是否添加成功

作者			|					修订					|	日期

陈慧明							创建						2010.06.02

*******************************************************************************/
TH_Error_t MID_GFX_AddFont(char *FontName_p, MID_GFX_FontParams_t *FontParams_p)
{
	int i;
	GFX_Font_t *Font_p;

	GFX_FONT_ACCESS_LOCK();
	for(i=0; i<GFX_MAX_FONT_NUM; i++)
	{
		if((GFX_Font[i].Type != MID_GFX_FONT_TYPE_INVALID) &&\
			(strcmp(GFX_Font[i].FontName, FontName_p) == 0))
		{
			GFX_FONT_ACCESS_UNLOCK();
			return TH_ERROR_ALREADY_EXIST;
		}
	}
	
	for(i=1; i<GFX_MAX_FONT_NUM; i++)
	{
		if(GFX_Font[i].Type == MID_GFX_FONT_TYPE_INVALID)
		{
			break;
		}
	}
	if(i >= GFX_MAX_FONT_NUM)
	{
		GFX_FONT_ACCESS_UNLOCK();
		return TH_ERROR_NO_FREE;
	}
	
	Font_p = &(GFX_Font[i]);
	
	switch(FontParams_p->Type)
	{
		case MID_GFX_FONT_TYPE_TTF:
			{
				FT_Error		Ft_Error;
				FTC_FaceID		FaceId;
				FT_Int		i;
			#if (GFX_USE_MEM_FONT)
				Font_p->Font.TtfFont.FontData_p = FontParams_p->Font.TtfFontParams.FontData_p;
			#else
				Font_p->Font.TtfFont.FontData_p = THOS_MallocBot(strlen((char *)(FontParams_p->Font.TtfFontParams.FontData_p))+4);
				strcpy((char *)(Font_p->Font.TtfFont.FontData_p), (char *)(FontParams_p->Font.TtfFontParams.FontData_p));
			#endif

				Font_p->Font.TtfFont.FontDataSize = FontParams_p->Font.TtfFontParams.FontDataSize;

				FaceId = (FTC_FaceID)(&(Font_p->Font.TtfFont));
				
				Ft_Error = FTC_Manager_LookupFace( Ft_CacheManager, FaceId, &(Font_p->Font.TtfFont.Ft_face));
				if(Ft_Error)
				{
					GFX_FONT_ACCESS_UNLOCK();
					return TH_ERROR_BIOS_ERROR;
				}
				for(i=0;i<Font_p->Font.TtfFont.Ft_face->num_charmaps;i++)
				{
					if(Font_p->Font.TtfFont.Ft_face->charmaps[i]->encoding == FT_ENCODING_UNICODE)
					{
						Font_p->Font.TtfFont.CurrMapIndex = i;
						break;
					}
				}
			}
			break;
		default:
			GFX_FONT_ACCESS_UNLOCK();
			return TH_ERROR_NOT_SUPPORT;
			break;
	}
	Font_p->Type = FontParams_p->Type;
	Font_p->ReferCount = 0;
	strncpy(Font_p->FontName, FontName_p, sizeof(Font_p->FontName)-1);
	Font_p->FontName[sizeof(Font_p->FontName)-1] = 0;

	Font_p->GetCharIndex_p = FontParams_p->GetCharIndex_p;
	GFX_FONT_ACCESS_UNLOCK();
	return TH_NO_ERROR;
}

/*******************************************************************************
函数名称	: MID_GFX_DelFont

函数功能	: 删除一个字库

函数参数	: IN:		FontName_p			字库名称

函数返回	: 是否删除成功

作者			|					修订					|	日期

陈慧明							创建						2010.06.02

*******************************************************************************/
TH_Error_t MID_GFX_DelFont(char *FontName_p)
{
	return TH_ERROR_NOT_SUPPORT;
}


/*end of font.c*/

