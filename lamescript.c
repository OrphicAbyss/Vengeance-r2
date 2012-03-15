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
 * lamescript.c - LameScript functions implementation 
 **/ 
#include "quakedef.h"
#include "lamescript.h" 

int      lsErrorCode = 0; 
/** 
 *  Removes comments, linefeeds 
 *  and trailling spaces from 
 *  original source text (except for 
 *  qoute-separated '"' expressions) 
 **/ 
token_t   *clearText (token_t *input) 
{ 
	token_t		*output = NULL; 
	int			count = 0; 
	int			skip = 0; 
	char		*i_char, *o_char; 
	int			newLine = 0, comment = 0, qoute = 0; 

	if ((input != NULL) || (input->len > 0)) 
	{
		output = malloc (sizeof(token_t));			//assign space for new token
		output->text = malloc (input->len);			//assign space for input token
		memset (output->text, '\0', input->len);	//set all output to null, so where ever we finish the next char will always be null
		output->len = 0;							//set output length to 0
		count = input->len;							//count of input chars
		i_char = input->text;						//input marker
		o_char = output->text;							//output marker
		
		for (count=input->len; count>0 && i_char != NULL; count--)	//loop for each input char
		{
			skip = false;							//reset the skip bool
			switch (*i_char)
			{
			case   '#':								//comment line
				newLine = false;					//not a blank line --- not sure we really need to toggle this as we are skipping the rest of the line
				comment = true;						//set the comment bool
				i_char++;							//we have done our work move on to next char
				continue;
				break;

			case   '"':								//qoute char
				qoute = !qoute;						//marks the start or end of a qoute so toggle qoute bool
				i_char++;							//move to the next char
				continue;							//we have done our work move on to next char
				break;

			case   '\n':							//new line chars
			case   '\r':
				comment = false;					//new line, no longer in comment section
				newLine = true;						//new line, no text yet
				break;

			case   ' ':								//whitespace chars
			case   '\t':
				skip = 1;							//skip whitespace
				break;

			default:
				if ((*i_char < 32) || (*i_char > 126))
				{
					skip = 1;						//outsite normal char range skip char
				}
				newLine = false;					//had some chars on this line, dont force skip
			}

			if ((comment) || (newLine))				//if its in a comment or if it is a newline char
			{
				skip = 1;							//force skip
			}
			else if (qoute)							//else if we are in a qouted text part
			{
				skip = 0;							//dont skip whitespace or charecters outside normal range
			}

			if (!skip)								//if we aren't skipping this char
			{	
				*o_char = *i_char;					//copy char over
				o_char++;							//move to next output char
				output->len++;						//increase the length of our output
			}
			
			i_char++;								//move to next imput char
		}
	}

	return(output);
}

/** 
 *  Scans the source text for substrings 
 **/ 
token_t   *getToken (char *src, char separator) 
{ 
	token_t   *t; 
	char   *p; 

	//check the src string to make sure it's not null
	if ((src == NULL) || (strlen(src) < 1) || (*src == '\0')) 
	{
		return(NULL); 
	} 

	t = malloc (sizeof(token_t));		//allocate token for this string
	t->text = src;						//default token to to be passed in src
	t->len = 0;							//default token length to 0
	
	//use p to itterate over the passed in src char looking for separator
	for (p = src; (*p != '\0') && (*p != separator); p++)
	{
		t->len++;						//we are using more of the src string in the output token
	} 

	if (*p == '\0') {
		return (NULL);					//we dont have a correct token, p should now point the separator passed in
	} else {
		*p = '\0';						//add a terminator to the string to replace what was the seperator
		return(t);						//return our token
	}
} 

/** 
 *  Scans the source text for pairs key=value;
 *	Also finds if the value text has multiple values seperated by commas
 **/ 
