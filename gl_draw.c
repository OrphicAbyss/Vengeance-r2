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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer


#include "quakedef.h"

#define GL_COLOR_INDEX8_EXT     0x80E5

extern unsigned char d_15to8table[65536];
//qmb :detail texture
extern int	detailtexture;
extern int	detailtexture2;
extern int	quadtexture;
byte *jpeg_rgba;

 
char *shaderScript = NULL;

cvar_t		gl_nobind = {"gl_nobind", "0"};
cvar_t		gl_max_size = {"gl_max_size", "4096"};
cvar_t		gl_quick_texture_reload = {"gl_quick_texture_reload", "1", true};

cvar_t		con_clock = {"con_clock", "1", true};

byte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

int			translate_texture;
int			char_texture;

//texture script stuff
int			texturescript = true;

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

byte		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t		*conback = (qpic_t *)&conback_buffer;

int		gl_lightmap_format = GL_RGBA;//GL_COMPRESSED_RGBA_ARB;//4;
int		gl_solid_format = GL_RGB;//GL_COMPRESSED_RGB_ARB;//3;
int		gl_alpha_format = GL_RGBA;//GL_COMPRESSED_RGBA_ARB;//4;

int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int		gl_filter_max = GL_LINEAR;

typedef struct
{
	int		texnum;
	char	identifier[64];
	int		width, height;
	qboolean	mipmap;

// Tomaz || TGA Begin
	int			bytesperpixel;
	int			lhcsum;
// Tomaz || TGA End
} gltexture_t;

#define	MAX_GLTEXTURES	4096 // Entar : was 1024 - edited
gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

int GL_LoadTexImage (char* filename, qboolean complain, qboolean mipmap);

// Tomaz || TGA Begin
int		image_width;
int		image_height;
// Tomaz end

extern int multitex_go;

void GL_Bind (int texnum)
{
	if (gl_nobind.value)
		texnum = char_texture;
	glBindTexture(GL_TEXTURE_2D, texnum);
}


//************************Texture script stuff********************************
/** 
 *  Generate a default script file (mostly as example) 
 *  If you don't want any texture filtering, 
 *  just replace the generated one with an empty file 
 **/ 
void   createDefaultShaderScript (void) 
{
   vfsfile_t   *fout; 
   char   buf[256]; 
   char *temp;

// sprintf (buf,"%s",SCRIPTFILENAME); // 
   strcpy (buf, SCRIPTFILENAME);
   fout = FS_OpenVFS(buf, "wb", FS_GAME);
   if (fout != NULL) 
   { 
      // change these textures patterns to fit your preference 
		temp = va("%s=metal*;\n",SK_SHINYMETAL);
		VFS_WRITE(fout, temp, strlen(temp));
//      fflush (fout); 
//      fclose(fout); 
		VFS_CLOSE(fout);
   }
} 

/** 
 * Try to load shader script 
 **/ 
void   loadShaderScript(char *filename) 
{ 
   vfsfile_t   *fin; 
   int      len; 

   fin = FS_OpenVFS(filename, "rb", FS_GAME);
   if (fin == NULL) 
   { 
      createDefaultShaderScript();
	  fin = FS_OpenVFS(filename, "rb", FS_GAME);
      if (fin == NULL) 
      { 
         Con_Printf ("loadShaderScript: failed to open \"%s\"\n", filename); 
         return; 
      } 
   } 

/* fseek (fin, 0, SEEK_SET); 
   fseek (fin, 0, SEEK_END); 
   len = ftell (fin); 
   fseek (fin, 0, SEEK_SET);*/
   len = VFS_GETLEN(fin);

   // unlikely, but... 
   if (shaderScript) 
   { 
      free (shaderScript); 
   } 

   shaderScript = malloc (len); 
// fread (shaderScript, len, 1, fin); 
// fclose (fin);
   VFS_READ (fin, shaderScript, len);
   VFS_CLOSE (fin);
} 

/** 
 * Checks if a value is binded 
 * to a key 
 **/ 
qboolean checkValue (char *key, char *value) 
{ 
   qboolean   result = false; 
   char      buf[64]; 
   char      *pKey, *pValue,*pBuf; 

   if (shaderScript != NULL) 
   { 
      if ((pKey = strstr(shaderScript, key)) != NULL) 
      { 
         pValue = pKey + strlen(key) + 1; 
         memset (buf, '\0', 64); 
         pBuf = buf; 
         while ((*pValue != ';') && (*pValue != '\0')) 
         { 
            if ((*pValue == ',') || (*pValue == '*')) 
            { 
               if (pBuf != buf) 
               { 
                  if (strstr (value, buf) != NULL) 
                  { 
                     result = true; 
                     break; 
                  } 

                  memset (buf, '\0', 64); 
                  pBuf = buf; 
               } 
            } 
            else 
            { 
               // check for buffer overflow 
               if (strlen(pBuf) < 63) 
               { 
                  pBuf[0] = pValue[0]; 
                  pBuf++; 
               } 
            } 

            pValue++; 
         } 
      } 
   } 

   return (result); 
} 
//************************End Texture Script********************************

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

int		pic_texels;
int		pic_count;

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;
	int texnum;
	char texname[128];

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	sprintf(texname,"textures/wad/%s", name);
	if (texnum = GL_LoadTexImage(&texname[0], false, false)){
		p->height = image_height;
		p->width = image_width;
		gl->texnum = texnum;
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
		return p;
	}

	sprintf(texname,"gfx/%s", name);
	if (texnum = GL_LoadTexImage(&texname[0], false, false)){
		p->height = image_height;
		p->width = image_width;
		gl->texnum = texnum;
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
		return p;
	}

	gl->texnum = GL_LoadTexture ("", p->width, p->height, p->data, false, true, 1);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return p;
}

qpic_t *Draw_PicFromWadXY (char *name, int height, int width)
{
	qpic_t	*p;
	glpic_t	*gl;
	int texnum;
	char texname[128];

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	sprintf(texname,"textures/wad/%s", name);
	if (texnum = GL_LoadTexImage(&texname[0], false, false)){
		p->height = height;
		p->width = width;
		gl->texnum = texnum;
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
		return p;
	}

	sprintf(texname,"gfx/%s", name);
	if (texnum = GL_LoadTexImage(&texname[0], false, false)){
		p->height = height;
		p->width = width;
		gl->texnum = texnum;
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
		return p;
	}

	gl->texnum = GL_LoadTexture ("", p->width, p->height, p->data, false, true, 1);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return p;
}
/*
qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	gl->texnum = GL_LoadTexture ("", p->width, p->height, p->data, false, true, 1);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return p;
}*/


qpic_t	*Draw_TryCachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy (pic->name, path);

//
// load the pic from disk
//
	dat = (qpic_t *)COM_LoadTempFile (path);	
	if (!dat)
		return NULL;
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	gl->texnum = GL_LoadTexture ("", dat->width, dat->height, dat->data, false, true, 1);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}

