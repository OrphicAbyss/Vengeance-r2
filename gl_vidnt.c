/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// gl_vidnt.c -- NT GL vid component

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"
#include <commctrl.h>

#define MAX_MODE_LIST	50
#define VID_ROW_SIZE	3
#define WARP_WIDTH		320
#define WARP_HEIGHT		200
#define MAXWIDTH		10000
#define MAXHEIGHT		10000
#define BASEWIDTH		320
#define BASEHEIGHT		200

#define MODE_WINDOWED			0
#define NO_MODE					(MODE_WINDOWED - 1)
#define MODE_FULLSCREEN_DEFAULT	(MODE_WINDOWED + 1)

typedef struct {
	modestate_t	type;
	int			width;
	int			height;
	int			modenum;
	int			dib;
	int			fullscreen;
	int			bpp;
	int			halfscreen;
	char		modedesc[17];
} vmode_t;

typedef struct {
	int			width;
	int			height;
} lmode_t;

lmode_t	lowresmodes[] = {
	{320, 200},
	{320, 240},
	{400, 300},
	{512, 384},
};

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

// name of driver library (opengl32.dll, libGL.so.1, or whatever)
char gl_driver[256];

qboolean		scr_skipupdate;

static vmode_t	modelist[MAX_MODE_LIST];
static int		nummodes;
static vmode_t	*pcurrentmode;
static vmode_t	badmode;

static DEVMODE	gdevmode;
static qboolean	vid_initialized = false;
//static qboolean	windowed, leavecurrentmode;
static qboolean leavecurrentmode;
qboolean windowed;
static qboolean vid_canalttab = false;
static qboolean vid_wassuspended = false;
static int		windowed_mouse;
extern qboolean	mouseactive;  // from in_win.c

int omousex = 0, omousey = 0;

int			DIBWidth, DIBHeight;
RECT		WindowRect;
DWORD		WindowStyle, ExWindowStyle;

HWND	mainwindow, dibwindow;

int			vid_modenum = NO_MODE;
int			vid_realmode;
int			vid_default = MODE_WINDOWED;
static int	windowed_default;
unsigned char	vid_curpal[256*3];
static qboolean fullsbardraw = false;

float vid_gamma = 1.0;

HGLRC	baseRC;
HDC		maindc;

static HINSTANCE gldll;

HWND WINAPI InitializeWindow (HINSTANCE hInstance, int nCmdShow);

viddef_t	vid;				// global video state

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];
unsigned char d_15to8table[65536];

float		gldepthmin, gldepthmax;

modestate_t	modestate = MS_UNINIT;

void VID_MenuDraw (void);
void VID_MenuKey (int key);

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void AppActivate(BOOL fActive, BOOL minimize);
char *VID_GetModeDescription (int mode);
void ClearAllStates (void);
void VID_UpdateWindowStatus (void);
void GL_Init (void);

typedef void (APIENTRY *lp3DFXFUNC) (int, int, int, int, int, const void*);
lp3DFXFUNC glColorTableEXT;

//ati truform
pnTrianglesIatiPROC glPNTrianglesiATI;

//qmb :extra stuff
qboolean gl_combine = false;
qboolean gl_point_sprite = false;
qboolean gl_sgis_mipmap = false;
qboolean gl_texture_non_power_of_two = false;
qboolean gl_n_patches = false;
qboolean gl_videosyncavailable = false;
qboolean gl_stencil = true;
qboolean gl_shader = false;
qboolean gl_filtering_anisotropic = false;
float gl_maximumAnisotropy;

// FIXME: remove this [Black]
/*
// GL_ARB_shader_objects
qboolean gl_support_shader_objects = false;
// GL_ARB_shading_language_100
qboolean gl_support_shading_language_100 = false;
// GL_ARB_vertex_shader
qboolean gl_support_vertex_shader = false;
// GL_ARB_fragment_shader
qboolean gl_support_GLSL_shaders = false;
*/
// GL_ARB_shader_objects GL_ARB_shading_language_100 GL_ARB_vertex_shader GL_ARB_fragment_shader
qboolean gl_support_GLSL_shaders = false;

// GL_ARB_texture_cube_map
qboolean gl_support_cubemaps = false;

//====================================

cvar_t		vid_mode = {"vid_mode","0", false};
// Note that 0 is MODE_WINDOWED
cvar_t		_vid_default_mode = {"_vid_default_mode","0", true};
// Note that 3 is MODE_FULLSCREEN_DEFAULT
cvar_t		_vid_default_mode_win = {"_vid_default_mode_win","3", true};
cvar_t		vid_wait = {"vid_wait","0"};
cvar_t		vid_nopageflip = {"vid_nopageflip","0", true};
cvar_t		_vid_wait_override = {"_vid_wait_override", "0", true};
cvar_t		vid_config_x = {"vid_config_x","800", true};
cvar_t		vid_config_y = {"vid_config_y","600", true};
cvar_t		vid_stretch_by_2 = {"vid_stretch_by_2","1", true};
cvar_t		_windowed_mouse = {"_windowed_mouse","1", true};
cvar_t		vid_hwgamma = {"vid_hwgamma", "1", true};
cvar_t		vid_vsync = {"vid_vsync", "0", true};

cvar_t		vid_width = {"vid_width", "640", true};
cvar_t		vid_height = {"vid_height", "480", true};
cvar_t		vid_bitsperpixel = {"vid_bitsperpixel", "32", true};
cvar_t		vid_fullscreen = {"vid_fullscreen", "1", true};
cvar_t		vid_minwidth = {"vid_minwidth", "0"};
cvar_t		vid_minheight = {"vid_minheight", "0"};

// good gamma stuff
int gammawaschanged;
unsigned short originalgammaramps[768];
unsigned short newgammaramps[768];
extern BOOL gammaworks;

int			window_center_x, window_center_y, window_x, window_y, window_width, window_height;
RECT		window_rect;

