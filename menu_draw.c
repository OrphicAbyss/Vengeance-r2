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
// menu_draw.c
// All "high-level" drawing stuff goes in here


#include "quakedef.h"
#include "winquake.h"
#include "menu_engine.h"

extern int grabbed_form;
extern void M_Func_Draw2Da (int x, int y, int width, int height, mrgba_t color);
extern int M_Func_GetFreePic (int formid);

vec3_t	color_window_focus = {0,0,0};
vec3_t	color_window_nofocus = {50,50,50};

float	test_rotation = 0;

void M_Draw_Cursor (void)
{
	M_DrawIMG (m_gfx_cursor, current_pos.x - omousex, current_pos.y - omousey, 23, 23);
	PrintWhite (0, 0, va("x: %d y:%d",current_pos.x+1,current_pos.y+1));

}

void M_Draw_Background (void)
{
	glDisable(GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f (1,1,1,0.5f);
	glBindTexture(GL_TEXTURE_2D,m_gfx_background);
	glBegin (GL_QUADS);
		glTexCoord2f (0, 0);	glVertex2f (0, 0);
		glTexCoord2f (1, 0);	glVertex2f (vid.width, 0);
		glTexCoord2f (1, 1);	glVertex2f (vid.width, vid.height);
		glTexCoord2f (0, 1);	glVertex2f (0, vid.height);
	glEnd ();
	glColor4f (1,1,1,1);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_ALPHA_TEST);
	glDisable (GL_BLEND);

}

