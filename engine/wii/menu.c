/*
 * OpenBOR - https://www.chronocrash.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c)  OpenBOR Team
 */
// Adapted from sdl/menu.c.  Uses s_screen images instead of SDL surfaces.

#include <dirent.h>
#include <unistd.h>
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include "wupc/wupc.h"
#include "wiiport.h"
#include "video.h"
#include "control.h"
#include "packfile.h"
#include "hankaku.h"
#include "menufont.h"
#include "menustrings.h"
#include "stristr.h"

#include "pngdec.h"
#include "../resources/OpenBOR_Logo_480x272_png.h"
#include "../resources/OpenBOR_Logo_320x240_png.h"
#include "../resources/OpenBOR_Menu_480x272_png.h"
#include "../resources/OpenBOR_Menu_320x240_png.h"

#include "openbor.h"


extern int videoMode;

extern int PLAYER_MIN_Z;
extern int PLAYER_MAX_Z;
extern int BGHEIGHT;
extern s_videomodes videomodes;

#define RGB32(R,G,B) ((R << 16) | ((G) << 8) | (B))
#define RGB16(R,G,B) ((B&0xF8)<<8) | ((G&0xFC)<<3) | (R>>3)
#define RGB(R,G,B)   (bpp==16?RGB16(R,G,B):RGB32(R,G,B))

#define BLACK		RGB(  0,   0,   0)
#define WHITE		RGB(255, 255, 255)
#define RED			RGB(255,   0,   0)
#define	GREEN		RGB(  0, 255,   0)
#define BLUE		RGB(  0,   0, 255)
#define YELLOW		RGB(255, 255,   0)
#define PURPLE		RGB(255,   0, 255)
#define ORANGE		RGB(255, 128,   0)
#define GRAY		RGB(112, 128, 144)
#define LIGHT_GRAY  RGB(223, 223, 223)
#define DARK_RED	RGB(128,   0,   0)
#define DARK_GREEN	RGB(  0, 128,   0)
#define DARK_BLUE	RGB(  0,   0, 128)

#define LOG_SCREEN_TOP 2
#define LOG_SCREEN_END (isWide ? 26 : 23)

#define FIRST_KEYPRESS      1
#define IMPULSE_TIME        0.12f
#define FIRST_IMPULSE_TIME  1.2f

#define DIR_UP			0x00000001
#define DIR_RIGHT		0x00000002
#define DIR_DOWN		0x00000004
#define DIR_LEFT		0x00000008
#define WIIMOTE_A		0x00000010
#define WIIMOTE_B		0x00000020
#define WIIMOTE_1		0x00000040
#define WIIMOTE_2		0x00000080
#define WIIMOTE_PLUS	0x00000100
#define WIIMOTE_MINUS	0x00000200
#define WIIMOTE_HOME	0x00000400
#define NUNCHUK_C		0x00000800
#define NUNCHUK_Z		0x00001000
#define CC_A			0x00002000
#define CC_B			0x00004000
#define CC_X			0x00008000
#define CC_Y			0x00010000
#define CC_L			0x00020000
#define CC_R			0x00040000
#define CC_ZL			0x00080000
#define CC_ZR			0x00100000
#define CC_PLUS			0x00200000
#define CC_MINUS		0x00400000
#define CC_HOME			0x00800000
#define GC_A			0x01000000
#define GC_B			0x02000000
#define GC_X			0x04000000
#define GC_Y			0x08000000
#define GC_L			0x10000000
#define GC_R			0x20000000
#define GC_Z			0x40000000
#define GC_START		0x80000000

s_screen *Source = NULL;
s_screen *Scaler = NULL;
s_screen *Screen = NULL;
int bpp = 32;
int factor = 1;
int isFull = 0;
int isWide = 0;
int flags;
int dListTotal;
int dListCurrentPosition;
int dListScrollPosition;
int which_logfile = OPENBOR_LOG;
int buttonsHeld = 0;
int buttonsPressed = 0;
FILE *bgmFile = NULL;
extern unsigned long bothkeys, bothnewkeys;
fileliststruct *filelist;

typedef struct{
	stringptr *buf;
	int *pos;
	int line;
	int rows;
	char ready;
}s_logfile;
s_logfile logfile[2];

typedef struct{
	int x;
	int y;
	int width;
	int height;
}Rect;

typedef int (*ControlInput)();

int ControlMenu();
int ControlBGM();
void PlayBGM();
void StopBGM();
void fillRect(s_screen* dest, Rect* rect, u32 color);
static ControlInput pControl;

int Control()
{
	return pControl();
}

