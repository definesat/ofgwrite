#include "ofgwrite.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <getopt.h>
#include <fcntl.h>
#include <linux/reboot.h>
#include <syslog.h>
#include <sys/mount.h>
#include <unistd.h>
#include <errno.h>
#include <mtd/mtd-user.h>

#include "gfx/gfx.h"
#include "gfx/font.h"

const char ofgwrite_version[] = "4.1.7";
int flash_kernel = 0;
int flash_rootfs = 0;
int no_write     = 0;
int force        = 0;
int quiet        = 0;
int show_help    = 0;
int found_kernel_device = 0;
int found_rootfs_device = 0;
int newroot_mounted = 0;
char kernel_filename[1000];
char kernel_device[1000];
char rootfs_filename[1000];
char rootfs_device[1000];
enum RootfsTypeEnum rootfs_type;
char media_mounts[30][500];
int media_mount_count = 0;
int stop_e2_needed = 1;
PAL_OSD_Handle	OsdHandle;

#define CN_DURATION_WIDTH(duration, maxtime, width)	 ((((duration) * (width)) / ((maxtime)==0?1:(maxtime))))

void my_printf(char const *fmt, ...)
{
	va_list ap, ap2;
	va_start(ap, fmt);
	va_copy(ap2, ap);
	// print to console
	vprintf(fmt, ap);
	va_end(ap);

	// print to syslog
	vsyslog(LOG_INFO, fmt, ap2);
	va_end(ap2);
}

void my_fprintf(FILE * f, char const *fmt, ...)
{
	va_list ap, ap2;
	va_start(ap, fmt);
	va_copy(ap2, ap);
	// print to file (normally stdout or stderr)
	vfprintf(f, fmt, ap);
	va_end(ap);

	// print to syslog
	vsyslog(LOG_INFO, fmt, ap2);
	va_end(ap2);
}

void printUsage()
{
	my_printf("Usage: ofgwrite <parameter> <image_directory>\n");
	my_printf("Options:\n");
	my_printf("   -k --kernel           flash kernel with automatic device recognition(default)\n");
	my_printf("   -kmtdx --kernel=mtdx  use mtdx device for kernel flashing\n");
	my_printf("   -kmmcblkxpx --kernel=mmcblkxpx  use mmcblkxpx device for kernel flashing\n");
	my_printf("   -r --rootfs           flash rootfs with automatic device recognition(default)\n");
	my_printf("   -rmtdy --rootfs=mtdy  use mtdy device for rootfs flashing\n");
	my_printf("   -rmmcblkxpx --rootfs=mmcblkxpx  use mmcblkxpx device for rootfs flashing\n");
	my_printf("   -mx --multi=x         flash multiboot partition x (x= 1, 2, 3,...). Only supported by some boxes.\n");
	my_printf("   -n --nowrite          show only found image and mtd partitions (no write)\n");
	my_printf("   -f --force            force kill e2\n");
	my_printf("   -q --quiet            show less output\n");
	my_printf("   -h --help             show help\n");
}

void  UI_GFX_DrawRect(PAL_OSD_Handle Handle, S32 LeftX, S32 TopY, S32 Width, S32 Height, U32 ARGB)
{
	MID_GFX_Obj_t		GfxObj;
	
	memset(&GfxObj, 0, sizeof(GfxObj));
	GfxObj.ObjType = MID_GFX_OBJ_GRAPH;
	GfxObj.Obj.Graph.Type = MID_GFX_GRAPH_RECTANGLE;
	GfxObj.Obj.Graph.Copy = FALSE;
	GfxObj.Obj.Graph.JustFrame = FALSE;
	GfxObj.Obj.Graph.FrameLineWidth = 0;
	GfxObj.Obj.Graph.Params.Rectangle.LeftX = LeftX;
	GfxObj.Obj.Graph.Params.Rectangle.TopY = TopY;
	GfxObj.Obj.Graph.Params.Rectangle.Width = Width;
	GfxObj.Obj.Graph.Params.Rectangle.Height = Height;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Type = PAL_OSD_COLOR_TYPE_ARGB8888;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.Alpha = (ARGB>>24)&0x000000FF;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.R = (ARGB>>16)&0x000000FF;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.G = (ARGB>>8)&0x000000FF;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.B = (ARGB)&0x000000FF;
	
	MID_GFX_Draw(Handle, &GfxObj);
}

void  UI_GFX_CopyRect(PAL_OSD_Handle Handle, S32 LeftX, S32 TopY, S32 Width, S32 Height, U32 ARGB)
{
	MID_GFX_Obj_t		GfxObj;
	
	memset(&GfxObj, 0, sizeof(GfxObj));
	GfxObj.ObjType = MID_GFX_OBJ_GRAPH;
	GfxObj.Obj.Graph.Type = MID_GFX_GRAPH_RECTANGLE;
	GfxObj.Obj.Graph.Copy = TRUE;
	GfxObj.Obj.Graph.JustFrame = FALSE;
	GfxObj.Obj.Graph.FrameLineWidth = 0;
	GfxObj.Obj.Graph.Params.Rectangle.LeftX = LeftX;
	GfxObj.Obj.Graph.Params.Rectangle.TopY = TopY;
	GfxObj.Obj.Graph.Params.Rectangle.Width = Width;
	GfxObj.Obj.Graph.Params.Rectangle.Height = Height;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Type = PAL_OSD_COLOR_TYPE_ARGB8888;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.Alpha = (ARGB>>24)&0x000000FF;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.R = (ARGB>>16)&0x000000FF;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.G = (ARGB>>8)&0x000000FF;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.B = (ARGB)&0x000000FF;
	
	MID_GFX_Draw(Handle, &GfxObj);
}
/********************************************************************
画一个矩形边宽

LeftX: 		目标X 轴
TopY: 		目标Y 轴
Width: 		目标宽度
Height: 		目标高度
LineWidth: 	线宽
ARGB: 		颜色

*********************************************************************/
void  UI_GFX_DrawRectFrame(PAL_OSD_Handle Handle, S32 LeftX, S32 TopY, S32 Width, S32 Height, S32 LineWidth, U32 ARGB)
{
	MID_GFX_Obj_t		GfxObj;

	memset(&GfxObj, 0, sizeof(GfxObj));
	GfxObj.ObjType = MID_GFX_OBJ_GRAPH;
	GfxObj.Obj.Graph.Type = MID_GFX_GRAPH_RECTANGLE;
	GfxObj.Obj.Graph.JustFrame = TRUE;
	GfxObj.Obj.Graph.FrameLineWidth = LineWidth;
	GfxObj.Obj.Graph.Params.Rectangle.LeftX = LeftX;
	GfxObj.Obj.Graph.Params.Rectangle.TopY = TopY;
	GfxObj.Obj.Graph.Params.Rectangle.Width = Width;
	GfxObj.Obj.Graph.Params.Rectangle.Height = Height;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Type = PAL_OSD_COLOR_TYPE_ARGB8888;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.Alpha = (ARGB>>24)&0x000000FF;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.R = (ARGB>>16)&0x000000FF;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.G = (ARGB>>8)&0x000000FF;
	GfxObj.Obj.Graph.Params.Rectangle.Color.Value.ARGB8888.B = (ARGB)&0x000000FF;
	
	MID_GFX_Draw(Handle, &GfxObj);
}

