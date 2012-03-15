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
// Menu engine by Darkie
// menu_links.c
// "links" for the menus (actions)

#include "quakedef.h"
#include "winquake.h"
#include "menu_engine.h"

extern void M_Func_ClearForm (mform_t *frmid);
extern void M_Form_Help();

void ParseLink (int link, int ref)
{
	switch (link)
	{
	case LINK_QUIT:
		m_entersound = true;
		key_dest = key_console;
		CL_Disconnect ();
		Host_ShutdownServer(false);		
		Sys_Quit ();
		break;
	case LINK_HELP:
		M_Func_ClearForm(&forms[ref]);
		M_Form_Help();
		break;
	}

}