void CenterWindow(HWND hWndCenter, int width, int height, BOOL lefttopjustify)
{
    int     CenterX, CenterY;

	CenterX = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	CenterY = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	if (CenterX > CenterY*2)
		CenterX >>= 1;	// dual screens
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;
	SetWindowPos (hWndCenter, NULL, CenterX, CenterY, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
}

qboolean VID_SetWindowedMode (int modenum)
{
	HDC				hdc;
	int				lastmodestate, width, height;
	RECT			rect;

	lastmodestate = modestate;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU |
				  WS_MINIMIZEBOX;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (ExWindowStyle,(LPCSTR)"VengeanceR2", (LPCSTR)"VengeanceR2", WindowStyle, rect.left, rect.top, width, height, NULL, NULL, global_hInstance, NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	// Center and show the DIB window
	CenterWindow(dibwindow, WindowRect.right - WindowRect.left,
				 WindowRect.bottom - WindowRect.top, false);

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	modestate = MS_WINDOWED;

// because we have set the background brush for the window to NULL
// (to avoid flickering when re-sizing the window on the desktop),
// we clear the window to black when created, otherwise it will be
// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	if ((signed)vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if ((signed)vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;

//unfixed by DrLabman (vid.width, vid.height are what the 2d stuff uses to draw, so its the console size)
//fix by darkie | wtf(?)
	vid.width = vid.conwidth;
	vid.height = vid.conheight;
//	vid.width = vid.realwidth;
//	vid.height = vid.realheight;

	vid.numpages = 2;

	mainwindow = dibwindow;

	return true;
}


qboolean VID_SetFullDIBMode (int modenum)
{
	HDC				hdc;
	int				lastmodestate, width, height;
	RECT			rect;

	if (!leavecurrentmode)
	{
		gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		gdevmode.dmBitsPerPel = modelist[modenum].bpp;
		gdevmode.dmPelsWidth = modelist[modenum].width <<
							   modelist[modenum].halfscreen;
		gdevmode.dmPelsHeight = modelist[modenum].height;
		gdevmode.dmSize = sizeof (gdevmode);

		if (ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			Sys_Error ("Couldn't set fullscreen DIB mode");
	}

	lastmodestate = modestate;
	modestate = MS_FULLDIB;

	WindowRect.top = WindowRect.left = 0;

	WindowRect.right = modelist[modenum].width;
	WindowRect.bottom = modelist[modenum].height;

	DIBWidth = modelist[modenum].width;
	DIBHeight = modelist[modenum].height;

	WindowStyle = WS_POPUP;
	ExWindowStyle = 0;

	rect = WindowRect;
	AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	// Create the DIB window
	dibwindow = CreateWindowEx (
		 ExWindowStyle,
		 "VengeanceR2",
		 "VengeanceR2",
		 WindowStyle,
		 rect.left, rect.top,
		 width,
		 height,
		 NULL,
		 NULL,
		 global_hInstance,
		 NULL);

	if (!dibwindow)
		Sys_Error ("Couldn't create DIB window");

	ShowWindow (dibwindow, SW_SHOWDEFAULT);
	UpdateWindow (dibwindow);

	// Because we have set the background brush for the window to NULL
	// (to avoid flickering when re-sizing the window on the desktop), we
	// clear the window to black when created, otherwise it will be
	// empty while Quake starts up.
	hdc = GetDC(dibwindow);
	PatBlt(hdc,0,0,WindowRect.right,WindowRect.bottom,BLACKNESS);
	ReleaseDC(dibwindow, hdc);

	if ((signed)vid.conheight > modelist[modenum].height)
		vid.conheight = modelist[modenum].height;
	if ((signed)vid.conwidth > modelist[modenum].width)
		vid.conwidth = modelist[modenum].width;

//unfixed by DrLabman (vid.width, vid.height are what the 2d stuff uses to draw, so its the console size)
//fix by darkie | wtf(?)
	vid.width = vid.conwidth;
	vid.height = vid.conheight;
//	vid.width = vid.realwidth;
//	vid.height = vid.realheight;


	vid.numpages = 2;

// needed because we're not getting WM_MOVE messages fullscreen on NT
	window_x = 0;
	window_y = 0;

	mainwindow = dibwindow;

	return true;
}

int VID_SetGamma(unsigned short *ramps)
{
	HDC hdc = GetDC (GetDesktopWindow());
	int i = SetDeviceGammaRamp(hdc, ramps);
	ReleaseDC (GetDesktopWindow(), hdc);

	gammawaschanged = (ramps != originalgammaramps);

	return i; // return success or failure
}

int VID_GetGamma(unsigned short *ramps)
{
	HDC hdc = GetDC (GetDesktopWindow());
	int i = GetDeviceGammaRamp(hdc, ramps);
	ReleaseDC (GetDesktopWindow(), hdc);
	return i; // return success or failure
}

int VID_SetMode (int modenum, unsigned char *palette)
{
	int				original_mode, temp;
	qboolean		stat;
    MSG				msg;

	if ((windowed && (modenum != 0)) ||
		(!windowed && (modenum < 1)) ||
		(!windowed && (modenum >= nummodes)))
	{
		Sys_Error ("Bad video mode\n");
	}

// so Con_Printfs don't mess us up by forcing vid and snd updates
	temp = scr_disabled_for_loading;
	scr_disabled_for_loading = true;

	CDAudio_Pause ();

	if (vid_modenum == NO_MODE)
		original_mode = windowed_default;
	else
		original_mode = vid_modenum;

	// Set either the fullscreen or windowed mode
	if (modelist[modenum].type == MS_WINDOWED)
	{
		if (_windowed_mouse.value && key_dest == key_game)
		{
			stat = VID_SetWindowedMode(modenum);
			IN_ActivateMouse ();
			IN_HideMouse ();
		}
		else
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			stat = VID_SetWindowedMode(modenum);
		}
		omousex = window_rect.left;
		omousey = window_rect.top;
	}
	else if (modelist[modenum].type == MS_FULLDIB)
	{
		stat = VID_SetFullDIBMode(modenum);
		IN_ActivateMouse ();
		IN_HideMouse ();
	}
	else
	{
		Sys_Error ("VID_SetMode: Bad mode type in modelist");
	}

	window_width = DIBWidth;
	window_height = DIBHeight;
	VID_UpdateWindowStatus ();

	CDAudio_Resume ();
	scr_disabled_for_loading = temp;

	if (!stat)
	{
		Sys_Error ("Couldn't set video mode");
	}

// now we try to make sure we get the focus on the mode switch, because
// sometimes in some systems we don't.  We grab the foreground, then
// finish setting up, pump all our messages, and sleep for a little while
// to let messages finish bouncing around the system, then we put
// ourselves at the top of the z order, then grab the foreground again,
// Who knows if it helps, but it probably doesn't hurt
	SetForegroundWindow (mainwindow);
	VID_SetPalette (palette);
	vid_modenum = modenum;
	Cvar_SetValue ("vid_mode", (float)vid_modenum);

	while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
	{
      	TranslateMessage (&msg);
      	DispatchMessage (&msg);
	}

	Sleep (100);

	SetWindowPos (mainwindow, HWND_TOP, 0, 0, 0, 0,
				  SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW |
				  SWP_NOCOPYBITS);

	SetForegroundWindow (mainwindow);

// fix the leftover Alt from any Alt-Tab or the like that switched us away
	ClearAllStates ();

	if (!msg_suppress_1)
		Con_SafePrintf ("Video mode %s initialized.\n", VID_GetModeDescription (vid_modenum));

	VID_SetPalette (palette);

	vid.recalc_refdef = 1;

	VID_GetGamma(originalgammaramps);

	return true;
}


/*
================
VID_UpdateWindowStatus
================
*/
void VID_UpdateWindowStatus (void)
{

	window_rect.left = window_x;
	window_rect.top = window_y;
	window_rect.right = window_x + window_width;
	window_rect.bottom = window_y + window_height;
	window_center_x = (window_rect.left + window_rect.right) / 2;
	window_center_y = (window_rect.top + window_rect.bottom) / 2;

	IN_UpdateClipCursor ();
}

GLenum TEXTURE0;
GLenum TEXTURE1;
GLenum TEXTURE2;
GLenum TEXTURE3;

//=================================================================

BOOL (WINAPI *qwglSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
BOOL (WINAPI *qwglSwapBuffers)(HDC);
HGLRC (WINAPI *qwglCreateContext)(HDC);
BOOL (WINAPI *qwglDeleteContext)(HGLRC);
HGLRC (WINAPI *qwglGetCurrentContext)(VOID);
HDC (WINAPI *qwglGetCurrentDC)(VOID);
PROC (WINAPI *qwglGetProcAddress)(LPCSTR);
BOOL (WINAPI *qwglMakeCurrent)(HDC, HGLRC);
BOOL (WINAPI *qwglSwapIntervalEXT)(int interval);
const char *(WINAPI *qwglGetExtensionsStringARB)(HDC hdc);

static void GL_CloseLibrary(void)
{
	FreeLibrary(gldll);
	gldll = 0;
	gl_driver[0] = 0;
	qwglGetProcAddress = NULL;
	gl_extensions = "";
//	gl_platform = "";
//	gl_platformextensions = "";
}

static int GL_OpenLibrary(const char *name)
{
	Con_Printf("Loading OpenGL driver %s\n", name);
	GL_CloseLibrary();
	if (!(gldll = LoadLibrary(name)))
	{
		Con_Printf("Unable to LoadLibrary %s\n", name);
		return false;
	}
	strcpy(gl_driver, name);
	return true;
}

void *GL_GetProcAddress(const char *name)
{
	void *p = NULL;
	if (wglGetProcAddress != NULL)
		p = (void *) wglGetProcAddress(name);
	if (p == NULL)
		p = (void *) GetProcAddress(gldll, name);
	return p;
}

int GL_CheckExtension(const char *name, const dllfunction_t *funcs, const char *disableparm, int silent)
{
	int failed = false;
	const dllfunction_t *func;

	if (extra_info)
		Con_Printf("&c999checking for %s...  ", name);

	for (func = funcs;func && func->name;func++)
		*func->funcvariable = NULL;

	if (disableparm && (COM_CheckParm((char *)disableparm) || COM_CheckParm("-safe")))
	{
		if (extra_info)
			Con_Print("disabled by commandline\n");
		return false;
	}

	if (strstr(gl_extensions, name) || /*strstr(gl_platformextensions, name) ||*/ (strncmp(name, "GL_", 3) && strncmp(name, "WGL_", 4) && strncmp(name, "GLX_", 4) && strncmp(name, "AGL_", 4)))
	{
		for (func = funcs;func && func->name != NULL;func++)
		{
			// functions are cleared before all the extensions are evaluated
			if (!(*func->funcvariable = (void *) GL_GetProcAddress(func->name)))
			{
				if (!silent)
					Con_Printf("&c922OpenGL extension \"%s\" is missing function \"%s\" - broken driver!\n", name, func->name);
				failed = true;
			}
		}
		// delay the return so it prints all missing functions
		if (failed)
			return false;
		if (extra_info)
			Con_Print("&c191enabled&r\n");
		return true;
	}
	else
	{
		if (extra_info)
			Con_Print("&c911not detected&r\n");
		return false;
	}
}

void (GLAPIENTRY *qglDeleteObjectARB)(GLhandleARB obj);
GLhandleARB (GLAPIENTRY *qglGetHandleARB)(GLenum pname);
void (GLAPIENTRY *qglDetachObjectARB)(GLhandleARB containerObj, GLhandleARB attachedObj);
GLhandleARB (GLAPIENTRY *qglCreateShaderObjectARB)(GLenum shaderType);
void (GLAPIENTRY *qglShaderSourceARB)(GLhandleARB shaderObj, GLsizei count, const GLcharARB **string, const GLint *length);
void (GLAPIENTRY *qglCompileShaderARB)(GLhandleARB shaderObj);
GLhandleARB (GLAPIENTRY *qglCreateProgramObjectARB)(void);
void (GLAPIENTRY *qglAttachObjectARB)(GLhandleARB containerObj, GLhandleARB obj);
void (GLAPIENTRY *qglLinkProgramARB)(GLhandleARB programObj);
void (GLAPIENTRY *qglUseProgramObjectARB)(GLhandleARB programObj);
void (GLAPIENTRY *qglValidateProgramARB)(GLhandleARB programObj);
void (GLAPIENTRY *qglUniform1fARB)(GLint location, GLfloat v0);
void (GLAPIENTRY *qglUniform2fARB)(GLint location, GLfloat v0, GLfloat v1);
void (GLAPIENTRY *qglUniform3fARB)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
void (GLAPIENTRY *qglUniform4fARB)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
void (GLAPIENTRY *qglUniform1iARB)(GLint location, GLint v0);
void (GLAPIENTRY *qglUniform2iARB)(GLint location, GLint v0, GLint v1);
void (GLAPIENTRY *qglUniform3iARB)(GLint location, GLint v0, GLint v1, GLint v2);
void (GLAPIENTRY *qglUniform4iARB)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
void (GLAPIENTRY *qglUniform1fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform2fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform3fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform4fvARB)(GLint location, GLsizei count, const GLfloat *value);
void (GLAPIENTRY *qglUniform1ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform2ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform3ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniform4ivARB)(GLint location, GLsizei count, const GLint *value);
void (GLAPIENTRY *qglUniformMatrix2fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglUniformMatrix3fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglUniformMatrix4fvARB)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void (GLAPIENTRY *qglGetObjectParameterfvARB)(GLhandleARB obj, GLenum pname, GLfloat *params);
void (GLAPIENTRY *qglGetObjectParameterivARB)(GLhandleARB obj, GLenum pname, GLint *params);
void (GLAPIENTRY *qglGetInfoLogARB)(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
void (GLAPIENTRY *qglGetAttachedObjectsARB)(GLhandleARB containerObj, GLsizei maxCount, GLsizei *count, GLhandleARB *obj);
GLint (GLAPIENTRY *qglGetUniformLocationARB)(GLhandleARB programObj, const GLcharARB *name);
void (GLAPIENTRY *qglGetActiveUniformARB)(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
void (GLAPIENTRY *qglGetUniformfvARB)(GLhandleARB programObj, GLint location, GLfloat *params);
void (GLAPIENTRY *qglGetUniformivARB)(GLhandleARB programObj, GLint location, GLint *params);
void (GLAPIENTRY *qglGetShaderSourceARB)(GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *source);

//void (GLAPIENTRY *qglVertexAttrib1fARB)(GLuint index, GLfloat v0);
//void (GLAPIENTRY *qglVertexAttrib1sARB)(GLuint index, GLshort v0);
//void (GLAPIENTRY *qglVertexAttrib1dARB)(GLuint index, GLdouble v0);
//void (GLAPIENTRY *qglVertexAttrib2fARB)(GLuint index, GLfloat v0, GLfloat v1);
//void (GLAPIENTRY *qglVertexAttrib2sARB)(GLuint index, GLshort v0, GLshort v1);
//void (GLAPIENTRY *qglVertexAttrib2dARB)(GLuint index, GLdouble v0, GLdouble v1);
//void (GLAPIENTRY *qglVertexAttrib3fARB)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
//void (GLAPIENTRY *qglVertexAttrib3sARB)(GLuint index, GLshort v0, GLshort v1, GLshort v2);
//void (GLAPIENTRY *qglVertexAttrib3dARB)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
//void (GLAPIENTRY *qglVertexAttrib4fARB)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
//void (GLAPIENTRY *qglVertexAttrib4sARB)(GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
//void (GLAPIENTRY *qglVertexAttrib4dARB)(GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
//void (GLAPIENTRY *qglVertexAttrib4NubARB)(GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
//void (GLAPIENTRY *qglVertexAttrib1fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib1svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib1dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib2fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib2svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib2dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib3fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib3svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib3dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib4fvARB)(GLuint index, const GLfloat *v);
//void (GLAPIENTRY *qglVertexAttrib4svARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib4dvARB)(GLuint index, const GLdouble *v);
//void (GLAPIENTRY *qglVertexAttrib4ivARB)(GLuint index, const GLint *v);
//void (GLAPIENTRY *qglVertexAttrib4bvARB)(GLuint index, const GLbyte *v);
//void (GLAPIENTRY *qglVertexAttrib4ubvARB)(GLuint index, const GLubyte *v);
//void (GLAPIENTRY *qglVertexAttrib4usvARB)(GLuint index, const GLushort *v);
//void (GLAPIENTRY *qglVertexAttrib4uivARB)(GLuint index, const GLuint *v);
//void (GLAPIENTRY *qglVertexAttrib4NbvARB)(GLuint index, const GLbyte *v);
//void (GLAPIENTRY *qglVertexAttrib4NsvARB)(GLuint index, const GLshort *v);
//void (GLAPIENTRY *qglVertexAttrib4NivARB)(GLuint index, const GLint *v);
//void (GLAPIENTRY *qglVertexAttrib4NubvARB)(GLuint index, const GLubyte *v);
//void (GLAPIENTRY *qglVertexAttrib4NusvARB)(GLuint index, const GLushort *v);
//void (GLAPIENTRY *qglVertexAttrib4NuivARB)(GLuint index, const GLuint *v);
void (GLAPIENTRY *qglVertexAttribPointerARB)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
void (GLAPIENTRY *qglEnableVertexAttribArrayARB)(GLuint index);
void (GLAPIENTRY *qglDisableVertexAttribArrayARB)(GLuint index);
void (GLAPIENTRY *qglBindAttribLocationARB)(GLhandleARB programObj, GLuint index, const GLcharARB *name);
void (GLAPIENTRY *qglGetActiveAttribARB)(GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
GLint (GLAPIENTRY *qglGetAttribLocationARB)(GLhandleARB programObj, const GLcharARB *name);
//void (GLAPIENTRY *qglGetVertexAttribdvARB)(GLuint index, GLenum pname, GLdouble *params);
//void (GLAPIENTRY *qglGetVertexAttribfvARB)(GLuint index, GLenum pname, GLfloat *params);
//void (GLAPIENTRY *qglGetVertexAttribivARB)(GLuint index, GLenum pname, GLint *params);
//void (GLAPIENTRY *qglGetVertexAttribPointervARB)(GLuint index, GLenum pname, GLvoid **pointer);

static dllfunction_t shaderobjectsfuncs[] =
{
	{"glDeleteObjectARB", (void **) &qglDeleteObjectARB},
	{"glGetHandleARB", (void **) &qglGetHandleARB},
	{"glDetachObjectARB", (void **) &qglDetachObjectARB},
	{"glCreateShaderObjectARB", (void **) &qglCreateShaderObjectARB},
	{"glShaderSourceARB", (void **) &qglShaderSourceARB},
	{"glCompileShaderARB", (void **) &qglCompileShaderARB},
	{"glCreateProgramObjectARB", (void **) &qglCreateProgramObjectARB},
	{"glAttachObjectARB", (void **) &qglAttachObjectARB},
	{"glLinkProgramARB", (void **) &qglLinkProgramARB},
	{"glUseProgramObjectARB", (void **) &qglUseProgramObjectARB},
	{"glValidateProgramARB", (void **) &qglValidateProgramARB},
	{"glUniform1fARB", (void **) &qglUniform1fARB},
	{"glUniform2fARB", (void **) &qglUniform2fARB},
	{"glUniform3fARB", (void **) &qglUniform3fARB},
	{"glUniform4fARB", (void **) &qglUniform4fARB},
	{"glUniform1iARB", (void **) &qglUniform1iARB},
	{"glUniform2iARB", (void **) &qglUniform2iARB},
	{"glUniform3iARB", (void **) &qglUniform3iARB},
	{"glUniform4iARB", (void **) &qglUniform4iARB},
	{"glUniform1fvARB", (void **) &qglUniform1fvARB},
	{"glUniform2fvARB", (void **) &qglUniform2fvARB},
	{"glUniform3fvARB", (void **) &qglUniform3fvARB},
	{"glUniform4fvARB", (void **) &qglUniform4fvARB},
	{"glUniform1ivARB", (void **) &qglUniform1ivARB},
	{"glUniform2ivARB", (void **) &qglUniform2ivARB},
	{"glUniform3ivARB", (void **) &qglUniform3ivARB},
	{"glUniform4ivARB", (void **) &qglUniform4ivARB},
	{"glUniformMatrix2fvARB", (void **) &qglUniformMatrix2fvARB},
	{"glUniformMatrix3fvARB", (void **) &qglUniformMatrix3fvARB},
	{"glUniformMatrix4fvARB", (void **) &qglUniformMatrix4fvARB},
	{"glGetObjectParameterfvARB", (void **) &qglGetObjectParameterfvARB},
	{"glGetObjectParameterivARB", (void **) &qglGetObjectParameterivARB},
	{"glGetInfoLogARB", (void **) &qglGetInfoLogARB},
	{"glGetAttachedObjectsARB", (void **) &qglGetAttachedObjectsARB},
	{"glGetUniformLocationARB", (void **) &qglGetUniformLocationARB},
	{"glGetActiveUniformARB", (void **) &qglGetActiveUniformARB},
	{"glGetUniformfvARB", (void **) &qglGetUniformfvARB},
	{"glGetUniformivARB", (void **) &qglGetUniformivARB},
	{"glGetShaderSourceARB", (void **) &qglGetShaderSourceARB},
	{NULL, NULL}
};

static dllfunction_t vertexshaderfuncs[] =
{
//	{"glVertexAttrib1fARB", (void **) &qglVertexAttrib1fARB},
//	{"glVertexAttrib1sARB", (void **) &qglVertexAttrib1sARB},
//	{"glVertexAttrib1dARB", (void **) &qglVertexAttrib1dARB},
//	{"glVertexAttrib2fARB", (void **) &qglVertexAttrib2fARB},
//	{"glVertexAttrib2sARB", (void **) &qglVertexAttrib2sARB},
//	{"glVertexAttrib2dARB", (void **) &qglVertexAttrib2dARB},
//	{"glVertexAttrib3fARB", (void **) &qglVertexAttrib3fARB},
//	{"glVertexAttrib3sARB", (void **) &qglVertexAttrib3sARB},
//	{"glVertexAttrib3dARB", (void **) &qglVertexAttrib3dARB},
//	{"glVertexAttrib4fARB", (void **) &qglVertexAttrib4fARB},
//	{"glVertexAttrib4sARB", (void **) &qglVertexAttrib4sARB},
//	{"glVertexAttrib4dARB", (void **) &qglVertexAttrib4dARB},
//	{"glVertexAttrib4NubARB", (void **) &qglVertexAttrib4NubARB},
//	{"glVertexAttrib1fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib1svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib1dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib2fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib2svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib2dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib3fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib3svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib3dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib4fvARB", (void **) &qglVertexAttrib1fvARB},
//	{"glVertexAttrib4svARB", (void **) &qglVertexAttrib1svARB},
//	{"glVertexAttrib4dvARB", (void **) &qglVertexAttrib1dvARB},
//	{"glVertexAttrib4ivARB", (void **) &qglVertexAttrib1ivARB},
//	{"glVertexAttrib4bvARB", (void **) &qglVertexAttrib1bvARB},
//	{"glVertexAttrib4ubvARB", (void **) &qglVertexAttrib1ubvARB},
//	{"glVertexAttrib4usvARB", (void **) &qglVertexAttrib1usvARB},
//	{"glVertexAttrib4uivARB", (void **) &qglVertexAttrib1uivARB},
//	{"glVertexAttrib4NbvARB", (void **) &qglVertexAttrib1NbvARB},
//	{"glVertexAttrib4NsvARB", (void **) &qglVertexAttrib1NsvARB},
//	{"glVertexAttrib4NivARB", (void **) &qglVertexAttrib1NivARB},
//	{"glVertexAttrib4NubvARB", (void **) &qglVertexAttrib1NubvARB},
//	{"glVertexAttrib4NusvARB", (void **) &qglVertexAttrib1NusvARB},
//	{"glVertexAttrib4NuivARB", (void **) &qglVertexAttrib1NuivARB},
	{"glVertexAttribPointerARB", (void **) &qglVertexAttribPointerARB},
	{"glEnableVertexAttribArrayARB", (void **) &qglEnableVertexAttribArrayARB},
	{"glDisableVertexAttribArrayARB", (void **) &qglDisableVertexAttribArrayARB},
	{"glBindAttribLocationARB", (void **) &qglBindAttribLocationARB},
	{"glGetActiveAttribARB", (void **) &qglGetActiveAttribARB},
	{"glGetAttribLocationARB", (void **) &qglGetAttribLocationARB},
//	{"glGetVertexAttribdvARB", (void **) &qglGetVertexAttribdvARB},
//	{"glGetVertexAttribfvARB", (void **) &qglGetVertexAttribfvARB},
//	{"glGetVertexAttribivARB", (void **) &qglGetVertexAttribivARB},
//	{"glGetVertexAttribPointervARB", (void **) &qglGetVertexAttribPointervARB},
	{NULL, NULL}
};

void (GLAPIENTRY *qglMultiTexCoord3f) (GLenum, GLfloat, GLfloat, GLfloat);
void (GLAPIENTRY *qglActiveTexture) (GLenum);
//void (GLAPIENTRY *qglClientActiveTexture) (GLenum);

qglClientActiveTexturePROC qglClientActiveTexture; 

static dllfunction_t multitexturefuncs[] =
{
//	{"glMultiTexCoord2fARB", (void **) &qglMultiTexCoord2f},
	{"glMultiTexCoord3fARB", (void **) &qglMultiTexCoord3f},
	{"glActiveTextureARB", (void **) &qglActiveTexture},
//	{"glClientActiveTextureARB", (void **) &qglClientActiveTexture},
	{NULL, NULL}
};

static dllfunction_t wglswapintervalfuncs[] =
{
	{"wglSwapIntervalEXT", (void **) &qwglSwapIntervalEXT},
	{NULL, NULL}
};

//int		texture_mode = GL_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_LINEAR;
int		texture_mode = GL_LINEAR;
//int		texture_mode = GL_LINEAR_MIPMAP_NEAREST;
//int		texture_mode = GL_LINEAR_MIPMAP_LINEAR;

int		texture_extension_number = 1;

int		multitex_go = false;
int		arrays_go = true;

// Entar: pretty much checks all the extensions now, except combine (below)
void CheckMultiTextureExtensions(void) 
{
//	if (strstr(gl_extensions, "GL_ARB_multitexture ")) {
	if (GL_CheckExtension("GL_ARB_multitexture", multitexturefuncs, "-nomtex", false)){
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_textureunits);
		if (COM_CheckParm("-2tmus"))
			gl_textureunits = 2;
//		Con_Printf("&c840Multitexture extensions found&r.\n");
		if (extra_info)
			Con_Printf("&c840Found %i texture unit support&r.\n", gl_textureunits);
		qglMTexCoord2fARB = (void *) wglGetProcAddress("glMultiTexCoord2fARB");
		qglMTexCoord1fARB = (void *) wglGetProcAddress("glMultiTexCoord1fARB");
		qglSelectTextureARB = (void *) wglGetProcAddress("glActiveTextureARB");
		qglPointParameterfvEXT = (void *)wglGetProcAddress("glPointParameterfvEXT");
		qglPointParameterfEXT = (void *)wglGetProcAddress("glPointParameterfEXT");
		glPNTrianglesiATI = (void *)wglGetProcAddress("glPNTrianglesiATI");
		qglClientActiveTexture = (void *) wglGetProcAddress("glClientActiveTextureARB");

		multitex_go = true;
	}

// COMMANDLINEOPTION: Windows WGL: -novideosync disables WGL_EXT_swap_control
	gl_videosyncavailable = GL_CheckExtension("WGL_EXT_swap_control", wglswapintervalfuncs, "-novideosync", false);
	if (!gl_videosyncavailable)
		Cvar_SetValue("vid_vsync", 0);

	if (COM_CheckParm("-noarrays"))
		arrays_go = false;
}

void CheckCombineExtension(void)
{
	if (strstr(gl_extensions, "GL_ARB_texture_env_combine ")){
		if (extra_info)
			Con_Printf("&c840Combine extension found&r.\n");
		gl_combine = true;
	}
	if (strstr(gl_extensions, "GL_NV_texture_shader ")){
		if (extra_info)
			Con_Printf("&c840Nvidia texture shader extension found&r.\n");
		gl_shader = true;
	}
	if (strstr(gl_extensions, "GL_NV_point_sprite ")){
		if (extra_info)
			Con_Printf("&c840Nvidia point sprite extension found&r.\n");
		gl_point_sprite = true;
	}
	if (strstr(gl_extensions, "GL_SGIS_generate_mipmap ")){
		if (extra_info)
			Con_Printf("&c840SGIS generate mipmap extension found&r.\n");
		gl_sgis_mipmap = true;
	}
	if (strstr(gl_extensions, "GL_ARB_texture_non_power_of_two ")){
		if (extra_info)
			Con_Printf("&c840ARB texture non power of two extension found&r.\n");
		gl_texture_non_power_of_two = true;
	}

	if (strstr(gl_extensions, "GL_EXT_texture_filter_anisotropic ")){
		//glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gl_maximumAnisotropy);
		if (extra_info)
			Con_Printf("&c840ARB anisotropic extension found&r.\n");
		gl_filtering_anisotropic = true;
	}

	if (strstr(gl_extensions, "GL_ATI_pn_triangles ") || glPNTrianglesiATI != NULL){
		if (extra_info)
			Con_Printf("&c840TruForm (n-patches) extension found&r.\n");
		gl_n_patches = true;
	}

	if (COM_CheckParm("-texture_compression") && strstr(gl_extensions, "GL_ARB_texture_compression "))
	{
		if (extra_info)
			Con_Printf("&c055Using texture compression&r.\n");
		gl_lightmap_format = GL_COMPRESSED_RGBA_ARB;
		gl_solid_format = GL_COMPRESSED_RGB_ARB;
		gl_alpha_format = GL_COMPRESSED_RGBA_ARB;
	} else {
		gl_lightmap_format = GL_RGBA;
		gl_solid_format = GL_RGB;
		gl_alpha_format = GL_RGBA;
	}

}
/*
===============
GL_Init
===============
*/
void GL_Init (void)
{
	extern char *ENGINE_EXTENSIONS;

	gl_vendor = glGetString (GL_VENDOR);
	if (extra_info)
		Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	if (extra_info)
		Con_Printf ("GL_RENDERER: %s\n", gl_renderer);

	gl_version = glGetString (GL_VERSION);
	if (extra_info)	
		Con_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glGetString (GL_EXTENSIONS);
	//Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	CheckMultiTextureExtensions ();
	CheckCombineExtension ();

	// check other custom extensions
	if ( GL_CheckExtension("GL_ARB_shader_objects", shaderobjectsfuncs, "-noGLSLshaders", false) &&
		GL_CheckExtension("GL_ARB_shading_language_100", NULL, "-noGLSLshaders", false) &&
		GL_CheckExtension("GL_ARB_vertex_shader", vertexshaderfuncs, "-noGLSLshaders", false) &&
		GL_CheckExtension("GL_ARB_fragment_shader", NULL, "-noGLSLshaders", false) ) {
		gl_support_GLSL_shaders = true;
	}

	if( GL_CheckExtension("GL_ARB_texture_cube_map", NULL, "-noCubemaps", false ) ) {
		gl_support_cubemaps = true;
	}

	// LordHavoc: report supported extensions
	if (extra_info)
		Con_Printf ("\n&c05cEngine extensions:&r &c989%s &r\n", ENGINE_EXTENSIONS);

	glClearColor (0,0,0,0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666f);

	//glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glPolygonMode (GL_FRONT, GL_FILL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
	extern cvar_t gl_clear;

	*x = *y = 0;
	*width = WindowRect.right - WindowRect.left;
	*height = WindowRect.bottom - WindowRect.top;

//    if (!wglMakeCurrent( maindc, baseRC ))
//		Sys_Error ("wglMakeCurrent failed");

//	glViewport (*x, *y, *width, *height);
}

static qboolean vid_usingvsync = true;
static qboolean vid_usevsync = false;

void GL_EndRendering (void)
{
	vid_usevsync = vid_vsync.value && !cls.timedemo && gl_videosyncavailable;
	if (vid_usingvsync != vid_usevsync && gl_videosyncavailable)
	{
		vid_usingvsync = vid_usevsync;
		qwglSwapIntervalEXT (vid_usevsync);
	}

	if (!scr_skipupdate)
		SwapBuffers(maindc);

// handle the mouse state when windowed if that's changed
	if (modestate == MS_WINDOWED)
	{
		if (!_windowed_mouse.value) {
			if (windowed_mouse)	{
				IN_DeactivateMouse ();
				IN_ShowMouse ();
				windowed_mouse = false;
			}
		} else {
			windowed_mouse = true;
			if (key_dest == key_game && !mouseactive && ActiveApp)
			{
				IN_ActivateMouse ();
				IN_HideMouse ();
				SetCursorPos (window_center_x, window_center_y);
			} else if (mouseactive && key_dest != key_game)
			{
				if (key_dest == key_console)
				{
					IN_DeactivateMouse ();
					IN_ShowMouse ();
				}
				IN_HideMouse ();
			}
		}
	}
	if (fullsbardraw)
		Sbar_Changed();
}

void VID_SetPalette (unsigned char *palette)
{
	byte	*pal;
	unsigned r,g,b;
	unsigned v;
	int     r1,g1,b1;
	int		j,k,l;
	unsigned short i;
	unsigned	*table;

//
// 8 8 8 encoding
//
	pal = palette;
	table = d_8to24table;
	for (i=0 ; i<256 ; i++)
	{
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;
		
//		v = (255<<24) + (r<<16) + (g<<8) + (b<<0);
//		v = (255<<0) + (r<<8) + (g<<16) + (b<<24);
		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		*table++ = v;
	}
	d_8to24table[255] &= 0x000000;	// 255 is transparent

	// JACK: 3D distance calcs - k is last closest, l is the distance.
	// FIXME: Precalculate this and cache to disk.
	for (i=0; i < (1<<15); i++) {
		/* Maps
			000000000000000
			000000000011111 = Red  = 0x1F
			000001111100000 = Blue = 0x03E0
			111110000000000 = Grn  = 0x7C00
		*/
		r = ((i & 0x1F) << 3)+4;
		g = ((i & 0x03E0) >> 2)+4;
		b = ((i & 0x7C00) >> 7)+4;
		pal = (unsigned char *)d_8to24table;
		for (v=0,k=0,l=10000*10000; v<256; v++,pal+=4) {
			r1 = r-pal[0];
			g1 = g-pal[1];
			b1 = b-pal[2];
			j = (r1*r1)+(g1*g1)+(b1*b1);
			if (j<l) {
				k=v;
				l=j;
			}
		}
		d_15to8table[i]=k;
	}
}

BOOL	gammaworks;

void	VID_ShiftPalette (unsigned char *palette)
{
	extern	byte ramps[3][256];
	
//	VID_SetPalette (palette);

//	gammaworks = SetDeviceGammaRamp (maindc, ramps);
}

void VID_SetDefaultMode (void)
{
	IN_DeactivateMouse ();
}

void VID_UnSetMode (void)
{
	HGLRC hRC;
   	HDC	  hDC;

	if (vid_initialized)
	{
		vid_canalttab = false;
		hRC = wglGetCurrentContext();
    	hDC = wglGetCurrentDC();

		wglMakeCurrent(hDC, NULL);

    	if (hRC)
    	    wglDeleteContext(hRC); // if you've run an hlbsp, it crashes here when you quit (beyond me)

		GL_CloseLibrary();

		if (hDC && dibwindow)
			ReleaseDC(dibwindow, hDC);

		if (modestate == MS_FULLDIB)
			ChangeDisplaySettings (NULL, 0);

		if (maindc && dibwindow)
			ReleaseDC (dibwindow, maindc);

		AppActivate(false, false);


		if (mainwindow)
			DestroyWindow(mainwindow);
		mainwindow = NULL;
	}
}

void VID_Shutdown (void)
{
	if (gammawaschanged)
	{
		gammaworks = VID_SetGamma(originalgammaramps);
	}
   	
	VID_UnSetMode();
}


//==========================================================================


BOOL bSetupPixelFormat(HDC hDC)
{
    static PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
	1,				// version number
	PFD_DRAW_TO_WINDOW 		// support window
	|  PFD_SUPPORT_OPENGL 	// support OpenGL
	|  PFD_DOUBLEBUFFER ,	// double buffered
	PFD_TYPE_RGBA,			// RGBA type
	32,				// 24-bit color depth
	0, 0, 0, 0, 0, 0,		// color bits ignored
	0,				// no alpha buffer
	0,				// shift bit ignored
	0,				// no accumulation buffer
	0, 0, 0, 0, 			// accum bits ignored
	32,				// 24-bit z-buffer	//qmb :stencil
	8,				// 8-bit stencil buffer //qmb :stencil
	0,				// no auxiliary buffer
	PFD_MAIN_PLANE,			// main layer
	0,				// reserved
	0, 0, 0				// layer masks ignored
    };
    int pixelformat;

    if ( (pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0 )
    {
        MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    if (SetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
    {
        MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    return TRUE;
}

BOOL bSetup3DFXPixelFormat(HDC hDC)
{
    static PIXELFORMATDESCRIPTOR pfd = {
	sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
	1,				// version number
	PFD_DRAW_TO_WINDOW 		// support window
	|  PFD_SUPPORT_OPENGL 	// support OpenGL
	|  PFD_DOUBLEBUFFER ,	// double buffered
	PFD_TYPE_RGBA,			// RGBA type
	24,				// 24-bit color depth
	0, 0, 0, 0, 0, 0,		// color bits ignored
	0,				// no alpha buffer
	0,				// shift bit ignored
	0,				// no accumulation buffer
	0, 0, 0, 0, 			// accum bits ignored
	32,				// 32-bit z-buffer	//qmb :stencil
	0,				// 3dfx dont have stencil buffer //qmb :stencil
	0,				// no auxiliary buffer
	PFD_MAIN_PLANE,			// main layer
	0,				// reserved
	0, 0, 0				// layer masks ignored
    };
    int pixelformat;

    if ( (pixelformat = ChoosePixelFormat(hDC, &pfd)) == 0 )
    {
        MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    if (SetPixelFormat(hDC, pixelformat, &pfd) == FALSE)
    {
        MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
        return FALSE;
    }

    return TRUE;
}

byte        scantokey[128] = 
					{ 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6', 
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0 
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i', 
	'o',    'p',    '[',    ']',    13 ,    K_CTRL,'a',  's',      // 1 
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';', 
	'\'' ,    '`',    K_SHIFT,'\\',  'z',    'x',    'c',    'v',      // 2 
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE  ,    0  , K_HOME, 
	K_UPARROW,K_PGUP,'-',K_LEFTARROW,'5',K_RIGHTARROW,'+',K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
					}; 

byte        shiftscantokey[128] = 
					{ 
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '!',    '@',    '#',    '$',    '%',    '^', 
	'&',    '*',    '(',    ')',    '_',    '+',    K_BACKSPACE, 9, // 0 
	'Q',    'W',    'E',    'R',    'T',    'Y',    'U',    'I', 
	'O',    'P',    '{',    '}',    13 ,    K_CTRL,'A',  'S',      // 1 
	'D',    'F',    'G',    'H',    'J',    'K',    'L',    ':', 
	'"' ,    '~',    K_SHIFT,'|',  'Z',    'X',    'C',    'V',      // 2 
	'B',    'N',    'M',    '<',    '>',    '?',    K_SHIFT,'*', 
	K_ALT,' ',   0  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10, K_PAUSE  ,    0  , K_HOME, 
	K_UPARROW,K_PGUP,'_',K_LEFTARROW,'%',K_RIGHTARROW,'+',K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
					}; 


/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	key = (key>>16)&255;
	if (key > 127)
		return 0;
	if (scantokey[key] == 0)
		Con_DPrintf("key 0x%02x has no translation\n", key);
	return scantokey[key];
}

/*
===================================================================

MAIN WINDOW

===================================================================
*/

/*
================
ClearAllStates
================
*/
void ClearAllStates (void)
{
	int		i;
	
// send an up event for each key, to make sure the server clears them all
	for (i=0 ; i<256 ; i++)
	{
		Key_Event (i, false);
	}

	Key_ClearStates ();
	IN_ClearStates ();
}

extern float oldgammavalue, oldcontrastvalue;

void AppActivate(BOOL fActive, BOOL minimize)
/****************************************************************************
*
* Function:     AppActivate
* Parameters:   fActive - True if app is activating
*
* Description:  If the application is activating, then swap the system
*               into SYSPAL_NOSTATIC mode so that our palettes will display
*               correctly.
*
****************************************************************************/
{
	static BOOL	sound_active;

	ActiveApp = fActive;
	Minimized = minimize;

// enable/disable sound on focus gain/loss
	if (!ActiveApp && sound_active)
	{
		S_BlockSound ();
		sound_active = false;
	}
	else if (ActiveApp && !sound_active)
	{
		S_UnblockSound ();
		sound_active = true;
	}

	if (fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();

			if (vid_canalttab && vid_wassuspended) {
				vid_wassuspended = false;
				ChangeDisplaySettings (&gdevmode, CDS_FULLSCREEN);
				ShowWindow(mainwindow, SW_SHOWNORMAL);

				// fix for NVIDIA drivers alt-tab bug
				MoveWindow (mainwindow, 0, 0, gdevmode.dmPelsWidth, gdevmode.dmPelsHeight, false);
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value && key_dest == key_game)
		{
			IN_ActivateMouse ();
			IN_HideMouse ();
		}

		oldgammavalue = 0;
		oldcontrastvalue = 0;
	}

	if (!fActive)
	{
		if (modestate == MS_FULLDIB)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
			if (vid_canalttab) { 
				ChangeDisplaySettings (NULL, 0);
				vid_wassuspended = true;
			}
		}
		else if ((modestate == MS_WINDOWED) && _windowed_mouse.value)
		{
			IN_DeactivateMouse ();
			IN_ShowMouse ();
		}

//		v_gamma.modified = true;	//wham bam thanks.

		VID_SetGamma(originalgammaramps);
	}
}


/* main window procedure */
LONG WINAPI MainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
    LONG    lRet = 1;
	int		fActive, fMinimized, temp;
	extern unsigned int uiWheelMessage;

	if ( uMsg == uiWheelMessage )
		uMsg = WM_MOUSEWHEEL;

    switch (uMsg)
    {
		case WM_KILLFOCUS:
			if (modestate == MS_FULLDIB)
				ShowWindow(mainwindow, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			break;

		case WM_MOVE:
			window_x = (int) LOWORD(lParam);
			window_y = (int) HIWORD(lParam);
			VID_UpdateWindowStatus ();
			break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			Key_Event (MapKey(lParam), true);
			break;
			
		case WM_KEYUP:
		case WM_SYSKEYUP:
			Key_Event (MapKey(lParam), false);
			break;

		case WM_SYSCHAR:
		// keep Alt-Space from happening
			break;

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEMOVE:
			temp = 0;

			if (wParam & MK_LBUTTON)
				temp |= 1;

			if (wParam & MK_RBUTTON)
				temp |= 2;

			if (wParam & MK_MBUTTON)
				temp |= 4;

			IN_MouseEvent (temp);

			break;

		// JACK: This is the mouse wheel with the Intellimouse
		// Its delta is either positive or neg, and we generate the proper
		// Event.
		case WM_MOUSEWHEEL: 
			if ((short) HIWORD(wParam) > 0) {
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			} else {
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
			break;

    	case WM_SIZE:
            break;

   	    case WM_CLOSE:
			if (MessageBox (mainwindow, "Are you sure you want to quit?", "Confirm Exit",
						MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Sys_Quit ();
			}

	        break;

		case WM_ACTIVATE:
			fActive = LOWORD(wParam);
			fMinimized = (BOOL) HIWORD(wParam);
			AppActivate(!(fActive == WA_INACTIVE), fMinimized);

		// fix the leftover Alt from any Alt-Tab or the like that switched us away
			ClearAllStates ();

			break;

   	    case WM_DESTROY:
        {
			if (dibwindow)
				DestroyWindow (dibwindow);

            PostQuitMessage (0);
        }
        break;

		case MM_MCINOTIFY:
            lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;

    	default:
            /* pass all unhandled messages to DefWindowProc */
            lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
        break;
    }

    /* return 1 if handled message, 0 if not */
    return lRet;
}


/*
=================
VID_NumModes
=================
*/
int VID_NumModes (void)
{
	return nummodes;
}

	
/*
=================
VID_GetModePtr
=================
*/
vmode_t *VID_GetModePtr (int modenum)
{

	if ((modenum >= 0) && (modenum < nummodes))
		return &modelist[modenum];
	else
		return &badmode;
}


/*
=================
VID_GetModeDescription
=================
*/
char *VID_GetModeDescription (int mode)
{
	char		*pinfo;
	vmode_t		*pv;
	static char	temp[100];

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	if (!leavecurrentmode)
	{
		pv = VID_GetModePtr (mode);
		pinfo = pv->modedesc;
	}
	else
	{
		sprintf (temp, "Desktop resolution (%dx%d)",
				 modelist[MODE_FULLSCREEN_DEFAULT].width,
				 modelist[MODE_FULLSCREEN_DEFAULT].height);
		pinfo = temp;
	}

	return pinfo;
}


// KJB: Added this to return the mode driver name in description for console

char *VID_GetExtModeDescription (int mode)
{
	static char	pinfo[40];
	vmode_t		*pv;

	if ((mode < 0) || (mode >= nummodes))
		return NULL;

	pv = VID_GetModePtr (mode);
	if (modelist[mode].type == MS_FULLDIB)
	{
		if (!leavecurrentmode)
		{
			sprintf(pinfo,"%s fullscreen", pv->modedesc);
		}
		else
		{
			sprintf (pinfo, "Desktop resolution (%dx%d)",
					 modelist[MODE_FULLSCREEN_DEFAULT].width,
					 modelist[MODE_FULLSCREEN_DEFAULT].height);
		}
	}
	else
	{
		if (modestate == MS_WINDOWED)
			sprintf(pinfo, "%s windowed", pv->modedesc);
		else
			sprintf(pinfo, "windowed");
	}

	return pinfo;
}


/*
=================
VID_DescribeCurrentMode_f
=================
*/
void VID_DescribeCurrentMode_f (void)
{
	Con_Printf ("%s\n", VID_GetExtModeDescription (vid_modenum));
}


/*
=================
VID_NumModes_f
=================
*/
void VID_NumModes_f (void)
{

	if (nummodes == 1)
		Con_Printf ("%d video mode is available\n", nummodes);
	else
		Con_Printf ("%d video modes are available\n", nummodes);
}


/*
=================
VID_DescribeMode_f
=================
*/
void VID_DescribeMode_f (void)
{
	int		t, modenum;
	
	modenum = Q_atoi (Cmd_Argv(1));

	t = leavecurrentmode;
	leavecurrentmode = 0;

	Con_Printf ("%s\n", VID_GetExtModeDescription (modenum));

	leavecurrentmode = t;
}


/*
=================
VID_DescribeModes_f
=================
*/
void VID_DescribeModes_f (void)
{
	int			i, lnummodes, t;
	char		*pinfo;
	vmode_t		*pv;

	lnummodes = VID_NumModes ();

	t = leavecurrentmode;
	leavecurrentmode = 0;

	for (i=1 ; i<lnummodes ; i++)
	{
		pv = VID_GetModePtr (i);
		pinfo = VID_GetExtModeDescription (i);
		Con_Printf ("%2d: %s\n", i, pinfo);
	}

	leavecurrentmode = t;
}


void VID_InitDIB (HINSTANCE hInstance)
{
	WNDCLASS		wc;

	/* Register the frame class */
    wc.style         = 0;
    wc.lpfnWndProc   = (WNDPROC)MainWndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = 0;
    wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
	wc.hbrBackground = NULL;
    wc.lpszMenuName  = 0;
    wc.lpszClassName = "VengeanceR2";

    RegisterClass (&wc);
//chances are its already initialised if it fails
//and if its not, it'll be caught when we try creating the window
//		Sys_Error ("Couldn't register window class");

	modelist[0].type = MS_WINDOWED;
	modelist[0].width = vid_width.value;
	vid.realwidth = vid_width.value; // light blooms

	if (modelist[0].width < 320)
	{
		modelist[0].width = 320;
		vid.realwidth = 320; // light blooms
		Cvar_SetValue("vid_width", 320);
	}

	modelist[0].height = vid_height.value;
	vid.realheight = vid_height.value;

	if (modelist[0].height < 240)
	{
		modelist[0].height = 240;
		vid.realheight = 240;
		Cvar_SetValue("vid_height", 240);
	}

	sprintf (modelist[0].modedesc, "%dx%d",
			 modelist[0].width, modelist[0].height);

	modelist[0].modenum = MODE_WINDOWED;
	modelist[0].dib = 1;
	modelist[0].fullscreen = 0;
	modelist[0].halfscreen = 0;
	modelist[0].bpp = 0;

	nummodes = 1;
}


/*
=================
VID_InitFullDIB
=================
*/
void VID_InitFullDIB (HINSTANCE hInstance)
{
	DEVMODE	devmode;
	int		i, modenum, originalnummodes, existingmode, numlowresmodes;
	int		j, bpp, done;
	BOOL	stat;

// enumerate >8 bpp modes
	originalnummodes = nummodes;
	modenum = 0;

	vid.realwidth = vid_width.value;

	do
	{
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		if ((devmode.dmBitsPerPel >= 15) &&
			(devmode.dmPelsWidth <= MAXWIDTH) &&
			(devmode.dmPelsHeight <= MAXHEIGHT) &&
			(nummodes < MAX_MODE_LIST))
		{
			devmode.dmFields = DM_BITSPERPEL |
							   DM_PELSWIDTH |
							   DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
						 devmode.dmPelsWidth, devmode.dmPelsHeight,
						 devmode.dmBitsPerPel);

			// if the width is more than twice the height, reduce it by half because this
			// is probably a dual-screen monitor
				if (!COM_CheckParm("-noadjustaspect"))
				{
					if (modelist[nummodes].width > (modelist[nummodes].height << 1))
					{
						modelist[nummodes].width >>= 1;
						modelist[nummodes].halfscreen = 1;
						sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
								 modelist[nummodes].width,
								 modelist[nummodes].height,
								 modelist[nummodes].bpp);
					}
				}

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					nummodes++;
				}
			}
		}

		modenum++;
	} while (stat);

// see if there are any low-res modes that aren't being reported
	numlowresmodes = sizeof(lowresmodes) / sizeof(lowresmodes[0]);
	bpp = 16;
	done = 0;

	do
	{
		for (j=0 ; (j<numlowresmodes) && (nummodes < MAX_MODE_LIST) ; j++)
		{
			devmode.dmBitsPerPel = bpp;
			devmode.dmPelsWidth = lowresmodes[j].width;
			devmode.dmPelsHeight = lowresmodes[j].height;
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) ==
					DISP_CHANGE_SUCCESSFUL)
			{
				modelist[nummodes].type = MS_FULLDIB;
				modelist[nummodes].width = devmode.dmPelsWidth;
				modelist[nummodes].height = devmode.dmPelsHeight;
				modelist[nummodes].modenum = 0;
				modelist[nummodes].halfscreen = 0;
				modelist[nummodes].dib = 1;
				modelist[nummodes].fullscreen = 1;
				modelist[nummodes].bpp = devmode.dmBitsPerPel;
				sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
						 devmode.dmPelsWidth, devmode.dmPelsHeight,
						 devmode.dmBitsPerPel);

				for (i=originalnummodes, existingmode = 0 ; i<nummodes ; i++)
				{
					if ((modelist[nummodes].width == modelist[i].width)   &&
						(modelist[nummodes].height == modelist[i].height) &&
						(modelist[nummodes].bpp == modelist[i].bpp))
					{
						existingmode = 1;
						break;
					}
				}

				if (!existingmode)
				{
					nummodes++;
				}
			}
		}
		switch (bpp)
		{
			case 16:
				bpp = 32;
				break;

			case 32:
				bpp = 24;
				break;

			case 24:
				done = 1;
				break;
		}
	} while (!done);

	if (nummodes == originalnummodes)
		Con_SafePrintf ("No fullscreen DIB modes found\n");
}

static void Check_Gamma (unsigned char *pal)
{
	float	f, inf;
	unsigned char	palette[768];
	int		i;

 	vid_gamma = 1.0f;
 	if (i = COM_CheckParm("-gamma"))
 		vid_gamma = Q_atof(com_argv[i+1]);

//	if ((i = COM_CheckParm("-gamma")) == 0) {
//		if ((gl_renderer && strstr(gl_renderer, "Voodoo")) ||
//			(gl_vendor && strstr(gl_vendor, "3Dfx")))
//			vid_gamma = 1;
//		else
//			vid_gamma = 0.7f; // default to 0.7 on non-3dfx hardware
//	} else
//		vid_gamma = Q_atof(com_argv[i+1]);

	for (i=0 ; i<768 ; i++)
	{
		f = pow ( (pal[i]+1)/256.0 , vid_gamma );
		inf = f*255 + 0.5;
		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;
		palette[i] = inf;
	}

	memcpy (pal, palette, sizeof(palette));
}

int findbpp;
extern HWND	hwnd_dialog;		// startup dialog box
/*
===================
VID_Init
===================
*/
void	VID_Init (unsigned char *palette)
{
	int		i, existingmode;
	//int		basenummodes, width, height, bpp, findbpp, done;
	int		basenummodes, done;
	char	gldir[MAX_OSPATH];
	HDC		hdc;
	DEVMODE	devmode;

	const char *gldrivername;

	memset(&devmode, 0, sizeof(devmode));

	gl_vendor = NULL;
	gl_renderer = NULL;
	gl_version = NULL;
	gl_extensions = NULL;

	//needs comctl32.lib
	//InitCommonControls();

	VID_InitDIB (global_hInstance);
	basenummodes = nummodes = 1;

	VID_InitFullDIB (global_hInstance);

	if (!vid_fullscreen.value)
	{
		hdc = GetDC (NULL);

		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
		{
			Sys_Error ("Can't run in non-RGB mode");
		}

		ReleaseDC (NULL, hdc);

		windowed = true;

		vid_default = MODE_WINDOWED;
	}
	else
	{
		if (nummodes == 1)
			Sys_Error ("No RGB fullscreen modes available");

		windowed = false;

		if (COM_CheckParm("-mode"))
		{
			vid_default = Q_atoi(com_argv[COM_CheckParm("-mode")+1]);
		}
		else
		{
			if (COM_CheckParm("-current"))
			{
				modelist[MODE_FULLSCREEN_DEFAULT].width =
						GetSystemMetrics (SM_CXSCREEN);
				modelist[MODE_FULLSCREEN_DEFAULT].height =
						GetSystemMetrics (SM_CYSCREEN);
				vid_default = MODE_FULLSCREEN_DEFAULT;
				leavecurrentmode = 1;
			}
			else
			{
			// if they want to force it, add the specified mode to the list
				if (COM_CheckParm("-force") && (nummodes < MAX_MODE_LIST))
				{
					modelist[nummodes].type = MS_FULLDIB;
					modelist[nummodes].width = vid_width.value;
					modelist[nummodes].height = vid_height.value;
					modelist[nummodes].modenum = 0;
					modelist[nummodes].halfscreen = 0;
					modelist[nummodes].dib = 1;
					modelist[nummodes].fullscreen = 1;
					modelist[nummodes].bpp = vid_bitsperpixel.value;
					sprintf (modelist[nummodes].modedesc, "%dx%dx%d",
							 devmode.dmPelsWidth, devmode.dmPelsHeight,
							 devmode.dmBitsPerPel);

					for (i=nummodes, existingmode = 0 ; i<nummodes ; i++)
					{
						if ((modelist[nummodes].width == modelist[i].width)   &&
							(modelist[nummodes].height == modelist[i].height) &&
							(modelist[nummodes].bpp == modelist[i].bpp))
						{
							existingmode = 1;
							break;
						}
					}

					if (!existingmode)
					{
						nummodes++;
					}
				}

				done = 0;

				do
				{
					for (i=1, vid_default=0 ; i<nummodes ; i++)
					{
						if ((modelist[i].width == vid_width.value) &&
							(modelist[i].height == vid_height.value) &&
							(modelist[i].bpp == vid_bitsperpixel.value))
						{
							vid_default = i;
							done = 1;
							break;
						}
					}

					if (!done)
					{
						if (findbpp)
						{
							switch ((int)vid_bitsperpixel.value)
							{
							case 15:
								Cvar_SetValue("vid_bitsperpixel", 16);
								break;
							case 16:
								Cvar_SetValue("vid_bitsperpixel", 32);
								break;
							case 32:
								Cvar_SetValue("vid_bitsperpixel", 24);
								break;
							case 24:
								done = 1;
								break;
							}
						}
						else
						{
							done = 1;
						}
					}
				} while (!done);

				if (!vid_default)
				{
					Sys_Error ("Specified video mode not available");
				}
			}
		}
	}

	vid_initialized = true;

	if ((i = COM_CheckParm("-conwidth")) != 0)
		vid.conwidth = Q_atoi(com_argv[i+1]);
	else
		vid.conwidth = vid_width.value;

	vid.conwidth &= 0xfff8; // make it a multiple of eight

	if (vid.conwidth < 320)
		vid.conwidth = 320;

	//DrLabman: Fix 'correct aspect' to be the correct aspect based on the actual vid height over vid width
	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * vid.realheight / vid.realwidth;

	if ((i = COM_CheckParm("-conheight")) != 0)
		vid.conheight = Q_atoi(com_argv[i+1]);
	if (vid.conheight < 200)
		vid.conheight = 200;

	if (gamemode == GAME_NORMAL || gamemode == GAME_ROGUE || gamemode == GAME_HIPNOTIC)
		if (hwnd_dialog)
		{
			Sleep(100);
			DestroyWindow (hwnd_dialog);
		}
	
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));

	Check_Gamma(palette);
	VID_SetPalette (palette);

	VID_SetMode (vid_default, palette);

    maindc = GetDC(mainwindow);

	//qmb :stencil
	//assume 3dfx cards dont have a stencil buffer
	if ((gl_renderer && strstr(gl_renderer, "Voodoo")) || (gl_vendor && strstr(gl_vendor, "3Dfx")))
		gl_stencil = false;

	//qmb :stencil
	//force a 3dfx card to use a stencil buffer
	if (COM_CheckParm("-stencil"))
		gl_stencil = true;

	//qmb :stencil
	if (COM_CheckParm("-nostencil")||gl_stencil==false){
		bSetup3DFXPixelFormat(maindc);
		gl_stencil = false;
	}else
		bSetupPixelFormat(maindc);

    baseRC = wglCreateContext( maindc );
	if (!baseRC)
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.");
    if (!wglMakeCurrent( maindc, baseRC ))
		Sys_Error ("wglMakeCurrent failed");

	GL_Init ();

	// Entar : .ms2 isn't used anymore, so we don't have to worry about this folder thing. less mess. (faster startup too)
	// let's replace it with creating the Vr2 folder, if it's not already there
	sprintf (gldir, "Vr2");
	Sys_mkdir (gldir);

	vid_realmode = vid_modenum;

	vid_menudrawfn = VID_MenuDraw;

	strcpy (badmode.modedesc, "Bad mode");
	vid_canalttab = true;

	if (COM_CheckParm("-fullsbar"))
		fullsbardraw = true;

	gldrivername = "opengl32.dll";
// COMMANDLINEOPTION: Windows WGL: -gl_driver <drivername> selects a GL driver library, default is opengl32.dll, useful only for 3dfxogl.dll or 3dfxvgl.dll, if you don't know what this is for, you don't need it
	i = COM_CheckParm("-gl_driver");
	if (i && i < com_argc - 1)
		gldrivername = com_argv[i + 1];
	if (!GL_OpenLibrary(gldrivername))
	{
		//Con_Printf("Unable to load GL driver %s\n", gldrivername);
		Sys_Error ("Unable to load GL driver %s\n", gldrivername); //if unable to load driver, no point in continue loading engine (darkie)
//		return false;
	}
}

