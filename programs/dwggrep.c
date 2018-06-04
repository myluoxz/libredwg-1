/*****************************************************************************/
/*  LibreDWG - free implementation of the DWG file format                    */
/*                                                                           */
/*  Copyright (C) 2018 Free Software Foundation, Inc.                        */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 3 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/

/*
 * dwggrep.c: search a string in all text values in a DWG
 * TODO: scan the dwg.spec for all text DXF codes, per object
 *       use pcre2,
 *       create matchers
 *
 * written by Reini Urban
 */

#include "../src/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dwg.h>
#include <dwg_api.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "../src/common.h"
#include "../src/bits.h"
#include "../src/logging.h"
#include "suffix.inc"
static int help(void);
int verbosity(int argc, char **argv, int i, unsigned int *opts);
#include "common.inc"

char buf[4096];
/* the current version per spec block */
static unsigned int cur_ver = 0;

#undef USE_MATCH_CONTEXT
/* pcre2_compile */
static pcre2_code_8 *ri;
static pcre2_match_data_8 *match_data;
static pcre2_match_context_8 *match_context = NULL;

#ifdef USE_MATCH_CONTEXT
static pcre2_jit_stack *jit_stack = NULL;
static pcre2_compile_context_8 *compile_context = NULL;
#endif
#define PUBLIC_JIT_MATCH_OPTIONS \
   (PCRE2_NO_UTF_CHECK|PCRE2_NOTBOL|PCRE2_NOTEOL|PCRE2_NOTEMPTY|\
    PCRE2_NOTEMPTY_ATSTART|PCRE2_PARTIAL_SOFT|PCRE2_PARTIAL_HARD)

static int usage(void) {
  printf("\nUsage: dwggrep [-cRr] pattern *.dwg\n");
  return 1;
}
static int opt_version(void) {
  printf("dwggrep %s\n", PACKAGE_VERSION);
  return 0;
}
static int help(void) {
  printf("\nUsage: dwggrep [OPTIONS]... pattern files\n");
  printf("Search regex pattern in a list of DWGs.\n"
         "\n");
  printf("  --type NAME               Search only NAME entities or objects.\n");
  printf("  --dxf NUM                 Search only DXF group NUM fields.\n");
  printf("  --text                    Search only in TEXT-like entities.\n");
  printf("  --tables                  Search only in table names.\n");
  printf("  -c, --count               Print only the count of matched elements.\n");
  printf("  -R, -r, --recursive       Recursively search subdirectories listed.\n");
  printf("      --help                Display this help and exit\n");
  printf("      --version             Output version information and exit\n"
         "\n");
  printf("GNU LibreDWG online manual: <https://www.gnu.org/software/libredwg/>\n");
  return 0;
}

static void
do_match (char *filename, char *entity, int dxf, char* text, int textlen)
{
  int found = pcre2_jit_match_8(ri, (PCRE2_SPTR8)text, textlen, 0,
                                PUBLIC_JIT_MATCH_OPTIONS,
                                match_data, /* block for storing the result */
                                match_context);
  if (found)
    printf("%s %s %d: %s\n", filename, entity, dxf, text);
}
  
static
void match_TEXT(char* filename, Dwg_Object* obj)
{
  char *text = obj->tio.entity->tio.TEXT->text_value;
  int textlen = strlen(text);
  do_match(filename, "TEXT", 1, text, textlen);
}
static
void match_ATTRIB(char* filename, Dwg_Object* obj)
{
  char *text = obj->tio.entity->tio.ATTRIB->text_value;
  int textlen = strlen(text);
  do_match(filename, "ATTRIB", 1, text, textlen);
}
static
void match_MTEXT(char* filename, Dwg_Object* obj)
{
  char *text = obj->tio.entity->tio.MTEXT->text;
  int textlen = strlen(text);
  do_match(filename, "MTEXT", 1, text, textlen);
}