void  UI_GFX_DrawTextInRectEx(PAL_OSD_Handle Handle, S32 LeftX, S32 TopY, S32 Width, S32 Height, U8 *Text_p, U8 FontSize, U8 PutType, U8 EffectType, U32 TextARGB, U32 OutlineARGB, BOOL Mix, BOOL Force)
{
	MID_GFX_Obj_t		GfxObj;

	if(LeftX < 0 || TopY < 0 || Width <= 0 || Height <= 0)
	{
		return ;
	}

	GfxObj.ObjType = MID_GFX_OBJ_TEXT;
	GfxObj.Obj.Text.LeftX = LeftX;
	GfxObj.Obj.Text.TopY = TopY;
	GfxObj.Obj.Text.Width = Width;
	GfxObj.Obj.Text.Height = Height;
	GfxObj.Obj.Text.XOffset = 0;

	GfxObj.Obj.Text.FontName_p = "English";
	GfxObj.Obj.Text.FontSize = FontSize;

	GfxObj.Obj.Text.HInterval = 0;
	GfxObj.Obj.Text.VInterval = 4;
	GfxObj.Obj.Text.EffectType = EffectType;
	GfxObj.Obj.Text.WriteType = MID_GFX_TEXT_WRITE_L_TO_R;
	GfxObj.Obj.Text.PutType = PutType;
	GfxObj.Obj.Text.Force = Force ;
	GfxObj.Obj.Text.Str_p = Text_p;
	GfxObj.Obj.Text.TextColor.Type = PAL_OSD_COLOR_TYPE_ARGB8888;
	GfxObj.Obj.Text.TextColor.Value.ARGB8888.Alpha = (TextARGB>>24)&0x000000FF;
	GfxObj.Obj.Text.TextColor.Value.ARGB8888.R = (TextARGB>>16)&0x000000FF;
	GfxObj.Obj.Text.TextColor.Value.ARGB8888.G = (TextARGB>>8)&0x000000FF;
	GfxObj.Obj.Text.TextColor.Value.ARGB8888.B = (TextARGB)&0x000000FF;
	
	GfxObj.Obj.Text.LineColor.Type = PAL_OSD_COLOR_TYPE_ARGB8888;
	GfxObj.Obj.Text.LineColor.Value.ARGB8888.Alpha = (OutlineARGB>>24)&0x000000FF;
	GfxObj.Obj.Text.LineColor.Value.ARGB8888.R = (OutlineARGB>>16)&0x000000FF;
	GfxObj.Obj.Text.LineColor.Value.ARGB8888.G = (OutlineARGB>>8)&0x000000FF;
	GfxObj.Obj.Text.LineColor.Value.ARGB8888.B = (OutlineARGB)&0x000000FF;
	GfxObj.Obj.Text.Mix = Mix;

	MID_GFX_Draw(Handle, &GfxObj);
}

void UI_GFX_Init(void)
{
	MID_GFX_FontParams_t		FontParams;
	PAL_OSD_OpenParam_t OpenParam;
	PAL_OSD_Win_t OsdWin;
	
	SDK_OsdInit();
	PAL_OSD_Init();
	MID_GFX_Init();
	
	PAL_OSD_GetMaxWin(&OsdWin);

	memset(&OpenParam, 0, sizeof(OpenParam));
	OpenParam.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
	OpenParam.Width = OsdWin.Width;
	OpenParam.Height = OsdWin.Height;
	OpenParam.IOWin.InputWin.LeftX = 0;
	OpenParam.IOWin.InputWin.TopY = 0;
	OpenParam.IOWin.InputWin.Width = OsdWin.Width;
	OpenParam.IOWin.InputWin.Height = OsdWin.Height;
	OpenParam.IOWin.OutputWin = OpenParam.IOWin.InputWin;

	if(PAL_OSD_Open(&OpenParam, &OsdHandle) != TH_NO_ERROR)
	{
		my_printf("PAL_OSD_Open error\n");
	}

	memset(&FontParams, 0, sizeof(FontParams));
	FontParams.Type = MID_GFX_FONT_TYPE_TTF;
	FontParams.Font.TtfFontParams.FontData_p = font_data;
	FontParams.Font.TtfFontParams.FontDataSize = sizeof(font_data);

	MID_GFX_SetDefaultFont("default", &FontParams);
	UI_GFX_DrawTextInRectEx(OsdHandle, 100, 50, 800, 60, (U8 *)"Updating system firmware", 
								50, 
								MID_GFX_TEXT_PUT_LEFT, MID_GFX_TEXT_NORMAL, 
								0xFFFFFFFF, 
								0, 
								TRUE, TRUE);

	UI_GFX_DrawTextInRectEx(OsdHandle, 100, 200, 800, 160, (U8 *)"Waiting system is stopped!", 
								50, 
								MID_GFX_TEXT_PUT_LEFT|MID_GFX_TEXT_PUT_TOP, MID_GFX_TEXT_NEWLINE, 
								0xFFFFFFFF, 
								0, 
								TRUE, TRUE);

	PAL_OSD_UpdateDisplay();
}

void UI_GFX_Reinit(char *text)
{
	PAL_OSD_OpenParam_t OpenParam;
	PAL_OSD_Win_t OsdWin;

	PAL_OSD_Close(OsdHandle);
	OsdHandle = TH_INVALID_HANDLE;
	SDK_OsdTerm();
	SDK_OsdInit();
	PAL_OSD_GetMaxWin(&OsdWin);

	memset(&OpenParam, 0, sizeof(OpenParam));
	OpenParam.ColorType = PAL_OSD_COLOR_TYPE_ARGB8888;
	OpenParam.Width = OsdWin.Width;
	OpenParam.Height = OsdWin.Height;
	OpenParam.IOWin.InputWin.LeftX = 0;
	OpenParam.IOWin.InputWin.TopY = 0;
	OpenParam.IOWin.InputWin.Width = OsdWin.Width;
	OpenParam.IOWin.InputWin.Height = OsdWin.Height;
	OpenParam.IOWin.OutputWin = OpenParam.IOWin.InputWin;

	if(PAL_OSD_Open(&OpenParam, &OsdHandle) != TH_NO_ERROR)
	{
		my_printf("PAL_OSD_Open error\n");
	}
	UI_GFX_DrawTextInRectEx(OsdHandle, 100, 50, 800, 60, (U8 *)"Updating system firmware", 
								50, 
								MID_GFX_TEXT_PUT_LEFT, MID_GFX_TEXT_NORMAL, 
								0xFFFFFFFF, 
								0, 
								TRUE, TRUE);

	UI_GFX_DrawTextInRectEx(OsdHandle, 100, 200, 800, 160, (U8 *)text, 
								50, 
								MID_GFX_TEXT_PUT_LEFT|MID_GFX_TEXT_PUT_TOP, MID_GFX_TEXT_NEWLINE, 
								0xFFFFFFFF, 
								0, 
								TRUE, TRUE);
	
	PAL_OSD_UpdateDisplay();
}

void draw_partition_text(char *text)
{
	if(OsdHandle == TH_INVALID_HANDLE)
		return ;
	UI_GFX_CopyRect(OsdHandle, 100, 200, 800, 160, 0);
	
	UI_GFX_DrawTextInRectEx(OsdHandle, 100, 200, 800, 160, (U8 *)text, 
								50, 
								MID_GFX_TEXT_PUT_LEFT|MID_GFX_TEXT_PUT_TOP, MID_GFX_TEXT_NEWLINE, 
								0xFFFFFFFF, 
								0, 
								TRUE, TRUE);
	PAL_OSD_UpdateDisplay();
}

void draw_partition_progess(int progress)
{
	if(OsdHandle == TH_INVALID_HANDLE)
		return ;
	
	UI_GFX_CopyRect(OsdHandle, 100, 360, 800, 60, 0);
	UI_GFX_DrawTextInRectEx(OsdHandle, 100, 360, 800, 60, (U8 *)"Current partition grogress", 
								50, 
								MID_GFX_TEXT_PUT_LEFT, MID_GFX_TEXT_NORMAL, 
								0xFFFFFFFF, 
								0, 
								TRUE, TRUE);
	UI_GFX_CopyRect(OsdHandle, 100, 420, 700, 40, 0);
	UI_GFX_CopyRect(OsdHandle, 100, 420, CN_DURATION_WIDTH(progress, 100, 700), 40, 0xFFFFFFFF);
	PAL_OSD_UpdateDisplay();
}

