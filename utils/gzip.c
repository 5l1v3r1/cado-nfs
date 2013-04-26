#include "cado.h"
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "portability.h"
#include "macros.h"
#include "gzip.h"
#include "misc.h"

struct suffix_handler {
    const char * suffix;
    const char * pfmt_in;
    const char * pfmt_out;
};

#if 0
const char * suffix = NULL;

const char * copy_suffix_noalloc(const char * name)
{
    const char * p = strrchr(name, '.');
    if (p == NULL)
        p = name + strlen(name);
    return strdup(p);
}

const char * copy_suffix_alloc(const char * name)
{
    return strdup(copy_suffix_noalloc(name));
}
const char * path_remove_suffix(char * name)
{
    char * p = strrchr(name, '.');
    if (p) *p=0;
    return name;
}

#endif

struct suffix_handler supported_compression_formats[] = {
    { ".gz", "gzip -dc %s", "gzip -c --fast > %s", },
    { ".bz2", "bzip2 -dc %s", "bzip2 -c --fast > %s", },
    { ".lzma", "lzma -dc  %s", "lzma -c -0 > %s", },
    /* These two have to be present */
    { "", NULL, NULL },
    { NULL, NULL, NULL },
};

const char * path_basename(const char * path)
{
    const char *p = strrchr(path, '/');
    if (p == NULL) {
        p = path;
    } else {
        p = p + 1;
    }
    return p;
}

int is_supported_compression_format(const char * s)
{
    struct suffix_handler * r = supported_compression_formats;
    for( ; r->suffix ; r++) {
        if (strcmp(r->suffix, s) == 0)
            return 1;
    }
    return 0;
}

/* Return a unix commands list. Exemple :
   cat file_relation1
   gzip -dc file_relation2.gz file_relation3.gz
   bzip2 -dc file_relation4.gz file_relation5.gz
   [empty string]
*/
char **
prempt_open_compressed_rs (char *rep_cado, char **ficname)
{
  const struct suffix_handler *cp_r = NULL, *r = supported_compression_formats;
  char **cmd;
  size_t s_cmds = 2;
  size_t p_cmds = 0;
  int suffix_choice = 0;
  char lastcom[256];
  char *antebuffer_realpath, *fic_realpath;

  if (!(cmd = calloc (s_cmds, sizeof(unsigned char *)))) {
    fprintf (stderr, "fopen_compressed_rs: calloc erreur : %s\n", strerror(errno));
    exit (1);
  }
  fic_realpath = (char *) malloc(PATH_MAX * sizeof(char));
  antebuffer_realpath = (char *) malloc(PATH_MAX * sizeof(char));
  fic_realpath = (char *) malloc(PATH_MAX * sizeof(char));
  if (fic_realpath == NULL || antebuffer_realpath == NULL || fic_realpath == NULL) {
    fprintf (stderr, "fopen_compressed_rs: malloc error : %s\n", strerror(errno));
    exit (1);
  }
  /* Use fic_realpath as temp storage for un-normalized path to antebuffer */
  strcpy (fic_realpath, rep_cado);
  strcat (fic_realpath, "utils/antebuffer");
#ifdef EXECUTABLE_SUFFIX
  strcat (fic_realpath, EXECUTABLE_SUFFIX);
#endif
  if (realpath(fic_realpath, antebuffer_realpath) == NULL) {
    fprintf (stderr, "fopen_compressed_rs: realpath error : %s\n", strerror(errno));
    exit (1);
  }
  while (*ficname) {
    if (realpath(*ficname, fic_realpath) == NULL) {
        fprintf (stderr, "fopen_compressed_rs: realpath error : %s\n", strerror(errno));
        exit (1);
    }
    if (!suffix_choice) {
      if (p_cmds + 1 >= s_cmds) {
	if (!(cmd = realloc (cmd, sizeof(unsigned char *) * (s_cmds<<1)))) {
	  fprintf (stderr, "fopen_compressed_rs: realloc erreur : %s\n", strerror(errno));
	  exit (1);
	}
	memset(&cmd[s_cmds], 0, sizeof(unsigned char *) * s_cmds);
	s_cmds <<= 1;
      }
      if (!(cmd[p_cmds] = malloc(PREMPT_S_CMD))) {
	fprintf (stderr, "fopen_compressed_rs: malloc erreur : %s\n", strerror(errno));
	exit (1);
      }
      for (cp_r = r ; cp_r->suffix ; cp_r++)
	if (has_suffix (fic_realpath, cp_r->suffix)) break;
      strcpy (cmd[p_cmds], antebuffer_realpath);
      strcat (cmd[p_cmds], " 24 ");
      strcpy (lastcom, " | ");
      strcat (lastcom, cp_r->pfmt_in ? cp_r->pfmt_in : "cat %s");
      strcpy (&(lastcom[strlen(lastcom)-2]), "-"); /* "%s" remplaces by "-" */
      suffix_choice = 1;
      if (strlen (fic_realpath) + strlen (cmd[p_cmds]) >= PREMPT_S_CMD) {
	fprintf(stderr, "prempt_open_compressed_rs: PREMPT_S_CMD (%d) too small. Please * 2\n", PREMPT_S_CMD);
	exit (1);
      }
      strcat (cmd[p_cmds], fic_realpath);
      ficname++;
    }
    else {
      if (has_suffix (fic_realpath, cp_r->suffix) &&
	  (strlen (fic_realpath) + strlen (cmd[p_cmds]) + strlen(lastcom) + 1 < PREMPT_S_CMD))
	{
	  strcat (cmd[p_cmds], " ");
	  strcat (cmd[p_cmds], fic_realpath);
	  ficname++;
	}
      else {
	strcat (cmd[p_cmds], lastcom);
	suffix_choice = 0;
	p_cmds++;
      }
    }
  }
  if (cmd[p_cmds][strlen(cmd[p_cmds])-1] != '-')
    strcat (cmd[p_cmds], lastcom);
  free (antebuffer_realpath);  
  free (fic_realpath);  
  return cmd;
}

