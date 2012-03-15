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
// cvar.c -- dynamic variable tracking

#include "quakedef.h"

cvar_t	*cvar_vars;
char	*cvar_null_string = "";

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;
	
	for (var=cvar_vars ; var ; var=var->next)
		if (!Q_strcmp (var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float	Cvar_VariableValue (char *var_name)
{
	cvar_t	*var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return Q_atof (var->string);
}


/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (char *var_name)
{
	cvar_t *var;
	
	var = Cvar_FindVar (var_name);
	if (!var)
		return cvar_null_string;
	return var->string;
}


/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable (char *partial)
{
	cvar_t		*cvar;
	int			len;
	
	len = Q_strlen(partial);
	
	if (!len)
		return NULL;
		
// check functions
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!Q_strncmp (partial,cvar->name, len))
			return cvar->name;

	return NULL;
}

/*
============
Cvar_Set
============
*/
void Cvar_SetQuick (cvar_t *var, char *value)
{
	qboolean changed;
	
	changed = Q_strcmp(var->string, value);
	
	Z_Free (var->string);	// free the old value string
	
	var->string = Z_Malloc (Q_strlen(value)+1);
	Q_strcpy (var->string, value);
	var->value = Q_atof (var->string);
	if (var->server && changed)
	{
		if (sv.active)
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name, var->string);
	}
}