void refreshInput()
{
	unsigned long btns = 0;
	unsigned short gcbtns;
	WPADData *wpad;
	struct WUPCData *wupc;

	PAD_Init();
	WUPC_Init();
	PAD_ScanPads();
	gcbtns = PAD_ButtonsDown(0) | PAD_ButtonsHeld(0);
	WUPC_UpdateButtonStats();
	WPAD_ScanPads();
	wpad = WPAD_Data(0);
	wupc = WUPC_Data(0);

	if(wpad->exp.type == WPAD_EXP_CLASSIC)
	{
		// Left thumb stick
		if(wpad->exp.classic.ljs.mag >= 0.3)
		{
			if (wpad->exp.classic.ljs.ang >= 310 || wpad->exp.classic.ljs.ang <= 50)   btns |= DIR_UP;
			if (wpad->exp.classic.ljs.ang >= 130 && wpad->exp.classic.ljs.ang <= 230)  btns |= DIR_DOWN;
			if (wpad->exp.classic.ljs.ang >= 220 && wpad->exp.classic.ljs.ang <= 320)  btns |= DIR_LEFT;
			if (wpad->exp.classic.ljs.ang >= 40 && wpad->exp.classic.ljs.ang <= 140)   btns |= DIR_RIGHT;
		}
		// D-pad
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_UP)         btns |= DIR_UP;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_DOWN)       btns |= DIR_DOWN;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_LEFT)       btns |= DIR_LEFT;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_RIGHT)      btns |= DIR_RIGHT;
	}
	else if (wupc != NULL) // Pro Controller
	{
		if(wupc->button & WPAD_CLASSIC_BUTTON_UP)		  btns |= DIR_UP;
		if(wupc->button & WPAD_CLASSIC_BUTTON_DOWN) 	  btns |= DIR_DOWN;
		if(wupc->button & WPAD_CLASSIC_BUTTON_LEFT) 	  btns |= DIR_LEFT;
		if(wupc->button & WPAD_CLASSIC_BUTTON_RIGHT) 	  btns |= DIR_RIGHT;
		if(wupc->button & WPAD_CLASSIC_BUTTON_PLUS) 	  btns |= CC_PLUS;
		if(wupc->button & WPAD_CLASSIC_BUTTON_MINUS)	  btns |= CC_MINUS;
		if(wupc->button & WPAD_CLASSIC_BUTTON_HOME)       btns |= CC_HOME;
		if(wupc->button & WPAD_CLASSIC_BUTTON_A) 		  btns |= CC_A;
		if(wupc->button & WPAD_CLASSIC_BUTTON_B)		  btns |= CC_B;
		if(wupc->button & WPAD_CLASSIC_BUTTON_Y)          btns |= CC_Y;
		if(wupc->button & WPAD_CLASSIC_BUTTON_X)          btns |= CC_X;
		if(wupc->button & WPAD_CLASSIC_BUTTON_FULL_R)     btns |= CC_R;
		if(wupc->button & WPAD_CLASSIC_BUTTON_FULL_L)     btns |= CC_L;
		if(wupc->button & WPAD_CLASSIC_BUTTON_ZL)         btns |= CC_ZL;
		if(wupc->button & WPAD_CLASSIC_BUTTON_ZR)         btns |= CC_ZR;
		
		//analog stick  
		if(wupc->yAxisL > 200)							  btns |= DIR_UP;
		if(wupc->yAxisL < -200)							  btns |= DIR_DOWN;
		if(wupc->xAxisL > 200)							  btns |= DIR_RIGHT;
		if(wupc->xAxisL < -200)							  btns |= DIR_LEFT;
			
	}
	else if(wpad->exp.type == WPAD_EXP_NUNCHUK) // Wiimote + Nunchuk
	{
		if(wpad->exp.nunchuk.js.pos.y >= 0xB0)            btns |= DIR_UP;
		if(wpad->exp.nunchuk.js.pos.y <= 0x40)            btns |= DIR_DOWN;
		if(wpad->exp.nunchuk.js.pos.x <= 0x40)            btns |= DIR_LEFT;
		if(wpad->exp.nunchuk.js.pos.x >= 0xB0)            btns |= DIR_RIGHT;
		if(wpad->btns_h & WPAD_BUTTON_UP)                 btns |= DIR_UP;
		if(wpad->btns_h & WPAD_BUTTON_DOWN)               btns |= DIR_DOWN;
		if(wpad->btns_h & WPAD_BUTTON_LEFT)               btns |= DIR_LEFT;
		if(wpad->btns_h & WPAD_BUTTON_RIGHT)              btns |= DIR_RIGHT;
	}
	else // Wiimote held sideways
	{
		if(wpad->btns_h & WPAD_BUTTON_UP)                 btns |= DIR_LEFT;
		if(wpad->btns_h & WPAD_BUTTON_DOWN)               btns |= DIR_RIGHT;
		if(wpad->btns_h & WPAD_BUTTON_LEFT)               btns |= DIR_DOWN;
		if(wpad->btns_h & WPAD_BUTTON_RIGHT)              btns |= DIR_UP;
	}

	// GameCube analog stick and D-pad
	if(PAD_StickY(0) > 18)                                btns |= DIR_UP;
	if(PAD_StickY(0) < -18)                               btns |= DIR_DOWN;
	if(PAD_StickX(0) < -18)                               btns |= DIR_LEFT;
	if(PAD_StickX(0) > 18)                                btns |= DIR_RIGHT;
	if(gcbtns & PAD_BUTTON_UP)                            btns |= DIR_UP;
	if(gcbtns & PAD_BUTTON_DOWN)                          btns |= DIR_DOWN;
	if(gcbtns & PAD_BUTTON_LEFT)                          btns |= DIR_LEFT;
	if(gcbtns & PAD_BUTTON_RIGHT)                         btns |= DIR_RIGHT;

	// Controller buttons
	if(wpad->exp.type <= WPAD_EXP_NUNCHUK)
	{
		if(wpad->btns_h & WPAD_BUTTON_1)                      btns |= WIIMOTE_1;
		if(wpad->btns_h & WPAD_BUTTON_2)                      btns |= WIIMOTE_2;
		if(wpad->btns_h & WPAD_BUTTON_A)                      btns |= WIIMOTE_A;
		if(wpad->btns_h & WPAD_BUTTON_B)                      btns |= WIIMOTE_B;
		if(wpad->btns_h & WPAD_BUTTON_MINUS)                  btns |= WIIMOTE_MINUS;
		if(wpad->btns_h & WPAD_BUTTON_PLUS)                   btns |= WIIMOTE_PLUS;
		if(wpad->btns_h & WPAD_BUTTON_HOME)                   btns |= WIIMOTE_HOME;
		if(wpad->btns_h & WPAD_NUNCHUK_BUTTON_Z)              btns |= NUNCHUK_Z;
		if(wpad->btns_h & WPAD_NUNCHUK_BUTTON_C)              btns |= NUNCHUK_C;
	}
	else if(wpad->exp.type == WPAD_EXP_CLASSIC)
	{
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_A)              btns |= CC_A;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_B)              btns |= CC_B;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_Y)              btns |= CC_Y;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_X)              btns |= CC_X;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_MINUS)          btns |= CC_MINUS;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_PLUS)           btns |= CC_PLUS;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_HOME)           btns |= CC_HOME;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_FULL_R)         btns |= CC_R;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_FULL_L)         btns |= CC_L;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_ZL)             btns |= CC_ZL;
		if(wpad->btns_h & WPAD_CLASSIC_BUTTON_ZR)             btns |= CC_ZR;
	}

	// GameCube face buttons always available (Nintendont-style merge)
	if(gcbtns & PAD_BUTTON_X)                             btns |= GC_X;
	if(gcbtns & PAD_BUTTON_Y)                             btns |= GC_Y;
	if(gcbtns & PAD_BUTTON_A)                             btns |= GC_A;
	if(gcbtns & PAD_BUTTON_B)                             btns |= GC_B;
	if(gcbtns & PAD_TRIGGER_R)                            btns |= GC_R;
	if(gcbtns & PAD_TRIGGER_L)                            btns |= GC_L;
	if(gcbtns & PAD_TRIGGER_Z)                            btns |= GC_Z;
	if(gcbtns & PAD_BUTTON_START)                         btns |= GC_START;

	// update buttons pressed (not held)
	buttonsPressed = btns & ~buttonsHeld;
	buttonsHeld = btns;
}

