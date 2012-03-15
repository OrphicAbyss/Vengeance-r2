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

#if 0// the old code

//jpeg stuff
#include "JCONFIG.h"
#include "JERROR.h"
#include "Jmorecfg.h"
#include "JPEGLIB.h"
#include <math.h>

//jpg loading
byte *LoadJPG (FILE *f)
{
    struct jpeg_decompress_struct cinfo;
    JDIMENSION num_scanlines;
    JSAMPARRAY in;
    struct jpeg_error_mgr jerr;
    int numPixels;
    int row_stride;
    byte *out;
    int count;
    int i;
    //int r, g, b;
    byte *image_rgba;

    // set up the decompression.
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress (&cinfo);

    // inititalize the source
    jpeg_stdio_src (&cinfo, f);

    // initialize decompression
    (void) jpeg_read_header (&cinfo, TRUE);
    (void) jpeg_start_decompress (&cinfo);

    numPixels = cinfo.image_width * cinfo.image_height;

    // initialize the input buffer - we'll use the in-built memory management routines in the
    // JPEG library because it will automatically free the used memory for us when we destroy
    // the decompression structure.  cool.
    row_stride = cinfo.output_width * cinfo.output_components;
    in = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    // bit of error checking
    if (cinfo.output_components != 3)
        goto error;


    // initialize the return data
    image_rgba = malloc ((numPixels * 4));

    // read the jpeg
    count = 0;

    while (cinfo.output_scanline < cinfo.output_height) 
    {
        num_scanlines = jpeg_read_scanlines(&cinfo, in, 1);
        out = in[0];

        for (i = 0; i < row_stride;)
        {
            image_rgba[count++] = out[i++];//r
            image_rgba[count++] = out[i++];//g
            image_rgba[count++] = out[i++];//b
            image_rgba[count++] = 255;
        }
    }

    // finish decompression and destroy the jpeg
    image_width = cinfo.image_width;
    image_height = cinfo.image_height;

    (void) jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress (&cinfo);

    fclose (f);
    return image_rgba;

error:
    // this should rarely (if ever) happen, but just in case...
    Con_DPrintf ("Invalid JPEG Format\n");
    jpeg_destroy_decompress (&cinfo);

    fclose (f);
    return NULL;
}

#endif















#ifndef NO_JPEG
#define XMD_H	//fix for mingw

#if defined(_WIN32)

	#define JPEG_API VARGS
	#include "jpeglib.h"
	#include "jerror.h"

#else

//	#include <jinclude.h>
	#include <jpeglib.h>
	#include <jerror.h>
#endif







struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

/*
 * Here's the routine that will replace the standard error_exit method:
 */

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


/*
 * Sample routine for JPEG decompression.  We assume that the source file name
 * is passed in.  We want to return 1 on success, 0 on error.
 */




/* Expanded data source object for stdio input */

typedef struct {
  struct jpeg_source_mgr pub;	/* public fields */

  qbyte * infile;		/* source stream */
  int currentpos;
  int maxlen;
  JOCTET * buffer;		/* start of buffer */
  boolean start_of_file;	/* have we gotten any data yet? */
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

#define INPUT_BUF_SIZE  4096	/* choose an efficiently fread'able size */


METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  src->start_of_file = TRUE;
}

METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
	my_source_mgr *src = (my_source_mgr*) cinfo->src;
	size_t nbytes;
  
	nbytes = src->maxlen - src->currentpos;
	if (nbytes > INPUT_BUF_SIZE)
		nbytes = INPUT_BUF_SIZE;
	memcpy(src->buffer, &src->infile[src->currentpos], nbytes);
	src->currentpos+=nbytes;

	if (nbytes <= 0) {
		if (src->start_of_file)	/* Treat empty input file as fatal error */
		ERREXIT(cinfo, JERR_INPUT_EMPTY);
		WARNMS(cinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
		src->buffer[0] = (JOCTET) 0xFF;
		src->buffer[1] = (JOCTET) JPEG_EOI;
		nbytes = 2;
	}

	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->start_of_file = FALSE;

	return TRUE;
}


METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  my_source_mgr *src = (my_source_mgr*) cinfo->src;

  if (num_bytes > 0) {
    while (num_bytes > (long) src->pub.bytes_in_buffer) {
      num_bytes -= (long) src->pub.bytes_in_buffer;
      (void) fill_input_buffer(cinfo);
    }
    src->pub.next_input_byte += (size_t) num_bytes;
    src->pub.bytes_in_buffer -= (size_t) num_bytes;
  }
}



METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
}