pair_t   *getPair (char *src) 
{ 
	pair_t   *p = NULL;								//will contain the par
	char   *c;										//pointer to position in src string passed

	if (src != NULL)								//as long as src is not null
	{
		c = src;									//start at the beginning of the passed string
		p = malloc (sizeof(pair_t));				//allocate space for pair
		p->key = NULL;
		p->values = NULL;
		p->numValues = 0;

		p->key = getToken (c, '=');					//find first token which will be the key
		if (p->key == NULL)							//if null was returned key couldnt be found
		{
			free(p);								//free allocated pair
			return (NULL);							//return null
		} 
		
		if (p->key->len == 0)						//if the length of the key is 0
		{
			lsErrorCode = LSERR_PAIRNOKEY;			//set an error
			free(p->key);
			free(p);								//free allocated pair
			return (NULL);							//return null
		}

		c += p->key->len + 1;						//go to the end of the first token found
		p->values = getToken (c, ';');				//find values token that go with key
		if (p->values == NULL)						//if incorrect token was returned
		{
			free(p->key);
			free(p);								//free allocated pair
			return (NULL);							//return null
		}
		if (p->values->len == 0)					//if the length of the values is 0
		{ 
			lsErrorCode = LSERR_PAIRNOVALUE;		//set an error
			free(p->key);
			free(p->values);
			free(p);								//free allocated pair
			return (NULL);							//return null
		}

		p->numValues = 1;							//set this pairs number of values for the key to default to 1
		for (c=p->values->text ;*c != '\0'; c++)	//for each char in the values text
		{ 
			if (*c == ',')							//check for a comma
			{
				*c = '\0';
				p->numValues++;						//we found a comma so inc the values count
			}
		}
	} 

	return(p);										//return the pair setup
} 

/** 
 *  Scans the source text for sections 
 **/ 
section_t   *getSection (char *src) 
{
	section_t	*s = NULL; 
	char		*p; 

	if (src != NULL)							//if the src string is not null
	{
		s = malloc (sizeof(section_t));			//allocate space for the section
		s->numPairs = 0;						//we dont have any pairs yet so set it to 0
		s->data = NULL;
		s->name = NULL;
		s->pairs = NULL;
		
		for (p = src; (*p != '\0') && (*p != '{'); p++)	//starting at the start of the src string, check each char until we find the start of our section
		{
			//we are just finding the start of the section, no actual code to run
			//at the end of this loop p will point to null or '{' the section start char
		}
		if (*p == '\0')							//if p points to null char
		{
			free(s);							//we dont have a section part to this section
			return(NULL);						//bail out early and return nothing
		}
		*p = '\0';								//p points to '{' replace it with null so our section name string is terminated
		s->name = src;							//our section name is found, set it
		
		p++;									//move to next char, we now need find the end of the section
		s->data = p;							//section starts here
		for ( ;(*p != '\0') && (*p != '}'); p++)	//starting at correct char, find the end of the section
		{ 
			//searching for the end of this section
		}
		if (*p == '\0'){						//sections should be terminated with a '}'
			free(s);							//if we found a null char insted our section is not a full section, unallocate it
			return(NULL);						//bail out early and return nothing
		}
		*p = '\0';								//set the end of section to be null to terminate the section data string
	} 

	return(s); 
} 


/**
 *	Frees all the memory allocated for a partially loaded lamescript
 */
void	freeHalfLoadedLameScript (ls_t *script)
{
	int	i, j;

	if (script == NULL)							//make sure its not already null
		return;

	for (i=0; i<script->numSections; i++)		//for each of the section
	{
		if (script->sections[i] != NULL)
		{
			if (script->sections[i]->pairs != NULL)
			{
				for (j=0; j<script->sections[i]->numPairs; j++)
				{
					free(script->sections[i]->pairs[j]->key);
					free(script->sections[i]->pairs[j]->values);
					free(script->sections[i]->pairs[j]);
				}
				free(script->sections[i]->pairs);		//unallocate the section pairs
			}
			free(script->sections[i]);
		}
	}
	free(script->sections);						//unallocate the sections
	free(script->buffer->text);
	free(script->buffer);						//unallocate the file loaded
	free(script);								//unallocate the actual script
}

