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

#include "quakedef.h"

extern int image_width;
extern int image_height;

/*
=========================================================

TARGA LOADING

=========================================================
*/
/*
typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
}
TargaHeader;

void PrintTargaHeader(TargaHeader *t)
{
	Con_Printf("TargaHeader:\n");
	Con_Printf("uint8 id_length = %i;\n", t->id_length);
	Con_Printf("uint8 colormap_type = %i;\n", t->colormap_type);
	Con_Printf("uint8 image_type = %i;\n", t->image_type);
	Con_Printf("uint16 colormap_index = %i;\n", t->colormap_index);
	Con_Printf("uint16 colormap_length = %i;\n", t->colormap_length);
	Con_Printf("uint8 colormap_size = %i;\n", t->colormap_size);
	Con_Printf("uint16 x_origin = %i;\n", t->x_origin);
	Con_Printf("uint16 y_origin = %i;\n", t->y_origin);
	Con_Printf("uint16 width = %i;\n", t->width);
	Con_Printf("uint16 height = %i;\n", t->height);
	Con_Printf("uint8 pixel_size = %i;\n", t->pixel_size);
	Con_Printf("uint8 attributes = %i;\n", t->attributes);
}

/*
=============
LoadTGA
=============
* /
byte *LoadTGA (char *file, int matchwidth, int matchheight)
{
	FILE *files;
	int x, y, row_inc, compressed, readpixelcount, red, green, blue, alpha, runlen;
	byte *pixbuf, *image_rgba, *f;
	byte *fin, *enddata;
	TargaHeader targa_header;
	unsigned char palette[256*4], *p;
	int fs_filesize;

	f = COM_LoadTempFile(file);
	fs_filesize = COM_FOpenFile(file,&files);

	if (fs_filesize < 19)
		return NULL;

	enddata = f + fs_filesize;

	targa_header.id_length = f[0];
	targa_header.colormap_type = f[1];
	targa_header.image_type = f[2];

	targa_header.colormap_index = f[3] + f[4] * 256;
	targa_header.colormap_length = f[5] + f[6] * 256;
	targa_header.colormap_size = f[7];
	targa_header.x_origin = f[8] + f[9] * 256;
	targa_header.y_origin = f[10] + f[11] * 256;
	targa_header.width = image_width = f[12] + f[13] * 256;
	targa_header.height = image_height = f[14] + f[15] * 256;
	if (image_width > 4096 || image_height > 4096 || image_width <= 0 || image_height <= 0)
	{
		Con_Printf("LoadTGA: invalid size\n");
		PrintTargaHeader(&targa_header);
		return NULL;
	}
	if ((matchwidth && image_width != matchwidth) || (matchheight && image_height != matchheight))
		return NULL;
	targa_header.pixel_size = f[16];
	targa_header.attributes = f[17];

	fin = f + 18;
	if (targa_header.id_length != 0)
		fin += targa_header.id_length;  // skip TARGA image comment
	if (targa_header.image_type == 2 || targa_header.image_type == 10)
	{
		if (targa_header.pixel_size != 24 && targa_header.pixel_size != 32)
		{
			Con_Printf ("LoadTGA: only 24bit and 32bit pixel sizes supported for type 2 and type 10 images\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
	}
	else if (targa_header.image_type == 1 || targa_header.image_type == 9)
	{
		if (targa_header.pixel_size != 8)
		{
			Con_Printf ("LoadTGA: only 8bit pixel size for type 1, 3, 9, and 11 images supported\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
		if (targa_header.colormap_length != 256)
		{
			Con_Printf ("LoadTGA: only 256 colormap_length supported\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
		if (targa_header.colormap_index)
		{
			Con_Printf ("LoadTGA: colormap_index not supported\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
		if (targa_header.colormap_size == 24)
		{
			for (x = 0;x < targa_header.colormap_length;x++)
			{
				palette[x*4+2] = *fin++;
				palette[x*4+1] = *fin++;
				palette[x*4+0] = *fin++;
				palette[x*4+3] = 255;
			}
		}
		else if (targa_header.colormap_size == 32)
		{
			for (x = 0;x < targa_header.colormap_length;x++)
			{
				palette[x*4+2] = *fin++;
				palette[x*4+1] = *fin++;
				palette[x*4+0] = *fin++;
				palette[x*4+3] = *fin++;
			}
		}
		else
		{
			Con_Printf ("LoadTGA: Only 32 and 24 bit colormap_size supported\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
	}
	else if (targa_header.image_type == 3 || targa_header.image_type == 11)
	{
		if (targa_header.pixel_size != 8)
		{
			Con_Printf ("LoadTGA: only 8bit pixel size for type 1, 3, 9, and 11 images supported\n");
			PrintTargaHeader(&targa_header);
			return NULL;
		}
	}
	else
	{
		Con_Printf ("LoadTGA: Only type 1, 2, 3, 9, 10, and 11 targa RGB images supported, image_type = %i\n", targa_header.image_type);
		PrintTargaHeader(&targa_header);
		return NULL;
	}

	if (targa_header.attributes & 0x10)
	{
		Con_Printf ("LoadTGA: origin must be in top left or bottom left, top right and bottom right are not supported\n");
		return NULL;
	}

	image_rgba = malloc(image_width * image_height * 4);
	if (!image_rgba)
	{
		Con_Printf ("LoadTGA: not enough memory for %i by %i image\n", image_width, image_height);
		return NULL;
	}

	// If bit 5 of attributes isn't set, the image has been stored from bottom to top
	if ((targa_header.attributes & 0x20) == 0)
	{
		pixbuf = image_rgba + (image_height - 1)*image_width*4;
		row_inc = -image_width*4*2;
	}
	else
	{
		pixbuf = image_rgba;
		row_inc = 0;
	}

	compressed = targa_header.image_type == 9 || targa_header.image_type == 10 || targa_header.image_type == 11;
	x = 0;
	y = 0;
	red = green = blue = alpha = 255;
	while (y < image_height)
	{
		// decoder is mostly the same whether it's compressed or not
		readpixelcount = 1000000;
		runlen = 1000000;
		if (compressed && fin < enddata)
		{
			runlen = *fin++;
			// high bit indicates this is an RLE compressed run
			if (runlen & 0x80)
				readpixelcount = 1;
			runlen = 1 + (runlen & 0x7f);
		}

		while((runlen--) && y < image_height)
		{
			if (readpixelcount > 0)
			{
				readpixelcount--;
				red = green = blue = alpha = 255;
				if (fin < enddata)
				{
					switch(targa_header.image_type)
					{
					case 1:
					case 9:
						// colormapped
						p = palette + (*fin++) * 4;
						red = p[0];
						green = p[1];
						blue = p[2];
						alpha = p[3];
						break;
					case 2:
					case 10:
						// BGR or BGRA
						blue = *fin++;
						if (fin < enddata)
							green = *fin++;
						if (fin < enddata)
							red = *fin++;
						if (targa_header.pixel_size == 32 && fin < enddata)
							alpha = *fin++;
						break;
					case 3:
					case 11:
						// greyscale
						red = green = blue = *fin++;
						break;
					}
				}
			}
			*pixbuf++ = red;
			*pixbuf++ = green;
			*pixbuf++ = blue;
			*pixbuf++ = alpha;
			x++;
			if (x == image_width)
			{
				// end of line, advance to next
				x = 0;
				y++;
				pixbuf += row_inc;
			}
		}
	}

	return image_rgba;
}/**/