void getAllLogs()
{
	int i, j, k;
	for(i=0; i<2; i++)
	{
		logfile[i].buf = readFromLogFile(i);
		if(logfile[i].buf != NULL)
		{
			logfile[i].pos = malloc(++logfile[i].rows * sizeof(int));
			if(logfile[i].pos == NULL) return;
			memset(logfile[i].pos, 0, logfile[i].rows * sizeof(int));

			for(k=0, j=0; j<logfile[i].buf->size; j++)
			{
				if(!k)
				{
					logfile[i].pos[logfile[i].rows - 1] = j;
					k = 1;
				}
				if(logfile[i].buf->ptr[j]=='\n')
				{
					int *_pos = malloc(++logfile[i].rows * sizeof(int));
					if(_pos == NULL) return;
					memcpy(_pos, logfile[i].pos, (logfile[i].rows - 1) * sizeof(int));
					_pos[logfile[i].rows - 1] = 0;
					free(logfile[i].pos);
					logfile[i].pos = NULL;
					logfile[i].pos = malloc(logfile[i].rows * sizeof(int));
					if(logfile[i].pos == NULL) return;
					memcpy(logfile[i].pos, _pos, logfile[i].rows * sizeof(int));
					free(_pos);
					_pos = NULL;
					logfile[i].buf->ptr[j] = 0;
					k = 0;
				}
				if(logfile[i].buf->ptr[j]=='\r') logfile[i].buf->ptr[j] = 0;
				if(logfile[i].rows>0xFFFFFFFE) break;
			}
			logfile[i].ready = 1;
		}
	}
}

void freeAllLogs()
{
	int i;
	for(i=0; i<2; i++)
	{
		if(logfile[i].ready)
		{
			free(logfile[i].buf);
			logfile[i].buf = NULL;
			free(logfile[i].pos);
			logfile[i].pos = NULL;
		}
	}
}

void sortList()
{
	int i, j;
	fileliststruct temp;
	if(dListTotal<2) return;
	for(j=dListTotal-1; j>0; j--)
	{
		for(i=0; i<j; i++)
		{
			if(stricmp(filelist[i].filename, filelist[i+1].filename)>0)
			{
				temp = filelist[i];
				filelist[i] = filelist[i+1];
				filelist[i+1] = temp;
			}
		}
	}
}

static int findPaks(void)
{
	int i = 0;
	DIR* dp = NULL;
	struct dirent* ds;
	dp = opendir(paksDir);
	if(dp != NULL)
   	{
		while((ds = readdir(dp)) != NULL)
		{
			if(packfile_supported(ds->d_name))
			{
				fileliststruct *copy = NULL;
				if(filelist == NULL) filelist = malloc(sizeof(fileliststruct));
				else
				{
					copy = malloc(i * sizeof(fileliststruct));
					memcpy(copy, filelist, i * sizeof(fileliststruct));
					free(filelist);
					filelist = malloc((i + 1) * sizeof(fileliststruct));
					memcpy(filelist, copy, i * sizeof(fileliststruct));
					free(copy); copy = NULL;
				}
				memset(&filelist[i], 0, sizeof(fileliststruct));
				strcpy(filelist[i].filename, ds->d_name);
				i++;
			}
		}
		closedir(dp);
   	}
	return i;
}

void copyScreens(s_screen *Image)
{
	// Copy Logo or Menu from Source to Scaler to give us a background
	// prior to printing to this s_screen.
	copyscreen_o(Scaler, Image, 0, 0);
}

void writeToScreen(s_screen* src)
{
	copyscreen(Screen, src);
}

void drawScreens(s_screen *Image, int x, int y)
{
	if(Image)
	{
		putscreen(Scaler, Image, x, y, NULL);
		freescreen(&Image);
		Image = NULL;
	}
	writeToScreen(Scaler);
	video_copy_screen(Screen);
}

static void drawMenuGlyph12(int x, int y, int col, int backcol, int fill, const unsigned short *glyph)
{
	int x1, y1;
	unsigned long data;
	unsigned short *line16 = NULL;
	unsigned long  *line32 = NULL;

	if(!glyph) return;

	if(bpp == 16) line16 = (unsigned short *)Scaler->data + x + y * Scaler->width;
	else          line32 = (unsigned long  *)Scaler->data + x + y * Scaler->width;

	for(y1 = 0; y1 < MENUFONT_HEIGHT; y1++)
	{
		data = glyph[y1];
		for(x1 = 0; x1 < MENUFONT_WIDTH; x1++)
		{
			if(data & (1 << (MENUFONT_WIDTH - 1 - x1)))
			{
				if(bpp == 16) *line16 = col;
				else          *line32 = col;
			}
			else if(fill)
			{
				if(bpp == 16) *line16 = backcol;
				else          *line32 = backcol;
			}

			if(bpp == 16) line16++;
			else          line32++;
		}
		if(bpp == 16) line16 += Scaler->width - MENUFONT_WIDTH;
		else          line32 += Scaler->width - MENUFONT_WIDTH;
	}
}