static
void match_BLOCK_HEADER(char* filename, Dwg_Object_Ref* ref)
{
  Dwg_Object* obj;
  Dwg_Object_BLOCK_HEADER* hdr;

  if (!ref || !ref->obj || !ref->obj->tio.object)
    return;
  hdr = ref->obj->tio.object->tio.BLOCK_HEADER;
  obj = get_first_owned_object(ref->obj, hdr);
  while (obj)
    {
      if (obj->type == DWG_TYPE_TEXT)
        match_TEXT(filename, obj);
      else if (obj->type == DWG_TYPE_ATTRIB)
        match_ATTRIB(filename, obj);
      else if (obj->type == DWG_TYPE_MTEXT)
        match_MTEXT(filename, obj);
      obj = get_next_owned_object(ref->obj, obj, hdr);
    }
}

int
main (int argc, char *argv[])
{
  int error = 0;
  int i = 1, j;
  char* filename;
  Dwg_Data dwg;
  Bit_Chain dat;
  short dxf[10];
  char* objtype[10];
  short numdxf = 0;
  short numtype = 0;
  char *pattern;
  int plen;
  int errcode;
  PCRE2_SIZE erroffset;
  /* pcre_compile */
  int options = PCRE2_DUPNAMES;
  int have_jit, found;

  // check args
  if (argc < 2)
    return usage();

  memset(dxf, 0, 10*sizeof(short));
  if (argc > 2 && !strcmp(argv[i], "--type"))
    {
      if (numtype>=10) exit(1);
      objtype[numtype++] = argv[i+1];
      argc -= 2; i += 2;
    }
  if (argc > 2 && !strcmp(argv[i], "--dxf"))
    {
      if (numdxf>=10) exit(1);
      dxf[numdxf++] = (short)strtol(argv[i+1], NULL, 10);
      argc -= 2; i += 2;
    }
  if (argc > 1 && !strcmp(argv[i], "--help"))
    return help();
  if (argc > 1 && !strcmp(argv[i], "--version"))
    return opt_version();

  if (argc < 3) // and no -R given
    return usage();

  options |= PCRE2_CASELESS;  /* -i */
  options |= PCRE2_EXTENDED;  /* -x */
  pattern = argv[i]; plen = strlen(pattern);
  pcre2_config_8(PCRE2_CONFIG_JIT, &have_jit);

  ri = pcre2_compile_8(
     (PCRE2_SPTR8)pattern, plen, /* pattern */
     options,      /* options */
     &errcode,     /* errors */
     &erroffset,   /* error offset */
#ifdef USE_MATCH_CONTEXT
     compile_context
#else
     NULL
#endif
    );
  match_data = pcre2_match_data_create_from_pattern_8(ri, NULL);
  pcre2_jit_compile_8(ri, PCRE2_JIT_COMPLETE); /* no partial matches */

  //for all filenames...
  for (j=1; j<i; j++)
    {
      long k;
      dwg_obj_block_header *block_hdr;
      dwg_obj_block_control *block_ctrl;

      filename = argv[i];
      memset(&dwg, 0, sizeof(Dwg_Data));
      dwg.opts = 0;
      error = dwg_read_file(filename, &dwg);
      if (error)
        {
          fprintf(stderr, "Error: Could not read DWG file %s.\n", filename);
          continue;
        }

      //TODO: for r2007+ use pcre2-16, before use pcre2-8

      block_hdr = dwg_get_block_header(&dwg, &error);
      block_ctrl = dwg_block_header_get_block_control(block_hdr, &error);
      for (k=0; k < block_ctrl->num_entries; k++)
        {
          match_BLOCK_HEADER(filename, block_ctrl->block_headers[k]);
        }
      match_BLOCK_HEADER(filename, block_ctrl->model_space);
      match_BLOCK_HEADER(filename, block_ctrl->paper_space);

      dwg_free(&dwg);
    }

  return error;
}