void draw_total_progess(int progress)
{
	if(OsdHandle == TH_INVALID_HANDLE)
		return ;
	
	UI_GFX_CopyRect(OsdHandle, 100, 460, 800, 60, 0);
	UI_GFX_DrawTextInRectEx(OsdHandle, 100, 460, 800, 60, (U8 *)"Total grogress", 
								50, 
								MID_GFX_TEXT_PUT_LEFT, MID_GFX_TEXT_NORMAL, 
								0xFFFFFFFF, 
								0, 
								TRUE, TRUE);
	UI_GFX_CopyRect(OsdHandle, 100, 520, 700, 40, 0);
	UI_GFX_CopyRect(OsdHandle, 100, 520, CN_DURATION_WIDTH(progress, 100, 700), 40, 0xFFFFFFFF);
	PAL_OSD_UpdateDisplay();
}

int find_image_files(char* p)
{
	DIR *d;
	struct dirent *entry;
	char path[4097];

	if (realpath(p, path) == NULL)
	{
		my_printf("Searching image files: Error path couldn't be resolved\n");
		return 0;
	}
	my_printf("Searching image files in %s resolved to %s\n", p, path);
	kernel_filename[0] = '\0';
	rootfs_filename[0] = '\0';

	// add / to the end of the path
	if (path[strlen(path)-1] != '/')
	{
		path[strlen(path)+1] = '\0';
		path[strlen(path)] = '/';
	}

	d = opendir(path);

	if (!d)
	{
		perror("Error reading image_directory");
		my_printf("\n");
		return 0;
	}

	do
	{
		entry = readdir(d);
		if (entry)
		{
			if ((strstr(entry->d_name, "kernel") != NULL
			  && strstr(entry->d_name, ".bin")   != NULL)			// ET-xx00, XP1000, VU boxes, DAGS boxes
			 || strcmp(entry->d_name, "uImage") == 0)				// Spark boxes
			{
				strcpy(kernel_filename, path);
				strcpy(&kernel_filename[strlen(path)], entry->d_name);
				stat(kernel_filename, &kernel_file_stat);
				my_printf("Found kernel file: %s\n", kernel_filename);
			}
			if (strcmp(entry->d_name, "rootfs.bin") == 0			// ET-xx00, XP1000
			 || strcmp(entry->d_name, "root_cfe_auto.bin") == 0		// Solo2
			 || strcmp(entry->d_name, "root_cfe_auto.jffs2") == 0	// other VU boxes
			 || strcmp(entry->d_name, "oe_rootfs.bin") == 0			// DAGS boxes
			 || strcmp(entry->d_name, "e2jffs2.img") == 0			// Spark boxes
			 || strcmp(entry->d_name, "rootfs.tar.bz2") == 0		// solo4k
			 || strcmp(entry->d_name, "rootfs.ubi") == 0)			// Zgemma H9
			{
				strcpy(rootfs_filename, path);
				strcpy(&rootfs_filename[strlen(path)], entry->d_name);
				stat(rootfs_filename, &rootfs_file_stat);
				my_printf("Found rootfs file: %s\n", rootfs_filename);
			}
		}
	} while (entry);

	closedir(d);

	return 1;
}

int read_args(int argc, char *argv[])
{
	int option_index = 0;
	int opt;
	static const char *short_options = "k::r::nm:fqh";
	static const struct option long_options[] = {
												{"kernel" , optional_argument, NULL, 'k'},
												{"rootfs" , optional_argument, NULL, 'r'},
												{"nowrite", no_argument      , NULL, 'n'},
												{"multi"  , required_argument, NULL, 'm'},
												{"force"  , optional_argument, NULL, 'f'},
												{"quiet"  , no_argument      , NULL, 'q'},
												{"help"   , no_argument      , NULL, 'h'},
												{NULL     , no_argument      , NULL,  0} };

	multiboot_partition = -1;
	user_kernel = 0;
	user_rootfs = 0;

	while ((opt= getopt_long(argc, argv, short_options, long_options, &option_index)) != -1)
	{
		switch (opt)
		{
			case 'k':
				flash_kernel = 1;
				if (optarg)
				{
					if ((!strncmp(optarg, "mtd", 3)) || (!strncmp(optarg, "mmcblk", 6)) || (!strncmp(optarg, "sd", 2)))
					{
						my_printf("Flashing kernel with arg %s\n", optarg);
						strcpy(kernel_device_arg, optarg);
						user_kernel = 1;
					}
				}
				else
					my_printf("Flashing kernel\n");
				break;
			case 'r':
				flash_rootfs = 1;
				if (optarg)
				{
					if ((!strncmp(optarg, "mtd", 3)) || (!strncmp(optarg, "mmcblk", 6)) || (!strncmp(optarg, "sd", 2)))
					{
						my_printf("Flashing rootfs with arg %s\n", optarg);
						strcpy(rootfs_device_arg, optarg);
						user_rootfs = 1;
					}
				}
				else
					my_printf("Flashing rootfs\n");
				break;
			case 'm':
				if (optarg)
					if (strlen(optarg) == 1 && ((int)optarg[0] >= 48) && ((int)optarg[0] <= 57))
					{
						multiboot_partition = strtol(optarg, NULL, 10);
						my_printf("Flashing multiboot partition %d\n", multiboot_partition);
					}
					else
					{
						my_printf("Error: Wrong multiboot partition value. Only values between 0 and 9 are allowed!\n");
						show_help = 1;
						return 0;
					}
				break;
			case 'n':
				no_write = 1;
				break;
			case 'f':
				force = 1;
				break;
			case 'q':
				quiet = 1;
				break;
			case '?':
				show_help = 1;
				return 0;
		}
	}

	if (argc == 1)
	{
		show_help = 1;
		return 0;
	}

	if (optind + 1 < argc)
	{
		my_printf("Wrong parameter: %s\n\n", argv[optind+1]);
		show_help = 1;
		return 0;
	}
	else if (optind + 1 == argc)
	{
		if (!find_image_files(argv[optind]))
			return 0;

		if (flash_kernel == 0 && flash_rootfs== 0) // set defaults
		{
			my_printf("Setting default parameter: Flashing kernel and rootfs\n");
			flash_kernel = 1;
			flash_rootfs = 1;
		}
	}
	else
	{
		my_printf("Error: Image_directory parameter missing!\n\n");
		show_help = 1;
		return 0;
	}

	return 1;
}