/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
	qpic_t		*dat;

	dat = Draw_TryCachePic	(path);
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	return dat;
}

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
void Draw_TextureMode_f (void)
{
	int		i;
	gltexture_t	*glt;
	extern	void	Host_InitVideo();

	// if the video isn't initialized already, it needs to be
	Host_InitVideo();

	if (Cmd_Argc() == 1)
	{
		for (i=0 ; i< 6 ; i++)
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf ("%s\n", modes[i].name);
				return;
			}
		Con_Printf ("current filter is unknown???\n");
		return;
	}

	for (i=0 ; i< 6 ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	}
	if (i == 6)
	{
		Con_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->mipmap)
		{
			glBindTexture(GL_TEXTURE_2D,glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
	extern	void fractalnoisequick(qbyte *noise, int size, int startgrid);
	int		i;
	qpic_t	*cb;
	glpic_t	*gl;
	int		start;
	byte	*ncdata;
	byte	detailtex[256][256];

	// 3dfx can only handle 256 wide textures
	if (!Q_strncasecmp ((char *)gl_renderer, "3dfx",4) || strstr((char *)gl_renderer, "Glide")){
		Cvar_Set ("gl_max_size", "256");
		Con_Printf("Setting max texture size to 256x256 since 3dfx cards can't handle bigger ones");
	}

	texture_extension_number = 1;

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	//TGA: begin
	char_texture = GL_LoadTexImage ("gfx/charset", false, true);
	if (char_texture == 0)// did not find a matching TGA...
	{
		draw_chars = W_GetLumpName ("conchars");
		for (i=0 ; i<256*64 ; i++)
			if (draw_chars[i] == 0)
				draw_chars[i] = 255;	// proper transparent color

		// now turn them into textures

		char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, 1);
	}
	//TGA: end

	gl = (glpic_t *)conback->data;
	//TGA: begin
	gl->texnum = GL_LoadTexImage ("gfx/conback", false, true);
	if (gl->texnum == 0)// did not find a matching TGA...
	{
		start = Hunk_LowMark();

		cb = (qpic_t *)COM_LoadTempFile ("gfx/conback.lmp");	
		if (!cb)
			Sys_Error ("Couldn't load gfx/conback.lmp");
		SwapPic (cb);

		conback->width = cb->width;
		conback->height = cb->height;
		ncdata = cb->data;

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		gl->texnum = GL_LoadTexture ("gfx/conback", conback->width, conback->height, ncdata, false, false, 1);

		// free loaded console
		Hunk_FreeToLowMark(start);
	}
	//TGA: end
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	conback->width = vid.width;
	conback->height = vid.height;

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWadXY ("disc",48,48);
	draw_backtile = Draw_PicFromWad ("backtile");
	//qmb :detail texture
	detailtexture = GL_LoadTexImage("textures/detail", false, true);
	if (!detailtexture)
	{
/*		for (x=0 ; x<32 ; x++)
		{
			for (y=0 ; y<32 ; y++)
			{
				data[x][y][0]	= 255;
				data[x][y][1]	= 255;
				data[x][y][2]	= 255;
				data[x][y][3]	= detailtexture[x][y];
			}
		}*/
		fractalnoisequick(&detailtex[0][0], 256, 256 >> 4);
		detailtexture = GL_LoadTexture ("detailtex", 32, 32, &detailtex[0][0], true, true, 4);
	}
	detailtexture2 = GL_LoadTexImage("textures/detail2", false, true);
	if (!detailtexture2)
	{
/*		for (x=0 ; x<32 ; x++)
		{
			for (y=0 ; y<32 ; y++)
			{
				data[x][y][0]	= 255;
				data[x][y][1]	= 255;
				data[x][y][2]	= 255;
				data[x][y][3]	= detailtexture[x][y];
			}
		}*/
		fractalnoisequick(&detailtex[0][0], 256, 256 >> 8);
		detailtexture2 = GL_LoadTexture ("detailtex", 32, 32, &detailtex[0][0], true, true, 4);
	}
	quadtexture = GL_LoadTexImage("textures/quad", false, true);
}

void Draw_Init_Register(void)
{
	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_nobind);
	Cvar_RegisterVariable (&gl_quick_texture_reload);

	Cvar_RegisterVariable (&con_clock);
	
	Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f);
}

/*
===============
Draw_Shutdown
===============
*/
extern unsigned int dst_texture;
extern	int			lightmap_textures;
void Draw_Shutdown(void)
{
	menu_numcachepics = 0;
	numgltextures = 0;

	char_texture = 0;
	translate_texture = 0;
	detailtexture = 0;
	detailtexture2 = 0;
	quadtexture = 0;


	lightmap_textures = 0;
	dst_texture = 0;
}


/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, int num)
{
	int				row, col;
	float			frow, fcol, size;

	if (num == 32)
		return;		// space

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	//qmb :larger text??
	frow = row*0.0625f;
	fcol = col*0.0625f;
	size = 0.0625f;

	glBindTexture(GL_TEXTURE_2D,char_texture);

	glBegin (GL_QUADS);
		glTexCoord2f (fcol, frow);
		glVertex2f (x, y);
		glTexCoord2f (fcol + size, frow);
		glVertex2f (x+8, y);
		glTexCoord2f (fcol + size, frow + size);
		glVertex2f (x+8, y+8);
		glTexCoord2f (fcol, frow + size);
		glVertex2f (x, y+8);
	glEnd ();
}

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
	while (*str)
	{
		Draw_Character (x, y, *str++);
		x += 8;
	}
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (char num)
{
}

/*
=============
Draw_AlphaColourPic
=============
*/
void Draw_AlphaColourPic (int x, int y, qpic_t *pic, vec3_t colour, float alpha)
{
	glpic_t			*gl;

	gl = (glpic_t *)pic->data;

	if (alpha!=1){
		glDisable(GL_ALPHA_TEST);
		glEnable (GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	glColor4f (colour[0],colour[1],colour[2],alpha);
	glBindTexture(GL_TEXTURE_2D,gl->texnum);
	glBegin (GL_QUADS);
		glTexCoord2f (gl->sl, gl->tl);		glVertex2f (x, y);
		glTexCoord2f (gl->sh, gl->tl);		glVertex2f (x+pic->width, y);
		glTexCoord2f (gl->sh, gl->th);		glVertex2f (x+pic->width, y+pic->height);
		glTexCoord2f (gl->sl, gl->th);		glVertex2f (x, y+pic->height);
	glEnd ();
	glColor4f (1,1,1,1);

	if (alpha!=1){
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_ALPHA_TEST);
		glDisable (GL_BLEND);
	}
}


/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	glpic_t			*gl;

	gl = (glpic_t *)pic->data;

	if (alpha!=1){
		glDisable(GL_ALPHA_TEST);
		glEnable (GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	glColor4f (1,1,1,alpha);
	glBindTexture(GL_TEXTURE_2D,gl->texnum);
	glBegin (GL_QUADS);
		glTexCoord2f (gl->sl, gl->tl);		glVertex2f (x, y);
		glTexCoord2f (gl->sh, gl->tl);		glVertex2f (x+pic->width, y);
		glTexCoord2f (gl->sh, gl->th);		glVertex2f (x+pic->width, y+pic->height);
		glTexCoord2f (gl->sl, gl->th);		glVertex2f (x, y+pic->height);
	glEnd ();
	glColor4f (1,1,1,1);

	if (alpha!=1){
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_ALPHA_TEST);
		glDisable (GL_BLEND);
	}
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		 (unsigned)(y + pic->height) > vid.height)
	{
		//Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	Draw_AlphaPic (x, y, pic, 1);
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	byte			*src;
	int				p;

	glBindTexture(GL_TEXTURE_2D,translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &menuplyr_pixels[ ((v*pic->height)>>6) *pic->width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*pic->width)>>6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] =  d_8to24table[translation[p]];
		}
	}

	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glColor3f (1,1,1);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);	glVertex2f (x, y);
	glTexCoord2f (1, 0);	glVertex2f (x+pic->width, y);
	glTexCoord2f (1, 1);	glVertex2f (x+pic->width, y+pic->height);
	glTexCoord2f (0, 1);	glVertex2f (x, y+pic->height);
	glEnd ();
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	extern void Sys_Strtime(char *buf);
	int y = (vid.height * 3) >> 2;
	int x, i; 

	char tl[80]; //Console Clock - Eradicator
	char timebuf[20];

	if (lines > y)
		Draw_AlphaPic(0, lines - vid.height, conback, 1);
	else
		Draw_AlphaPic (0, lines - vid.height, conback, (float)(gl_conalpha.value * lines)/y);

	if ( con_clock.value )
	{
		Sys_Strtime( timebuf );
		y = lines-14; 
		sprintf (tl, "Time: %s",timebuf); //Console Clock - Eradicator
		x = vid.realwidth - (vid.realwidth*12/vid.width*12) + 30; 
		for (i=0 ; i < strlen(tl) ; i++) 
		   Draw_Character (x + i * 8, y, tl[i] | 0x80);
	}
}

