/*
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
// Menu engine by Darkie, edited by Entar
// menu_functions.c
// All of the generic functions used explicitly by the engine goes here


#include "quakedef.h"
#include "winquake.h"
#include "menu_engine.h"


//graphics

void M_Func_DrawGBG (int formid)
{
	int x, y, width, height;

	x = forms[formid].pos.x;
	y = forms[formid].pos.y;
	width = forms[formid].size.x;
	height = 10;

	glEnable (GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	glDisable (GL_TEXTURE_2D);
	if (form_focus == formid)
		glColor4f (0.3f, 0.3f, 1, 1);
	else
		glColor4f (0, 0, 0.5f, 1);
	glBegin (GL_QUADS);

	x++;
	y++;
	width=width-2;
	height=height-2;

	glVertex2f (x,y);
	glVertex2f (x+width, y);
	glVertex2f (x+width, y+height);
	glVertex2f (x, y+height);

	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glEnable(GL_ALPHA_TEST);

	if (form_focus == formid)
	{
		M_DrawIMG (m_gfx_aform, x, y, width, height);
	}
}

void M_Func_Draw2D (int x, int y, int width, int height, float color[3])
{
	glDisable (GL_TEXTURE_2D);
	glColor4f (color[0], color[1], color[2], 1);
	glBegin (GL_QUADS);
		glVertex2f (x,y);
		glVertex2f (x+width, y);
		glVertex2f (x+width, y+height);
		glVertex2f (x, y+height);
	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
}

void M_Func_Draw2Da (int x, int y, int width, int height, mrgba_t color)
{
	glEnable (GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	glDisable (GL_TEXTURE_2D);

	glColor4f (color.r, color.g, color.b, color.a);
	glBegin (GL_QUADS);
		glVertex2f (x,y);
		glVertex2f (x+width, y);
		glVertex2f (x+width, y+height);
		glVertex2f (x, y+height);
	glEnd ();
	glBlendFunc(GL_ONE,GL_ONE);
	glColor4f (color.r, color.g, color.b, color.a);
	glBegin (GL_QUADS);
		glVertex2f (x,y);
		glVertex2f (x+width, y);
		glVertex2f (x+width, y+height);
		glVertex2f (x, y+height);
	glEnd ();

	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glEnable(GL_ALPHA_TEST);
}

void M_Func_Draw3D (int x, int y, int width, int height, int style)
{

	float	color3d_face[3];
	float	color3d_dark[3];
	float	color3d_light[3];
	float	color3d_active[3];

	color3d_face[0] = 0.831f;
	color3d_face[1] = 0.815f;
	color3d_face[2] = 0.784f;
	color3d_light[0] = 1;
	color3d_light[1] = 1;
	color3d_light[2] = 1;
	color3d_dark[0] = 0.501f;
	color3d_dark[1] = 0.501f;
	color3d_dark[2] = 0.501f;
	color3d_active[0] = 1;
	color3d_active[1] = 1;
	color3d_active[2] = 1;

	if (style == 0) //raised
	{
		M_Func_Draw2D (x, y, width, height, color3d_face);
		M_Func_Draw2D (x, y, width, 1, color3d_light);
		M_Func_Draw2D (x, y, 1, height, color3d_light);
		M_Func_Draw2D (x+width, y, 1, height, color3d_dark);
		M_Func_Draw2D (x, y+height, width, 1, color3d_dark);
	}
	if (style == 1) //lowered
	{
		M_Func_Draw2D (x, y, width, height, color3d_face);
		M_Func_Draw2D (x, y, width, 1, color3d_dark);
		M_Func_Draw2D (x, y, 1, height, color3d_dark);
		M_Func_Draw2D (x+width, y, 1, height, color3d_light);
		M_Func_Draw2D (x, y+height, width, 1, color3d_light);
	}
	if (style == 2) //lowered
	{
		M_Func_Draw2D (x, y, width, height, color3d_active);
		M_Func_Draw2D (x, y, width, 1, color3d_dark);
		M_Func_Draw2D (x, y, 1, height, color3d_dark);
		M_Func_Draw2D (x+width, y, 1, height, color3d_light);
		M_Func_Draw2D (x, y+height, width, 1, color3d_light);
	}
}

void M_DrawIMG (int pic, int x, int y, int width, int height)
{
	glColor4f (1,1,1,1);
	glBindTexture(GL_TEXTURE_2D,pic);
	glBegin (GL_QUADS);
		glTexCoord2f (0, 0);	glVertex2f (x, y);
		glTexCoord2f (1, 0);	glVertex2f (x + width, y);
		glTexCoord2f (1, 1);	glVertex2f (x + width, y + height);
		glTexCoord2f (0, 1);	glVertex2f (x, y + height);
	glEnd ();
	glColor4f (1,1,1,1);
}

void M_DrawIMGa (int pic, int x, int y, int width, int height, float alpha)
{
	glEnable (GL_BLEND);
	glBlendFunc (GL_ONE, GL_ONE);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);

	glColor4f (1,1,1,0.5f);
	glBindTexture(GL_TEXTURE_2D,pic);
	glBegin (GL_QUADS);
		glTexCoord2f (0, 0);	glVertex2f (x, y);
		glTexCoord2f (1, 0);	glVertex2f (x + width, y);
		glTexCoord2f (1, 1);	glVertex2f (x + width, y + height);
		glTexCoord2f (0, 1);	glVertex2f (x, y + height);
	glEnd ();
	glColor4f (1,1,1,1);

	glDisable (GL_BLEND);
	glEnable(GL_ALPHA_TEST);
}

void M_Func_DrawIMG (int formid, mpicture_t pic)
{
	int x, y, width, height;

	x = forms[formid].pos.x + pic.pos.x;
	y = forms[formid].pos.y + pic.pos.y;
	width = pic.picdata.size.x;
	height = pic.picdata.size.y;
	
	M_DrawIMG (pic.picdata.image, x, y, width, height);
}

void M_Func_DrawIMGa (int formid, mpicture_t pic)
{
	int x, y, width, height;

	x = forms[formid].pos.x + pic.pos.x;
	y = forms[formid].pos.y + pic.pos.y;
	width = pic.picdata.size.x;
	height = pic.picdata.size.y;
	
	M_DrawIMGa (pic.picdata.image, x, y, width, height, pic.gclass.alpha);
}

void M_Func_cCharacter (int x, int y, int num, float color[3], float alpha)
{
	int				row, col;
	float			frow, fcol, size;

	if (num == 32)
		return;		// space

	num &= 255;
	
	if (y <= -FONTSIZE)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	//qmb :larger text??
	frow = row*0.0625f;
	fcol = col*0.0625f;
	size = 0.0625f;

	glColor4f (color[0], color[1], color[2], alpha);
	glEnable (GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);

	glBegin (GL_QUADS);
		glTexCoord2f (fcol, frow);
		glVertex2f (x, y);
		glTexCoord2f (fcol + size, frow);
		glVertex2f (x+FONTSIZE, y);
		glTexCoord2f (fcol + size, frow + size);
		glVertex2f (x+FONTSIZE, y+FONTSIZE);
		glTexCoord2f (fcol, frow + size);
		glVertex2f (x, y+FONTSIZE);
	glEnd ();
	glColor4f (1,1,1,1);

	glDisable (GL_BLEND);
	glEnable(GL_ALPHA_TEST);
}

void M_Func_PrintColor (int cx, int cy, char *str, float color[3], float alpha)
{
	while (*str)
	{
		M_Func_cCharacter (cx, cy, *str, color, alpha);
		str++;
		cx += FONTSIZE;
	}
}

mrgba_t M_Func_RGBA (float red, float green, float blue, float alpha)
{
	mrgba_t retval;
	retval.r = red;
	retval.g = green;
	retval.b = blue;
	retval.a = alpha;
	return retval;
}

//keys
qboolean isingrabbfield (int id, int x, int y)
{
	if ( ( (x > forms[id].pos.x + 10) && (x < forms[id].pos.x + forms[id].size.x - 10) ) && ( (y > forms[id].pos.y) && (y < forms[id].pos.y + 10) ) )
		return true;
	else
		return false;
}

qboolean isinform (int id, int x, int y)
{
	if ( ( (x > forms[id].pos.x) && (x < forms[id].pos.x + forms[id].size.x) ) && ( (y > forms[id].pos.y) && (y < forms[id].pos.y + forms[id].size.y) ) )
		return true;
	else
		return false;
}

qboolean isinlabel (int formid, int labelid, int x, int y)
{
	//if ( ( (x > forms[id].pos.x + 10) && (x < forms[id].pos.x + forms[id].size.x - 10) ) && ( (y > forms[id].pos.y) && (y < forms[id].pos.y + 10) ) )
	if ( ( (x > forms[formid].pos.x + forms[formid].label[labelid].pos.x) && (x < forms[formid].pos.x + forms[formid].label[labelid].pos.x + (strlen(forms[formid].label[labelid].value) * FONTSIZE)) ) && ( (y > forms[formid].pos.y + forms[formid].label[labelid].pos.y) && (y < forms[formid].pos.y + forms[formid].label[labelid].pos.y + FONTSIZE) ) )
		return true;
	else
		return false;
}

qboolean isinpicture (int formid, int picid, int x, int y)
{
	if ((x > forms[formid].pos.x + forms[formid].picture[picid].pos.x) && (x < forms[formid].pos.x + forms[formid].picture[picid].pos.x + forms[formid].picture[picid].picdata.size.x))
	{
		if ((y > forms[formid].pos.y + forms[formid].picture[picid].pos.y) && (y < forms[formid].pos.y + forms[formid].picture[picid].pos.y + forms[formid].picture[picid].picdata.size.y))
		{
			return true;
		}
	}
	return false;
}

// FIXME, probably bugged
qboolean isinbutton (int formid, int chkid, int x, int y)
{
	if (x > forms[formid].pos.x + forms[formid].button[chkid].pos.x)
		if (x < forms[formid].pos.x + forms[formid].button[chkid].pos.x + (strlen(forms[formid].button[chkid].value) * FONTSIZE))
			if (y > forms[formid].pos.y + forms[formid].button[chkid].pos.y)
				if (y < forms[formid].pos.y + forms[formid].button[chkid].pos.y + (strlen(forms[formid].button[chkid].value) * FONTSIZE))
					return true;

	return false;
}

qboolean isincheckbox (int formid, int chkid, int x, int y)
{
	if ( ( (x > forms[formid].pos.x + forms[formid].checkbox[chkid].pos.x) && (x < forms[formid].pos.x + forms[formid].checkbox[chkid].pos.x + FONTSIZE) ) && ( (y > forms[formid].pos.y + forms[formid].checkbox[chkid].pos.y) && (y < forms[formid].pos.y + forms[formid].checkbox[chkid].pos.y + FONTSIZE) ) )
		return true;
	else
		return false;
}

qboolean check_windowcollision (int id, int x, int y)
{
	int i;

	for (i=id+1; i<MAX_FIELDS-1; i++)
	{
		if (forms[i].enabled)
		{
			if (isinform(i, x, y))
				return true;
		}
	}

	return false;
}

int make_topmost (int id)
{
//	FILE *logfile;
	vfsfile_t *logfile;
	int i,j=0;
	mform_t tempform;
	char	*temp;

	temp = va("Vr2_console.log");
	memcpy(&tempform, &forms[id], sizeof(mform_t));
//	logfile = fopen(va("%s/QMB.log", com_gamedir),"w");
	logfile = FS_OpenVFS (temp, "wb", FS_GAME);

	for (i=id; i<MAX_FIELDS-1; i++)
	{
		if (forms[i+1].enabled)
		{
			if (logfile)
			{
				char *temp;
//				vfprintf(logfile, va("i: %d", i), "");
//				vfprintf(logfile, va("copy: %d to %d", i+1, i), "");
				temp = va("i: %d", i);
				VFS_WRITE(logfile, temp, strlen(temp));
				temp = va("copy: %d to %d", i+1, i);
				VFS_WRITE(logfile, temp, strlen(temp));
			}
		memcpy(&forms[i], &forms[i+1], sizeof(mform_t));
		//forms[i] = forms[i+1];
		}
		if ( (forms[i].enabled == false) && (j==0) )
			j = i;
	}
	memcpy(&forms[j-1], &tempform, sizeof(mform_t));
	form_focus = j-1;

	if (logfile)
		VFS_CLOSE(logfile);
//		fclose (logfile);

	form_focus = j-1;

	return j-1;
}

int M_Func_GetFreeFormID (void)
{
	int i;

	for (i=0; i<MAX_FIELDS-1; i++)
		if (!forms[i].enabled)
			return i;
	return -1;
}

mbutton_t M_Func_Button (int x, int y, int target, char *val)
{
	mbutton_t tmpbtn;
	tmpbtn.enabled = true;
	tmpbtn.pos.x = x;
	tmpbtn.pos.y = y;
	tmpbtn.target = target;
	tmpbtn.value = val;
	return tmpbtn;
}

int M_Func_Form (char *name, int x, int y, int w, int h)
{
	int freeform;
	freeform = M_Func_GetFreeFormID();
	forms[freeform].name = name;
	if (x == -1)
		forms[freeform].pos.x = (vid.width/2) - (w/2);
	else
		forms[freeform].pos.x = x;

	if (y == -1)
		forms[freeform].pos.y = (vid.height/2) - (h/2);
	else
		forms[freeform].pos.y = y;

	forms[freeform].size.x = w;
	forms[freeform].size.y = h;
	forms[freeform].enabled = true;
	return freeform;
}

void M_Func_LoadPicture (mpicture_t *tmppic, char *path, int width, int height)
{
	if (!tmppic)
		return;

	tmppic->picdata.size.x = width;
	tmppic->picdata.size.y = height;
	tmppic->picdata.image = GL_LoadTexImage (path, false, false); //"gfx/menu/icons/apacheconf.png"
	tmppic->picdata.isloaded = true;
}

int M_Func_GetFreePic (int formid)
{
	int i;

	for (i=0; i<MAX_FIELDS-1; i++)
		if (forms[formid].picture[i].enabled != true)
			return i;
	return -1;
}

void M_Func_Picture (mpicture_t *tmppic, int formid, int x, int y)
{
	int freepic;

	if (!tmppic)
		return;

	if (tmppic->picdata.isloaded != true)
		return;

	if (x == -1)
		x = (vid.width / 2) - (tmppic->picdata.size.x / 2);

	if (y == -1)
		y = (vid.height / 2) - (tmppic->picdata.size.y / 2);

	tmppic->pos.x = x;
	tmppic->pos.y = y;
	tmppic->enabled = true;

	freepic = M_Func_GetFreePic(formid);
	forms[formid].picture[freepic] = *tmppic;
}

void M_Func_ClearForm (mform_t *frmid)
{
	memset(frmid, 0, sizeof(*frmid));
}