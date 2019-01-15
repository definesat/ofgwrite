
/*****************************************************************************
文件名称	: arabic.c
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


typedef struct 
{
	unsigned short Key;
	unsigned short Isolated;
	unsigned short Final;
	unsigned short Initial;
	unsigned short Medial;
}KEY_INFO;

static const KEY_INFO _aKeyInfo[] =
{
	/*    Base      Isol.   Final   Initial Medial */
	{ /* 0  */0x0621, 0xFE80, 0x0000, 0x0000, 0x0000 },
	{ /* 1  */0x0622, 0xFE81, 0xFE82, 0x0000, 0x0000 },
	{ /* 2  */0x0623, 0xFE83, 0xFE84, 0x0000, 0x0000 },
	{ /* 3  */0x0624, 0xFE85, 0xFE86, 0x0000, 0x0000 },
	{ /* 4  */0x0625, 0xFE87, 0xFE88, 0x0000, 0x0000 },
	{ /* 5  */0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C },
	{ /* 6  */0x0627, 0xFE8D, 0xFE8E, 0x0000, 0x0000 },
	{ /* 7  */0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92 },
	{ /* 8  */0x0629, 0xFE93, 0xFE94, 0x0000, 0x0000 },
	{ /* 9  */0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98 },
	{ /* 10 */0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C },
	{ /* 11 */0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0 },
	{ /* 12 */0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4 },
	{ /* 13 */0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8 },
	{ /* 14 */0x062F, 0xFEA9, 0xFEAA, 0x0000, 0x0000 },
	{ /* 15 */0x0630, 0xFEAB, 0xFEAC, 0x0000, 0x0000 },
	{ /* 16 */0x0631, 0xFEAD, 0xFEAE, 0x0000, 0x0000 },
	{ /* 17 */0x0632, 0xFEAF, 0xFEB0, 0x0000, 0x0000 },
	{ /* 18 */0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4 },
	{ /* 19 */0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8 },
	{ /* 20 */0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC },
	{ /* 21 */0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0 },
	{ /* 22 */0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4 },
	{ /* 23 */0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8 },
	{ /* 24 */0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC },
	{ /* 25 */0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0 },
	{ /* 26 */0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4 },
	{ /* 27 */0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8 },
	{ /* 28 */0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC },
	{ /* 29 */0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0 },
	{ /* 30 */0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4 },
	{ /* 31 */0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8 },
	{ /* 32 */0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC },
	{ /* 33 */0x0648, 0xFEED, 0xFEEE, 0x0000, 0x0000 },
	{ /* 34 */0x0649, 0xFEEF, 0xFEF0, 0x0000, 0x0000 },
	{ /* 35 */0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4 },
	{ /* 36 */0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59 },
	{ /* 37 */0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D },
	{ /* 38 */0x0698, 0xFB8A, 0xFB8B, 0x0000, 0x0000 },
	{ /* 39 */0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91 },
	{ /* 40 */0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95 },
	{ /* 41 */0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF }
};


static int GxIsArabicCharacter(unsigned short Char)
{
	return ((Char >= 0x0600) && (Char <= 0x06ff)) ? 1 : 0;
}

static int _GetTableIndex(unsigned short Char)
{
	if (Char < 0x0621)
	{
		return 0;
	}
	if (Char > 0x06cc)
	{
		return 0;
	}
	if ((Char >= 0x0621) && (Char <= 0x063a))
	{
		return Char - 0x0621;
	}
	if ((Char >= 0x0641) && (Char <= 0x064a))
	{
		return Char - 0x0641 + 26;
	}
	if (Char == 0x067e)
	{
		return 36;
	}
	if (Char == 0x0686)
	{
		return 37;
	}
	if (Char == 0x0698)
	{
		return 38;
	}
	if (Char == 0x06a9)
	{
		return 39;
	}
	if (Char == 0x06af)
	{
		return 40;
	}
	if (Char == 0x06cc)
	{
		return 41;
	}
	return 0;
}

static int _IsTransparent(unsigned short Char)
{
	if (Char >= 0x064b)
	{
		return 1;
	}
	if (Char == 0x0670)
	{
		return 1;
	}
	else
		return 0;
}