void Draw_SpiralConsoleBackground (int lines) //Spiral Console - Eradicator
{ 
   int x, i; 
   int y; 
   static float xangle = 0, xfactor = .3f, xstep = .01f; 
   
   char tl[80]; //Console Clock - Eradicator
   char timebuf[20];
   Sys_Strtime( timebuf );


   glPushMatrix(); 
   glMatrixMode(GL_TEXTURE); 
   glPushMatrix(); 
   glLoadIdentity(); 
   xangle += 1.0f; 
   xfactor += xstep; 
   if (xfactor > 8 || xfactor < .3f) 
      xstep = -xstep; 
   glRotatef(xangle, 0, 0, 1); 
   glScalef(xfactor, xfactor, xfactor); 
   y = (vid.height * 3) >> 2;  
	if (lines > y) 
		Draw_AlphaPic(0, lines-vid.height, conback, 1); 
//      Draw_Pic(0, lines-vid.height, conback); 
   else 
      Draw_AlphaPic (0, lines - vid.height, conback, (float)(1.2 * lines)/y); 
   glPopMatrix(); 
   glMatrixMode(GL_MODELVIEW); 
   glPopMatrix(); 

   	if ( con_clock.value )
	{
		y = lines-14; 
		sprintf (tl, "Time: %s",timebuf); //Console Clock - Eradicator
		x = vid.realwidth - (vid.realwidth*12/vid.width*12) + 30; 
		for (i=0 ; i < strlen(tl) ; i++) 
			Draw_Character (x + i * 8, y, tl[i] | 0x80);
	}
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
	glColor4f (0,0.5f,0.5f,0.5f);
	glBindTexture(GL_TEXTURE_2D,*(int *)draw_backtile->data);
	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND);
	glBegin (GL_QUADS);
	//glTexCoord2f (x/64.0, y/64.0);
	glVertex2f (x, y);
	//glTexCoord2f ( (x+w)/64.0, y/64.0);
	glVertex2f (x+w, y);
	//glTexCoord2f ( (x+w)/64.0, (y+h)/64.0);
	glVertex2f (x+w, y+h);
	//glTexCoord2f ( x/64.0, (y+h)/64.0 );
	glVertex2f (x, y+h);
	glEnd ();
	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	glDisable (GL_TEXTURE_2D);
	glColor3f (host_basepal[c*3]/255.0,
		host_basepal[c*3+1]/255.0,
		host_basepal[c*3+2]/255.0);

	glBegin (GL_QUADS);

	glVertex2f (x,y);
	glVertex2f (x+w, y);
	glVertex2f (x+w, y+h);
	glVertex2f (x, y+h);

	glEnd ();
	glColor3f (1,1,1);
	glEnable (GL_TEXTURE_2D);
}

/*
=============
Draw_AlphaFill

JHL: Fills a box of pixels with a single transparent color
=============
*/
void Draw_AlphaFill (int x, int y, int width, int height, vec3_t colour, float alpha)
{
	glEnable (GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable (GL_ALPHA_TEST);
	glDisable (GL_TEXTURE_2D);
	glColor4f (colour[0]/255, colour[1]/255, colour[2]/255, alpha/255);
	glBegin (GL_QUADS);

	glVertex2f (x,y);
	glVertex2f (x+width, y);
	glVertex2f (x+width, y+height);
	glVertex2f (x, y+height);

	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_ALPHA_TEST);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
}

/*
=============
Draw_AlphaFillFade

JHL: Fills a box of pixels with a single transparent color
=============
*/
void Draw_AlphaFillFade (int x, int y, int width, int height, vec3_t colour, float alpha[2])
{
	glEnable (GL_BLEND);
	glDisable (GL_TEXTURE_2D);
	glDisable (GL_ALPHA_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBegin (GL_QUADS);

	glColor4f (colour[0]/255,colour[1]/255,colour[2]/255,alpha[1]/255);
	glVertex2f (x,y);
	glVertex2f (x, y+height);
	glColor4f (colour[0]/255,colour[1]/255,colour[2]/255,alpha[0]/255);
	glVertex2f (x+width, y+height);
	glVertex2f (x+width, y);

	glEnd ();

	glColor4f (1,1,1,1);

	glEnable (GL_ALPHA_TEST);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
}

/*
===============
Draw_Crosshair

function that draws the crosshair to the center of the screen
===============
*
void Draw_Crosshair (int texnum, vec3_t colour, float alpha)
{
	int		x	= 0;
	int 	y	= 0;
	float	xsize,ysize;
	vec3_t	v1, v2, end, normal, right, up;
 
	// trace the shot path up to a certain distance
	VectorCopy(cl_entities[cl.viewentity].origin, v1);
	v1[2] += 16; // HACK: this depends on the QC
 
	// get the forward vector for the gun (not the view)
	AngleVectors(cl.viewangles, v2, right, up);
	//VectorCopy(r_vieworigin, v1);
	VectorMA(v1, 8192, v2, v2);
	if (cl.worldmodel)
		TraceLineN(v1, v2, end, normal, 0);
 
	//
	// Default for if it isn't set...
	//
	xsize = 32;
	ysize = 32;
 
	//
	// Crosshair offset
	//
//	x = (vid.width /2) - 16; // was 14
//	y = (vid.height/2) - 8;  // was 14
 
	x = (vid.width /2) - 16; // was 14
	y = (vid.height/2) - 8 - (r_refdef.vieworg[2] - end[2]);  // was 14
 
//	glTranslatef(end[0], end[1], end[2]);
 
	//
	// Start drawing
	//
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glColor4f(colour[0],colour[1],colour[2],alpha);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture (GL_TEXTURE_2D, texnum);
 
/*	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);	glVertex2f (x, y);
	glTexCoord2f (1, 0);	glVertex2f (x+xsize, y);
	glTexCoord2f (1, 1);	glVertex2f (x+xsize, y+ysize);
	glTexCoord2f (0, 1);	glVertex2f (x, y+ysize);
	glEnd ();*

	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);	glVertex3f (end[0], end[1], end[2]);
	glTexCoord2f (1, 0);	glVertex3f (end[0]+xsize, end[1], end[2]);
	glTexCoord2f (1, 1);	glVertex3f (end[0]+xsize, end[1]+ysize, end[2]);
	glTexCoord2f (0, 1);	glVertex3f (end[0], end[1]+ysize, end[2]);
	glEnd ();

/*	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);	glVertex2f (-16, -8);
	glTexCoord2f (1, 0);	glVertex2f (16, -8);
	glTexCoord2f (1, 1);	glVertex2f (16, 24);
	glTexCoord2f (0, 1);	glVertex2f (-16, 24);
	glEnd ();*
 
	// restore display settings
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glColor4f(1,1,1,1);
	glEnable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
}*/

void Draw_Crosshair (int texnum, vec3_t colour, float alpha) // Entar : HACKY
{
	extern	void ML_Project (vec3_t in, vec3_t out, vec3_t viewangles, vec3_t vieworg, float wdivh, float fovy);
	int		x = 0, x1, x2;
	int 	y = 0, y1, y2;
	float	plus;
	int doit;

	extern cvar_t	crosshair_static, cl_crossx, cl_crossy;
	extern vrect_t	scr_vrect;

	if (r_letterbox.value)
		return;

	if (crosshair_static.value || scr_viewsize.value < 100 || !cl.worldmodel)
		doit = false;
	else
		doit = true;

	if (doit)
	{
		float adj;
		trace_t tr;
		vec3_t end;
		vec3_t start;
		vec3_t right, up, fwds;

//		AngleVectors(r_refdef.viewangles, fwds, right, up);
		AngleVectors(cl.viewangles, fwds, right, up);

		//VectorCopy(r_refdef.vieworg, start);
		VectorCopy(cl_entities[cl.viewentity].origin, start);
		start[2]+=16;
		VectorMA(start, 4096, fwds, end); // tweaked from 100000 to 8192 to 4096

		memset(&tr, 0, sizeof(tr));
		tr.fraction = 1;
		//Trace(cl.worldmodel, 0, 0, start, end, vec3_origin, vec3_origin, &tr);
		SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, start, end, &tr);
		start[2]-=16;
		if (tr.fraction == 1)
		{
			x = scr_vrect.x + scr_vrect.width/2 + cl_crossx.value; 
			y = scr_vrect.y + scr_vrect.height/2 + cl_crossy.value;
		}
		else
		{
			adj=cl.viewheight + 4;

			start[2]+=adj;
			ML_Project(tr.endpos, end, r_refdef.viewangles, start, (float)scr_vrect.width/scr_vrect.height, r_refdef.fov_y);
//			x = scr_vrect.x+scr_vrect.width*end[0];
			x = (vid.width /2);
			y = (scr_vrect.y+scr_vrect.height*(end[1]));
			y = scr_vrect.height - y; // Entar : HACKYNESS yes, but it works pretty well
//			Con_Printf("1: %i\n", y);
			y -= (y - (scr_vrect.height / 2)) / 2;//
//			y -= (y - (scr_vrect.height / 2)) / 2;//
//			Con_Printf("2: %i\n", y);
		}
	}
	else
	{
		x = (vid.width /2) - 16; // was 14
		y = (vid.height/2) - 8;  // was 14
	}

	plus = sin(cl.time*0.6f);
	if (plus < 0)
		plus = 0;

	//
	// Start drawing
	//
	if (texnum != 0)
	{
		glEnable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glDepthMask(GL_FALSE);
		glColor4f((colour[0] / 400) + plus,(colour[1] / 400) + plus,(colour[2] / 400) + plus,alpha);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBindTexture (GL_TEXTURE_2D, texnum);
 
		if (!doit)
		{
			glBegin (GL_QUADS);
			glTexCoord2f (0, 0);	glVertex2f (x, y);
			glTexCoord2f (1, 0);	glVertex2f (x+32, y);
			glTexCoord2f (1, 1);	glVertex2f (x+32, y+32);
			glTexCoord2f (0, 1);	glVertex2f (x, y+32);
			glEnd ();
		}
		else
		{
			x1 = x - 16;
			x2 = x + 16;
			y1 = y - 16;
			y2 = y + 16;
			glBegin (GL_QUADS);
			glTexCoord2f (0, 0);
			glVertex2f (x1, y1);
			glTexCoord2f (1, 0);
			glVertex2f (x2, y1);
			glTexCoord2f (1, 1);
			glVertex2f (x2, y2);
			glTexCoord2f (0, 1);
			glVertex2f (x1, y2);
			glEnd ();
		}
		// restore display settings
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor4f(1,1,1,1);
		glEnable(GL_ALPHA_TEST);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}
	else
	{
		x1 = x - 4;
		y1 = y - 4;
		Draw_Character (x1, y1, '+');
	}
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	vec3_t	colour;
	float	alpha[2];
	//JHL; changed...
	int	start_x, start_y,
		end_x,	 end_y;

	start_x = (vid.width-340)/2; // 320 -> 340
	start_y = (vid.height-240)/2;
	end_x = (vid.width+340)/2; // 320 -> 340
	end_y = (vid.height+340)/2; // Entar : made it longer (240 -> 340)
	colour[0]=colour[1]=colour[2]=0;
	alpha[0]=0;
	alpha[1]=178;

	Draw_AlphaFill(start_x, start_y, end_x - start_x, end_y - start_y, colour, alpha[1]);
	Draw_AlphaFillFade(start_x, start_y, -10, end_y - start_y, colour, alpha);
	Draw_AlphaFillFade(end_x, start_y, 10, end_y - start_y, colour, alpha);
}