void VID_Init_Register(void)
{
	Cvar_RegisterVariable (&vid_mode);
	Cvar_RegisterVariable (&vid_wait);
	Cvar_RegisterVariable (&vid_nopageflip);
	Cvar_RegisterVariable (&_vid_wait_override);
	Cvar_RegisterVariable (&_vid_default_mode);
	Cvar_RegisterVariable (&_vid_default_mode_win);
	Cvar_RegisterVariable (&vid_config_x);
	Cvar_RegisterVariable (&vid_config_y);
	Cvar_RegisterVariable (&vid_stretch_by_2);
	Cvar_RegisterVariable (&_windowed_mouse);
	Cvar_RegisterVariable (&vid_hwgamma);
	Cvar_RegisterVariable (&vid_vsync);
	Cvar_RegisterVariable (&vid_minwidth);
	Cvar_RegisterVariable (&vid_minheight);

	Cmd_AddCommand ("vid_nummodes", VID_NumModes_f);
	Cmd_AddCommand ("vid_describecurrentmode", VID_DescribeCurrentMode_f);
	Cmd_AddCommand ("vid_describemode", VID_DescribeMode_f);
	Cmd_AddCommand ("vid_describemodes", VID_DescribeModes_f);
}

//========================================================
// Video menu stuff
//========================================================

