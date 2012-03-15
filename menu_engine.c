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
// menu_engine.c
// The initializing code for the menu engine

#include "quakedef.h"
#include "winquake.h"
#include "menu_engine.h"


//cvars
cvar_t	mouse_size = {"mouse_size","32", false};

void M_Engine_Init (void)
{
	Cvar_RegisterVariable (&mouse_size);
}

extern void M_Load_Forms (void);

void M_Engine_Load (void)
{
//	int rndnum;
	m_gfx_checkbox[0] = GL_LoadTexImage ("gfx/menu/check_disabled.png", false, false);
	m_gfx_checkbox[1] = GL_LoadTexImage ("gfx/menu/check_enabled.png", false, false);

	m_gfx_cursor = GL_LoadTexImage ("gfx/menu/cursor.png", false, false);
	m_gfx_background = GL_LoadTexImage ("gfx/menu/background.png", false, false);
	m_gfx_form = GL_LoadTexImage ("gfx/menu/formbg.png", false, false);
	m_gfx_aform = GL_LoadTexImage ("gfx/menu/activebar.png", false, false);
	
	m_ico[0] = GL_LoadTexImage ("gfx/menu/icons/apacheconf.png", false, false);

	M_Load_Forms ();
}