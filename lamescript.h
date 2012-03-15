/** 
 * (c) 2005 by Carlos H. P. da Silva. 
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License 
 * as published by the Free Software Foundation; either version 2 
 * of the License, or (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * See the GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. 
 * --------------------------------------------------------------------------- 
 * lamescript.h 
 **/ 

#ifndef   _LAMESCRIPT_H 
#define   _LAMESCRIPT_H   1 
#define   LSVERSION      "1.01" 
#include <stdio.h> 
#include <stdlib.h> 
#include <ctype.h> 
#include <string.h> 

// error codes 
#define   LSERR_PAIRNOKEY			1	//	found a pair without key (ex:"=bar;") 
#define   LSERR_PAIRNOVALUE			2	//	found a pair without value (ex: "foo=;") 
#define   LSERR_SECTNONAME			3	//	found a section without name (ex: "{foo=bar;}") 
#define   LSERR_SECTNODATA			4	//	found a section without pair data (ex: "section1{}") 
#define   LSERR_REDEFSECTION		5	//	found a duplicated section name along the script 
#define   LSERR_SECTBADDATA			6	//	found a section with corrupted pair data (ex:"section1{foobar}") 
#define   LSERR_REDEFKEY			7	//	found a duplicated key name inside the same section 
#define   LSERR_NOSECTIONS			8	//	no sections found into the script 
#define   LSERR_INCOMPLETESECTION	9	//	section was incomplete, missing start or end chars { or }

/** 
 * LameScript has a similar syntax to CSS (cascade 
 * style sheets), allowing very complex combinations. 
 * 
 * LameScript is case-sensitive. There is support for 
 * value expressions terminated by a wildcard ('*'). 
 * 
 * Comment lines in LameScript starts with ('#'). 
 * spaces, tabs and linefeeds are discarded, except if 
 * they are between double commas ('"'). 
 * LameScript supports the following constructions: 
 * 
 * 1) simple key/exact value pairs 
 * key=value; 
 * 2) keys /wildcarded values pairs 
 * key=value*; 
 * 3) composite key/valuelist pairs (including expressions) 
 * key=value1,value2,"all your base are belong to us",foo*; 
 * 
 * 4) data sections (so keys can be repeated) 
 * section1 
 * { 
 *   key1=value1; 
 *   key2=val*; 
 *   key3=foobar,fub*,"all your base are belong to us"; 
 * } 
 * section2 
 * { 
 *   key1=value99; 
 *   key2=foob*,hello*; 
 *   key3="cannot find those weapons of mass destruction"; 
 * } 
 **/ 

// --------------------------------------------------------- 
// LameScript structures 
// points a string into the text buffer 
typedef struct 
{ 
   char      *text; 
   int         len; 
}token_t; 

// points a pair key=value into the text buffer 
typedef struct 
{ 
   token_t      *key; 
   int         numValues; 
   token_t      *values; 
}pair_t; 

// points a section into the text buffer 
typedef struct 
{ 
   char			*name; 
   char			*data; 
   int			numPairs; 
   pair_t		**pairs; 
}section_t; 

// holds a complete script and its pointers 
typedef struct 
{ 
   token_t		*buffer;
   int			size;
   int			numSections;
   section_t	**sections;
}ls_t; 

// --------------------------------------------------------- 
// LameScript globals 
extern   int      lsErrorCode; 

// --------------------------------------------------------- 
// LameScript public API 
ls_t   *loadLameScript (vfsfile_t *script);
void	freeLameScript (ls_t *script);
int      hasSection (ls_t *script, char *target); 
int      hasKey (ls_t *script, char *section, char *target); 
char   *getValue (ls_t *script, char *section, char *key, int index); 
int      hasValue (ls_t *script, char *section, char *key, char *value);
#endif 