//=============================================================================

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);
//	glDisable (GL_ALPHA_TEST);

	glColor4f (1,1,1,1);
}

//====================================================================

void Image_Resample32LerpLine (const byte *in, byte *out, int inwidth, int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, endx, lerp;
	fstep = (int) (inwidth*65536.0f/outwidth);
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep)
	{
		xi = f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 4;
			oldx = xi;
		}
		if (xi < endx)
		{
			lerp = f & 0xFFFF;
			*out++ = (byte) ((((in[4] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (byte) ((((in[5] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (byte) ((((in[6] - in[2]) * lerp) >> 16) + in[2]);
			*out++ = (byte) ((((in[7] - in[3]) * lerp) >> 16) + in[3]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
			*out++ = in[3];
		}
	}
}

void Image_Resample24LerpLine (const byte *in, byte *out, int inwidth, int outwidth)
{
	int		j, xi, oldx = 0, f, fstep, endx, lerp;
	fstep = (int) (inwidth*65536.0f/outwidth);
	endx = (inwidth-1);
	for (j = 0,f = 0;j < outwidth;j++, f += fstep)
	{
		xi = f >> 16;
		if (xi != oldx)
		{
			in += (xi - oldx) * 3;
			oldx = xi;
		}
		if (xi < endx)
		{
			lerp = f & 0xFFFF;
			*out++ = (byte) ((((in[3] - in[0]) * lerp) >> 16) + in[0]);
			*out++ = (byte) ((((in[4] - in[1]) * lerp) >> 16) + in[1]);
			*out++ = (byte) ((((in[5] - in[2]) * lerp) >> 16) + in[2]);
		}
		else // last pixel of the line has no pixel to lerp to
		{
			*out++ = in[0];
			*out++ = in[1];
			*out++ = in[2];
		}
	}
}

int resamplerowsize = 0;
byte *resamplerow1 = NULL;
byte *resamplerow2 = NULL;

#define LERPBYTE(i) r = resamplerow1[i];out[i] = (byte) ((((resamplerow2[i] - r) * lerp) >> 16) + r)
void Image_Resample32Lerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight-1), inwidth4 = inwidth*4, outwidth4 = outwidth*4;
	byte *out;
	const byte *inrow;
	out = outdata;
	fstep = (int) (inheight*65536.0f/outheight);

	inrow = indata;
	oldy = 0;
	Image_Resample32LerpLine (inrow, resamplerow1, inwidth, outwidth);
	Image_Resample32LerpLine (inrow + inwidth4, resamplerow2, inwidth, outwidth);
	for (i = 0, f = 0;i < outheight;i++,f += fstep)
	{
		yi = f >> 16;
		if (yi < endy)
		{
			lerp = f & 0xFFFF;
			if (yi != oldy)
			{
				inrow = (byte *)indata + inwidth4*yi;
				if (yi == oldy+1)
					memcpy(resamplerow1, resamplerow2, outwidth4);
				else
					Image_Resample32LerpLine (inrow, resamplerow1, inwidth, outwidth);
				Image_Resample32LerpLine (inrow + inwidth4, resamplerow2, inwidth, outwidth);
				oldy = yi;
			}
			j = outwidth - 4;
			while(j >= 0)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				LERPBYTE(12);
				LERPBYTE(13);
				LERPBYTE(14);
				LERPBYTE(15);
				out += 16;
				resamplerow1 += 16;
				resamplerow2 += 16;
				j -= 4;
			}
			if (j & 2)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				out += 8;
				resamplerow1 += 8;
				resamplerow2 += 8;
			}
			if (j & 1)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				out += 4;
				resamplerow1 += 4;
				resamplerow2 += 4;
			}
			resamplerow1 -= outwidth4;
			resamplerow2 -= outwidth4;
		}
		else
		{
			if (yi != oldy)
			{
				inrow = (byte *)indata + inwidth4*yi;
				if (yi == oldy+1)
					memcpy(resamplerow1, resamplerow2, outwidth4);
				else
					Image_Resample32LerpLine (inrow, resamplerow1, inwidth, outwidth);
				oldy = yi;
			}
			memcpy(out, resamplerow1, outwidth4);
		}
	}
}

void Image_Resample32Nearest(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int i, j;
	unsigned frac, fracstep;
	// relies on int being 4 bytes
	int *inrow, *out;
	out = outdata;

	fracstep = inwidth*0x10000/outwidth;
	for (i = 0;i < outheight;i++)
	{
		inrow = (int *)indata + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		j = outwidth - 4;
		while (j >= 0)
		{
			out[0] = inrow[frac >> 16];frac += fracstep;
			out[1] = inrow[frac >> 16];frac += fracstep;
			out[2] = inrow[frac >> 16];frac += fracstep;
			out[3] = inrow[frac >> 16];frac += fracstep;
			out += 4;
			j -= 4;
		}
		if (j & 2)
		{
			out[0] = inrow[frac >> 16];frac += fracstep;
			out[1] = inrow[frac >> 16];frac += fracstep;
			out += 2;
		}
		if (j & 1)
		{
			out[0] = inrow[frac >> 16];frac += fracstep;
			out += 1;
		}
	}
}

void Image_Resample24Lerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int i, j, r, yi, oldy, f, fstep, lerp, endy = (inheight-1), inwidth3 = inwidth * 3, outwidth3 = outwidth * 3;
	byte *out;
	const byte *inrow;
	out = outdata;
	fstep = (int) (inheight*65536.0f/outheight);

	inrow = indata;
	oldy = 0;
	Image_Resample24LerpLine (inrow, resamplerow1, inwidth, outwidth);
	Image_Resample24LerpLine (inrow + inwidth3, resamplerow2, inwidth, outwidth);
	for (i = 0, f = 0;i < outheight;i++,f += fstep)
	{
		yi = f >> 16;
		if (yi < endy)
		{
			lerp = f & 0xFFFF;
			if (yi != oldy)
			{
				inrow = (byte *)indata + inwidth3*yi;
				if (yi == oldy+1)
					memcpy(resamplerow1, resamplerow2, outwidth3);
				else
					Image_Resample24LerpLine (inrow, resamplerow1, inwidth, outwidth);
				Image_Resample24LerpLine (inrow + inwidth3, resamplerow2, inwidth, outwidth);
				oldy = yi;
			}
			j = outwidth - 4;
			while(j >= 0)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				LERPBYTE( 6);
				LERPBYTE( 7);
				LERPBYTE( 8);
				LERPBYTE( 9);
				LERPBYTE(10);
				LERPBYTE(11);
				out += 12;
				resamplerow1 += 12;
				resamplerow2 += 12;
				j -= 4;
			}
			if (j & 2)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				LERPBYTE( 3);
				LERPBYTE( 4);
				LERPBYTE( 5);
				out += 6;
				resamplerow1 += 6;
				resamplerow2 += 6;
			}
			if (j & 1)
			{
				LERPBYTE( 0);
				LERPBYTE( 1);
				LERPBYTE( 2);
				out += 3;
				resamplerow1 += 3;
				resamplerow2 += 3;
			}
			resamplerow1 -= outwidth3;
			resamplerow2 -= outwidth3;
		}
		else
		{
			if (yi != oldy)
			{
				inrow = (byte *)indata + inwidth3*yi;
				if (yi == oldy+1)
					memcpy(resamplerow1, resamplerow2, outwidth3);
				else
					Image_Resample24LerpLine (inrow, resamplerow1, inwidth, outwidth);
				oldy = yi;
			}
			memcpy(out, resamplerow1, outwidth3);
		}
	}
}