void printText(int x, int y, int col, int backcol, int fill, char *format, ...)
{
	int x1, y1, i;
	unsigned long data;
	unsigned short *line16 = NULL;
	unsigned long  *line32 = NULL;
	unsigned char *font;
	unsigned char ch = 0;
	const unsigned short *gbglyph = NULL;
	char buf[128] = {""};
	va_list arglist;
		va_start(arglist, format);
		vsprintf(buf, format, arglist);
		va_end(arglist);
	if(factor > 1){ y += 5; }

	for(i = 0; i < (int)sizeof(buf) && buf[i]; )
	{
		ch = (unsigned char)buf[i];

		if(ch >= 0x81 && buf[i + 1])
		{
			gbglyph = menu_font_lookup(ch, (unsigned char)buf[i + 1]);
			if(gbglyph)
			{
				drawMenuGlyph12(x, y, col, backcol, fill, gbglyph);
				x += MENUFONT_WIDTH;
				i += 2;
				continue;
			}
		}

		// ASCII / half-width mapping
		if (ch<0x20) ch = 0;
		else if (ch<0x80) { ch -= 0x20; }
		else if (ch<0xa0) { ch = 0; }
		else ch -= 0x40;
		font = (u8 *)&hankaku_font10[ch*10];
		// draw
		if (bpp == 16) line16 = (unsigned short *)Scaler->data + x + y * Scaler->width;
		else           line32 = (unsigned long  *)Scaler->data + x + y * Scaler->width;

		for (y1=0; y1<10; y1++)
		{
			data = *font++;
			for (x1=0; x1<5; x1++)
			{
				if (data & 1)
				{
					if (bpp == 16) *line16 = col;
				    else           *line32 = col;
				}
				else if (fill)
				{
					if (bpp == 16) *line16 = backcol;
					else           *line32 = backcol;
				}

				if (bpp == 16) line16++;
				else           line32++;

				data = data >> 1;
			}
			if (bpp == 16) line16 += Scaler->width-5;
			else           line32 += Scaler->width-5;
		}
		x += 5;
		i++;
	}
}

s_screen *getPreview(char *filename)
{
	int width = 160; //preview width
	int height = 120; //preview height
	s_screen *title = NULL;
	s_screen *scale = NULL;
	FILE *preview = NULL;
	
	char ssPath[MAX_FILENAME_LEN] = "";
	getBasePath(ssPath,"ScreenShots/",0); //get screenshots directory from base path
	strncat(ssPath, filename, strrchr(filename, '.') - filename); //remove extension from pak filename
	strcat(ssPath, " - 0000.png"); //add to end of pak filename
	preview = fopen(ssPath, "r"); //open preview image
	
	if(preview) //if preview image found
	{
		fclose(preview); //close preview image
		strcpy(packfile,"null.file"); //dummy pak file since we are loading outside a pak file
		
		//Create & Load & Scale Image
		if(!loadscreen32(ssPath, packfile, &title)) return NULL; //exit if image screen not loaded
		if((scale = allocscreen(width, height, title->pixelformat)) == NULL) return NULL; //exit if scaled screen not 
		scalescreen32(scale, title); //copy image to scaled down screen 
		
	} else { return NULL; }
	
	// ScreenShots within Menu will be saved as "Menu"
	strncpy(packfile,"Menu.ext",MAX_FILENAME_LEN);
	
	// Free Images and Terminate FileCaching
	if(title) freescreen(&title); //free image screen
	return scale; // return scaled down screen
}

static int hold_key_impulse(int key, float time_range, int start_press_flag, float start_time_eta)
{
	static int hold_time[64];
	static int first_keypress[64];
	static int second_keypress[64];
	int key_index = 0;
	int tmp_key = key;

	while(tmp_key >>= 1)
	{
		key_index++;
	}

	if(buttonsHeld & key)
	{
		unsigned time = timer_gettick();

		time_range *= GAME_SPEED;
		start_time_eta *= GAME_SPEED;
		if(!hold_time[key_index])
		{
			hold_time[key_index] = time;

			if(start_press_flag > 0 && !first_keypress[key_index])
			{
				first_keypress[key_index] = 1;
				return key;
			}
		}
		else if(time - hold_time[key_index] >= time_range)
		{
			if(start_time_eta > 0 && !second_keypress[key_index])
			{
				if(time - hold_time[key_index] < start_time_eta)
				{
					return 0;
				}
			}

			if(!second_keypress[key_index])
			{
				second_keypress[key_index] = 1;
			}
			hold_time[key_index] = 0;
			return key;
		}
	}
	else
	{
		hold_time[key_index] = 0;
		first_keypress[key_index] = 0;
		second_keypress[key_index] = 0;
	}

	return 0;
}

int ControlMenu()
{
	int status = -1;
	int dListMaxDisplay = MAX_PAGE_MODS_LENGTH - 1;
	//bothnewkeys = 0;
	//inputrefresh(0);
	refreshInput();

	buttonsPressed |= hold_key_impulse(DIR_DOWN, IMPULSE_TIME, FIRST_KEYPRESS, FIRST_IMPULSE_TIME);
	buttonsPressed |= hold_key_impulse(DIR_LEFT, IMPULSE_TIME, FIRST_KEYPRESS, FIRST_IMPULSE_TIME);
	buttonsPressed |= hold_key_impulse(DIR_UP, IMPULSE_TIME, FIRST_KEYPRESS, FIRST_IMPULSE_TIME);
	buttonsPressed |= hold_key_impulse(DIR_RIGHT, IMPULSE_TIME, FIRST_KEYPRESS, FIRST_IMPULSE_TIME);

	switch(buttonsPressed)
	{
		case DIR_UP:
			dListScrollPosition--;
			if(dListScrollPosition < 0)
			{
				dListScrollPosition = 0;
				dListCurrentPosition--;
			}
			if(dListCurrentPosition < 0) dListCurrentPosition = 0;
			break;

		case DIR_DOWN:
			dListCurrentPosition++;
			if(dListCurrentPosition > dListTotal - 1) dListCurrentPosition = dListTotal - 1;
			if(dListCurrentPosition > dListMaxDisplay)
	        {
		        if((dListCurrentPosition+dListScrollPosition) < dListTotal) dListScrollPosition++;
			    dListCurrentPosition = dListMaxDisplay;
			}
			break;

		case DIR_LEFT:
			dListScrollPosition -= MAX_PAGE_MODS_FAST_FORWARD;
			if(dListScrollPosition < 0)
			{
				dListScrollPosition = 0;
				dListCurrentPosition -= MAX_PAGE_MODS_FAST_FORWARD;
			}
			if(dListCurrentPosition < 0) dListCurrentPosition = 0;
			break;

		case DIR_RIGHT:
			dListCurrentPosition += MAX_PAGE_MODS_FAST_FORWARD;
			if(dListCurrentPosition > dListTotal - 1) dListCurrentPosition = dListTotal - 1;
			if(dListCurrentPosition > dListMaxDisplay)
	        {
		        dListScrollPosition += MAX_PAGE_MODS_FAST_FORWARD;
		        if((dListCurrentPosition + dListScrollPosition) > dListTotal - 1)
		        {
		        	dListScrollPosition = dListTotal - MAX_PAGE_MODS_LENGTH;
		        }
			    dListCurrentPosition = dListMaxDisplay;
			}
			break;

		case WIIMOTE_PLUS:
		case WIIMOTE_1:
		case WIIMOTE_A:
		case CC_PLUS:
		case CC_A:
		case GC_START:
		case GC_A:
			// Start Engine!
			status = 1;
			break;

		case WIIMOTE_HOME: // TODO? make a nice-looking Home menu
		case CC_HOME:
		case GC_Z:
			// Exit Engine!
			status = 2;
			break;

		case WIIMOTE_2:
		case CC_X:
		case GC_X:
			status = 3;
			break;

		default:
			// No Update Needed!
			status = 0;
			break;
	}
	return status;
}

