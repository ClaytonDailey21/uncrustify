/**
 * @file tokenize_cleanup.cpp
 * Looks at simple sequences to refine the chunk types.
 * Examples:
 *  - change '[' + ']' into '[]'/
 *  - detect "version = 10;" vs "version (xxx) {"
 *
 * @author  Ben Gardner
 * @license GPL v2+
 *
 * $Id$
 */

#include "uncrustify_types.h"
#include "prototypes.h"
#include "chunk_list.h"
#include "char_table.h"
#include "unc_ctype.h"
#include <cstring>

static void check_template(chunk_t *start);


void tokenize_cleanup(void)
{
   chunk_t *pc   = chunk_get_head();
   chunk_t *prev = NULL;
   chunk_t *next;
   chunk_t *tmp;
   chunk_t *tmp2;
   bool    in_type_cast = false;

   pc   = chunk_get_head();
   next = chunk_get_next_ncnl(pc);
   while ((pc != NULL) && (next != NULL))
   {
      /* Change '[' + ']' into '[]' */
      if ((pc->type == CT_SQUARE_OPEN) && (next->type == CT_SQUARE_CLOSE))
      {
         pc->type = CT_TSQUARE;
         pc->str  = "[]";
         pc->len  = 2;
         chunk_del(next);
         pc->orig_col_end += 1;
         next = chunk_get_next_ncnl(pc);
      }

      if ((pc->type == CT_DOT) && ((cpd.lang_flags & LANG_ALLC) != 0))
      {
         pc->type = CT_MEMBER;
      }

      /* Determine the version stuff (D only) */
      if (pc->type == CT_VERSION)
      {
         if (next->type == CT_PAREN_OPEN)
         {
            pc->type = CT_IF;
         }
         else
         {
            if (next->type != CT_ASSIGN)
            {
               LOG_FMT(LERR, "%s:%d %s: version: Unexpected token %s\n",
                       cpd.filename, pc->orig_line, __func__, get_token_name(next->type));
               cpd.error_count++;
            }
            pc->type = CT_WORD;
         }
      }

      /**
       * Change CT_BASE before CT_PAREN_OPEN to CT_WORD.
       * public myclass() : base() {
       * }
       */
      if ((pc->type == CT_BASE) && (next->type == CT_PAREN_OPEN))
      {
         pc->type = CT_WORD;
      }

      /**
       * Change CT_WORD after CT_ENUM, CT_UNION, or CT_STRUCT to CT_TYPE
       * Change CT_WORD before CT_WORD to CT_TYPE
       */
      if (next->type == CT_WORD)
      {
         if ((pc->type == CT_ENUM) ||
             (pc->type == CT_UNION) ||
             (pc->type == CT_STRUCT))
         {
            next->type = CT_TYPE;
         }
         if (pc->type == CT_WORD)
         {
            pc->type = CT_TYPE;
         }
      }

      /* change extern to qualifier if extern isn't followed by a string or
       * an open paren
       */
      if (pc->type == CT_EXTERN)
      {
         if (next->type == CT_STRING)
         {
            /* Probably 'extern "C"' */
         }
         else if (next->type == CT_PAREN_OPEN)
         {
            /* Probably 'extern (C)' */
         }
         else
         {
            /* Something else followed by a open brace */
            tmp = chunk_get_next_ncnl(next);
            if ((tmp != NULL) || (tmp->type != CT_BRACE_OPEN))
            {
               pc->type = CT_QUALIFIER;
            }
         }
      }

      /**
       * Change CT_STAR to CT_PTR_TYPE if preceeded by CT_TYPE,
       * CT_QUALIFIER, or CT_PTR_TYPE.
       */
      if ((next->type == CT_STAR) &&
          ((pc->type == CT_TYPE) ||
           (pc->type == CT_QUALIFIER) ||
           (pc->type == CT_PTR_TYPE)))
      {
         next->type = CT_PTR_TYPE;
      }

      if ((pc->type == CT_TYPE_CAST) &&
          (next->type == CT_ANGLE_OPEN))
      {
         next->parent_type = CT_TYPE_CAST;
         in_type_cast      = true;
      }

      /**
       * Change angle open/close to CT_COMPARE, if not a template thingy
       */
      if ((pc->type == CT_ANGLE_OPEN) && (pc->parent_type != CT_TYPE_CAST))
      {
         if (pc->flags & PCF_IN_PREPROC)
         {
            pc->type = CT_COMPARE;
         }
         else
         {
            check_template(pc);
         }
      }
      if ((pc->type == CT_ANGLE_CLOSE) && (pc->parent_type != CT_TEMPLATE))
      {
         if (in_type_cast)
         {
            in_type_cast    = false;
            pc->parent_type = CT_TYPE_CAST;
         }
         else
         {
            pc->type = CT_COMPARE;
         }
      }

      if ((cpd.lang_flags & LANG_D) != 0)
      {
         /* Check for the D string concat symbol '~' */
         if ((pc->type == CT_INV) &&
             ((prev->type == CT_STRING) ||
              (prev->type == CT_WORD) ||
              (next->type == CT_STRING)))
         {
            pc->type = CT_CONCAT;
         }

         /* Check for the D template symbol '!' */
         if ((pc->type == CT_NOT) &&
             (prev->type == CT_WORD) &&
             (next->type == CT_PAREN_OPEN))
         {
            pc->type = CT_D_TEMPLATE;
         }
      }

      if ((cpd.lang_flags & LANG_CPP) != 0)
      {
         /* Change Word before '::' into a type */
         if ((pc->type == CT_WORD) && (next->type == CT_DC_MEMBER))
         {
            pc->type = CT_TYPE;
         }
      }

      /* Change get/set to CT_WORD if not followed by a brace open */
      if ((pc->type == CT_GETSET) && (next->type != CT_BRACE_OPEN))
      {
         if ((next->type == CT_SEMICOLON) &&
             ((prev->type == CT_BRACE_CLOSE) ||
              (prev->type == CT_BRACE_OPEN) ||
              (prev->type == CT_SEMICOLON)))
         {
            pc->type          = CT_GETSET_EMPTY;
            next->parent_type = CT_GETSET;
         }
         else
         {
            pc->type = CT_WORD;
         }
      }

      /* Change item after operator (>=, ==, etc) to a CT_FUNCTION */
      if (pc->type == CT_OPERATOR)
      {
         LOG_FMT(LOPERATOR, "%s: %d:%d operator", __func__, pc->orig_line, pc->orig_col);

         /* Handle special case of () operator -- [] already handled */
         if (next->type == CT_PAREN_OPEN)
         {
            tmp = chunk_get_next(next);
            if ((tmp != NULL) && (tmp->type == CT_PAREN_CLOSE))
            {
               next->str         = "()";
               next->len         = 2;
               next->type        = CT_FUNCTION;
               next->parent_type = CT_OPERATOR;
               chunk_del(tmp);
               next->orig_col_end += 1;
               LOG_FMT(LOPERATOR, " ()");
            }
         }
         else
         {
            /* Scan until we hit a '(' or ';' */
            tmp = next;
            while ((tmp != NULL) &&
                   (tmp->type != CT_PAREN_OPEN) &&
                   (tmp->type != CT_SEMICOLON))
            {
               LOG_FMT(LOPERATOR, " %.*s", tmp->len, tmp->str);
               tmp = chunk_get_next_ncnl(tmp);
            }

            if ((tmp != NULL) && (tmp->type == CT_PAREN_OPEN))
            {
               /* Mark chunks between 'operator' and '('.
                * If 'next' is a WORD, then the last 'type' present
                * is the function name. Otherwise, the item after the
                * 'operator' is the function name.
                */
               tmp2 = next;
               if ((next->flags & PCF_PUNCTUATOR) == 0)
               {
                  tmp = chunk_get_next_ncnl(next);
                  while ((tmp != NULL) && (tmp->type != CT_PAREN_OPEN))
                  {
                     tmp->parent_type = CT_OPERATOR;
                     make_type(tmp);
                     if (tmp->type == CT_TYPE)
                     {
                        tmp2 = tmp;
                     }
                     tmp = chunk_get_next_ncnl(tmp);
                  }
                  if (tmp2->type != CT_TYPE)
                  {
                     tmp2 = next;
                  }
               }

               LOG_FMT(LOPERATOR, " [%.*s]", tmp2->len, tmp2->str);

               tmp2->type        = CT_FUNCTION;
               tmp2->parent_type = CT_OPERATOR;
            }
            LOG_FMT(LOPERATOR, "\n");
         }
         if (chunk_is_addr(prev))
         {
            prev->type = CT_BYREF;
         }
      }

      /* Change private, public, protected into either a qualifier or label */
      if (pc->type == CT_PRIVATE)
      {
         /* Handle Qt slots - maybe should just check for a CT_WORD? */
         if (chunk_is_str(next, "slots", 5))
         {
            tmp = chunk_get_next(next);
            if ((tmp != NULL) && (tmp->type == CT_COLON))
            {
               next = tmp;
            }
         }
         if (next->type == CT_COLON)
         {
            next->type = CT_PRIVATE_COLON;
            if ((tmp = chunk_get_next_ncnl(next)) != NULL)
            {
               tmp->flags |= PCF_STMT_START | PCF_EXPR_START;
            }
         }
         else
         {
            pc->type = chunk_is_str(pc, "signals", 7) ? CT_WORD : CT_QUALIFIER;
         }
      }

      /* Look for <newline> 'EXEC' 'SQL' */
      if (chunk_is_str(pc, "EXEC", 4) && chunk_is_str(next, "SQL", 3))
      {
         tmp = chunk_get_prev(pc);
         if (chunk_is_newline(tmp))
         {
            tmp = chunk_get_next(next);
            if (chunk_is_str_case(tmp, "BEGIN", 5))
            {
               pc->type = CT_SQL_BEGIN;
            }
            else if (chunk_is_str_case(tmp, "END", 3))
            {
               pc->type = CT_SQL_END;
            }
            else
            {
               pc->type = CT_SQL_EXEC;
            }

            /* Change words into CT_SQL_WORD until CT_SEMICOLON */
            while (tmp != NULL)
            {
               if (tmp->type == CT_SEMICOLON)
               {
                  break;
               }
               if ((tmp->len > 0) && isalpha(*tmp->str))
               {
                  tmp->type = CT_SQL_WORD;
               }
               tmp = chunk_get_next_ncnl(tmp);
            }
         }
      }

      /* Detect Objective C class name */
      if ((pc->type == CT_OC_IMPL) || (pc->type == CT_OC_INTF))
      {
         next->type        = CT_OC_CLASS;
         next->parent_type = pc->type;

         tmp = chunk_get_next_ncnl(next);
         if (tmp != NULL)
         {
            tmp->flags |= PCF_STMT_START | PCF_EXPR_START;
         }

         tmp = chunk_get_next_type(pc, CT_OC_END, pc->level);
         if (tmp != NULL)
         {
            tmp->parent_type = pc->type;
         }
      }

      /**
       * Objective C @dynamic and @synthesize
       *  @dynamic xxx, yyy;
       *  @synthesize xxx, yyy;
       * Just find the semicolon and mark it.
       */
      if (pc->type == CT_OC_DYNAMIC)
      {
         tmp = chunk_get_next_type(pc, CT_SEMICOLON, pc->level);
         if (tmp != NULL)
         {
            tmp->parent_type = pc->type;
         }
      }

      /* Detect Objective C @property
       *  @property NSString *stringProperty;
       *  @property(nonatomic, retain) NSMutableDictionary *shareWith;
       */
      if (pc->type == CT_OC_PROPERTY)
      {
         if (next->type != CT_PAREN_OPEN)
         {
            next->flags |= PCF_STMT_START | PCF_EXPR_START;
         }
         else
         {
            next->parent_type = pc->type;

            tmp = chunk_get_next_type(pc, CT_PAREN_CLOSE, pc->level);
            if (tmp != NULL)
            {
               tmp->parent_type = pc->type;
               tmp = chunk_get_next_ncnl(tmp);
               if (tmp != NULL)
               {
                  tmp->flags |= PCF_STMT_START | PCF_EXPR_START;

                  tmp = chunk_get_next_type(tmp, CT_SEMICOLON, pc->level);
                  if (tmp != NULL)
                  {
                     tmp->parent_type = pc->type;
                  }
               }
            }
         }
      }

      /* Handle special preprocessor junk */
      if (pc->type == CT_PREPROC)
      {
         pc->parent_type = next->type;
      }

      /* Detect "pragma region" and "pragma endregion" */
      if ((pc->type == CT_PP_PRAGMA) && (next->type == CT_PREPROC_BODY))
      {
         if ((memcmp(next->str, "region", 6) == 0) ||
             (memcmp(next->str, "endregion", 9) == 0))
         {
            pc->type = (*next->str == 'r') ? CT_PP_REGION : CT_PP_ENDREGION;

            prev->parent_type = pc->type;
         }
      }

      /* Check for C# nullable types '?' is in next */
      if ((cpd.lang_flags & LANG_CS) &&
          (next->type == CT_QUESTION) &&
          (next->orig_col == (pc->orig_col + pc->len)))
      {
         tmp = chunk_get_next_ncnl(next);
         if (tmp != NULL)
         {
            bool doit = ((tmp->type == CT_PAREN_CLOSE) ||
                         (tmp->type == CT_ANGLE_CLOSE));

            if (tmp->type == CT_WORD)
            {
               tmp2 = chunk_get_next_ncnl(tmp);
               if ((tmp2 != NULL) &&
                   ((tmp2->type == CT_SEMICOLON) ||
                    (tmp2->type == CT_ASSIGN) ||
                    (tmp2->type == CT_COMMA)))
               {
                  doit = true;
               }
            }

            if (doit)
            {
               pc->len++;
               chunk_del(next);
               next = tmp;
            }
         }
      }

      /* Convert '>' + '>' into '>>' */
      if ((cpd.lang_flags & LANG_CS) &&
          (pc->type == CT_ANGLE_CLOSE) &&
          (next->type == CT_ANGLE_CLOSE) &&
          (pc->parent_type == CT_NONE) &&
          ((pc->orig_col + pc->len) == next->orig_col) &&
          (next->parent_type == CT_NONE))
      {
         pc->len++;
         pc->type = CT_ARITH;
         tmp = chunk_get_next_ncnl(next);
         chunk_del(next);
         next = tmp;
      }

      /* TODO: determine other stuff here */

      prev = pc;
      pc   = next;
      next = chunk_get_next_ncnl(pc);
   }
}