void Image_Resample24Nolerp(const void *indata, int inwidth, int inheight, void *outdata, int outwidth, int outheight)
{
	int i, j, f, inwidth3 = inwidth * 3;
	unsigned frac, fracstep;
	byte *inrow, *out;
	out = outdata;

	fracstep = inwidth*0x10000/outwidth;
	for (i = 0;i < outheight;i++)
	{
		inrow = (byte *)indata + inwidth3*(i*inheight/outheight);
		frac = fracstep >> 1;
		j = outwidth - 4;
		while (j >= 0)
		{
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			j -= 4;
		}
		if (j & 2)
		{
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			out += 2;
		}
		if (j & 1)
		{
			f = (frac >> 16)*3;*out++ = inrow[f+0];*out++ = inrow[f+1];*out++ = inrow[f+2];frac += fracstep;
			out += 1;
		}
	}
}

/*
================
Image_Resample
================
*/
void Image_Resample (const void *indata, int inwidth, int inheight, int indepth, void *outdata, int outwidth, int outheight, int outdepth, int bytesperpixel, int quality)
{
	if (indepth != 1 || outdepth != 1)
		Sys_Error("Image_Resample: 3D resampling not supported\n");
	if (resamplerowsize < outwidth*4)
	{
		if (resamplerow1)
			free(resamplerow1);
		resamplerowsize = outwidth*4;
		resamplerow1 = malloc(resamplerowsize*2);
		resamplerow2 = resamplerow1 + resamplerowsize;
	}
	if (bytesperpixel == 4)
	{
		if (quality)
			Image_Resample32Lerp(indata, inwidth, inheight, outdata, outwidth, outheight);
		else
			Image_Resample32Nearest(indata, inwidth, inheight, outdata, outwidth, outheight);
	}
	else if (bytesperpixel == 3)
	{
		if (quality)
			Image_Resample24Lerp(indata, inwidth, inheight, outdata, outwidth, outheight);
		else
			Image_Resample24Nolerp(indata, inwidth, inheight, outdata, outwidth, outheight);
	}
	else
		Sys_Error("Image_Resample: unsupported bytesperpixel %i\n", bytesperpixel);
}

// in can be the same as out
void Image_MipReduce(const byte *in, byte *out, int *width, int *height, int *depth, int destwidth, int destheight, int destdepth, int bytesperpixel)
{
	int x, y, nextrow;
	if (*depth != 1 || destdepth != 1)
		Sys_Error("Image_Resample: 3D resampling not supported\n");
	nextrow = *width * bytesperpixel;
	if (*width > destwidth)
	{
		*width >>= 1;
		if (*height > destheight)
		{
			// reduce both
			*height >>= 1;
			if (bytesperpixel == 4)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (byte) ((in[0] + in[4] + in[nextrow  ] + in[nextrow+4]) >> 2);
						out[1] = (byte) ((in[1] + in[5] + in[nextrow+1] + in[nextrow+5]) >> 2);
						out[2] = (byte) ((in[2] + in[6] + in[nextrow+2] + in[nextrow+6]) >> 2);
						out[3] = (byte) ((in[3] + in[7] + in[nextrow+3] + in[nextrow+7]) >> 2);
						out += 4;
						in += 8;
					}
					in += nextrow; // skip a line
				}
			}
			else if (bytesperpixel == 3)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (byte) ((in[0] + in[3] + in[nextrow  ] + in[nextrow+3]) >> 2);
						out[1] = (byte) ((in[1] + in[4] + in[nextrow+1] + in[nextrow+4]) >> 2);
						out[2] = (byte) ((in[2] + in[5] + in[nextrow+2] + in[nextrow+5]) >> 2);
						out += 3;
						in += 6;
					}
					in += nextrow; // skip a line
				}
			}
			else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		}
		else
		{
			// reduce width
			if (bytesperpixel == 4)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (byte) ((in[0] + in[4]) >> 1);
						out[1] = (byte) ((in[1] + in[5]) >> 1);
						out[2] = (byte) ((in[2] + in[6]) >> 1);
						out[3] = (byte) ((in[3] + in[7]) >> 1);
						out += 4;
						in += 8;
					}
				}
			}
			else if (bytesperpixel == 3)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (byte) ((in[0] + in[3]) >> 1);
						out[1] = (byte) ((in[1] + in[4]) >> 1);
						out[2] = (byte) ((in[2] + in[5]) >> 1);
						out += 3;
						in += 6;
					}
				}
			}
			else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		}
	}
	else
	{
		if (*height > destheight)
		{
			// reduce height
			*height >>= 1;
			if (bytesperpixel == 4)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (byte) ((in[0] + in[nextrow  ]) >> 1);
						out[1] = (byte) ((in[1] + in[nextrow+1]) >> 1);
						out[2] = (byte) ((in[2] + in[nextrow+2]) >> 1);
						out[3] = (byte) ((in[3] + in[nextrow+3]) >> 1);
						out += 4;
						in += 4;
					}
					in += nextrow; // skip a line
				}
			}
			else if (bytesperpixel == 3)
			{
				for (y = 0;y < *height;y++)
				{
					for (x = 0;x < *width;x++)
					{
						out[0] = (byte) ((in[0] + in[nextrow  ]) >> 1);
						out[1] = (byte) ((in[1] + in[nextrow+1]) >> 1);
						out[2] = (byte) ((in[2] + in[nextrow+2]) >> 1);
						out += 3;
						in += 3;
					}
					in += nextrow; // skip a line
				}
			}
			else
				Sys_Error("Image_MipReduce: unsupported bytesperpixel %i\n", bytesperpixel);
		}
		else
			Sys_Error("Image_MipReduce: desired size already achieved\n");
	}
}