int read_mtd_file()
{
	FILE* f;

	f = fopen("/proc/mtd", "r");
	if (f == NULL)
	{ 
		perror("Error while opening /proc/mtd");
		// for testing try to open local mtd file
		f = fopen("./mtd", "r");
		if (f == NULL)
			return 0;
	}

	char line [1000];
	char dev  [1000];
	char size [1000];
	char esize[1000];
	char name [1000];
	char dev_path[] = "/dev/";
	int line_nr = 0;
	unsigned long devsize;
	int wrong_user_mtd = 0;

	my_printf("Found /proc/mtd entries:\n");
	my_printf("Device:   Size:     Erasesize:  Name:                   Image:\n");
	while (fgets(line, 1000, f) != NULL)
	{
		line_nr++;
		if (line_nr == 1) // check header
		{
			sscanf(line, "%s%s%s%s", dev, size, esize, name);
			if (strcmp(dev  , "dev:") != 0
			 || strcmp(size , "size") != 0
			 || strcmp(esize, "erasesize") != 0
			 || strcmp(name , "name") != 0)
			{
				my_printf("Error: /proc/mtd has an invalid format\n");
				fclose(f);
				return 0;
			}
		}
		else
		{
			sscanf(line, "%s%s%s%s", dev, size, esize, name);
			my_printf("%s %12s %9s    %-18s", dev, size, esize, name);
			devsize = strtoul(size, 0, 16);
			if (dev[strlen(dev)-1] == ':') // cut ':'
				dev[strlen(dev)-1] = '\0';
			// user selected kernel
			if (user_kernel && !strcmp(dev, kernel_device_arg))
			{
				strcpy(&kernel_device[0], dev_path);
				strcpy(&kernel_device[5], kernel_device_arg);
				if (kernel_file_stat.st_size <= devsize)
				{
					if ((strcmp(name, "\"kernel\"") == 0
						|| strcmp(name, "\"nkernel\"") == 0
						|| strcmp(name, "\"kernel2\"") == 0))
					{
						if (kernel_filename[0] != '\0')
							my_printf("  ->  %s <- User selected!!\n", kernel_filename);
						else
							my_printf("  <-  User selected!!\n");
						found_kernel_device = 1;
					}
					else
					{
						my_printf("  <-  Error: Selected by user is not a kernel mtd!!\n");
						wrong_user_mtd = 1;
					}
				}
				else
				{
					my_printf("  <-  Error: Kernel file is bigger than device size!!\n");
					wrong_user_mtd = 1;
				}
			}
			// user selected rootfs
			else if (user_rootfs && !strcmp(dev, rootfs_device_arg))
			{
				strcpy(&rootfs_device[0], dev_path);
				strcpy(&rootfs_device[5], rootfs_device_arg);
				if (rootfs_file_stat.st_size <= devsize
					&& strcmp(esize, "0001f000") != 0)
				{
					if (strcmp(name, "\"rootfs\"") == 0
						|| strcmp(name, "\"rootfs2\"") == 0)
					{
						if (rootfs_filename[0] != '\0')
							my_printf("  ->  %s <- User selected!!\n", rootfs_filename);
						else
							my_printf("  <-  User selected!!\n");
						found_rootfs_device = 1;
					}
					else
					{
						my_printf("  <-  Error: Selected by user is not a rootfs mtd!!\n");
						wrong_user_mtd = 1;
					}
				}
				else if (strcmp(esize, "0001f000") == 0)
				{
					my_printf("  <-  Error: Invalid erasesize\n");
					wrong_user_mtd = 1;
				}
				else
				{
					my_printf("  <-  Error: Rootfs file is bigger than device size!!\n");
					wrong_user_mtd = 1;
				}
			}
			// auto kernel
			else if (!user_kernel
					&& (strcmp(name, "\"kernel\"") == 0
						|| strcmp(name, "\"nkernel\"") == 0))
			{
				if (found_kernel_device)
				{
					my_printf("\n");
					continue;
				}
				strcpy(&kernel_device[0], dev_path);
				strcpy(&kernel_device[5], dev);
				if (kernel_file_stat.st_size <= devsize)
				{
					if (kernel_filename[0] != '\0')
						my_printf("  ->  %s\n", kernel_filename);
					else
						my_printf("\n");
					found_kernel_device = 1;
				}
				else
					my_printf("  <-  Error: Kernel file is bigger than device size!!\n");
			}
			// auto rootfs
			else if (!user_rootfs && strcmp(name, "\"rootfs\"") == 0)
			{
				if (found_rootfs_device)
				{
					my_printf("\n");
					continue;
				}
				strcpy(&rootfs_device[0], dev_path);
				strcpy(&rootfs_device[5], dev);
				unsigned long devsize;
				devsize = strtoul(size, 0, 16);
				if (rootfs_file_stat.st_size <= devsize
					&& strcmp(esize, "0001f000") != 0)
				{
					if (rootfs_filename[0] != '\0')
						my_printf("  ->  %s\n", rootfs_filename);
					else
						my_printf("\n");
					found_rootfs_device = 1;
				}
				else if (strcmp(esize, "0001f000") == 0)
					my_printf("  <-  Error: Invalid erasesize\n");
				else
					my_printf("  <-  Error: Rootfs file is bigger than device size!!\n");
			}
			else
				my_printf("\n");
		}
	}

	my_printf("Using kernel mtd device: %s\n", kernel_device);
	my_printf("Using rootfs mtd device: %s\n", rootfs_device);

	fclose(f);

	if (wrong_user_mtd)
	{
		my_printf("Error: User selected mtd device cannot be used!\n");
		return 0;
	}

	return 1;
}

int kernel_flash(char* device, char* filename)
{
	if (rootfs_type == EXT4)
		return flash_ext4_kernel(device, filename, kernel_file_stat.st_size, quiet, no_write);
	else
		return flash_ubi_jffs2_kernel(device, filename, quiet, no_write);
}

int rootfs_flash(char* device, char* filename)
{
	if (rootfs_type == EXT4)
		return flash_ext4_rootfs(filename, quiet, no_write);
	else
		return flash_ubi_jffs2_rootfs(device, filename, rootfs_type, quiet, no_write);
}

// read root filesystem and checks whether /newroot is mounted as tmpfs
void readMounts()
{
	FILE* f;
	char* pos_start;
	char* pos_end;
	int k;

	for (k = 0; k < 30; k++)
		media_mounts[k][0] = '\0';

	rootfs_type = UNKNOWN;

	f = fopen("/proc/mounts", "r");
	if (f == NULL)
	{ 
		perror("Error while opening /proc/mounts");
		return;
	}

	char line [1000];
	while (fgets(line, 1000, f) != NULL)
	{
		if (strstr (line, " / ") != NULL &&
			strstr (line, "rootfs") != NULL &&
			strstr (line, "ubifs") != NULL)
		{
			my_printf("Found UBIFS rootfs\n");
			rootfs_type = UBIFS;
		}
		else if (strstr (line, " / ") != NULL &&
				 strstr (line, "root") != NULL &&
				 strstr (line, "jffs2") != NULL)
		{
			my_printf("Found JFFS2 rootfs\n");
			rootfs_type = JFFS2;
		}
		else if (strstr (line, " / ") != NULL &&
				 strstr (line, "ext4") != NULL)
		{
			my_printf("Found EXT4 rootfs\n");
			rootfs_type = EXT4;
		}
		else if (strstr (line, "/newroot") != NULL &&
				 strstr (line, "tmpfs") != NULL)
		{
			my_printf("Found mounted /newroot\n");
			newroot_mounted = 1;
		}
		else if ((pos_start = strstr (line, " /media/")) != NULL && media_mount_count < 30)
		{
			pos_end = strstr(pos_start + 1, " ");
			if (pos_end)
			{
				strncpy(media_mounts[media_mount_count], pos_start + 1, pos_end - pos_start - 1);
				media_mount_count++;
			}
		}
	}

	fclose(f);

	if (rootfs_type == UNKNOWN)
		my_printf("Found unknown rootfs\n");
}

int exec_ps()
{
	// call ps
	optind = 0; // reset getopt_long
	char* argv[] = {
		"ps",		// program name
		NULL
	};
	int argc = (int)(sizeof(argv) / sizeof(argv[0])) - 1;

	my_printf("Execute: ps\n");
	if (ps_main(argc, argv) == 9999)
	{
		return 1; // e2 found
	}
	return 0; // e2 not found
}

int check_e2_stopped()
{
	int time = 0;
	int max_time = 70;
	int e2_found = 1;

//	set_step_progress(0);
	if (!quiet)
		my_printf("Checking E2 is running...\n");
	while (time < max_time && e2_found)
	{
		e2_found = exec_ps();

		if (!e2_found)
		{
			if (!quiet)
				my_printf("E2 is stopped\n");
		}
		else
		{
			sleep(2);
			time += 2;
			if (!quiet)
				my_printf("E2 still running\n");
		}
	//	set_step_progress(time * 100 / max_time);
	}

	if (e2_found)
		return 0;

	return 1;
}

int exec_fuser_kill()
{
	optind = 0; // reset getopt_long
	char* argv[] = {
		"fuser",		// program name
		"-k",			// kill
		"-m",			// mount point
		"/oldroot/",	// rootfs
		NULL
	};
	int argc = (int)(sizeof(argv) / sizeof(argv[0])) - 1;

	my_printf("Execute: fuser -k -m /oldroot/\n");
	if (!no_write)
		if (fuser_main(argc, argv) != 0)
			return 0;

	return 1;
}