/*
=============
TARGA LOADING
=============
*/
int fgetLittleShort (FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + b2*256);
}

int fgetLittleLong (FILE *f)
{
	byte	b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}

/*
========
LoadTGA
========
*/
extern int filelength (FILE *f);


/* 
================== 
LoadTGA  enhanced 
================== 
*/ 
typedef struct _TargaHeader 
{ 
   unsigned char    id_length, colormap_type, image_type; 
   unsigned short   colormap_index, colormap_length; 
   unsigned char   colormap_size; 
   unsigned short   x_origin, y_origin, width, height; 
   unsigned char   pixel_size, attributes; 
} TargaHeader; 

TargaHeader      targa_header; 

// Definitions for image types 
#define TGA_Null      0   // no image data 
#define TGA_Map         1   // Uncompressed, color-mapped images 
#define TGA_RGB         2   // Uncompressed, RGB images 
#define TGA_Mono      3   // Uncompressed, black and white images 
#define TGA_RLEMap      9   // Runlength encoded color-mapped images 
#define TGA_RLERGB      10   // Runlength encoded RGB images 
#define TGA_RLEMono      11   // Compressed, black and white images 
#define TGA_CompMap      32   // Compressed color-mapped data, using Huffman, Delta, and runlength encoding 
#define TGA_CompMap4   33   // Compressed color-mapped data, using Huffman, Delta, and runlength encoding. 4-pass quadtree-type process 
// Definitions for interleave flag 
#define TGA_IL_None      0   // non-interleaved 
#define TGA_IL_Two      1   // two-way (even/odd) interleaving 
#define TGA_IL_Four      2   // four way interleaving 
#define TGA_IL_Reserved   3   // reserved 
// Definitions for origin flag 
#define TGA_O_UPPER      0   // Origin in lower left-hand corner 
#define TGA_O_LOWER      1   // Origin in upper left-hand corner 
#define MAXCOLORS 16384 