//*****************************

#define FILTER_SIZE 5 
#define BLUR_FILTER 0 
#define LIGHT_BLUR   1 
#define EDGE_FILTER 2 
#define EMBOSS_FILTER 3 

float FilterMatrix[][FILTER_SIZE][FILTER_SIZE] = 
{ 
   // regular blur 
   { 
      {0, 0, 0, 0, 0}, 
      {0, 1, 1, 1, 0}, 
      {0, 1, 1, 1, 0}, 
      {0, 1, 1, 1, 0}, 
      {0, 0, 0, 0, 0}, 
   }, 
   // light blur 
   { 
      {0, 0, 0, 0, 0}, 
      {0, 1, 1, 1, 0}, 
      {0, 1, 4, 1, 0}, 
      {0, 1, 1, 1, 0}, 
      {0, 0, 0, 0, 0}, 
   }, 
   // find edges 
   { 
      {0,  0,  0,  0, 0}, 
      {0, -1, -1, -1, 0}, 
      {0, -1,  8, -1, 0}, 
      {0, -1, -1, -1, 0}, 
      {0,  0,  0,  0, 0}, 
   }, 
   // emboss 
   { 
      {-1, -1, -1, -1, 0}, 
      {-1, -1, -1,  0, 1}, 
      {-1, -1,  0,  1, 1}, 
      {-1,  0,  1,  1, 1}, 
      { 0,  1,  1,  1, 1}, 
   } 
}; 


/* 
================== 
R_FilterTexture 

Applies a 5 x 5 filtering matrix to the texture, then runs it through a simulated OpenGL texture environment 
blend with the original data to derive a new texture.  Freaky, funky, and *f--king* *fantastic*.  You can do 
reasonable enough "fake bumpmapping" with this baby... 

Filtering algorithm from http://www.student.kuleuven.ac.be/~m0216922/CG/filtering.html 
All credit due 
================== 
*/ 
void R_FilterTexture (int filterindex, unsigned int *data, int width, int height, float factor, float bias, qboolean greyscale, GLenum GLBlendOperator) 
{ 
   int i; 
   int x; 
   int y; 
   int filterX; 
   int filterY; 
   unsigned int *temp; 

   // allocate a temp buffer 
   temp = malloc (width * height * 4); 

   for (x = 0; x < width; x++) 
   { 
      for (y = 0; y < height; y++) 
      { 
         float rgbFloat[3] = {0, 0, 0}; 

         for (filterX = 0; filterX < FILTER_SIZE; filterX++) 
         { 
            for (filterY = 0; filterY < FILTER_SIZE; filterY++) 
            { 
               int imageX = (x - (FILTER_SIZE / 2) + filterX + width) % width; 
               int imageY = (y - (FILTER_SIZE / 2) + filterY + height) % height; 

               // casting's a unary operation anyway, so the othermost set of brackets in the left part 
               // of the rvalue should not be necessary... but i'm paranoid when it comes to C... 
               rgbFloat[0] += ((float) ((byte *) &data[imageY * width + imageX])[0]) * FilterMatrix[filterindex][filterX][filterY]; 
               rgbFloat[1] += ((float) ((byte *) &data[imageY * width + imageX])[1]) * FilterMatrix[filterindex][filterX][filterY]; 
               rgbFloat[2] += ((float) ((byte *) &data[imageY * width + imageX])[2]) * FilterMatrix[filterindex][filterX][filterY]; 
            } 
         } 

         // multiply by factor, add bias, and clamp 
         for (i = 0; i < 3; i++) 
         { 
            rgbFloat[i] *= factor; 
            rgbFloat[i] += bias; 

            if (rgbFloat[i] < 0) rgbFloat[i] = 0; 
            if (rgbFloat[i] > 255) rgbFloat[i] = 255; 
         } 

         if (greyscale) 
         { 
            // NTSC greyscale conversion standard 
            float avg = (rgbFloat[0] * 30 + rgbFloat[1] * 59 + rgbFloat[2] * 11) / 100; 

            // divide by 255 so GL operations work as expected 
            rgbFloat[0] = avg / 255.0; 
            rgbFloat[1] = avg / 255.0; 
            rgbFloat[2] = avg / 255.0; 
         } 

         // write to temp - first, write data in (to get the alpha channel quickly and 
         // easily, which will be left well alone by this particular operation...!) 
         temp[y * width + x] = data[y * width + x]; 

         // now write in each element, applying the blend operator.  blend 
         // operators are based on standard OpenGL TexEnv modes, and the 
         // formulae are derived from the OpenGL specs (http://www.opengl.org). 
         for (i = 0; i < 3; i++) 
         { 
            // divide by 255 so GL operations work as expected 
            float TempTarget; 
            float SrcData = ((float) ((byte *) &data[y * width + x])[i]) / 255.0; 

            switch (GLBlendOperator) 
            { 
            case GL_ADD: 
               TempTarget = rgbFloat[i] + SrcData; 
               break; 

            case GL_BLEND: 
               // default is FUNC_ADD here 
               // CsS + CdD works out as Src * Dst * 2 
               TempTarget = rgbFloat[i] * SrcData * 2.0; 
               break; 

            case GL_DECAL: 
               // same as GL_REPLACE unless there's alpha, which we ignore for this 
            case GL_REPLACE: 
               TempTarget = rgbFloat[i]; 
               break; 

            case GL_ADD_SIGNED: 
               TempTarget = (rgbFloat[i] + SrcData) - 0.5; 
               break; 

            case GL_MODULATE: 
               // same as default 
            default: 
               TempTarget = rgbFloat[i] * SrcData; 
               break; 
            } 

            // multiply back by 255 to get the proper byte scale 
            TempTarget *= 255.0; 

            // bound the temp target again now, cos the operation may have thrown it out 
            if (TempTarget < 0) TempTarget = 0; 
            if (TempTarget > 255) TempTarget = 255; 

            // and copy it in 
            ((byte *) &temp[y * width + x])[i] = (byte) TempTarget; 
         } 
      } 
   } 

   // copy temp back to data 
   for (i = 0; i < (width * height); i++) 
   { 
      data[i] = temp[i]; 
   } 

   // release the temp buffer 
   free (temp); 
} 
//*****************************

//====================================================================
/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (!strcmp (identifier, glt->identifier))
			return gltextures[i].texnum;
	}

	return -1;
}

/*
===============
GL_Upload32

first converts strange sized textures
to ones in the form 2^n*2^m where n is the height and m is the width
===============
*/

static unsigned char tobig[] = {255,0,0,0,255,0,0,0,255,255,255,255};