//qmb :changed
//extern void M_Menu_Options_f (void);
extern void M_Menu_Video_f (void); //JHL;
extern void M_Print (int cx, int cy, char *str);
extern void M_PrintWhite (int cx, int cy, char *str);
extern void M_DrawCharacter (int cx, int line, int num);
extern void M_DrawTransPic (int x, int y, qpic_t *pic);
extern void M_DrawPic (int x, int y, qpic_t *pic);
extern void M_DrawCheckbox (int x, int y, int on);

static int	vid_line, vid_wmodes;

typedef struct
{
	int		modenum;
	char	*desc;
	int		iscur;
} modedesc_t;

#define MAX_COLUMN_SIZE		9
#define MODE_AREA_HEIGHT	(MAX_COLUMN_SIZE + 2)
#define MAX_MODEDESCS		(MAX_COLUMN_SIZE*3)

static modedesc_t	modedescs[MAX_MODEDESCS];

// note: if modes are added to the beginning of this list, update VID_DEFAULT
typedef struct video_resolution_s
{
	const char *type;
	int width, height;
	int conwidth, conheight;
	double pixelheight; // pixel aspect
}
video_resolution_t;
video_resolution_t video_resolutions[] =
{
{"Standard 4x3"              ,  320, 240, 320, 240, 1     },
{"Standard 4x3"              ,  400, 300, 400, 300, 1     },
{"Standard 4x3"              ,  512, 384, 512, 384, 1     },
{"Standard 4x3"              ,  640, 480, 640, 480, 1     },
{"Standard 4x3"              ,  800, 600, 640, 480, 1     },
{"Standard 4x3"              , 1024, 768, 640, 480, 1     },
{"Standard 4x3"              , 1152, 864, 640, 480, 1     },
{"Standard 4x3"              , 1280, 960, 640, 480, 1     },
{"Standard 4x3"              , 1400,1050, 640, 480, 1     },
{"Standard 4x3"              , 1600,1200, 640, 480, 1     },
{"Standard 4x3"              , 1792,1344, 640, 480, 1     },
{"Standard 4x3"              , 1856,1392, 640, 480, 1     },
{"Standard 4x3"              , 1920,1440, 640, 480, 1     },
{"Standard 4x3"              , 2048,1536, 640, 480, 1     },
//{"Short Pixel (CRT) 5x4"     ,  320, 256, 320, 256, 0.9375},
//{"Short Pixel (CRT) 5x4"     ,  640, 512, 640, 512, 0.9375},
//{"Short Pixel (CRT) 5x4"     , 1280,1024, 640, 512, 0.9375},
//{"Tall Pixel (CRT) 8x5"      ,  320, 200, 320, 200, 1.2   },
//{"Tall Pixel (CRT) 8x5"      ,  640, 400, 640, 400, 1.2   },
//{"Tall Pixel (CRT) 8x5"      ,  840, 525, 640, 400, 1.2   },
//{"Tall Pixel (CRT) 8x5"      ,  960, 600, 640, 400, 1.2   },
//{"Tall Pixel (CRT) 8x5"      , 1680,1050, 640, 400, 1.2   },
//{"Tall Pixel (CRT) 8x5"      , 1920,1200, 640, 400, 1.2   },
{"Square Pixel (LCD) 5x4"    ,  320, 256, 320, 256, 1     },
{"Square Pixel (LCD) 5x4"    ,  640, 512, 640, 512, 1     },
{"Square Pixel (LCD) 5x4"    , 1280,1024, 640, 512, 1     },
{"WideScreen 5x3"            ,  640, 384, 640, 384, 1     },
{"WideScreen 5x3"            , 1280, 768, 640, 384, 1     },
{"WideScreen 8x5"            ,  320, 200, 320, 200, 1     },
{"WideScreen 8x5"            ,  640, 400, 640, 400, 1     },
{"WideScreen 8x5"            ,  720, 450, 720, 450, 1     },
{"WideScreen 8x5"            ,  840, 525, 640, 400, 1     },
{"WideScreen 8x5"            ,  960, 600, 640, 400, 1     },
{"WideScreen 8x5"            , 1280, 800, 640, 400, 1     },
{"WideScreen 8x5"            , 1440, 900, 720, 450, 1     },
{"WideScreen 8x5"            , 1680,1050, 640, 400, 1     },
{"WideScreen 8x5"            , 1920,1200, 640, 400, 1     },
{"WideScreen 8x5"            , 2560,1600, 640, 400, 1     },
{"WideScreen 8x5"            , 3840,2400, 640, 400, 1     },
{"WideScreen 14x9"           ,  840, 540, 640, 400, 1     },
{"WideScreen 14x9"           , 1680,1080, 640, 400, 1     },
{"WideScreen 16x9"           ,  640, 360, 640, 360, 1     },
{"WideScreen 16x9"           ,  683, 384, 683, 384, 1     },
{"WideScreen 16x9"           ,  960, 540, 640, 360, 1     },
{"WideScreen 16x9"           , 1280, 720, 640, 360, 1     },
{"WideScreen 16x9"           , 1366, 768, 683, 384, 1     },
{"WideScreen 16x9"           , 1920,1080, 640, 360, 1     },
{"WideScreen 16x9"           , 2560,1440, 640, 360, 1     },
{"WideScreen 16x9"           , 3840,2160, 640, 360, 1     },
//{"NTSC 3x2"                  ,  360, 240, 360, 240, 1.125 },
//{"NTSC 3x2"                  ,  720, 480, 720, 480, 1.125 },
//{"PAL 14x11"                 ,  360, 283, 360, 283, 0.9545},
//{"PAL 14x11"                 ,  720, 566, 720, 566, 0.9545},
//{"NES 8x7"                   ,  256, 224, 256, 224, 1.1667},
//{"SNES 8x7"                  ,  512, 448, 512, 448, 1.1667},
{NULL, 0, 0, 0, 0, 0}
};
// this is the number of the default mode (640x480) in the list above
#define VID_DEFAULT 3
#define VID_RES_COUNT ((int)(sizeof(video_resolutions) / sizeof(video_resolutions[0])) - 1)

