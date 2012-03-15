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
// menu_key.c
// Keyboard/mouse handling


#include "quakedef.h"
#include "winquake.h"
#include "menu_engine.h"


int grabbed_form = -1;
int form_focus = -1;

extern void ParseLink (int link, int ref);
extern qboolean isinpicture (int formid, int picid, int x, int y);
extern qboolean isinbutton (int formid, int chkid, int x, int y); // fixme

void M_Engine_MouseUp (void)
{
	grabbed_form = -1;
}

void M_Engine_Keydown (int key)
{
	int i, j, new_id;
	char *quitstr = "quit";

	switch (key)
	{
	case K_ESCAPE:
		current_pos.x = window_center_x;
		current_pos.y = window_center_y;
		old_mouse_x = 0;
		old_mouse_y = 0;
		grabbed_form = -1;
		IN_ClearStates();
		SetCursorPos(window_center_x, window_center_y);

		key_dest = key_game;
		m_state = m_none;
//		cls.demonum = m_save_demonum;
//		if (cls.demonum != -1 && !cls.demoplayback && cls.state != ca_connected)
//			CL_NextDemo ();
		break;

	//case K_ENTER:
	case K_MOUSE1:
		if (grabbed_form != -1)
		{
			grabbed_form = -1;
			return;
		}

		for (i=0; i<MAX_FIELDS-1; i++)
		{
			if (forms[i].enabled)
			{
				if (!check_windowcollision(i,current_pos.x - omousex,current_pos.y - omousey))
				{
//test hit for grabbing form
					if (forms[i].gclass.type != 0)
					{
						if (isingrabbfield(i,current_pos.x - omousex,current_pos.y - omousey))
						{
							if (form_focus == i)
								grabbed_form = i;
							new_id = make_topmost (i);
							return;
						}
						else
						{
							new_id = make_topmost (i);
						}
					}

					for (j=0; j<MAX_FIELDS-1; j++)
					{
//test hit on labels
						if (forms[i].label[j].enabled)
						{
							if ( (isinlabel(i,j,current_pos.x - omousex,current_pos.y - omousey)) && (forms[i].label[j].clsval.flagg == LABEL_LINK) )
							{
								if (forms[i].label[j].target)
								{
									ParseLink(forms[i].label[j].target, i);
								}

							}
						}
						if (forms[i].picture[j].enabled)
						{
							if (isinpicture(i,j,current_pos.x, current_pos.y))
							{
								if (forms[i].picture[j].target)
								{
									ParseLink(forms[i].picture[j].target, i);
								}
							}
						}
						if (forms[i].button[j].enabled)
						{
							if (isinbutton(i, j, current_pos.x - omousex, current_pos.y - omousey))
							{
								if (forms[i].button[j].target)
								{
									ParseLink(forms[i].button[j].target, i);
								}
							}
						}
						if (forms[i].checkbox[j].enabled)
						{
							if (isincheckbox(i,j,current_pos.x - omousex,current_pos.y - omousey))
							{
								m_entersound = true;
								if (forms[i].checkbox[j].value == true)
									forms[i].checkbox[j].value = false;
								else
									forms[i].checkbox[j].value = true;
							}
						}
					}
				}
			}
		}
		break;

	}
}