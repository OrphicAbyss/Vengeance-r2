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
// menu_forms.c
// All form loading/shaping is done here

#include "quakedef.h"
#include "winquake.h"
#include "menu_engine.h"

extern void M_Func_LoadPicture (mpicture_t *tmppic, char *path, int width, int height);
extern void M_Func_Picture (mpicture_t *tmppic, int formid, int x, int y);

void M_Form_Main (void)
{
	int			formid;
	mpicture_t	tmppic;
	mclass_t	form_class;

//create a class for the form
	form_class.back_color = M_Func_RGBA(0, 0, 0, 0.8f);
	//form_class.back_color.a = 0.8f;
	form_class.type = 0;			//0=static/noborder

//create a form
	formid = M_Func_Form("Main",0,-1,vid.width,100);
	forms[formid].gclass = form_class;	//apply the created class

//create a image
	M_Func_LoadPicture (&tmppic, "gfx/menu/icons/quit.png", 55, 21);
	M_Func_Picture(&tmppic, formid, -1, 10);
	tmppic.target = LINK_QUIT;
	forms[formid].picture[0] = tmppic;

	//M_Func_Picture(&tmppic, -1, 35);
	//tmppic.target = LINK_HELP;
	//forms[formid].picture[1] = tmppic;

	make_topmost (formid);
}

void M_Form_Help (void)
{
	int			formid;
//	mpicture_t	tmppic;
	mclass_t	form_class;

//create a class for the form
	form_class.back_color = M_Func_RGBA(1, 0, 0, 0.8f);
	//form_class.back_color.a = 0.8f;
	form_class.type = 0;			//0=static/noborder

//create a form
	formid = M_Func_Form("Help",0,((vid.height/2)-150),vid.width,150);
	forms[formid].gclass = form_class;	//apply the created class
	make_topmost (formid);
}

void M_Form_Quit (void)
{
	int			formid;
	formid = M_Func_Form("Quit",0,-1,vid.width,100);
	make_topmost (formid);
}



/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This is where all initial formshaping is done
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void M_Load_Forms (void)
{
	M_Form_Main ();
}
