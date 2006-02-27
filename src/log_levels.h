/**
 * @file log_levels.h
 *
 * Enum for log levels.
 * Use these for the log severities in LOG_FMT(), etc.
 *
 * $Id$
 */
#ifndef LOG_LEVELS_H_INCLUDED
#define LOG_LEVELS_H_INCLUDED

#include "logmask.h"

enum
{
   LSYS  = 0,
   LERR  = 1,
   LWARN = 2,
   LNOTE = 3,
   LINFO = 4,
   LDATA = 5,

   LCASTS   = 10,    /* align casts */
   LALBR    = 11,    /* align braces */
   LALTD    = 12,    /* Align Typedef */
   LALPP    = 13,    /* align #define */
   LALPROTO = 14,    /* align prototype */
   LALNLC   = 15,    /* align backslash-newline */
   LALTC    = 16,    /* align trailing comments */
   LALADD   = 17,    /* align add */
   LALASS   = 18,    /* align assign */
   LFVD     = 19,    /* fix_var_def */
   LINDENT  = 20,    /* indent_text */
   LNEWLINE = 21,    /* newlines */
   LPF      = 22,    /* Parse Frame */
   LFRMSTK  = 23,    /* Frame Stack */
   LTOK     = 24,    /* Tokenize */
   LALRC    = 25,    /* align right comment */
   LCMTIND  = 26,    /* Comment Indent */
   LINDLINE = 27,    /* indent line */
   LSIB     = 28,    /* Scan IB */
   LRETURN  = 29,    /* add/remove parens for return */
   LBRDEL   = 30,    /* brace removal */
   LFCN     = 31,    /* function detection */
   LFCNP    = 32,    /* function parameters */
   LPCU     = 33,    /* parse cleanup */
   LDYNKW   = 34,    /* dynamic keywords */
};

#endif   /* LOG_LEVELS_H_INCLUDED */