int daemonize()
{
	// Prevents that ofgwrite will be killed when init 1 is performed
	my_printf("daemonize\n");

	pid_t pid = fork();
	if (pid < 0)
	{
		my_printf("Error fork failed\n");
		return 0;
	}
	else if (pid > 0)
	{
		// stop parent
		exit(EXIT_SUCCESS);
	}

	if (setsid() < 0)
	{
		my_printf("Error setsid failed\n");
		return 0;
	}

	pid = fork();
	if (pid < 0)
	{
		my_printf("Error 2. fork failed\n");
		return 0;
	}
	else if (pid > 0)
	{
		// stop child
		exit(EXIT_SUCCESS);
	}

	umask(0);
	my_printf(" successful\n");
	return 1;
}

int umount_rootfs(int steps)
{
	DIR *dir;
	int multilib = 1;

	if ((dir = opendir("/lib64")) == NULL)
	{
		multilib = 0;
	}

	int ret = 0;
	my_printf("start umount_rootfs\n");
	// the start script creates /newroot dir and mount tmpfs on it
	// create directories
	ret += chdir("/newroot");
	ret += mkdir("/newroot/bin", 777);
	ret += mkdir("/newroot/dev", 777);
	ret += mkdir("/newroot/etc", 777);
	ret += mkdir("/newroot/dev/pts", 777);
	ret += mkdir("/newroot/lib", 777);
	ret += mkdir("/newroot/media", 777);
	ret += mkdir("/newroot/oldroot", 777);
	ret += mkdir("/newroot/oldroot_bind", 777);
	ret += mkdir("/newroot/proc", 777);
	ret += mkdir("/newroot/run", 777);
	ret += mkdir("/newroot/sbin", 777);
	ret += mkdir("/newroot/sys", 777);
	ret += mkdir("/newroot/usr", 777);
	ret += mkdir("/newroot/usr/lib", 777);
	ret += mkdir("/newroot/usr/lib/autofs", 777);
	ret += mkdir("/newroot/var", 777);
	ret += mkdir("/newroot/var/volatile", 777);

	if (multilib)
	{
		ret += mkdir("/newroot/lib64", 777);
		ret += mkdir("/newroot/usr/lib64", 777);
		ret += mkdir("/newroot/usr/lib64/autofs", 777);
	}

	if (ret != 0)
	{
		my_printf("Error creating necessary directories\n");
		return 0;
	}

	// we need init and libs to be able to exec init u later
	if (multilib)
	{
		ret =  system("cp -arf /bin/busybox*     /newroot/bin");
		ret += system("cp -arf /bin/sh*          /newroot/bin");
		ret += system("cp -arf /bin/bash*        /newroot/bin");
		ret += system("cp -arf /sbin/init*       /newroot/sbin");
		ret += system("cp -arf /sbin/getty       /newroot/sbin");
		ret += system("cp -arf /lib64/libc*        /newroot/lib64");
		ret += system("cp -arf /lib64/ld*          /newroot/lib64");
		ret += system("cp -arf /lib64/libtinfo*    /newroot/lib64");
		ret += system("cp -arf /lib64/libdl*       /newroot/lib64");
	}
	else
	{
		ret =  system("cp -arf /bin/busybox*     /newroot/bin");
		ret += system("cp -arf /bin/sh*          /newroot/bin");
		ret += system("cp -arf /bin/bash*        /newroot/bin");
		ret += system("cp -arf /sbin/init*       /newroot/sbin");
		ret += system("cp -arf /sbin/getty       /newroot/sbin");
		ret += system("cp -arf /lib/libc*        /newroot/lib");
		ret += system("cp -arf /lib/ld*          /newroot/lib");
		ret += system("cp -arf /lib/libtinfo*    /newroot/lib");
		ret += system("cp -arf /lib/libdl*       /newroot/lib");
	}

	if (ret != 0)
	{
		my_printf("Error copying binary and libs\n");
		return 0;
	}

	// libcrypt is moved from /lib to /usr/libX in new OE versions
	if (multilib)
	{
		ret = system("cp -arf /lib64/libcrypt*    /newroot/lib64");
		if (ret != 0)
		{
			ret = system("cp -arf /usr/lib64/libcrypt*    /newroot/usr/lib64");
			if (ret != 0)
			{
				my_printf("Error copying libcrypto lib\n");
				return 0;
			}
		}
	}
	else
	{
		ret = system("cp -arf /lib/libcrypt*    /newroot/lib");
		if (ret != 0)
		{
			ret = system("cp -arf /usr/lib/libcrypt*    /newroot/usr/lib");
			if (ret != 0)
			{
				my_printf("Error copying libcrypto lib\n");
				return 0;
			}
		}
	}

	// copy for automount ignore errors as autofs is maybe not installed
	if (multilib)
	{
		ret = system("cp -arf /usr/sbin/autom*  /newroot/bin");
		ret += system("cp -arf /etc/auto*        /newroot/etc");
		ret += system("cp -arf /lib64/libpthread*  /newroot/lib64");
		ret += system("cp -arf /lib64/libnss*      /newroot/lib64");
		ret += system("cp -arf /lib64/libnsl*      /newroot/lib64");
		ret += system("cp -arf /lib64/libresolv*   /newroot/lib64");
		ret += system("cp -arf /usr/lib64/libtirp* /newroot/usr/lib64");
		ret += system("cp -arf /usr/lib64/autofs/* /newroot/usr/lib64/autofs");
		ret += system("cp -arf /etc/nsswitch*    /newroot/etc");
		ret += system("cp -arf /etc/resolv*      /newroot/etc");
	}
	else
	{
		ret = system("cp -arf /usr/sbin/autom*  /newroot/bin");
		ret += system("cp -arf /etc/auto*        /newroot/etc");
		ret += system("cp -arf /lib/libpthread*  /newroot/lib");
		ret += system("cp -arf /lib/libnss*      /newroot/lib");
		ret += system("cp -arf /lib/libnsl*      /newroot/lib");
		ret += system("cp -arf /lib/libresolv*   /newroot/lib");
		ret += system("cp -arf /usr/lib/libtirp* /newroot/usr/lib");
		ret += system("cp -arf /usr/lib/autofs/* /newroot/usr/lib/autofs");
		ret += system("cp -arf /etc/nsswitch*    /newroot/etc");
		ret += system("cp -arf /etc/resolv*      /newroot/etc");
	}

	// Switch to user mode 1
	my_printf("Switching to user mode 2\n");
	ret = system("init 2");
	if (ret)
	{
		my_printf("Error switching runmode!\n");
		draw_partition_text("Error switching runmode! Abort flashing.");
		sleep(5);
		return 0;
	}

	// it can take several seconds until E2 is shut down
	// wait because otherwise remounting read only is not possible
	draw_partition_text("Waiting system is stopped!");
	if (!check_e2_stopped())
	{
		my_printf("Error system can't be stopped! Abort flashing.\n");
		draw_partition_text("Error E2 can't be stopped! Abort flashing.");
		ret = system("init 3");
		return 0;
	}

	// some boxes don't allow to open framebuffer while e2 is running
	// reopen framebuffer to show the GUI
//	close_framebuffer();
//	init_framebuffer(steps);
//	show_main_window(1, ofgwrite_version);
//	set_overall_text("Flashing image");
	UI_GFX_Reinit("Waiting system is stopped!");
	sleep(2);

	ret = pivot_root("/newroot/", "oldroot");
	if (ret)
	{
		my_printf("Error executing pivot_root!\n");
		draw_partition_text("Error pivot_root! Abort flashing.");
		sleep(5);
		ret = system("init 3");
		return 0;
	}

	ret = chdir("/");
	// move mounts to new root
	ret =  mount("/oldroot/dev/", "dev/", NULL, MS_MOVE, NULL);
	ret += mount("/oldroot/proc/", "proc/", NULL, MS_MOVE, NULL);
	ret += mount("/oldroot/sys/", "sys/", NULL, MS_MOVE, NULL);
	ret += mount("/oldroot/var/volatile", "var/volatile/", NULL, MS_MOVE, NULL);
	// create link for tmp
	ret += symlink("/var/volatile/tmp", "/tmp");
	if (ret != 0)
	{
		my_printf("Error move mounts to newroot\n");
		draw_partition_text("Error move mounts to newroot. Abort flashing! Rebooting in 30 seconds!");
		sleep(30);
		reboot(LINUX_REBOOT_CMD_RESTART);
		return 0;
	}
#if 1
	ret = mount("/oldroot/media/", "media/", NULL, MS_MOVE, NULL);
	if (ret != 0)
	{
		// /media is no tmpfs -> move every mount
		my_printf("/media is not tmpfs\n");
		int k;
		char oldroot_path[1000];
		for (k = 0; k < media_mount_count; k++)
		{
			strcpy(oldroot_path, "/oldroot");
			strcat(oldroot_path, media_mounts[k]);
			mkdir(media_mounts[k], 777);
			my_printf("Moving %s to %s\n", oldroot_path, media_mounts[k]);
			// mount move: ignore errors as e.g. network shares cannot be moved
			mount(oldroot_path, media_mounts[k], NULL, MS_MOVE, NULL);
		}
	}
#endif
	// create link for mount/umount for autofs
	ret = symlink("/bin/busybox", "/bin/mount");
	ret += symlink("/bin/busybox", "/bin/umount");

	// try to restart autofs
	ret =  system("/bin/automount");
	if (ret != 0)
	{
		my_printf("Error starting autofs\n");
	}
	// restart init process
	ret = system("exec init u");
	sleep(3);

	// kill all remaining open processes which prevent umounting rootfs
	ret = exec_fuser_kill();
	if (!ret)
		my_printf("fuser successful\n");
	sleep(3);

	ret = umount("/oldroot/newroot");
	if (!ret)
	{
		my_printf("umount /oldroot/newroot successful\n");
	}
	else
	{
		my_printf("umount /oldroot/newroot not successful\n");
	}
	
	ret = umount("/oldroot/");
	if (!ret)
	{
		my_printf("umount /oldroot/ successful\n");
	}
	else
	{
		my_printf("umount /oldroot/ not successful\n");
	}

	if (!ret && rootfs_type == EXT4) // umount success and ext4 -> remount again
	{
		ret = mount(rootfs_device, "/oldroot_bind/", "ext4", 0, NULL);
		if (!ret)
			my_printf("remount to /oldroot_bind/ successful\n");
		else
		{
			my_printf("Error mounting(bind) root! Abort flashing.\n");
			draw_partition_text("Error remounting(bind) root! Abort flashing. Rebooting in 30 seconds.");
			sleep(30);
			reboot(LINUX_REBOOT_CMD_RESTART);
			return 0;
		}
	}
	else if (ret && rootfs_type == EXT4)
	// umount failed and ext4 -> bind mountpoint to new /oldroot_bind/
	// Using bind because otherwise all data in not moved filesystems under /oldroot will be deleted
	{
		ret = mount("/oldroot/", "/oldroot_bind/", "", MS_BIND, NULL);
		if (!ret)
			my_printf("bind to /oldroot_bind/ successful\n");
		else
		{
			my_printf("Error binding root! Abort flashing.\n");
			draw_partition_text("Error binding root! Abort flashing. Rebooting in 30 seconds.");
			sleep(30);
			reboot(LINUX_REBOOT_CMD_RESTART);
			return 0;
		}
	}
	else if (ret && rootfs_type != EXT4) // umount failed -> remount read only
	{
		ret = mount("/oldroot/", "/oldroot/", "", MS_REMOUNT | MS_RDONLY, NULL);
		if (ret)
		{
			my_printf("Error remounting root! Abort flashing.\n");
			draw_partition_text("Error remounting root! Abort flashing. Rebooting in 30 seconds");
			sleep(30);
			reboot(LINUX_REBOOT_CMD_RESTART);
			return 0;
		}
	}

	return 1;
}