void initMenu(int type)
{
	// Read Logo or Menu from Array.
	if(!type) Source = pngToScreen(isWide ? (void*) openbor_logo_480x272_png.data : (void*) openbor_logo_320x240_png.data);
	else Source = pngToScreen(isWide ? (void*) openbor_menu_480x272_png.data : (void*) openbor_menu_320x240_png.data);

	// Depending on which mode we are in (WideScreen/FullScreen)
	// allocate proper size for final screen.
	Screen = allocscreen(Source->width, Source->height, PIXEL_32);

	// Allocate Scaler.
	Scaler = allocscreen(Screen->width, Screen->height, PIXEL_32);

	control_init(2);
	apply_controls();
}

void termMenu()
{
	freescreen(&Source);
	Source = NULL;
	freescreen(&Scaler);
	Scaler = NULL;
	freescreen(&Screen);
	Screen = NULL;
	control_exit();
}

static void draw_vscrollbar(void)
{
	Rect track;
	Rect thumb;
	int offset_x = (isWide ? 30 : 7) - 3;
	int offset_y = (isWide ? 33 : 22) + 4;
	int box_width = 144;
	int box_height = 194;
	int min_vscrollbar_height = 2;
	int vbar_height = box_height;
	int vbar_width = 4;
	float vbar_ratio;
	int vspace = 0;
	int vbar_y = 0;

	if(dListTotal <= MAX_PAGE_MODS_LENGTH)
	{
		return;
	}

	vbar_ratio = ((MAX_PAGE_MODS_LENGTH * 100.0f) / dListTotal) / 100.0f;
	vbar_height = (int)(box_height * vbar_ratio);
	if(vbar_height < min_vscrollbar_height)
	{
		vbar_height = min_vscrollbar_height;
	}

	vspace = box_height - vbar_height;
	vbar_y = (int)(((dListScrollPosition) * vspace) / (dListTotal - MAX_PAGE_MODS_LENGTH));

	track.x = offset_x + box_width - vbar_width;
	track.y = offset_y;
	track.width = vbar_width;
	track.height = box_height;
	thumb.x = offset_x + box_width - vbar_width;
	thumb.y = offset_y + vbar_y;
	thumb.width = vbar_width;
	thumb.height = vbar_height;

	fillRect(Scaler, &track, LIGHT_GRAY);
	fillRect(Scaler, &thumb, GRAY);
}

void drawMenu()
{
	s_screen *Image = NULL;
	char listing[45] = {""};
	int list = 0;
	int shift = 0;
	int colors = 0;
	int clipX=0, clipY=0;

	copyScreens(Source);
	if(dListTotal < 1)
	{
		printText((isWide ? 30 : 8), (isWide ? 33 : 24), RED, 0, 0, MENU_STR_NO_MODS);
	}
	else
	{
		printText((isWide ? 30 : 7), (isWide ? 22 : 14), YELLOW, 0, 0, MENU_STR_GAME_COUNT,
			dListCurrentPosition + dListScrollPosition + 1, dListTotal);
	}
	for(list=0; list<dListTotal; list++)
	{
		if(list < MAX_PAGE_MODS_LENGTH)
		{
			shift = 0;
			colors = GRAY;
			strncpy(listing, "", (isWide ? 44 : 28));
			if(strlen(filelist[list+dListScrollPosition].filename)-4 < (isWide ? 44 : 28))
				safe_strncpy(listing, filelist[list+dListScrollPosition].filename, strlen(filelist[list+dListScrollPosition].filename)-4);
			if(strlen(filelist[list+dListScrollPosition].filename)-4 > (isWide ? 44 : 28))
				safe_strncpy(listing, filelist[list+dListScrollPosition].filename, (isWide ? 44 : 28));
			if(list == dListCurrentPosition)
			{
				shift = 2;
				colors = RED;
				Image = getPreview(filelist[list+dListScrollPosition].filename);
				if(Image)
				{
					clipX = factor * (isWide ? 286 : 155);
					clipY = factor * (isWide ? (factor == 4 ? (s16)32.5 : 32) : (factor == 4 ? (s16)21.5 : 21));
				}
				//else printText((isWide ? 288 : 157), (isWide ? 141 : 130), RED, 0, 0, "No Preview Available!");
			}
			printText((isWide ? 30 : 7) + shift, (isWide ? 33 : 22)+(11*list) , colors, 0, 0, "%s", listing);
		}
	}
	draw_vscrollbar();

	printText((isWide ? 26 : 5), (isWide ? 11 : 4), WHITE, 0, 0, "OpenBoR %s", VERSION);
	printText((isWide ? 392 : 261),(isWide ? 11 : 4), WHITE, 0, 0, __DATE__);
	printText((isWide ? 23 : 4),(isWide ? 251 : 226), WHITE, 0, 0, MENU_STR_START_GAME, control_getkeyname(savedata.keys[0][SDID_ATTACK]));
	printText((isWide ? 150 : 84),(isWide ? 251 : 226), WHITE, 0, 0, MENU_STR_BGM_PLAYER, control_getkeyname(savedata.keys[0][SDID_ATTACK2]));
	printText((isWide ? 270 : 164),(isWide ? 251 : 226), WHITE, 0, 0, MENU_STR_VIEW_LOGS, control_getkeyname(savedata.keys[0][SDID_JUMP]));
	printText((isWide ? 390 : 244),(isWide ? 251 : 226), WHITE, 0, 0, MENU_STR_QUIT_GAME, control_getkeyname(savedata.keys[0][SDID_SPECIAL]));
   	printText((isWide ? 330 : 197),(isWide ? 170 : 155), BLACK, 0, 0, "www.LavaLit.com");
	printText((isWide ? 322 : 190),(isWide ? 180 : 165), BLACK, 0, 0, "www.SenileTeam.com");

#ifdef SPK_SUPPORTED
	printText((isWide ? 324 : 192),(isWide ? 191 : 176), DARK_RED, 0, 0, "SecurePAK Edition");
#endif

	if(Image)
	{
	//draw screen with the preview image
	drawScreens(Image, clipX, clipY);
	}
	else
	{
	//draw screen without preview
	drawScreens(NULL, 0, 0);
	}
}