static int _GetLigature(unsigned short Char, unsigned short Next, int PrevAffectsJoining)
{
	if (((Next != 0x0622) && (Next != 0x0623) && (Next != 0x0625) && (Next != 0x0627)) || (Char != 0x0644))
	{
		return 0;
	}
	if (PrevAffectsJoining)
	{
		switch (Next)
		{
			case 0x0622:
				return 0xfef6;
			case 0x0623:
				return 0xfef8;
			case 0x0625:
				return 0xfefa;
			case 0x0627:
				return 0xfefc;
		}
	}
	else
	{
		switch (Next)
		{
			case 0x0622:
				return 0xfef5;
			case 0x0623:
				return 0xfef7;
			case 0x0625:
				return 0xfef9;
			case 0x0627:
				return 0xfefb;
		}
	}
	return 0;
}

unsigned short GxGetPresentationForm(unsigned short PrePrev, unsigned short Char, unsigned short Next, unsigned short Prev, int* pIgnoreNext)
{
	int CharIndex, PrevIndex, NextAffectsJoining, PrevAffectsJoining, Final, Initial, Medial, Ligature;
	int PrePreIndex, PrePreAffectiosJoining;

	CharIndex = _GetTableIndex(Char);

	if (!CharIndex)
	{
		if (Char == 0x0621)
		{
			return 0xfe80;
		}
		else
		{
			return Char;
		}
	}

//	PrePrev = *(p_uc + 2);
	PrePreIndex = _GetTableIndex(PrePrev);
	PrePreAffectiosJoining = (_GetTableIndex(PrePrev) || _IsTransparent(PrePrev)) && (_aKeyInfo[PrePreIndex].Medial);

	PrevIndex = _GetTableIndex(Prev);
	PrevAffectsJoining = (_GetTableIndex(Prev) || _IsTransparent(Prev)) && (_aKeyInfo[PrevIndex].Medial);

	Ligature = _GetLigature(Char, Next, PrevAffectsJoining);
	if (!Ligature)
	{
		Ligature = _GetLigature(Prev, Char, PrePreAffectiosJoining);
	}

	if (Ligature)
	{
		if (pIgnoreNext)
		{
			*pIgnoreNext = 1;
		}
		return Ligature;
	}
	else
	{
		if (pIgnoreNext)
		{
			*pIgnoreNext = 0;
		}
	}

	NextAffectsJoining = (_GetTableIndex(Next)||_IsTransparent(Next)) && (_aKeyInfo[CharIndex].Medial);
	if ((!PrevAffectsJoining) && (!NextAffectsJoining))
	{
		return _aKeyInfo[CharIndex].Isolated;
	}
	else if ((!PrevAffectsJoining) && (NextAffectsJoining))
	{
		Initial = _aKeyInfo[CharIndex].Initial;
		if (Initial)
		{
			return Initial;
		}
		else
		{
			return _aKeyInfo[CharIndex].Isolated;
		}
	}
	else if ((PrevAffectsJoining) && (NextAffectsJoining))
	{
		Medial = _aKeyInfo[CharIndex].Medial;
		if (Medial)
		{
			return Medial;
		}
		else
		{
			return _aKeyInfo[CharIndex].Isolated;
		}
	}
	else if ((PrevAffectsJoining) && (!NextAffectsJoining))
	{
		Final = _aKeyInfo[CharIndex].Final;
		if (Final)
		{
			return Final;
		}
		else
		{
			return _aKeyInfo[CharIndex].Isolated;
		}
	}
	return Char;
}