void writeDebugScript (ls_t *script){
	char debugout[64] = "particles.script.out";
	vfsfile_t   *fout;
	int			section, pair, value;
	char		*valueStr, *maxvaluepos;
	
	fout = FS_OpenVFS(debugout, "wb", FS_GAMEONLY);
	if (fout)
	{
		for (section = 0; section < script->numSections; section++)
		{
			VFS_WRITE (fout, script->sections[section]->name, strlen(script->sections[section]->name));
			VFS_WRITE (fout, "\n{\n", strlen("\n{\n"));
			VFS_FLUSH (fout);

			for (pair = 0; pair < script->sections[section]->numPairs; pair++)
			{
				VFS_WRITE (fout, script->sections[section]->pairs[pair]->key->text, script->sections[section]->pairs[pair]->key->len);
				VFS_WRITE (fout, "=", strlen("="));
				VFS_FLUSH (fout);

				valueStr = script->sections[section]->pairs[pair]->values->text;
				maxvaluepos = valueStr + script->sections[section]->pairs[pair]->values->len;
				for (value = 0; value < script->sections[section]->pairs[pair]->numValues && valueStr < maxvaluepos; value++, valueStr += strlen(valueStr) + 1)
				{
					if (value > 0)
					{
						VFS_WRITE (fout, ",", strlen(","));
					}
					VFS_WRITE (fout, valueStr, strlen(valueStr));
					VFS_FLUSH (fout);
				}
				VFS_WRITE (fout, ";\n", strlen(";\n"));
				VFS_FLUSH (fout);
			}
			VFS_WRITE (fout, "}\n", strlen("}\n"));
			VFS_FLUSH (fout);
		}
		VFS_CLOSE (fout);
	}
	//done

}

/** 
 *  Loads a LameScript file 
 *  Several checkings are made 
 *  TODO: make a global error var and 
 *  report errors using it 
 **/ 