/* The pipe capacity is 2^16 by default, we can increase it, but it does not
   seem to make a difference, thus we don't change it by default (patch from
   Alain Filbois). */
#define PIPE_CAPACITY 1UL << 20

FILE*
fopen_maybe_compressed2 (const char * name, const char * mode, int* p_pipeflag, char const ** suf)
{
    const struct suffix_handler * r = supported_compression_formats;
    FILE * f;

    for( ; r->suffix ; r++) {
        if (!has_suffix(name, r->suffix)) continue;
        if (suf) *suf = r->suffix;
        char * command = NULL;
        if (strchr(mode, 'r') && r->pfmt_in) {
            int ret = asprintf(&command, r->pfmt_in, name);
            ASSERT_ALWAYS(ret >= 0);
        } else if (strchr(mode, 'w') && r->pfmt_out) {
            int ret = asprintf(&command, r->pfmt_out, name);
            ASSERT_ALWAYS(ret >= 0);
        }

        if (command) {
            f = popen(command, mode);
            if (p_pipeflag) *p_pipeflag = 1;
#ifdef F_SETPIPE_SZxxx
                /* increase the pipe capacity (2^16 by default), thanks to
                   Alain Filbois */
                fcntl (fileno (f), F_SETPIPE_SZ, PIPE_CAPACITY);
#endif
            free(command);
        } else {
            f = fopen(name, mode);
            if (p_pipeflag) *p_pipeflag = 0;
        }
        return f;
    }
    /* If we arrive here, it's because "" is not among the suffixes */
    abort();
    return NULL;
}


FILE*
fopen_maybe_compressed (const char * name, const char * mode)
{
    return fopen_maybe_compressed2(name, mode, NULL, NULL);
}


void
fclose_maybe_compressed (FILE * f, const char * name)
{
    const struct suffix_handler * r = supported_compression_formats;

    for( ; r->suffix ; r++) {
        if (!has_suffix(name, r->suffix)) continue;
        /* It doesn't really make sense to imagine that one of these two
         * may exist and not the other */
        ASSERT_ALWAYS((r->pfmt_out == NULL) == (r->pfmt_in == NULL));
        if (r->pfmt_in || r->pfmt_out) {
            pclose(f);
        } else {
            fclose(f);
        }
        return;
    }
    /* If we arrive here, it's because "" is not among the suffixes */
    abort();
    return;
}