#define VIDEO_ITEMS 7
static int video_resolution=VID_DEFAULT;

int video_modes_cursor=0;
static int video_cursor_table[] = {56, 68, 88, 100, 112, 132, 162};
/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
	qpic_t		*p;
//	int i;

	p = Draw_CachePic ("gfx/vidmodes.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	// Current Resolution
	M_Print(16, video_cursor_table[0], "    Current Resolution");
//	if (vid_supportrefreshrate && vid.fullscreen)
//		M_Print(220, video_cursor_table[0], va("%dx%d %dhz", vid.width, vid.height, vid.refreshrate));
//	else
		M_Print(220, video_cursor_table[0], va("%ix%i", (int)vid_width.value, (int)vid_height.value));

	// Proposed Resolution
	M_Print(16, video_cursor_table[1], "  New Resolution (WxH)");
	M_Print(220, video_cursor_table[1], va("%dx%d", video_resolutions[video_resolution].width, video_resolutions[video_resolution].height));
	M_Print(96, video_cursor_table[1] + 8, va("Type: %s", video_resolutions[video_resolution].type));

	// Bits per pixel
	M_Print(16, video_cursor_table[2], "Bits per pixel (16/32)");
	M_Print(220, video_cursor_table[2], (vid_bitsperpixel.value == 32) ? "32" : "16");

	// Refresh Rate
	M_PrintWhite(16, video_cursor_table[3], "          Refresh Rate");
//	M_DrawSlider(220, video_cursor_table[3], vid_refreshrate.value, 60, 150);

	// Fullscreen
	M_Print(16, video_cursor_table[4], "            Fullscreen");
	M_DrawCheckbox(220, video_cursor_table[4], (int)vid_fullscreen.value);

	// Vertical Sync
	M_Print(16, video_cursor_table[5], "         Vertical Sync");
	M_DrawCheckbox(220, video_cursor_table[5], (int)vid_vsync.value);

	// "Apply" button
	M_Print(220, video_cursor_table[6], "Apply");

	// Cursor
	M_DrawCharacter(200, video_cursor_table[video_modes_cursor], 12+((int)(realtime*4)&1));
}