/**
 * If there is nothing but CT_WORD and CT_MEMBER, then it's probably a
 * template thingy.  Otherwise, it's likely a comparison.
 */
static void check_template(chunk_t *start)
{
   chunk_t *pc;
   chunk_t *end;
   chunk_t *prev;
   chunk_t *next;
   bool    in_if = false;

   LOG_FMT(LTEMPL, "%s: Line %d, col %d:", __func__, start->orig_line, start->orig_col);

   prev = chunk_get_prev_ncnl(start);
   if (prev == NULL)
   {
      return;
   }

   if (prev->type == CT_TEMPLATE)
   {
      LOG_FMT(LTEMPL, " CT_TEMPLATE:");

      /* We have: "template< ... >", which is a template declaration */
      int level = 1;
      for (pc = chunk_get_next_ncnl(start); pc != NULL; pc = chunk_get_next_ncnl(pc))
      {
         LOG_FMT(LTEMPL, " [%s,%d]", get_token_name(pc->type), level);

         if (chunk_is_str(pc, "<", 1))
         {
            level++;
         }
         else if (chunk_is_str(pc, ">", 1))
         {
            level--;
            if (level == 0)
            {
               break;
            }
         }
      }
      end = pc;
   }
   else
   {
      /* We may have something like "a< ... >", which is a template use
       * '...' may consist of anything except braces {}, a semicolon, and
       * unbalanced parens.
       * if we are inside an 'if' statement and hit a CT_BOOL, then it isn't a
       * template.
       */

      /* A template requires a word/type right before the open angle */
      if ((prev->type != CT_WORD) && (prev->type != CT_TYPE) && (prev->parent_type != CT_OPERATOR))
      {
         LOG_FMT(LTEMPL, " - after %s + ( - Not a template\n", get_token_name(prev->type));
         start->type = CT_COMPARE;
         return;
      }

      LOG_FMT(LTEMPL, " - prev %s -", get_token_name(prev->type));

      /* Scan back and make sure we aren't inside square parens */
      pc = start;
      while ((pc = chunk_get_prev_ncnl(pc)) != NULL)
      {
         if ((pc->type == CT_SEMICOLON) ||
             (pc->type == CT_BRACE_OPEN) ||
             (pc->type == CT_BRACE_CLOSE) ||
             (pc->type == CT_SQUARE_CLOSE) ||
             (pc->type == CT_SEMICOLON))
         {
            break;
         }
         if (pc->type == CT_IF)
         {
            in_if = true;
            break;
         }
         if (pc->type == CT_SQUARE_OPEN)
         {
            LOG_FMT(LTEMPL, " - Not a template: after a square open\n");
            start->type = CT_COMPARE;
            return;
         }
      }

      /* Scan forward to the angle close
       * If we have a comparison in there, then it can't be a template.
       */
      c_token_t tokens[16];
      int       num_tokens = 1;

      tokens[0] = CT_ANGLE_OPEN;
      for (pc = chunk_get_next_ncnl(start); pc != NULL; pc = chunk_get_next_ncnl(pc))
      {
         LOG_FMT(LTEMPL, " [%s,%d]", get_token_name(pc->type), num_tokens);

         if (chunk_is_str(pc, "<", 1))
         {
            tokens[num_tokens++] = CT_ANGLE_OPEN;
         }
         else if (chunk_is_str(pc, ">", 1))
         {
            if (--num_tokens <= 0)
            {
               break;
            }
            if (tokens[num_tokens] != CT_ANGLE_OPEN)
            {
               /* unbalanced parens */
               break;
            }
         }
         else if (in_if &&
                  ((pc->type == CT_BOOL) ||
                   (pc->type == CT_COMPARE)))
         {
            break;
         }
         else if ((pc->type == CT_BRACE_OPEN) ||
                  (pc->type == CT_BRACE_CLOSE) ||
                  (pc->type == CT_SEMICOLON))
         {
            break;
         }
         else if (pc->type == CT_PAREN_OPEN)
         {
            if (num_tokens >= (int)(ARRAY_SIZE(tokens) - 1))
            {
               break;
            }
            tokens[num_tokens++] = pc->type;
         }
         else if (pc->type == CT_PAREN_CLOSE)
         {
            num_tokens--;
            if (tokens[num_tokens] != (pc->type - 1))
            {
               /* unbalanced parens */
               break;
            }
         }
      }
      end = pc;
   }

   if ((end != NULL) && (end->type == CT_ANGLE_CLOSE))
   {
      pc = chunk_get_next_ncnl(end);
      if ((pc != NULL) &&
          (pc->type != CT_NUMBER))
      {
         LOG_FMT(LTEMPL, " - Template Detected\n");

         start->parent_type = CT_TEMPLATE;

         pc = start;
         while (pc != end)
         {
            next = chunk_get_next_ncnl(pc);
            pc->flags |= PCF_IN_TEMPLATE;
            if (next->type != CT_PAREN_OPEN)
            {
               make_type(pc);
            }
            pc = next;
         }
         end->parent_type = CT_TEMPLATE;
         end->flags      |= PCF_IN_TEMPLATE;
         return;
      }
   }

   LOG_FMT(LTEMPL, " - Not a template: end = %s\n",
           (end != NULL) ? get_token_name(end->type) : "<null>");
   start->type = CT_COMPARE;
}