void Cvar_Set (char *var_name, char *value)
{
	cvar_t	*var;
	qboolean changed;
	
	var = Cvar_FindVar (var_name);
	if (!var)
	{	// there is an error in C code if this happens
		Con_Printf ("Cvar_Set: variable %s not found\n", var_name);
		return;
	}

	changed = Q_strcmp(var->string, value);
	
	Z_Free (var->string);	// free the old value string
	
	var->string = Z_Malloc (Q_strlen(value)+1);
	Q_strcpy (var->string, value);
	var->value = Q_atof (var->string);
	if (var->server && changed)
	{
		if (sv.active)
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n", var->name, var->string);
	}
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValueQuick(cvar_t *var, float value)
{
	char val[32];

	if ((float)((int)value) == value)
		sprintf(val, "%i", (int)value);
	else
		sprintf(val, "%f", value);
	Cvar_SetQuick(var, val);
}

void Cvar_SetValue(char *var_name, float value)
{
	char val[32];

	if ((float)((int)value) == value)
		sprintf(val, "%i", (int)value);
	else
		sprintf(val, "%f", value);
	Cvar_Set(var_name, val);
}


/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable (cvar_t *variable)
{
	char	*oldstr;
	
// first check to see if it has allready been defined
	if (Cvar_FindVar (variable->name))
	{
		Con_Printf ("Can't register variable %s, allready defined\n", variable->name);
		return;
	}
	
// check for overlap with a command
	if (Cmd_Exists (variable->name))
	{
		Con_Printf ("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}
		
// copy the value off, because future sets will Z_Free it
	oldstr = variable->string;
	variable->string = Z_Malloc (Q_strlen(variable->string)+1);	
	Q_strcpy (variable->string, oldstr);
	variable->value = Q_atof (variable->string);
	
// link the variable in
	variable->next = cvar_vars;
	cvar_vars = variable;
}

/*
============
Cvar_Get

Adds a newly allocated variable to the variable list or sets its value.
============
*/
cvar_t *Cvar_Get (char *name, char *value, int flags)
{
	cvar_t *cvar;

	if (developer.value)
		Con_Printf("Cvar_Get(\"%s\", \"%s\", %i);\n", name, value, flags);
	
// first check to see if it has already been defined
	cvar = Cvar_FindVar (name);
	if (cvar)
	{
//		cvar->flags |= flags;
//		Cvar_SetQuick_Internal (cvar, value);
		Cvar_Set(cvar->name, value);
		return cvar;
	}

// check for overlap with a command
	if (Cmd_Exists (name))
	{
		Con_Printf("Cvar_Get: %s is a command\n", name);
		return NULL;
	}

// allocate a new cvar, cvar name, and cvar string
// FIXME: these never get Z_Free'd
	cvar = Z_Malloc(sizeof(cvar_t));
//	cvar->flags = flags | CVAR_ALLOCATED;
	cvar->name = Z_Malloc(strlen(name)+1);
	strcpy(cvar->name, name);
	cvar->string = Z_Malloc(strlen(value)+1);
	strcpy(cvar->string, value);
	cvar->value = atof (cvar->string);
//	cvar->integer = (int) cvar->value;

// link the variable in
	cvar->next = cvar_vars;
	cvar_vars = cvar;
	return cvar;
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean	Cvar_Command (void)
{
	cvar_t			*v;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;
		
// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	Cvar_Set (v->name, Cmd_Argv(1));
	return true;
}


/*
============
Cvar_WriteVariables

Writes lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (vfsfile_t *f)
{
	cvar_t	*var;
	
	for (var = cvar_vars ; var ; var = var->next)
		if (var->archive)
		{
			char *temp;
			temp = va("%s \"%s\"\n", var->name, var->string);
			VFS_WRITE(f, temp, strlen(temp));
//			fprintf (f, "%s \"%s\"\n", var->name, var->string);
		}
}

/*
=========
Cvar_List

Displays a list of all cvars similar to Quake3 cvarlist - Eradicator
=========
*/
void Cvar_List_f (void)
{
	cvar_t		*cvar;
	char 		*partial;
	int		len;
	int		count;

	if (Cmd_Argc() > 1)
	{
		partial = Cmd_Argv (1);
		len = Q_strlen(partial);
	}
	else
	{
		partial = NULL;
		len = 0;
	}

	count=0;
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
	{
		if (partial && Q_strncmp (partial,cvar->name, len))
		{
			continue;
		}
		Con_Printf ("\"%s\" is \"%s\"\n", cvar->name, cvar->string);
		count++;
	}

	Con_Printf ("%i cvar(s)", count);
	if (partial)
	{
		Con_Printf (" beginning with \"%s\"", partial);
	}
	Con_Printf ("\n");
}

/*
	CVar_CompleteCountPossible

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com

*/
int Cvar_CompleteCountPossible (const char *partial)
{
	cvar_t	*cvar;
	size_t	len;
	int		h;

	h = 0;
	len = strlen(partial);

	if (!len)
		return	0;

	// Loop through the cvars and count all possible matches
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncasecmp(partial, cvar->name, len))
			h++;

	return h;
}

/*
	CVar_CompleteBuildList

	New function for tab-completion system
	Added by EvilTypeGuy
	Thanks to Fett erich@heintz.com
	Thanks to taniwha

*/
const char **Cvar_CompleteBuildList (const char *partial)
{
	const cvar_t *cvar;
	size_t len = 0;
	size_t bpos = 0;
	size_t sizeofbuf = (Cvar_CompleteCountPossible (partial) + 1) * sizeof (const char *);
	const char **buf;

	len = strlen(partial);
	buf = (const char **)malloc(sizeofbuf + sizeof (const char *));
	// Loop through the alias list and print all matches
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncasecmp(partial, cvar->name, len))
			buf[bpos++] = cvar->name;

	buf[bpos] = NULL;
	return buf;
}

// written by LordHavoc
void Cvar_CompleteCvarPrint (const char *partial)
{
	cvar_t *cvar;
	size_t len = strlen(partial);
	// Loop through the command list and print all matches
	for (cvar = cvar_vars; cvar; cvar = cvar->next)
		if (!strncasecmp(partial, cvar->name, len))
			Con_Printf ("%s : \"%s\"\n", cvar->name, cvar->string);
}

void Cvar_Set_f (void)
{
	cvar_t *cvar;

	// make sure it's the right number of parameters
	if (Cmd_Argc() < 3)
	{
		Con_Printf("Set: wrong number of parameters, usage: set <variablename> <value>\n");
		return;
	}

	// check if it's read-only
	cvar = Cvar_FindVar(Cmd_Argv(1));
/*	if (cvar && cvar->flags & CVAR_READONLY)
	{
		Con_Printf("Set: %s is read-only\n", cvar->name);
		return;
	}*/

	if (developer.value)
		Con_Print("Set: ");

	// all looks ok, create/modify the cvar
	Cvar_Get(Cmd_Argv(1), Cmd_Argv(2), 0);
}

void Cvar_SetA_f (void)
{
	cvar_t *cvar;

	// make sure it's the right number of parameters
	if (Cmd_Argc() < 3)
	{
		Con_Printf("SetA: wrong number of parameters, usage: seta <variablename> <value>\n");
		return;
	}

	// check if it's read-only
	cvar = Cvar_FindVar(Cmd_Argv(1));
/*	if (cvar && cvar->flags & CVAR_READONLY)
	{
		Con_Printf("SetA: %s is read-only\n", cvar->name);
		return;
	}*/

	if (developer.value)
		Con_Print("SetA: ");

	// all looks ok, create/modify the cvar
	Cvar_Get(Cmd_Argv(1), Cmd_Argv(2), 0);
}