int check_env()
{
	if (!newroot_mounted)
	{
		my_printf("Please use ofgwrite command to start flashing!\n");
		return 0;
	}

	return 1;
}

void ext4_kernel_dev_found(const char* dev, int partition_number)
{
	found_kernel_device = 1;
	sprintf(kernel_device, "%sp%d", dev, partition_number);
	my_printf("Using %s as kernel device\n", kernel_device);
}

void ext4_rootfs_dev_found(const char* dev, int partition_number)
{
	// Check whether rootfs is on the same device as current used rootfs
	sprintf(rootfs_device, "%sp", dev);
	if (strncmp(rootfs_device, current_rootfs_device, strlen(rootfs_device)) != 0)
	{
		my_printf("Rootfs(%s) is on different device than current rootfs(%s). Maybe wrong device selected. -> Aborting\n", dev, current_rootfs_device);
		return;
	}

	found_rootfs_device = 1;
	sprintf(rootfs_device, "%sp%d", dev, partition_number);
	my_printf("Using %s as rootfs device\n", rootfs_device);
}

void determineCurrentUsedRootfs()
{
	my_printf("Determine current rootfs\n");
	// Read /proc/cmdline to distinguish whether current running image should be flashed
	FILE* f;

	f = fopen("/proc/cmdline", "r");
	if (f == NULL)
	{
		perror("Error while opening /proc/cmdline");
		return;
	}

	char line[1000];
	char dev [1000];
	char* pos;
	char* pos2;
	char mmcblk_header[] = "blkdevparts=mmcblk0:";
	int mmcblk_header_len = strlen(mmcblk_header);
	int mmcblk_count = 0;
	
	memset(current_rootfs_device, 0, sizeof(current_rootfs_device));

	if (fgets(line, 1000, f) != NULL)
	{
		pos = strstr(line, "root=");
		if (pos)
		{
			pos2 = strstr(pos, " ");
			if (pos2)
			{
				strncpy(current_rootfs_device, pos + 5, pos2-pos-5);
			}
			else
			{
				strcpy(current_rootfs_device, pos + 5);
			}
		}
	}
	my_printf("Current rootfs is: %s\n", current_rootfs_device);
	
	if (strlen(line))
	{
		pos = strstr(line, mmcblk_header);
		if (pos)
		{
			pos2 = strstr(pos, " ");
			if (pos2)
			{
				*pos2 = '\0';
			}
			
			pos = pos + mmcblk_header_len;

			while(pos2 = strstr(pos, ",")){
				*pos2 = '\0';
				if (strstr(pos,"kernel") && !user_kernel)
				{
					found_kernel_device = 1;
					sprintf(kernel_device, "/dev/mmcblk0p%d", mmcblk_count + 1);
					my_printf("Using %s as kernel device\n", kernel_device);
				}
				
				if (strstr(pos,"rootfs") && !user_rootfs)
				{
					found_rootfs_device = 1;
					sprintf(rootfs_device, "/dev/mmcblk0p%d", mmcblk_count + 1);
					my_printf("Using %s as rootfs device\n", rootfs_device);
				}
				pos = pos2+1;
				mmcblk_count ++;
			}
		}
	}
	
	fclose(f);
}

void find_kernel_rootfs_device()
{
	determineCurrentUsedRootfs();

	// call fdisk -l
	optind = 0; // reset getopt_long
	char* argv[] = {
		"fdisk",		// program name
		"-l",			// list
		NULL
	};
	int argc = (int)(sizeof(argv) / sizeof(argv[0])) - 1;

	my_printf("Execute: fdisk -l\n");
	if (fdisk_main(argc, argv) != 0)
		return;

	// force user kernel
	if (user_kernel)
	{
		found_kernel_device = 1;
		sprintf(kernel_device, "/dev/%s", kernel_device_arg);
		my_printf("Using %s as kernel device\n", kernel_device);
	}

	// force user rootfs
	if (user_rootfs)
	{
		found_rootfs_device = 1;
		sprintf(rootfs_device, "/dev/%s", rootfs_device_arg);
		my_printf("Using %s as rootfs device\n", rootfs_device);
	}

	if (!found_kernel_device)
	{
		my_printf("Error: No kernel device found!\n");
		return;
	}

	if (!found_rootfs_device)
	{
		my_printf("Error: No rootfs device found!\n");
		return;
	}

	if (strcmp(rootfs_device, current_rootfs_device) != 0 && !force)
	{
		stop_e2_needed = 0;
		my_printf("Flashing currently not running image\n");
	}
}