void drawLogs()
{
	int i=which_logfile, j, k, l, done=0;
	s_screen *Viewer = NULL;

	bothkeys = bothnewkeys = 0;
	Viewer = allocscreen(Source->width, Source->height, Source->pixelformat);
	clearscreen(Viewer);
	bothkeys = bothnewkeys = 0;

	while(!done)
	{
	    copyScreens(Viewer);
	    //inputrefresh(0);
	    refreshInput();
	    printText((isWide ? 410 : 250), 3, RED, 0, 0, MENU_STR_QUIT_HINT);
		if(buttonsPressed & (WIIMOTE_1|CC_B|GC_B)) done = 1;

		if(logfile[i].ready)
		{
			printText(5, 3, RED, 0, 0, "OpenBorLog.txt");
			if(buttonsHeld & DIR_UP) --logfile[i].line;
	        if(buttonsHeld & DIR_DOWN) ++logfile[i].line;
			if(buttonsHeld & DIR_LEFT) logfile[i].line = 0;
			if(buttonsHeld & DIR_RIGHT) logfile[i].line = logfile[i].rows - (LOG_SCREEN_END - LOG_SCREEN_TOP);
			if(logfile[i].line > logfile[i].rows - (LOG_SCREEN_END - LOG_SCREEN_TOP) - 1) logfile[i].line = logfile[i].rows - (LOG_SCREEN_END - LOG_SCREEN_TOP) - 1;
			if(logfile[i].line < 0) logfile[i].line = 0;
			for(l=LOG_SCREEN_TOP, j=logfile[i].line; j<logfile[i].rows-1; l++, j++)
			{
				if(l<LOG_SCREEN_END)
				{
					char textpad[480] = {""};
					for(k=0; k<480; k++)
					{
						if(!logfile[i].buf->ptr[logfile[i].pos[j]+k]) break;
						textpad[k] = logfile[i].buf->ptr[logfile[i].pos[j]+k];
					}
					if(logfile[i].rows>0xFFFF)
						printText(5, l*10, WHITE, 0, 0, "0x%08x:  %s", j, textpad);
					else
						printText(5, l*10, WHITE, 0, 0, "0x%04x:  %s", j, textpad);
				}
				else break;
			}
		}
		else if(i == SCRIPT_LOG) printText(5, 3, RED, 0, 0, MENU_STR_SCRIPT_LOG_NF);
		else                     printText(5, 3, RED, 0, 0, MENU_STR_LOG_NOT_FOUND);

	    drawScreens(NULL, 0, 0);
	}
	freescreen(&Viewer);
	Viewer = NULL;
	drawMenu();
}

void drawLogo()
{
	unsigned startTime;
	initMenu(0);
	copyScreens(Source);
	drawScreens(NULL, 0, 0);
	vga_vwait();
	startTime = timer_gettick();

	// The logo displays for 2 seconds.  Let's put that time to good use.
	dListTotal = findPaks();

	while(1) { // display logo for remainder of time
		if(timer_gettick() - startTime >= 2000) break;
	}
	termMenu();
}

void fillRect(s_screen *dest, Rect *rect, u32 color)
{
	u32 *data = (u32*)dest->data;
	int x, y, width=dest->width;
	for(y=rect->y; y<rect->y+rect->height; y++)
	{
		for(x=rect->x; x<rect->x+rect->width; x++)
		{
			data[x+y*width] = color;
		}
	}
}

void setVideoMode()
{
	if(isWide) // 480x272
	{
		videomodes.mode    = savedata.screen[videoMode][0];
		videomodes.filter  = savedata.screen[videoMode][1];
		videomodes.hRes    = 480;
		videomodes.vRes    = 272;
		videomodes.hScale  = (float)1.5;
		videomodes.vScale  = (float)1.13;
		videomodes.hShift  = 80;
		videomodes.vShift  = 20;
		videomodes.dOffset = 263;
		videomodes.pixel   = 4;
	}
	else // 320x240
	{
		videomodes.mode    = savedata.screen[videoMode][0];
		videomodes.filter  = savedata.screen[videoMode][1];
		videomodes.hRes    = 320;
		videomodes.vRes    = 240;
		videomodes.hScale  = 1;
		videomodes.vScale  = 1;
		videomodes.hShift  = 0;
		videomodes.vShift  = 0;
		videomodes.dOffset = 231;
		videomodes.pixel   = 4;
	}

	video_set_mode(videomodes);
}

#if WII
/* Mode 0 (320x240) and mode 1 (480x272) are normal on Wii; HD mods use mode 2+ */
#define WII_SAFE_VIDEO_PIXELS (480 * 272)

int wii_hd_video_downgraded = 0;
short wii_render_hres = 0;
short wii_render_vres = 0;
float wii_render_scale_h = 1.0f;
float wii_render_scale_v = 1.0f;