GLOBAL(void)
jpeg_mem_src (j_decompress_ptr cinfo, qbyte * infile, int maxlen)
{
  my_source_mgr *src;

  if (cinfo->src == NULL) {	/* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(my_source_mgr));
    src = (my_source_mgr*) cinfo->src;
    src->buffer = (JOCTET *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  INPUT_BUF_SIZE * sizeof(JOCTET));
  }

  src = (my_source_mgr*) cinfo->src;
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->pub.term_source = term_source;
  src->infile = infile;
  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */

  src->currentpos = 0;
  src->maxlen = maxlen;
}

qbyte *LoadJPG(vfsfile_t *fin)
{
	qbyte *mem=NULL, *in, *out;
	int i;

  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  struct jpeg_decompress_struct cinfo;
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct my_error_mgr jerr;
  /* More stuff */  
  JSAMPARRAY buffer;		/* Output row buffer */
  int size_stride;		/* physical row width in output buffer */


	qbyte *infile;
	int length;

	length = VFS_GETLEN(fin);
	infile = malloc(length);
	VFS_READ(fin, infile, length);
	VFS_CLOSE(fin);



  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    // If we get here, the JPEG code has signaled an error.

    jpeg_destroy_decompress(&cinfo);    

	if (mem)
		free(mem);
    return 0;
  }  
  jpeg_create_decompress(&cinfo);

  jpeg_mem_src(&cinfo, infile, length);

  (void) jpeg_read_header(&cinfo, TRUE);

  (void) jpeg_start_decompress(&cinfo);


  if (cinfo.output_components!=3)
	  Sys_Error("Bad number of componants in jpeg");
  size_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
   buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, size_stride, 1);

   out=mem=malloc(cinfo.output_height*cinfo.output_width*4);
   memset(out, 0, cinfo.output_height*cinfo.output_width*4);

  while (cinfo.output_scanline < cinfo.output_height) {
    (void) jpeg_read_scanlines(&cinfo, buffer, 1);    

	in = buffer[0];
	for (i = 0; i < cinfo.output_width; i++)
	{//rgb to rgba
		*out++ = *in++;
		*out++ = *in++;
		*out++ = *in++;
		*out++ = 255;	
	}	
  }

  (void) jpeg_finish_decompress(&cinfo);

  jpeg_destroy_decompress(&cinfo);

  image_width      = cinfo.output_width;
  image_height   = cinfo.output_height;

  return mem;

}


#define OUTPUT_BUF_SIZE 4096
typedef struct  {	
  struct jpeg_error_mgr pub;

  jmp_buf setjmp_buffer;
} jpeg_error_mgr_wrapper;

typedef struct {
	struct jpeg_destination_mgr pub;

	vfsfile_t *vfs;


	JOCTET  buffer[OUTPUT_BUF_SIZE];		/* start of buffer */
} my_destination_mgr;

METHODDEF(void) init_destination (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}
METHODDEF(boolean) empty_output_buffer (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	VFS_WRITE(dest->vfs, dest->buffer, OUTPUT_BUF_SIZE);
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

	return TRUE;
}
METHODDEF(void) term_destination (j_compress_ptr cinfo)
{
	my_destination_mgr *dest = (my_destination_mgr*) cinfo->dest;

	VFS_WRITE(dest->vfs, dest->buffer, OUTPUT_BUF_SIZE - dest->pub.free_in_buffer);
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

void jpeg_mem_dest (j_compress_ptr cinfo, vfsfile_t *vfs)
{
  my_destination_mgr *dest;

  if (cinfo->dest == NULL) {	/* first time for this JPEG object? */
    cinfo->dest = (struct jpeg_destination_mgr *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				  sizeof(my_destination_mgr));
	dest = (my_destination_mgr*) cinfo->dest;
//    dest->buffer = (JOCTET *)
//      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
//				  OUTPUT_BUF_SIZE * sizeof(JOCTET));
  }

  dest = (my_destination_mgr*) cinfo->dest;
  dest->pub.init_destination = init_destination;
  dest->pub.empty_output_buffer = empty_output_buffer;
  dest->pub.term_destination = term_destination;
  dest->pub.free_in_buffer = 0; /* forces fill_input_buffer on first read */
  dest->pub.next_output_byte = NULL; /* until buffer loaded */
  dest->vfs = vfs;
}



METHODDEF(void) jpeg_error_exit (j_common_ptr cinfo) {	
  longjmp(((jpeg_error_mgr_wrapper *) cinfo->err)->setjmp_buffer, 1);
}
void screenshotJPEG(char *filename, qbyte *screendata, int screenwidth, int screenheight)	//input is rgb NOT rgba
{
	qbyte	*buffer;
	vfsfile_t	*outfile;
	jpeg_error_mgr_wrapper jerr;
	struct jpeg_compress_struct cinfo;
	JSAMPROW row_pointer[1];

	if (!(outfile = FS_OpenVFS(filename, "wb", FS_GAMEONLY)))
	{
		FS_CreatePath (filename, FS_GAME);
		if (!(outfile = FS_OpenVFS(filename, "wb", FS_GAMEONLY)))
		{
			Con_Printf("Error opening %s\n", filename);
			return;
		}
	}

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = jpeg_error_exit;
	if (setjmp(jerr.setjmp_buffer))
	{
		jpeg_destroy_compress(&cinfo);
		VFS_CLOSE(outfile);
		FS_Remove(filename, FS_GAME);
		Con_Printf("Failed to create jpeg\n");
		return;
	}
	jpeg_create_compress(&cinfo);

	buffer = screendata;
	
	jpeg_mem_dest(&cinfo, outfile);
	cinfo.image_width = screenwidth; 	
	cinfo.image_height = screenheight;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality (&cinfo, 75/*bound(0, (int) gl_image_jpeg_quality_level.value, 100)*/, true);
	jpeg_start_compress(&cinfo, true);

	while (cinfo.next_scanline < cinfo.image_height)
	{
	    *row_pointer = &buffer[(cinfo.image_height - cinfo.next_scanline - 1) * cinfo.image_width * 3];
	    jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	VFS_CLOSE(outfile);
	jpeg_destroy_compress(&cinfo);
}

#endif