// Checks whether kernel and rootfs device is bigger than the kernel and rootfs file
int check_device_size()
{
	unsigned long long devsize = 0;
	int fd = 0;
	// check kernel
	if (found_kernel_device && kernel_filename[0] != '\0')
	{
		fd = open(kernel_device, O_RDONLY);
		if (fd <= 0)
		{
			my_printf("Unable to open kernel device %s. Aborting\n", kernel_device);
			return 0;
		}
		if (ioctl(fd, BLKGETSIZE64, &devsize))
		{
			my_printf("Couldn't determine kernel device size. Aborting\n");
			return 0;
		}
		if (kernel_file_stat.st_size > devsize)
		{
			my_printf("Kernel file(%lld) is bigger than kernel device(%llu). Aborting\n", kernel_file_stat.st_size, devsize);
			return 0;
		}
	}

	// check rootfs
	if (found_rootfs_device && rootfs_filename[0] != '\0')
	{
		fd = open(rootfs_device, O_RDONLY);
		if (fd <= 0)
		{
			my_printf("Unable to open rootfs device %s. Aborting\n", rootfs_device);
			return 0;
		}
		if (ioctl(fd, BLKGETSIZE64, &devsize))
		{
			my_printf("Couldn't determine rootfs device size. Aborting\n");
			return 0;
		}
		if (rootfs_file_stat.st_size > devsize)
		{
			my_printf("Rootfs file (%lld) is bigger than rootfs device(%llu). Aborting\n", rootfs_file_stat.st_size, devsize);
			return 0;
		}
	}

	return 1;
}

void handle_busybox_fatal_error()
{
	my_printf("Error flashing rootfs! System won't boot. Please flash backup! System will reboot in 60 seconds\n");
	set_error_text1("Error untar rootfs. System won't boot!");
	set_error_text2("Please flash backup! Rebooting in 60 sec");
	if (stop_e2_needed)
	{
		sleep(60);
		reboot(LINUX_REBOOT_CMD_RESTART);
	}
	sleep(30);
	close_framebuffer();
	exit(EXIT_FAILURE);
}

typedef struct
{
	char mtd_name[128];
	char partition_name[128];
	char img_name[128];
	char erase_full;
}update_partition_t;

static update_partition_t update_mtd_partitions[32];
static int				update_mtd_num = 0;
static char			update_need_recover = 0;
static unsigned long long update_img_total_size = 0;
/* partitions.txt format
partition name:     mtd index:      img_name

boot:0:fastboo.bin
bootargs:1:bootargs.bin
baseparam:2:baseparam.bin
pqparam:3:pqparam.bin
logo:4:logo.jpg
deviceinfo:5:deviceinfo.bin
softwareinfo:6:softwareinfo.bin
loader:7:apploader.bin
kernel:8:uImage
rootfs:9:rootfs.bin


*/
void prepare_update_partitions(char *folder)
{
	struct stat64 filestat;
	struct mtd_info_user mtdInfo;
	int	mtd_fd, mtd_index;
	FILE *fp_partitions;
	char line[256];
	char tmp_buff[128];

	update_mtd_num = 0;
	update_img_total_size = 0;
	
	sprintf(tmp_buff, "%s/partitions.txt", folder);
	fp_partitions = fopen(tmp_buff,"rt");

	if(fp_partitions)
	{
	//	my_printf("open %s success\n", tmp_buff);

		while(!feof(fp_partitions))
		{
			if (!fgets(line, sizeof(line), fp_partitions))
			{
				break;
			}
		//	my_printf("got line %s\n", line);

			if(sscanf(line, "%[^:]:%d:%s", update_mtd_partitions[update_mtd_num].partition_name, &mtd_index, tmp_buff) == 3)
			{
				sprintf(update_mtd_partitions[update_mtd_num].img_name, "%s/%s", folder, tmp_buff);
				sprintf(update_mtd_partitions[update_mtd_num].mtd_name, "/dev/mtd%d", mtd_index);
				if(strcmp(update_mtd_partitions[update_mtd_num].partition_name, "rootfs") == 0)
				{
					update_mtd_partitions[update_mtd_num].erase_full = 1;
					ubi_detach_dev(update_mtd_partitions[update_mtd_num].mtd_name, 0, 0);
				}
				else
				{
					update_mtd_partitions[update_mtd_num].erase_full = 0;
				}

				if(update_mtd_partitions[update_mtd_num].erase_full)
				{
					mtd_fd = open(update_mtd_partitions[update_mtd_num].mtd_name, O_RDONLY|O_SYNC);
					if(mtd_fd != -1)
					{
						memset(&mtdInfo,0,sizeof(struct mtd_info_user));
						ioctl(mtd_fd, MEMGETINFO, &mtdInfo);
						update_img_total_size += mtdInfo.size;
						close(mtd_fd);
					}
					else
					{
						if (stat64(update_mtd_partitions[update_mtd_num].img_name, &filestat) == 0)
						{
							update_img_total_size += filestat.st_size;
						}
					}
				}
				else
				{
					if (stat64(update_mtd_partitions[update_mtd_num].img_name, &filestat) == 0)
					{
						update_img_total_size += filestat.st_size;
					}
				}

				my_printf("got partition: %s, %s, %s\n", update_mtd_partitions[update_mtd_num].partition_name, update_mtd_partitions[update_mtd_num].img_name, update_mtd_partitions[update_mtd_num].mtd_name);
				update_mtd_num ++;
			}
		}
		fclose(fp_partitions);
	}
	else
	{
		my_printf("open %s failed\n", tmp_buff);
	}

	my_printf("got update_img_total_size %lld\n", update_img_total_size);
	if(access("/tmp/update_img/uImage", F_OK) == 0)
	{
		my_printf("got /tmp/update_img/uImage\n");
	}
	if(access("/tmp/update_img/rootfs.bin", F_OK) == 0)
	{
		my_printf("got /tmp/update_img/rootfs.bin\n");
	}

	if(access("/tmp/weather", F_OK) == 0)
	{
		my_printf("got /tmp/weather\n");
	}
	if(access("/tmp/youtube", F_OK) == 0)
	{
		my_printf("got /tmp/youtube\n");
	}
	if(access("/tmp/iptv", F_OK) == 0)
	{
		my_printf("got /tmp/iptv\n");
	}
}
static unsigned long long next_good_eraseblock(int fd, struct mtd_info_user *meminfo,
		unsigned long long block_offset)
{
	while (1) {
		loff_t offs;

		if (block_offset >= meminfo->size) {
			return block_offset; /* let the caller exit */
		}
		offs = block_offset;
		if (ioctl(fd, MEMGETBADBLOCK, &offs) == 0)
			return block_offset;
		/* ioctl returned 1 => "bad block" */
		block_offset += meminfo->erasesize;
	}
}