#define WII_HD_MAX_OPTIONS 12

typedef struct
{
	short target_h;
	short target_v;
} wii_hd_render_option;

static wii_hd_render_option wii_hd_options[WII_HD_MAX_OPTIONS];
static int wii_hd_option_count = 0;
static short wii_user_render_width = 0;

static void wii_hd_compute_render_size(int orig_h, int orig_v, int req_w, short *out_h, short *out_v)
{
	short target_h, target_v;

	if(req_w <= 0 || req_w >= orig_h)
	{
		*out_h = (short)orig_h;
		*out_v = (short)orig_v;
		return;
	}

	target_h = (short)req_w;
	target_v = (short)(target_h * (float)orig_v / (float)orig_h + 0.5f);
	target_v = (short)((target_v + 3) & ~3);
	if(target_v < 160)
	{
		target_v = 160;
	}
	if(target_v > 272)
	{
		target_v = 272;
		target_h = (short)(target_v * (float)orig_h / (float)orig_v + 0.5f);
		target_h = (short)((target_h + 3) & ~3);
	}

	*out_h = target_h;
	*out_v = target_v;
}

static int wii_hd_build_render_options(int orig_h, int orig_v)
{
	static const short preset_widths[] = { 0, 640, 560, 480, 432, 400, 360, 320, 280, 240 };
	int i, n = 0;
	short th, tv, last_h = -1;

	wii_hd_option_count = 0;
	if(orig_h <= 0 || orig_v <= 0)
	{
		return 0;
	}

	for(i = 0; i < (int)(sizeof(preset_widths) / sizeof(preset_widths[0])) && n < WII_HD_MAX_OPTIONS; i++)
	{
		if(preset_widths[i] == 0)
		{
			wii_hd_options[n].target_h = 0;
			wii_hd_options[n].target_v = 0;
			n++;
			continue;
		}

		wii_hd_compute_render_size(orig_h, orig_v, preset_widths[i], &th, &tv);
		if(th >= orig_h)
		{
			continue;
		}
		if(th == last_h)
		{
			continue;
		}

		last_h = th;
		wii_hd_options[n].target_h = th;
		wii_hd_options[n].target_v = tv;
		n++;
	}

	wii_hd_option_count = n;
	return n;
}

void wii_hd_scale_videomode_down(int *videoMode)
{
	short orig_h, orig_v, target_h, target_v;

	(void)videoMode;

	orig_h = videomodes.hRes;
	orig_v = videomodes.vRes;
	if(orig_h <= 0 || orig_v <= 0)
	{
		printf("[HDVM] scale skip: invalid source %dx%d\n", orig_h, orig_v);
		return;
	}

	if(wii_user_render_width <= 0)
	{
		wii_user_render_width = 320;
	}

	wii_hd_compute_render_size(orig_h, orig_v, wii_user_render_width, &target_h, &target_v);

	wii_render_hres = target_h;
	wii_render_vres = target_v;
	wii_render_scale_h = (float)target_h / (float)orig_h;
	wii_render_scale_v = (float)target_v / (float)orig_v;

	/* Keep videomodes.hRes/vRes and game params at HD values for camera/world coords.
	 * Only the vscreen framebuffer and sprite drawing use wii_render_* scale. */
	printf("[HDVM] render downgrade: logic=%dx%d -> framebuffer=%dx%d scale=%.3fx%.3f (camera unchanged)\n",
		orig_h, orig_v, target_h, target_v, wii_render_scale_h, wii_render_scale_v);
}

static int wii_mode_pixels(int mode, int hres, int vres)
{
	if(mode == 255)
	{
		if(hres > 0 && vres > 0)
		{
			return hres * vres;
		}
		return WII_SAFE_VIDEO_PIXELS + 1;
	}

	switch(mode)
	{
	case 0: return 320 * 240;
	case 1: return 480 * 272;
	case 2: return 640 * 480;
	case 3: return 720 * 480;
	case 4: return 800 * 480;
	case 5: return 800 * 600;
	case 6: return 960 * 540;
	default: return 320 * 240;
	}
}

static void wii_mode_dimensions(int mode, int hres, int vres, int *out_h, int *out_v)
{
	if(mode == 255 && hres > 0 && vres > 0)
	{
		*out_h = hres;
		*out_v = vres;
		return;
	}

	switch(mode)
	{
	case 0: *out_h = 320; *out_v = 240; break;
	case 1: *out_h = 480; *out_v = 272; break;
	case 2: *out_h = 640; *out_v = 480; break;
	case 3: *out_h = 720; *out_v = 480; break;
	case 4: *out_h = 800; *out_v = 480; break;
	case 5: *out_h = 800; *out_v = 600; break;
	case 6: *out_h = 960; *out_v = 540; break;
	default: *out_h = 320; *out_v = 240; break;
	}
}

static void wii_hd_log_state(const char *tag, int mode, int hres, int vres, int downgraded, const char *note)
{
	float orig_aspect = (hres > 0 && vres > 0) ? ((float)hres / (float)vres) : 0.0f;
	printf("[HDVM] %s: mode=%d res=%dx%d pixels=%d aspect=%.3f downgraded=%d %s\n",
		tag, mode, hres, vres, hres * vres, orig_aspect, downgraded, note ? note : "");
}

static int wii_hd_default_option(int count)
{
	int i;

	for(i = 0; i < count; i++)
	{
		if(wii_hd_options[i].target_h == 320)
		{
			return i;
		}
	}

	for(i = count - 1; i >= 0; i--)
	{
		if(wii_hd_options[i].target_h > 0)
		{
			return i;
		}
	}

	return 0;
}