/*
================
VID_MenuKey
================
*/
static void M_Menu_Modes_AdjustSliders (int dir)
{
	S_LocalSound ("sound/misc/menu3.wav");

	switch (video_modes_cursor)
	{
		// Resolution
		case 1:
		{
			int r;
			for(r = 0;r < VID_RES_COUNT;r++)
			{
				video_resolution += dir;
				if (video_resolution >= VID_RES_COUNT)
					video_resolution = 0;
				if (video_resolution < 0)
					video_resolution = VID_RES_COUNT - 1;
				if (video_resolutions[video_resolution].width >= vid_minwidth.value && video_resolutions[video_resolution].height >= vid_minheight.value)
					break;
			}
			break;
		}

		// Bits per pixel
		case 2:
			Cvar_SetValue ("vid_bitsperpixel", (vid_bitsperpixel.value == 32) ? 16 : 32);
			break;
		// Refresh Rate
//		case 3:
//			Cvar_SetValue ("vid_refreshrate", vid_refreshrate.value + dir);
//			break;
		case 4:
			Cvar_SetValue ("vid_fullscreen", !vid_fullscreen.value);
			break;

		case 5:
			if (gl_videosyncavailable)
				Cvar_SetValue ("vid_vsync", !vid_vsync.value);
			break;
	}
}