void M_Draw_TMPwindow (int x, int y, int width, int height)
{
//border
	glEnable (GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	glDisable (GL_TEXTURE_2D);
	glColor4f (0.15f, 0.15f, 0.15f, 1);
	glBegin (GL_QUADS);

	glVertex2f (x-1,y-1);
	glVertex2f (x+1 + width, y-1);
	glVertex2f (x+1 + width, y+1 + height);
	glVertex2f (x-1, y+1 + height);

	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glEnable(GL_ALPHA_TEST);

//shadow
	glEnable (GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_ALPHA_TEST);
	glDisable (GL_TEXTURE_2D);
	glColor4f (0, 0, 0, 0.10f);
	glBegin (GL_QUADS);

	glVertex2f (x+5,y+5);
	glVertex2f (x+5 + width, y+5);
	glVertex2f (x+5 + width, y+5 + height);
	glVertex2f (x+5, y+5 + height);

	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
	glEnable(GL_ALPHA_TEST);

//body
	M_DrawIMG (m_gfx_form, x, y, width, height);
}

void M_Draw_TextBoxes (int formid)
{
	int i;

	for (i=0; i<MAX_FIELDS-1; i++)
	{
		if (forms[formid].textbox[i].enabled)
		{
			M_Func_Draw3D (forms[formid].pos.x + forms[formid].textbox[i].pos.x-1, forms[formid].pos.y + forms[formid].textbox[i].pos.y-1, (forms[formid].textbox[i].size * FONTSIZE)+2, FONTSIZE+2,2);
		}
	}
}

void M_Draw_Pictures (int id)
{
	int i;
	float tmpcolor[3];

	tmpcolor[0] = 1;
	tmpcolor[1] = 1;
	tmpcolor[2] = 1;

	for (i=0; i<MAX_FIELDS-1; i++)
	{
		if ( (forms[id].picture[i].enabled) && (forms[id].picture[i].picdata.isloaded) )
		{
			//if ((forms[id].picture[i].gclass.alpha != 1) && (forms[id].picture[i].gclass.alpha))
			//	M_Func_DrawIMGa (id, forms[id].picture[i]);
			//else
				M_Func_DrawIMG (id, forms[id].picture[i]);
		}
	}
}

void M_Draw_CheckBoxes (int id)
{
	int i;

	for (i=0; i<MAX_FIELDS-1; i++)
	{
		if ( (forms[id].checkbox[i].enabled) )
		{
			if (forms[id].checkbox[i].value == true)
				M_DrawIMG (m_gfx_checkbox[1], forms[id].pos.x + forms[id].checkbox[i].pos.x, forms[id].pos.y + forms[id].checkbox[i].pos.y, FONTSIZE, FONTSIZE);
			else
				M_DrawIMG (m_gfx_checkbox[0], forms[id].pos.x + forms[id].checkbox[i].pos.x, forms[id].pos.y + forms[id].checkbox[i].pos.y, FONTSIZE, FONTSIZE);
		}
	}
}

void M_Draw_Labels (int id)
{
	int i;

	for (i=0; i<MAX_FIELDS-1; i++)
	{
		if (forms[id].label[i].enabled)
		{
			if (forms[id].label[i].clsval.flagg == LABEL_LINK)
			{
				if ( (isinlabel(id,i,current_pos.x - omousex,current_pos.y - omousey)) && (!check_windowcollision(id,current_pos.x - omousex,current_pos.y - omousey)) )
					forms[id].label[i].clsval.alpha = 1;
				else
					forms[id].label[i].clsval.alpha = 0.6f;
			}
			if (forms[id].label[i].clsval.flagg == LABEL_NORMAL)
				forms[id].label[i].clsval.alpha = 1;
			M_Func_PrintColor (forms[id].pos.x + forms[id].label[i].pos.x, forms[id].pos.y + forms[id].label[i].pos.y, forms[id].label[i].value, forms[id].label[i].clsval.color, forms[id].label[i].clsval.alpha);
		}
	}
}

void M_Draw_Buttons (int formid)
{
	int i;
	float	defaultcolor[3];

	defaultcolor[0] = 0;
	defaultcolor[1] = 0;
	defaultcolor[2] = 0;
	for (i=0; i<MAX_FIELDS-1; i++)
	{
		if (forms[formid].button[i].enabled)
		{
			M_Func_Draw3D (forms[formid].pos.x + forms[formid].button[i].pos.x, forms[formid].pos.y + forms[formid].button[i].pos.y, (strlen(forms[formid].button[i].value) * FONTSIZE + 4), FONTSIZE + 4, 0);
			M_Func_PrintColor (forms[formid].pos.x + forms[formid].button[i].pos.x + 2, forms[formid].pos.y + forms[formid].button[i].pos.y + 2, forms[formid].button[i].value, defaultcolor, 1);
		}
	}
}

void M_Draw_Forms (void)
{
	int i;

	for (i=0; i<MAX_FIELDS-1; i++)
	{
		if (forms[i].enabled)
		{
			if (forms[i].gclass.type == 0)
			{
//					M_Func_Draw2Da(forms[i].pos.x, forms[i].pos.y, forms[i].size.x, forms[i].size.y, forms[i].gclass.back_color, forms[i].gclass.alpha);
					M_Func_Draw2Da(forms[i].pos.x, forms[i].pos.y, forms[i].size.x, forms[i].size.y, forms[i].gclass.back_color);
					if (i == 0)
					PrintWhite(20, 80, va("%f", forms[i].gclass.alpha));
			}
			else
			{
				M_Func_Draw3D (forms[i].pos.x, forms[i].pos.y, forms[i].size.x, forms[i].size.y, 0);
			}
			M_Draw_Pictures (i);
			M_Draw_Labels (i);
			M_Draw_CheckBoxes (i);
			M_Draw_TextBoxes (i);
			M_Draw_Buttons (i);

			if (forms[i].gclass.type != 0)
			{
				M_Func_DrawGBG (i);
				PrintWhite (forms[i].pos.x + (forms[i].size.x / 2) - ((strlen(forms[i].name) * FONTSIZE) / 2), forms[i].pos.y, forms[i].name); //form label
			}
		}
	}

}

void M_Engine_Draw (void)
{
	char timebuf[20];
	float cBlack[3];

	Sys_Strtime( timebuf );
	cBlack[0] = 0;
	cBlack[1] = 0;
	cBlack[2] = 1;

	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw)
	{
		scr_copyeverything = 1;

		if (scr_con_current)
		{
			Draw_ConsoleBackground (vid.height);
			S_ExtraUpdate ();
		}

		scr_fullupdate = 0;
	}
	else
	{
		m_recursiveDraw = false;
	}

	M_Draw_Background ();

	if (grabbed_form != -1)
	{
		forms[grabbed_form].pos.x = current_pos.x - (forms[grabbed_form].size.x / 2) - omousex;
		forms[grabbed_form].pos.y = current_pos.y - omousey;
	}

	M_Draw_Forms ();

	PrintWhite (40, vid.height - FONTSIZE, va("x:%i y:%i", current_pos.x, current_pos.y));
	//if (isinpicture(0,0,current_pos.x, current_pos.y))
	PrintWhite (40, vid.height - (FONTSIZE*3), va("%i", vid.width/2));

	M_Draw_Cursor ();

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

	S_ExtraUpdate ();
}