ls_t   *loadLameScript (vfsfile_t *script) 
{ 
	token_t		*t_original, *t;
	ls_t			*result = NULL;
	int			i, j, k, r, count, OpenBrace, CloseBrace; 
	char			*p; 

	lsErrorCode = 0;										//no error yet
	t_original = malloc(sizeof(token_t));					//allocate a token to contain input text
	t_original->len = VFS_GETLEN(script);					//calculate length of file
	t_original->text = malloc (t_original->len);			//allocate text buffer for complete file

	VFS_READ (script, t_original->text, t_original->len);	//read in file to buffer
	VFS_CLOSE (script);										//close file

	// clean the script text 
	t = clearText (t_original);								//clean up incoming file text
	free(t_original);										//free original text

	// determine the number of sections 
	i = 0; 
	count = 0; 
	OpenBrace = 0;
	CloseBrace = 0;
	p = t->text;
	while (p[i] != '\0')			//while not null we still have more text
	{
		if (p[i] == '{')			//if this char is an open brace
		{
			OpenBrace++;			//add one to our count
		} 
		else if (p[i] == '}')		//if this char is a close brace
		{
			CloseBrace++;			//add one to our count
		}
		p++;
	}

	if (OpenBrace != CloseBrace)	//if our counts are not equal then there is an incomplete section or malformed section
	{
		lsErrorCode = LSERR_INCOMPLETESECTION;
		return(NULL);
	} else if (OpenBrace == 0)		//if there was not any braces then there were no sections
	{
		lsErrorCode = LSERR_NOSECTIONS;
		return(NULL);
	}
	count = OpenBrace;

	result = malloc (sizeof(ls_t));								//allocate place for our script to return
	result->sections = malloc (sizeof(section_t *) * count);	//allocate place for sections
	memset(result->sections,0,sizeof(section_t *) * count);
	result->numSections = count;								//we have already parsed sections once so we know how many there are
	count = 0;													//reset our count
	i = 0;														//reset our position in the file
	while (i < t->len && t->text[i] != '\0')					//while we have not made it to the end of the file
	{
		result->sections[count] = getSection(&t->text[i]);		//parse the section

		if (result->sections[count] == NULL){
			lsErrorCode = LSERR_INCOMPLETESECTION;
			freeHalfLoadedLameScript(result);
			return(NULL);
		}

		if (strlen(result->sections[count]->name) == 0) 
		{ 
			lsErrorCode = LSERR_SECTNONAME;
			freeHalfLoadedLameScript(result);
			return(NULL);
		} 

		if (strlen(result->sections[count]->data) == 0) 
		{ 
			lsErrorCode = LSERR_SECTNODATA;
			freeHalfLoadedLameScript(result);
			return(NULL);
		}

		// check for duplicated sections
		for (r = 0; r < count; r++)
		{
			if (!strcmp(result->sections[count]->name, result->sections[r]->name))
			{
				lsErrorCode = LSERR_REDEFSECTION;				//we already have a section called this
				freeHalfLoadedLameScript(result);
				return (NULL);									//return nothing
			}
		}

		i += strlen(result->sections[count]->name) + strlen(result->sections[count]->data) + 2;	//move past found section
		
		// counts all pairs into the section
		p = result->sections[count]->data;		//for the data in the found section
		j = 0;									//reset count of pairs
		while (*p != '\0')						//while we have not got to the end of the section
		{
			if (*p == '=')						//if this is an = char we have a pair
			{
				j++;							//increment count
			}

			p++;								//move to next char
		}

		if (j==0)
		{
			lsErrorCode = LSERR_SECTBADDATA;	//no pairs in section, set error code
			freeHalfLoadedLameScript(result);
			return (NULL);						//return nothing
		}

		result->sections[count]->pairs = malloc (sizeof(pair_t *) * j);		//allocate pair structure based on pair count
		memset(result->sections[count]->pairs,0,sizeof(pair_t *) * j);
		result->buffer = t;
		result->sections[count]->numPairs = j;								//set sections number of pairs based on count

		//parse pairs
		j = 0;
		k = 0;
		while (result->sections[count]->data[k] != '\0')
		{
			result->sections[count]->pairs[j] = getPair (&result->sections[count]->data[k]);	//parse the first pair
			if
				(
				(result->sections[count]->pairs[j] == NULL) ||
				(result->sections[count]->pairs[j]->key->len == 0) ||
				(result->sections[count]->pairs[j]->values->len == 0)
				)
			{
				if (lsErrorCode == 0)
				{
					lsErrorCode = LSERR_SECTBADDATA;	//set our error code if one hasn't been set yet
				}
				freeHalfLoadedLameScript(result);
				return (NULL);
			}

			//find duplicate pair key names
			for (r = 0; r < j; r++)
			{
				if (!strcmp(result->sections[count]->pairs[j]->key->text, result->sections[count]->pairs[r]->key->text))
				{
					lsErrorCode = LSERR_REDEFKEY;
					freeHalfLoadedLameScript(result);
					return (NULL);
				}
			}

			//move past pairs data to rest of section text
			k += result->sections[count]->pairs[j]->key->len + result->sections[count]->pairs[j]->values->len + 2;
			j++;
		}

		//add one to our count
		count++;
	}

	//writeDebugScript(result);

	return(result);
} 

/**
 *	Frees all the memory that was allocated in the lamescript that is passed in
 */
void	freeLameScript (ls_t *script)
{
	int	i, j;

	if (script == NULL)							//make sure its not already null
		return;

	for (i=0; i<script->numSections; i++)		//for each of the section
	{
		for (j=0; j<script->sections[i]->numPairs; j++)
		{
			free(script->sections[i]->pairs[j]->key);
			free(script->sections[i]->pairs[j]->values);
		}
		free(script->sections[i]->pairs);		//unallocate the section pairs
	}
	free(script->sections);						//unallocate the sections
	free(script->buffer->text);
	free(script->buffer);						//unallocate the file loaded
	free(script);								//unallocate the actual script
}