void VID_MenuKey (int key)
{
	switch (key)
	{
		case K_ESCAPE:
			S_LocalSound ("sound/misc/menu1.wav");
			M_Menu_Video_f ();
			break;

		case K_ENTER:
			switch (video_modes_cursor)
			{
				case 6:
					Cvar_SetValue ("vid_width", video_resolutions[video_resolution].width);
					Cvar_SetValue ("vid_height", video_resolutions[video_resolution].height);
//					Cvar_SetValue ("vid_conwidth", video_resolutions[video_resolution].conwidth);
//					Cvar_SetValue ("vid_conheight", video_resolutions[video_resolution].conheight);
					Cvar_SetValue ("vid_pixelheight", video_resolutions[video_resolution].pixelheight);
					Cbuf_AddText ("vid_restart\n");
					M_Menu_Video_f ();
					video_modes_cursor=0;
					break;
				default:
					M_Menu_Modes_AdjustSliders (1);
			}
			break;

		case K_UPARROW:
			S_LocalSound ("sound/misc/menu1.wav");
			video_modes_cursor--;
			if (video_modes_cursor < 0)
				video_modes_cursor = VIDEO_ITEMS-1;
			break;

		case K_DOWNARROW:
			S_LocalSound ("sound/misc/menu1.wav");
			video_modes_cursor++;
			if (video_modes_cursor >= VIDEO_ITEMS)
				video_modes_cursor = 0;
			break;

		case K_LEFTARROW:
			M_Menu_Modes_AdjustSliders (-1);
			break;

		case K_RIGHTARROW:
			M_Menu_Modes_AdjustSliders (1);
			break;
	}
}