char *TGAErrorList[] = 
{ 
   // deal with error numbers not set 
   "Generic Error BLAH", 
   "Invalid format in \"header.image_type\"", 
   "Invalid color depth in \"header.pixel_size\"", 
   "Invalid color depth in \"header.colormap_size\"", 
   "(temp1 + temp2 + 1) >= MAXCOLORS", 
   "Failed to allocate Memory for data", 
   // deal with any place where i've set the array index too high... 
   "Generic Error BLAH", 
   "Generic Error BLAH", 
   "Generic Error BLAH", 
   "Generic Error BLAH", 
   "Generic Error BLAH", 
   "Generic Error BLAH", 
   "Generic Error BLAH", 
   "Generic Error BLAH", 
   "Generic Error BLAH", 
}; 

/* 
============= 
LoadTGA 
NiceA*: LoadTGA() from Q2Ice, it supports more formats 

MH - Hacked to the same general functionality as Q1's LoadTGA.  fin is assumed to be already open 
coming in here... 
============= 
*/ 
byte *LoadTGA (vfsfile_t *fin, char *name) 
{ 
   int         w, h, x, y, i, temp1, temp2; 
   int         realrow, truerow, baserow, size, interleave, origin; 
   int         pixel_size, map_idx, mapped, rlencoded, RLE_count, RLE_flag; 
   TargaHeader   header; 
   byte      tmp[2], r, g, b, a, j, k, l; 

   // MH - initied all of these to NULL so we can easily test for freeing them when returning from an error 
   byte *dst = NULL; 
   byte *ColorMap = NULL; 
   byte *data = NULL; 
   byte *pdata = NULL; 
   byte *image_rgba = NULL; 

   // MH - error tracking 
   int TGAErrNum = 0; 

   // MH - copied this down from a param in the original, and made it a regular pointer 
   byte *pic; 

   // MH - added variable for file len 
   unsigned int         flen; 

   // MH - see NULL init comment above 
   image_rgba = NULL; 

   flen = VFS_GETLEN(fin);

   data = (byte *) malloc (flen); 

   if (!data) 
   { 
	  VFS_CLOSE(fin);
      TGAErrNum = 5; 
      goto BadTGA; 
   } 

   VFS_READ(fin, data, flen);

   // MH - we're done with the file now... 
   VFS_CLOSE(fin);

   // MH - back to the original code.  set a pointer to the data pointer 
   pdata = data; 

   // read and validate the header 
   header.id_length = *pdata++; 
   header.colormap_type = *pdata++; 
   header.image_type = *pdata++; 

   tmp[0] = pdata[0]; 
   tmp[1] = pdata[1]; 
   header.colormap_index = LittleShort( *((short *)tmp) ); 
   pdata+=2; 
   tmp[0] = pdata[0]; 
   tmp[1] = pdata[1]; 
   header.colormap_length = LittleShort( *((short *)tmp) ); 
   pdata+=2; 
   header.colormap_size = *pdata++; 
   header.x_origin = LittleShort( *((short *)pdata) ); 
   pdata+=2; 
   header.y_origin = LittleShort( *((short *)pdata) ); 
   pdata+=2; 
   header.width = LittleShort( *((short *)pdata) ); 
   pdata+=2; 
   header.height = LittleShort( *((short *)pdata) ); 
   pdata+=2; 
   header.pixel_size = *pdata++; 
   header.attributes = *pdata++; 

   if( header.id_length ) 
   pdata += header.id_length; 

   // validate TGA type 
   switch( header.image_type ) 
   { 
   case TGA_Map: 
   case TGA_RGB: 
   case TGA_Mono: 
   case TGA_RLEMap: 
   case TGA_RLERGB: 
   case TGA_RLEMono: 
      break; 
   default: 
      // MH - this is handled as a Sys_Error in the original loader but let's fail gracefully here 
      TGAErrNum = 1; 
      goto BadTGA; 
   } 

   // validate color depth 
   switch( header.pixel_size ) 
   { 
   case 8: 
   case 15: 
   case 16: 
   case 24: 
   case 32: 
      break; 
   default: 
      // MH - likewise amigo 
      TGAErrNum = 2; 
      goto BadTGA; 
   } 

   r = g = b = a = l = 0; 

   // if required, read the color map information 
   ColorMap = NULL; 
   mapped = ( header.image_type == TGA_Map || header.image_type == TGA_RLEMap || header.image_type == TGA_CompMap || header.image_type == TGA_CompMap4 ) && header.colormap_type == 1; 
   if( mapped ) 
   { 
      // validate colormap size 
      switch( header.colormap_size ) 
      { 
      case 8: 
      case 16: 
      case 32: 
      case 24: 
         break; 
      default: 
         TGAErrNum = 3; 
         goto BadTGA; 
      } 

      temp1 = header.colormap_index; 
      temp2 = header.colormap_length; 

      if( (temp1 + temp2 + 1) >= MAXCOLORS ) 
      { 
         // MH - let's Sys_Error here too 
         TGAErrNum = 4; 
         goto BadTGA; 
      } 

      ColorMap = (byte *)malloc( MAXCOLORS * 4 ); 

      map_idx = 0; 

      // MH - cleaned up unreadable opening braces style and added some whitespace 
      for( i = temp1; i < temp1 + temp2; ++i, map_idx += 4 ) 
      { 
         // read appropriate number of bytes, break into rgb & put in map 
         switch( header.colormap_size ) 
         { 
         case 8: 
            r = g = b = *pdata++; 
            a = 255; 
            break; 

         case 15: 
            j = *pdata++; 
            k = *pdata++; 
            l = ((unsigned int) k << 8) + j; 
            r = (byte) ( ((k & 0x7C) >> 2) << 3 ); 
            g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 ); 
            b = (byte) ( (j & 0x1F) << 3 ); 
            a = 255; 
            break; 

         case 16: 
            j = *pdata++; 
            k = *pdata++; 
            l = ((unsigned int) k << 8) + j; 
            r = (byte) ( ((k & 0x7C) >> 2) << 3 ); 
            g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 ); 
            b = (byte) ( (j & 0x1F) << 3 ); 
            a = (k & 0x80) ? 255 : 0; 
            break; 

         case 24: 
            b = *pdata++; 
            g = *pdata++; 
            r = *pdata++; 
            a = 255; 
            l = 0; 
            break; 

         case 32: 
            b = *pdata++; 
            g = *pdata++; 
            r = *pdata++; 
            a = *pdata++; 
            l = 0; 
            break; 
         } 

         ColorMap[map_idx + 0] = r; 
         ColorMap[map_idx + 1] = g; 
         ColorMap[map_idx + 2] = b; 
         ColorMap[map_idx + 3] = a; 
      } 
   } 

   // check run-length encoding 
   rlencoded = header.image_type == TGA_RLEMap || header.image_type == TGA_RLERGB || header.image_type == TGA_RLEMono; 
   RLE_count = 0; 
   RLE_flag = 0; 

   w = header.width; 
   h = header.height; 

   size = w * h * 4; 
   pic = (byte *)malloc( size ); 

   // MH - targa_rgba is a global pointer so let's set it to the pic pointer we've just created 
   image_rgba = pic; 

   memset( pic, 0, size ); 

   // read the Targa file body and convert to portable format 
   pixel_size = header.pixel_size; 
   origin = (header.attributes & 0x20) >> 5; 
   interleave = (header.attributes & 0xC0) >> 6; 
   truerow = 0; 
   baserow = 0; 

   // MH - massive cleanup of unreadable opening braces and more whitespace going in here 
   for( y = 0; y < h; y++ ) 
   { 
      realrow = truerow; 
      if( origin == TGA_O_UPPER ) 
      realrow = h - realrow - 1; 

      dst = pic + realrow * w * 4; 

      for( x = 0; x < w; x++ ) 
      { 
         // check if run length encoded 
         if( rlencoded ) 
         { 
            if( !RLE_count ) 
            { 
               // have to restart run 
               i = *pdata++; 
               RLE_flag = (i & 0x80); 

               if( !RLE_flag ) 
               { 
                  // stream of unencoded pixels 
                  RLE_count = i + 1; 
               } 
               else 
               { 
                  // single pixel replicated 
                  RLE_count = i - 127; 
               } 

               // decrement count & get pixel 
               --RLE_count; 
            } 
            else 
            { 
               // have already read count & (at least) first pixel 
               --RLE_count; 
               if( RLE_flag ) 
               { 
                  // replicated pixels 
                  goto PixEncode; 
               } 
            } 
         } 

         // read appropriate number of bytes, break into RGB 
         switch( pixel_size ) 
         { 
         case 8: 
            r = g = b = l = *pdata++; 
            a = 255; 
            break; 

         case 15: 
            j = *pdata++; 
            k = *pdata++; 
            l = ((unsigned int) k << 8) + j; 
            r = (byte) ( ((k & 0x7C) >> 2) << 3 ); 
            g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 ); 
            b = (byte) ( (j & 0x1F) << 3 ); 
            a = 255; 
            break; 

         case 16: 
            j = *pdata++; 
            k = *pdata++; 
            l = ((unsigned int) k << 8) + j; 
            r = (byte) ( ((k & 0x7C) >> 2) << 3 ); 
            g = (byte) ( (((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3 ); 
            b = (byte) ( (j & 0x1F) << 3 ); 
            a = 255; 
            break; 

         case 24: 
            b = *pdata++; 
            g = *pdata++; 
            r = *pdata++; 
            a = 255; 
            l = 0; 
            break; 

         case 32: 
            b = *pdata++; 
            g = *pdata++; 
            r = *pdata++; 
            a = *pdata++; 
            l = 0; 
            break; 

         default: 
            // MH - was this already tested for above? 
            // MH - yup. 
            TGAErrNum = 2; 
            goto BadTGA; 
         } 

PixEncode:; 

         if ( mapped ) 
         { 
            map_idx = l * 4; 

            *dst++ = ColorMap[map_idx + 0]; 
            *dst++ = ColorMap[map_idx + 1]; 
            *dst++ = ColorMap[map_idx + 2]; 
            *dst++ = ColorMap[map_idx + 3]; 
         } 
         else 
         { 
            *dst++ = r; 
            *dst++ = g; 
            *dst++ = b; 
            *dst++ = a; 
         } 
      } 

      if (interleave == TGA_IL_Four) 
         truerow += 4; 
      else if (interleave == TGA_IL_Two) 
         truerow += 2; 
      else truerow++; 

      if (truerow >= h) truerow = ++baserow; 
   } 

   if (mapped)   free( ColorMap ); 

   // MH - release our data pointer. 
   free (data); 

   image_width = w; 
   image_height = h; 

   return image_rgba; 

BadTGA:; 
   // release any memory that was allocated 
   if (ColorMap) free (ColorMap); 
   if (data) free (data); 
   if (image_rgba) free (image_rgba); 

   Con_DPrintf ("&c900LoadTGA:&r %s &c090[%s]&r\n", TGAErrorList[TGAErrNum], name); 
   return NULL; 
}