void program_update_partitions(void)
{
	int 	i;
	int	mtd_fd, img_fd;
	struct erase_info_user erase;
	struct mtd_info_user mtdInfo;
	
	void						*CompareBuff_p, *file_buff;
	unsigned long long			ReadSize, ComparedSize, WriteSize, TotalUpdatedSize;
	struct stat64 filestat;
	long long		CurrPartDataSize, CurrPartRestSize, CurrPartReadOffset;
	char		NeedWrite, EraseFull;
	int 		need_recover = 0;
	long long		WriteOffset;
	char tmp_str[128];

	TotalUpdatedSize = 0;
	draw_total_progess(0);
	for(i=0; i<update_mtd_num; i++)
	{
		sprintf(tmp_str, "Programing %s", update_mtd_partitions[i].partition_name);
		draw_partition_text(tmp_str);
		draw_partition_progess(0);
		
		mtd_fd = -1;
		img_fd = -1;
		CompareBuff_p = NULL;
		file_buff = NULL;
		
		img_fd = open(update_mtd_partitions[i].img_name, O_RDONLY|O_SYNC);
		if(img_fd == -1)
		{
			my_printf("file = %s, line = %d, %s\n", __FILE__, __LINE__, update_mtd_partitions[i].img_name);
			continue;
		}
		
		mtd_fd = open(update_mtd_partitions[i].mtd_name, O_RDWR|O_SYNC);
		if(mtd_fd == -1)
		{
			my_printf("file = %s, line = %d, %s\n", __FILE__, __LINE__, update_mtd_partitions[i].mtd_name);
			goto end;
		}
		
		/*deal with one mtdblock*/
		memset(&mtdInfo,0,sizeof(struct mtd_info_user));
		ioctl(mtd_fd, MEMGETINFO, &mtdInfo);
		erase.length = mtdInfo.erasesize;
		erase.start = 0;
		WriteOffset = 0;
		
		CompareBuff_p = malloc(mtdInfo.erasesize);
		if(CompareBuff_p == NULL)
		{
			my_printf("file = %s, line = %d\n", __FILE__, __LINE__);
			goto end;
		}
		file_buff = malloc(mtdInfo.erasesize);
		if(file_buff == NULL)
		{
			my_printf("file = %s, line = %d\n", __FILE__, __LINE__);
			goto end;
		}
		
		if (stat64(update_mtd_partitions[i].img_name, &filestat) != 0)
		{
			my_printf("file = %s, line = %d\n", __FILE__, __LINE__);
			goto end;
		}
		CurrPartDataSize = filestat.st_size;

		EraseFull = update_mtd_partitions[i].erase_full;
		need_recover = update_need_recover;
		
		CurrPartRestSize = CurrPartDataSize;
		CurrPartReadOffset = 0;
		my_printf("start part %s, data size %lld\n", update_mtd_partitions[i].partition_name, CurrPartDataSize);
		while(WriteOffset < mtdInfo.size)/**/
		{
			/*compare one erase block data is different or same*/
			NeedWrite = FALSE;
			WriteOffset = next_good_eraseblock(mtd_fd, &mtdInfo, WriteOffset);
			if(WriteOffset >= mtdInfo.size)
			{
				break;
			}
			ReadSize = mtdInfo.erasesize;
			if(CurrPartRestSize)
			{
				if(CurrPartRestSize > mtdInfo.erasesize)
				{
					ReadSize = mtdInfo.erasesize;
				}
				else
				{
					ReadSize = CurrPartRestSize;
				}
				lseek(mtd_fd, WriteOffset, SEEK_SET);
				read(mtd_fd, CompareBuff_p, mtdInfo.erasesize);
				
				lseek(img_fd, CurrPartReadOffset, SEEK_SET);
				read(img_fd, file_buff, ReadSize);

				if(memcmp(file_buff, CompareBuff_p, ReadSize) != 0)
				{
					my_printf("data different %08llx, offset %08llx\n", CurrPartReadOffset, WriteOffset);
					NeedWrite = TRUE;
				}
			}
			
			if((CurrPartRestSize == 0 && EraseFull) || NeedWrite)
			{
				/*erase block
				erase.start = WriteOffset;
				if(ioctl(mtd_fd, MEMERASE, &erase) != 0)
				{
					goto end;
				}*/
				my_printf("erase mtd %s, offset %08llx\n", update_mtd_partitions[i].partition_name, WriteOffset);
			}

			if(NeedWrite)
			{
				lseek(mtd_fd, WriteOffset, SEEK_SET);
			//	WriteSize = write(mtd_fd, file_buff, mtdInfo.erasesize);
				WriteSize = mtdInfo.erasesize;
				my_printf("write mtd %s, offset %08llx, size %lld, wrote size %lld\n", update_mtd_partitions[i].partition_name, WriteOffset, mtdInfo.erasesize, WriteSize);
			}
			
			if(CurrPartRestSize)
			{
				CurrPartReadOffset += ReadSize;
				CurrPartRestSize -= ReadSize;
			}
			WriteOffset += mtdInfo.erasesize;

			/*calculate progress*/
			if(EraseFull)
			{
				draw_partition_progess(WriteOffset*100/mtdInfo.size);
				TotalUpdatedSize += mtdInfo.erasesize;
			}
			else
			{
				draw_partition_progess(CurrPartReadOffset*100/CurrPartDataSize);
				if(CurrPartRestSize)
				{
					TotalUpdatedSize += ReadSize;
				}
			}
			draw_total_progess(TotalUpdatedSize*100/update_img_total_size);
		}
		free(file_buff); file_buff = NULL;
		free(CompareBuff_p); CompareBuff_p = NULL;
		close(img_fd); img_fd = -1;
		close(mtd_fd); mtd_fd = -1;
	}
	
	draw_partition_progess(100);
	draw_total_progess(100);
	draw_partition_text("Updating system firmware successful, will restart in few seconds.");
	sleep(2);
end:
	if(file_buff)
	{
		free(file_buff); file_buff = NULL;
	}
	if(CompareBuff_p)
	{
		free(CompareBuff_p); CompareBuff_p = NULL;
	}
	
	if(img_fd != -1)
	{
		close(img_fd); img_fd = -1;
	}
	if(mtd_fd != -1)
	{
		close(mtd_fd); mtd_fd = -1;
	}
}

int main(int argc, char *argv[])
{
	// Check if running on a box or on a PC. Stop working on PC to prevent overwriting important files
	// Open log
	openlog("ofgwrite", LOG_CONS | LOG_NDELAY, LOG_USER);

	my_printf("\nofgwrite Utility v%s\n", ofgwrite_version);
	my_printf("Author: Betacentauri\n");
	my_printf("Based upon: mtd-utils-native-1.5.1 and busybox 1.24.1\n");
	my_printf("Use at your own risk! Make always a backup before use!\n");
	my_printf("Don't use it if you use multiple ubi volumes in ubi layer!\n\n");
	
	{
		UI_GFX_Init();

		rootfs_type = UBIFS;
		// kill nmbd, smbd, rpc.mountd and rpc.statd -> otherwise remounting root read-only is not possible
		{
			system("killall nmbd");
			system("killall smbd");
			system("killall rpc.mountd");
			system("killall rpc.statd");
			system("/etc/init.d/softcam stop");
			system("killall CCcam");
			system("pkill -9 -f '[Oo][Ss][Cc][Aa][Mm]'");
			system("ps w | grep -i oscam | grep -v grep | awk '{print $1}'| xargs kill -9");
			system("pkill -9 -f '[Ww][Ii][Cc][Aa][Rr][Dd][Dd]'");
			system("ps w | grep -i wicardd | grep -v grep | awk '{print $1}'| xargs kill -9");
			system("killall kodi.bin");
			system("killall hddtemp");
			system("killall transmission-daemon");
			system("killall openvpn");
			system("/etc/init.d/sabnzbd stop");
			system("pkill -9 -f cihelper");
			system("pkill -9 -f ciplus_helper");
			system("pkill -9 -f ciplushelper");
			// kill VMC
			system("pkill -f vmc.sh");
			system("pkill -f DBServer.py");
			// stop autofs
			system("/etc/init.d/autofs stop");
			// ignore return values, because the processes might not run
		}

		// sync filesystem
		my_printf("Syncing filesystem\n");
		sync();
		sleep(1);

		{
			if (!daemonize())
			{
				closelog();
				return EXIT_FAILURE;
			}
			if (!umount_rootfs(0))
			{
				closelog();
				return EXIT_FAILURE;
			}
		}

		prepare_update_partitions("/tmp/update_img");
		program_update_partitions();
	}

	my_printf("\n");
	closelog();
	reboot(LINUX_REBOOT_CMD_RESTART);
	return EXIT_SUCCESS;
}