void GL_Upload32 ( GLenum texTarget, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	static byte	temp_buffer[512*512*4];
	int			samples;
	byte		*scaled;
	int			scaled_width, scaled_height;
	const GLenum objTarget = (texTarget == GL_TEXTURE_2D) ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP_ARB;

	if (gl_texture_non_power_of_two){
		scaled_width = width;
		scaled_height = height;

		//this seems buggered (will squash really large textures, but then again, not many huge textures around)
		//if (scaled_width > gl_max_size.value) scaled_width = gl_max_size.value; //make sure its not bigger than the max size
		//if (scaled_height > gl_max_size.value) scaled_height = gl_max_size.value;//make sure its not bigger than the max size
	}else{
		scaled_width = 1 << (int) ceil(log(width) / log(2.0));
		scaled_height = 1 << (int) ceil(log(height) / log(2.0));

		//this seems buggered (will squash really large textures, but then again, not many huge textures around)
		if (scaled_width > gl_max_size.value) scaled_width = gl_max_size.value; //make sure its not bigger than the max size
		if (scaled_height > gl_max_size.value)scaled_height = gl_max_size.value;//make sure its not bigger than the max size
	}

	samples = alpha ? gl_alpha_format : gl_solid_format;					//internal format

	if (scaled_width*scaled_height < 512*512)					//see if we can use our buffer
		scaled = (byte *)&temp_buffer[0];
	else
	{
		scaled = malloc(sizeof(unsigned)*scaled_width*scaled_height);
		Con_SafeDPrintf("&c500Upload32:&r Needed Dynamic Buffer for texture resize...\n");
		
		if (scaled==NULL)
			Sys_Error ("GL_LoadTexture: texture is too big, cannot resample textures bigger than %i",scaled_width*scaled_height);
	}


	if (scaled_width == width && scaled_height == height)					//if we dont need to scale
	{
		if (!mipmap)														//and we dont need to mipmap
		{																	//upload directly
			glTexImage2D (texTarget, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}else{																//else build mipmaps for it
			if (gl_sgis_mipmap){
				glTexParameteri(objTarget, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
				glTexImage2D (texTarget, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				glTexParameteri(objTarget, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);
			}else{
				/*
				int depth = 1;
				int mip = 0;
				
				//upload first without mipmapping
				glTexImage2D (GL_TEXTURE_2D, mip++, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

				while (scaled_width>1 || scaled_height>1){
					Image_MipReduce(scaled, scaled, &scaled_width, &scaled_height, &depth, 1, 1, 1, 4);
					glTexImage2D (GL_TEXTURE_2D, mip++, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				}*/
				gluBuild2DMipmaps (texTarget, samples, scaled_width, scaled_height, GL_RGBA, GL_UNSIGNED_BYTE, data);
			}
		}
	}
	else																	//if we need to scale
	{
		Con_SafeDPrintf("&c500Upload32:&r Textures too big or not a power of two in size: %ix%i...\n",width,height);

		//fix size so its a power of 2
		Image_Resample (data, width, height, 1, scaled, scaled_width, scaled_height, 1, 4, 1);

		if (gl_sgis_mipmap){
			glTexParameteri(objTarget, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
			glTexImage2D (texTarget, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
			glTexParameteri(objTarget, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);
		}else{
			gluBuild2DMipmaps (texTarget, samples, scaled_width, scaled_height, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}

		if (scaled != (byte *)&temp_buffer[0])
			free(scaled);
	}

	if (mipmap)
	{
		glTexParameterf(objTarget, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(objTarget, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		glTexParameterf(objTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic.value);
	}
	else
	{
		glTexParameterf(objTarget, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(objTarget, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		glTexParameterf(objTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_anisotropic.value);
	}
}

/*
===============
GL_Upload8
===============
*/
int GL_Upload8 (GLenum texTarget, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	static unsigned	temp_buffer[512*256];
	unsigned	*trans;
	int			i, s;
	int			p;
	
	s = width*height;
	
	if (s<512*256)
	{
		trans = &temp_buffer[0];
		memset(trans,512*256, sizeof(unsigned));
	}
	else
	{
		trans = malloc(s*sizeof(unsigned));
		Con_SafeDPrintf("&c500GL_Upload8:&r Needed Dynamic Buffer for 8bit to 24bit texture convert...\n");
	}
	// if there are no transparent pixels, make it a 3 component (ie: RGB not RGBA)
	// texture even if it was specified as otherwise
	if (alpha == 2)	{
// when alpha 2 we are checking to see if it is fullbright
// this is a fullbright mask, so make all non-fullbright
// colors transparent
		for (i = 0 ; i<s ; i++) {
			p = data[i];
			if (p < 224){
				trans[i] = d_8to24table[255];			// transparent 
			}else {
				trans[i] = d_8to24table[p];				// fullbright
				alpha = 1;	//found a fullbright texture, need to tell upload32 that this texture will have alpha
			}
		}
		//if alpha is still == 2 then no fullbright texture was found
		if (alpha == 2){
			return false;
		}
		alpha = true;
	} else if (alpha){
		alpha = false;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
				alpha = true;
			trans[i] = d_8to24table[p];
		}
	} else {
		//if (s&3)
		//	Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

	GL_Upload32 (texTarget, trans, width, height, mipmap, alpha);

	if (trans != &temp_buffer[0])
		free(trans);

	return true;
}

/*
================
GL_LoadTexture stuff
================
*/
// from DP's gl_textures.cs
static GLenum cubemapside[6] =
{
	GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB
};
//--

// Tomaz || TGA Begin
int lhcsumtable[256];
static int GL_SetupTexture ( GLenum target, char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, int bytesperpixel )
{
	int			i, s, lhcsum, fullbright;
	gltexture_t	*glt;

	if( target != GL_TEXTURE_2D && target != GL_TEXTURE_CUBE_MAP_ARB ) {
		Con_Printf( "GL_SetupTexture: Bad texture target\n" );
		return 0;
	}

	// LordHavoc: do a checksum to confirm the data really is the same as previous
	// occurances. well this isn't exactly a checksum, it's better than that but
	// not following any standards.
	lhcsum = 0;
	s = width*height*bytesperpixel;
	for (i = 0;i < 256;i++) lhcsumtable[i] = i + 1;
	for (i = 0;i < s;i++) lhcsum += (lhcsumtable[data[i] & 255]++);

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i < numgltextures ; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				if (lhcsum != glt->lhcsum || width != glt->width || height != glt->height)
				{
					Con_DPrintf("GL_LoadTexture: cache mismatch\n");
					goto GL_LoadTexture_setup;
				}
				return glt->texnum;
			}
		}
	}
	// whoever at id or threewave must've been half asleep...
	glt = &gltextures[numgltextures++];

	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number++;

GL_LoadTexture_setup:
	glt->lhcsum = lhcsum;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;
	glt->bytesperpixel = bytesperpixel; 

	if (!isDedicated)
	{
		GLenum texTarget;

		if( bytesperpixel != 1 && bytesperpixel != 4 ) {
			Sys_Error("GL_LoadTexture: unknown bytesperpixel\n");
		}

		glBindTexture(target,glt->texnum);

		if( target == GL_TEXTURE_2D ) {
			if (bytesperpixel == 1)
				fullbright = GL_Upload8 ( GL_TEXTURE_2D, data, width, height, mipmap, alpha);
			else if (bytesperpixel == 4)
				GL_Upload32 ( GL_TEXTURE_2D, (void *)data, width, height, mipmap, true);
		} else {
			for( i = 0 ; i < 6 ; i++ ) {
				byte *faceData = &data[ i * width * height * bytesperpixel ]; 
				texTarget = cubemapside[ i ];
				if (bytesperpixel == 1)
					fullbright = GL_Upload8 ( texTarget, faceData, width, height, mipmap, alpha);
				else if (bytesperpixel == 4)
					GL_Upload32 ( texTarget, (void *)faceData, width, height, mipmap, true);
			}
		}			
	}

	//if there wasnt a fullbright texture to load
	if ((alpha == 2)&&(fullbright == false)){
		numgltextures--;
		texture_extension_number--;
		glt->texnum = 0;
		return 0;
	}

	return glt->texnum;
}
// Tomaz || TGA End

int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, int bytesperpixel) {
	return GL_SetupTexture( GL_TEXTURE_2D, identifier, width, height, data, mipmap, alpha, bytesperpixel );
}

int GL_LoadCubemapTexture (char *identifier, int size, byte *data, qboolean mipmap, qboolean alpha, int bytesperpixel) {
	return GL_SetupTexture( GL_TEXTURE_CUBE_MAP_ARB, identifier, size, size, data, mipmap, alpha, bytesperpixel );
}

/****************************************/


extern int		multitex_go;
static GLenum oldtarget = GL_TEXTURE0_ARB;

void GL_SelectTexture (GLenum target) 
{
	if (multitex_go)
		qglSelectTextureARB(target);
	if (target == oldtarget) 
		return;
	oldtarget = target;
}

extern byte *LoadPCX (vfsfile_t *f);
extern byte *LoadTGA (vfsfile_t *f, char *name);
extern byte *LoadJPG (vfsfile_t *f);
extern byte *LoadPNG (vfsfile_t *f,char * name);
extern char *COM_FileExtension (char *in);

byte* loadimagepixels (char* filename, qboolean complain)
{
	vfsfile_t	*f;
//	char	*data;
	char	basename[128], name[128];
	byte	*c;
//	int		found;
//	int		i;
//	char	*filefound[8];
	byte	*output = 0;

	COM_StripExtension(filename, basename); // strip the extension to allow PNG, JPG, TGA and PCX

	for (c = basename; *c; c++)
		if (*c == '*')
			*c = '#';

#if 0
	found = COM_MultipleSearch(va("%s.*", basename), filefound, 8, false);

	for (i=0; i<found; i++){
		if (output == 0){
			if (Q_strcmp (COM_FileExtension(filefound[i]), "png")==0){
				COM_FOpenFile (filefound[i], &f);
				if (f)
					output = LoadPNG (f, filefound[i]);
			}
			if (Q_strcmp (COM_FileExtension(filefound[i]), "tga")==0){
				COM_FOpenFile (filefound[i], &f);
				if (f)
					output = LoadTGA (f, filefound[i]);
			}
			if (Q_strcmp (COM_FileExtension(filefound[i]), "jpg")==0){
				COM_FOpenFile (filefound[i], &f);
				if (f)
					output = LoadJPG (f);

			}
			if (Q_strcmp (COM_FileExtension(filefound[i]), "pcx")==0){
				COM_FOpenFile (filefound[i], &f);
				if (f)
					output = LoadPCX (f);

			}
		}
		Z_Free(filefound[i]);
	}

	if (output != 0){
		return output;
	}

#else
	//old way

#ifndef NO_PNG
	//png loading
	sprintf (name, "%s.png", basename);
	f = FS_OpenVFS(name, "rb", FS_GAME);
	if (f)
		return LoadPNG (f, basename);
#endif

	//tga loading
	sprintf (name, "%s.tga", basename);
	f = FS_OpenVFS(name, "rb", FS_GAME);
	if (f)
		return LoadTGA (f, basename);

#ifndef NO_JPEG
	//jpg loading
	sprintf (name, "%s.jpg", basename);
	f = FS_OpenVFS(name, "rb", FS_GAME);
	if (f)
		return LoadJPG (f);
#endif

	//pcx loading
	sprintf (name, "%s.pcx", basename);
	f = FS_OpenVFS(name, "rb", FS_GAME);
	if (f)
		return LoadPCX (f);
#endif

	if (complain)
		Con_Printf ("Couldn't load %s with extension .png, .tga, .jpg or .pcx\n", filename);

	return NULL;
}

int GL_LoadTexImage (char* filename, qboolean complain, qboolean mipmap)
{
	int texnum;
	byte *data;

	if (gl_quick_texture_reload.value)
	{
		texnum = GL_FindTexture (filename);
		if (-1!=texnum)
			return texnum;
	}

	data = loadimagepixels (filename, complain);
	if (!data)
		return 0;
	texnum = GL_LoadTexture (filename, image_width, image_height, data, mipmap, true, 4);
	free(data);
	return texnum;
}
// Tomaz || TGA End

// from DP's r_shadow.c
typedef struct suffixinfo_s
{
	char *suffix;
	qboolean flipx, flipy, flipdiagonal;
}
suffixinfo_t;
static suffixinfo_t suffix[3][6] =
{
	{
		{"px",   false, false, false},
		{"nx",   false, false, false},
		{"py",   false, false, false},
		{"ny",   false, false, false},
		{"pz",   false, false, false},
		{"nz",   false, false, false}
	},
	{
		{"posx", false, false, false},
		{"negx", false, false, false},
		{"posy", false, false, false},
		{"negy", false, false, false},
		{"posz", false, false, false},
		{"negz", false, false, false}
	},
	{
		{"rt",    true, false,  true},
		{"lf",   false,  true,  true},
		{"ft",    true,  true, false},
		{"bk",   false, false, false},
		{"up",    true, false,  true},
		{"dn",    true, false,  true}
	}
};
//--

// only supports RGBA images
static void Image_TinyMux( unsigned *outData, unsigned *inData, qboolean flipX, qboolean flipY, qboolean flipDiagonal )
{
	unsigned x, y;

	// assert: only works with image_width = imageheight

	// if nothing needs to be done, simply copy the data over
	if( !flipX && !flipY && !flipDiagonal ) {
		memcpy( outData, inData, sizeof( *inData ) * image_width * image_height );
		return;
	}

	for( x = 0 ; x < image_width ; x++ ) {
		for( y = 0 ; y < image_height ; y++ ) {
			unsigned outX, outY;

			if( !flipDiagonal ) {
				outX = x;
				outY = y;
			} else {
				outX = y;
				outY = x;
			} 
			if( flipX ) {
				outX = image_width - x - 1;
			}
			if( flipY ) {
				outY = image_height - y - 1;
			}
			
			outData[ outX + outY * image_width ] = inData[ x + y * image_width ];			
		}
	}
}

int GL_LoadCubeTexImage (char* basename, qboolean complain, qboolean mipmap) {
	byte *data = NULL;
	unsigned faceSize = 0;
	unsigned i;
	int texnum;

	if (gl_quick_texture_reload.value)
	{
		texnum = GL_FindTexture (basename);
		if (-1 != texnum)
			return texnum;
	}

	// if the gl extension isnt present, we cant help it
	if( !gl_support_cubemaps && complain ) {
		Con_Printf( "GL_LoadCubeTexImage: Cubemaps are not supported by your graphics driver\n" );
		return 0;
	}

	// try all different suffixes until data is filled
	for( i = 0 ; !data && i < sizeof( suffix ) / sizeof( suffixinfo_t[6] ) ; i++ ) {
		unsigned j;
		suffixinfo_t *suffixes = suffix[ i ];
		for( j = 0 ; j < 6 ; j++ ) {
			char filename[ 256 ];
			byte *faceData;
			dpsnprintf( filename, 256, "%s%s", basename,  suffixes[ j ] );
			
			faceData = loadimagepixels (filename, complain);
			if( !faceData ) {
				break;
			}
			if( image_width != image_height || image_width == 0 || 
				(faceSize > 0 && image_width != faceSize ) ) {
				free( faceData );	
				break;
			}
			if( !data ) {
				faceSize = image_width;
				data = malloc( 6 * faceSize * faceSize * 4 );
			}

            Image_TinyMux( (unsigned *) &data[ j * faceSize * faceSize * 4 ], (unsigned *) faceData, suffixes[ j ].flipx, suffixes[ j ].flipy, suffixes[ j ].flipdiagonal );
			free( faceData );
		}
		if( j != 6 && data ) {
			free( data );
			data = NULL;
			faceSize = 0;
		}
	}

	if( !data && complain ) {
		Con_Printf( "GL_LoadCubeTexImage: Unable to find cubemap '%s'\n", basename );
		return 0;
	}

	texnum = GL_LoadCubemapTexture (basename, image_width, data, mipmap, true, 4);
	free( data );
	return texnum;
}