void GFX_PrepareArabicStr(GFX_CharData_t *StrData_p, GFX_Font_t *Font_p, MID_GFX_PutText_t *TextParams_p)
{
	int i, j, IgnoreNext;
	unsigned short PrePrev, Next, Prev;
	S32 CharIdx, CharWidth, CharBitWidth, CharAscent, CharDescent, CharLeft;
	BOOL MonoBmp = FALSE;
	int	RevertStart = -1, RevertCnt = 0;
	GFX_CharData_t TempCharData;
	int Start, End;
	
	i = 0;
	while(StrData_p[i].BaseCode)
	{
		if(GxIsArabicCharacter(StrData_p[i].BaseCode) ||\
			(StrData_p[i].BaseCode >= 0x590 && StrData_p[i].BaseCode <= 0x5ff))
		{
			if(RevertCnt > 1)
			{
				/*revert char*/
				Start = RevertStart;
				End = RevertStart + RevertCnt - 1;
				for(j=0; j<(RevertCnt>>1); j++)
				{
					TempCharData = StrData_p[Start];
					StrData_p[Start] = StrData_p[End];
					StrData_p[End] = TempCharData;
					Start ++;
					End --;
				}
			}
			RevertStart = -1;
			RevertCnt = 0;
			if(StrData_p[i+1].BaseCode != 0 &&\
				StrData_p[i+2].BaseCode != 0)
			{
				PrePrev = StrData_p[i+2].BaseCode;
			}
			else
			{
				PrePrev = 0;
			}
			if(StrData_p[i+1].BaseCode != 0)
			{
				Next = StrData_p[i+1].BaseCode;
			}
			else
			{
				Next = 0;
			}
			
			if(i > 0 &&
				StrData_p[i-1].BaseCode != 0)
			{
				Prev = StrData_p[i-1].BaseCode;
			}
			else
			{
				Prev = 0;
			}
			
	//		printf("Unicode = %d, PrePrev = %d, Prev = %d, Next = %d\n", StrData_p[i].BaseCode, PrePrev, Prev, Next);
			if(StrData_p[i].BaseCode >= 0x590 && StrData_p[i].BaseCode <= 0x5ff)
			{
				StrData_p[i].Unicode = StrData_p[i].BaseCode;
			}
			else
			{
				StrData_p[i].Unicode = GxGetPresentationForm(PrePrev, StrData_p[i].BaseCode, Next, Prev, &IgnoreNext);
			}
			StrData_p[i].Font_p = GFX_LocateGlyphForSize(Font_p, TextParams_p->FontSize, StrData_p[i].Unicode, MonoBmp,
															&CharIdx, &CharWidth, &CharLeft, &CharBitWidth, &CharAscent, &CharDescent);
			if(CharIdx != -1 )
			{
		//		printf("left = %d, xadvance = %d, width = %d\n", CharLeft, CharWidth, CharBitWidth);
				StrData_p[i].GlyphIdx = CharIdx;
				StrData_p[i].Width = CharWidth;
				StrData_p[i].Left = CharLeft;
				StrData_p[i].BitmapW = CharBitWidth;
				StrData_p[i].Ascent = CharAscent;
				StrData_p[i].Descent = CharDescent;
			}
			else
			{
				StrData_p[i].GlyphIdx = -1;
				StrData_p[i].Width = 0;
				StrData_p[i].Left = 0;
				StrData_p[i].BitmapW = 0;
				StrData_p[i].Ascent = 0;
				StrData_p[i].Descent = 0;
			}
		}
		else
		{
			if(StrData_p[i].BaseCode == 0x20)/*space no need revert*/
			{
				if(RevertCnt > 1)
				{
					/*revert char*/
					Start = RevertStart;
					End = RevertStart + RevertCnt - 1;
					for(j=0; j<(RevertCnt>>1); j++)
					{
						TempCharData = StrData_p[Start];
						StrData_p[Start] = StrData_p[End];
						StrData_p[End] = TempCharData;
						Start ++;
						End --;
					}
				}
				RevertStart = -1;
				RevertCnt = 0;
			}
			else
			{
				if(RevertStart == -1)
				{
					RevertStart = i;
				}
				RevertCnt ++;
			}
		}
		i ++;
	}
	if(RevertCnt > 1)
	{
		/*revert char*/
		Start = RevertStart;
		End = RevertStart + RevertCnt - 1;
		for(j=0; j<(RevertCnt>>1); j++)
		{
			TempCharData = StrData_p[Start];
			StrData_p[Start] = StrData_p[End];
			StrData_p[End] = TempCharData;
			Start ++;
			End --;
		}
	}
}