static int wii_hd_video_prompt(int hres, int vres)
{
	int done = 0;
	int count = 0;
	int selected = 0;
	int scroll = 0;
	int list = 0;
	int index = 0;
	int shift = 0;
	int colors = 0;
	int visible = 0;
	int base_x = 0;
	int base_y = 0;
	int line_h = 11;

	if(CONF_GetAspectRatio() == CONF_ASPECT_16_9)
	{
		isWide = 1;
	}
	else
	{
		isWide = 0;
	}

	count = wii_hd_build_render_options(hres, vres);
	if(count < 1)
	{
		return 0;
	}

	selected = wii_hd_default_option(count);
	visible = isWide ? 8 : 6;
	base_x = isWide ? 36 : 10;
	base_y = isWide ? 96 : 86;

	setVideoMode();
	initMenu(1);

	while(!done)
	{
		copyScreens(Source);
		printText((isWide ? 40 : 12), (isWide ? 52 : 44), YELLOW, 0, 0, MENU_STR_HD_TITLE);
		printText((isWide ? 40 : 12), (isWide ? 68 : 60), ORANGE, 0, 0, MENU_STR_HD_INFO, hres, vres);

		for(list = 0; list < visible; list++)
		{
			index = scroll + list;
			if(index >= count)
			{
				break;
			}

			shift = 0;
			colors = GRAY;
			if(index == selected)
			{
				shift = 2;
				colors = RED;
			}

			if(wii_hd_options[index].target_h <= 0)
			{
				printText(base_x + shift, base_y + line_h * list, colors, 0, 0,
					MENU_STR_HD_OPT_FULL, hres, vres);
			}
			else
			{
				printText(base_x + shift, base_y + line_h * list, colors, 0, 0,
					MENU_STR_HD_OPT_RENDER,
					wii_hd_options[index].target_h, wii_hd_options[index].target_v);
			}
		}

		printText((isWide ? 40 : 12), (isWide ? 248 : 218), GREEN, 0, 0, MENU_STR_HD_HINT);
		drawScreens(NULL, 0, 0);

		refreshInput();
		buttonsPressed |= hold_key_impulse(DIR_UP, IMPULSE_TIME, FIRST_KEYPRESS, FIRST_IMPULSE_TIME);
		buttonsPressed |= hold_key_impulse(DIR_DOWN, IMPULSE_TIME, FIRST_KEYPRESS, FIRST_IMPULSE_TIME);

		switch(buttonsPressed)
		{
		case DIR_UP:
			if(selected > 0)
			{
				selected--;
			}
			if(selected < scroll)
			{
				scroll = selected;
			}
			break;

		case DIR_DOWN:
			if(selected < count - 1)
			{
				selected++;
			}
			if(selected >= scroll + visible)
			{
				scroll = selected - visible + 1;
			}
			break;

		case WIIMOTE_A:
		case WIIMOTE_1:
		case WIIMOTE_PLUS:
		case CC_A:
		case CC_PLUS:
		case GC_A:
		case GC_START:
			done = 1;
			break;

		default:
			break;
		}
	}

	wii_user_render_width = wii_hd_options[selected].target_h;
	printf("[HDVM] user selected render width=%d (%dx%d logic=%dx%d)\n",
		wii_user_render_width,
		wii_hd_options[selected].target_h, wii_hd_options[selected].target_v,
		hres, vres);

	termMenu();
	return wii_user_render_width;
}

void wii_apply_hd_videomode_policy(int *mode, short *hres, short *vres)
{
	int pixels;
	int display_h = 0;
	int display_v = 0;
	int choice;
	int orig_mode;
	int orig_h = 0;
	int orig_v = 0;

	if(mode == NULL)
	{
		return;
	}

	orig_mode = *mode;
	if(hres)
	{
		orig_h = *hres;
	}
	if(vres)
	{
		orig_v = *vres;
	}
	wii_mode_dimensions(orig_mode, orig_h, orig_v, &orig_h, &orig_v);

	wii_hd_video_downgraded = 0;
	wii_user_render_width = 0;
	wii_render_hres = 0;
	wii_render_vres = 0;
	wii_render_scale_h = 1.0f;
	wii_render_scale_v = 1.0f;

	pixels = wii_mode_pixels(*mode, hres ? *hres : 0, vres ? *vres : 0);
	if(pixels <= WII_SAFE_VIDEO_PIXELS)
	{
		wii_hd_log_state("skip (safe res)", orig_mode, orig_h, orig_v, 0, "below HD threshold");
		return;
	}

	wii_hd_log_state("HD mod detected", orig_mode, orig_h, orig_v, 0, "show resolution picker every launch");

	wii_mode_dimensions(*mode, hres ? *hres : 0, vres ? *vres : 0, &display_h, &display_v);
	printf("[HDVM] showing resolution menu for logic %dx%d\n", display_h, display_v);
	choice = wii_hd_video_prompt(display_h, display_v);

	if(choice <= 0)
	{
		wii_hd_log_state("user chose original", orig_mode, orig_h, orig_v, 0, "full framebuffer");
	}
	else
	{
		wii_hd_video_downgraded = 1;
		wii_hd_log_state("user chose render buffer", orig_mode, orig_h, orig_v, 1, "scaled draw after VIDEOMODES");
	}
}
#endif

void Menu()
{
	int done = 0;
	int ctrl = 0;

	// Set video mode based on aspect ratio
	if(CONF_GetAspectRatio() == CONF_ASPECT_16_9) isWide = 1;
	setVideoMode();
	drawLogo();

	// Skips menu if we already have a .pak to load
	int quicklaunch = (packfile[0] == '\0') ? 0 : 1;

	if(!quicklaunch)
	{
		dListTotal = findPaks();
		dListCurrentPosition = 0;
		if(dListTotal != 1)
		{
			sortList();
			getAllLogs();
			initMenu(1);
			drawMenu();
			pControl = ControlMenu;

			while(!done)
			{
				ctrl = Control();
				switch(ctrl)
				{
					case 1:
						if (dListTotal > 0) done = 1;
						break;

					case 2:
						done = 1;
						break;

					case 3:
						drawLogs();
						break;

					case -1:
						drawMenu();
						break;

					case -2:
						// BGM player isn't supported
						break;

		            default:
						break;
				}
			}
			freeAllLogs();
			termMenu();
			if(ctrl == 2)
			{
				if (filelist)
				{
					free(filelist);
					filelist = NULL;
				}
				borExit(0);
			}
		}
		getBasePath(packfile, filelist[dListCurrentPosition+dListScrollPosition].filename, 1);
	}
	free(filelist);
}