/** 
 *  Returns the section index into the script 
 *  or -1 if this section does not exist 
 **/ 
int      hasSection (ls_t *script, char *target) 
{ 
   int   r; 

   if (script != NULL) 
   { 
      if (script->sections != NULL) 
      { 
         for (r = 0; r < script->numSections; r++) 
         { 
            if (script->sections[r]->name != NULL) 
            { 
               if (!strcmp (target, script->sections[r]->name)) 
               { 
                  return r; 
               } 
            } 
         } 
      } 
   } 

   return -1; 
} 

/** 
 * Returns the index to the given section/key combination 
 * or -1 if there is no such combination 
 **/ 
int      hasKey (ls_t *script, char *section, char *target) 
{ 
   int sect;
   int r; 
   sect = hasSection (script, section); 
   if (sect != -1) 
   { 
      if (script->sections[sect]->pairs != NULL) 
      { 
         for (r = 0; r < script->sections[sect]->numPairs; r++) 
         { 
            if (!strcmp (target, script->sections[sect]->pairs[r]->key->text)) 
            {
				//found it
               return r; 
            } 
         } 
      } 
   } 

   return -1; 
} 

// hasKey2 when we already have the sectnum like in getValue
int      hasKey2 (ls_t *script, int sectnum, char *target) 
{ 
	int r;

	if (sectnum != -1)
	{ 
		if (script->sections[sectnum]->pairs != NULL) 
		{ 
			for (r = 0; r < script->sections[sectnum]->numPairs; r++) 
			{ 
				if (!strcmp (target, script->sections[sectnum]->pairs[r]->key->text)) 
				{ 
					//found it
					return r;
				} 
			} 
		} 
	} 

	return -1; 
} 

/** 
 * Returns the value for the desired section and key 
 * Allows to select one into several values associated to a key 
 **/ 
char   *getValue (ls_t *script, char *section, char *key, int index) 
{ 
	int is, ik, index_count;
	char *result = NULL;
	pair_t *pair = NULL;

	is = hasSection (script, section);							//make sure the section exists and get the section number
	ik = hasKey (script, section, key);							//make sure the key exists in that section and get the key number
	if ((is != -1) && (ik != -1))								//if we have both the section and they key number then
	{
		pair = script->sections[is]->pairs[ik];					//pointer to the pair we found
		if (index >= pair->numValues)							//make sure it has our value in it
		{
			return (NULL);
		}

		result = pair->values->text;							//start at the beginning of the values text
		for (index_count = 0; index_count < index; index_count++)	//for each index
		{
			result += strlen(result) + 1;						//move the length of the string + 1 null char
		}

		if (result - pair->values->text > pair->values->len)	//if our result pointer - the pointer to the start of the values string is more than the lenght of the string we are pointing at something else
		{
			return (NULL);										//passed the end of the pairs data
		}
		return (result);										//return our string
	} 

	return (NULL);												//didnt have a section or key match
} 

/** 
 *  Checks if a value exists for the 
 *  specified section/key combination. 
 *  Wildcards (*) are supported only 
 *  in the value list, not the target 
 **/ 
int      hasValue (ls_t *script, char *section, char *key, char *value) 
{ 
   char      *listItem; 
   int         result = -1; 
   unsigned   len, i, count = 0; 

   listItem = getValue (script, section, key, count); 
   while ((listItem != NULL) && (result == -1)) 
   { 
      len = strlen (listItem); 
      if (strlen (value) < len) 
      { 
         len = strlen (value); 
      } 

      i = 0; 
      while (i < len) 
      { 
         if (listItem[i] == '*') 
         { 
            result = count; 
            i = len; 
         } 
         else if (listItem[i] != value[i]) 
         { 
            i = len; 
         } 
         else if (i == len - 1) 
         { 
            result = count; 
         } 

         i++; 
      } 

      listItem = getValue (script, section, key, ++count); 
   } 

   return (result); 
} 
