/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007-2009 Chris Reinhardt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <stdio.h>
#include <stdbool.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include "mport.h"
#include "mport_private.h"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

void mport_default_msg_cb(const char *msg) 
{
  (void)printf("%s\n", msg);
}

bool mport_is_terminal(void) {
    const char *term = getenv("TERM");
    const char *magus = getenv("MAGUS");
    return (magus == NULL && term != NULL && isatty(fileno(stdout)));
}

bool mport_is_color_terminal(void)
{
  const char *term = getenv("TERM");
  const char *colorterm = getenv("COLORTERM");
  const char *clicolor = getenv("CLICOLOR");

  if (!mport_is_terminal()) {
    return false; // Not a terminal or TERM not set
  }

  // Check if the terminal supports colors
  bool term_supports_color =
    strcmp(term, "dumb") != 0 &&
    strcmp(term, "cons25") != 0;

  bool term_is_256color =
    strcmp(term, "xterm-256color") == 0 ||
    strcmp(term, "screen-256color") == 0 ||
    strcmp(term, "tmux-256color") == 0;

  bool colorterm_support =
    colorterm != NULL &&
    (strcmp(colorterm, "truecolor") == 0 ||
     strcmp(colorterm, "24bit") == 0 ||
     strcmp(colorterm, "yes") == 0);
  bool clicolor_support = clicolor != NULL;
  
  return colorterm_support || term_supports_color || term_is_256color || clicolor_support;
}

int mport_default_confirm_cb(const char *msg, const char *yes, const char *no, int def)
{
  size_t len;
  char *ans;
  bool color_terminal = mport_is_color_terminal();
  
  if (getenv("ASSUME_ALWAYS_YES") != NULL || getenv("MAGUS") != NULL) {
    return (MPORT_OK);
  }

  if(color_terminal) {
    (void)fprintf(stderr, "%s%s (Y/N) [%s]:%s ", KCYN, msg, def == 1 ? yes : no, KNRM);
  } else {
    (void)fprintf(stderr, "%s (Y/N) [%s]: ", msg, def == 1 ? yes : no);
  }
  
  while (1) {
    /* get answer, if just \n, then default. */
    ans = fgetln(stdin, &len);

    if (len == 1) { 
      /* user just hit return */
      return def == 1 ? MPORT_OK : -1;
    }  

    bool answer = mport_check_answer_bool(ans);
    
    if (answer) 
      return (MPORT_OK);
    if (*ans == 'N' || *ans == 'n')
      return (-1);
    
    if (color_terminal) {
      (void)fprintf(stderr, "%sPlease enter yes or no:%s ", KRED, KNRM);
    } else {  
      (void)fprintf(stderr, "Please enter yes or no: ");   
    }
  }
  
  /* Not reached */
  return (MPORT_OK);
}


void mport_default_progress_init_cb(const char *title)
{
  /* do nothing */
  (void)puts(title);
  return;
}


#define ESC "\x1b"
#define BACK ESC "[%iD"
#define UP   ESC "[A"
#define DEL  ESC "[2K"

void mport_default_progress_step_cb(int current, int total, const char *msg)
{
  struct termios term;
  struct winsize win;
  int width, bar_width, bar_on, bar_off;
  double percent;
  char *bar = NULL;

  if (current > total)
    current = total;

  if (!mport_is_terminal() || (tcgetattr(STDIN_FILENO, &term) < 0) || (ioctl(STDIN_FILENO, TIOCGWINSZ, &win) < 0)) {
    /* not a terminal or couldn't get terminal width*/
    (void)printf("%s\n", msg);
    return;
  }

  width = win.ws_col;
  if (width > 10) {
    bar_width = width - 10;
  } else {
    bar_width = 10;
  }

  if ((bar = (char *)calloc(width, sizeof(char))) == NULL) {
    /* no memory, we're outa here */
    (void)printf("%s\n", msg);
    return;
  }

  percent = (double)current / (double)total;
  
  bar_on = (int)(percent * (bar_width - 2));
  bar_off = bar_width - 2 - bar_on;

  bar[0] = '[';
  (void)memset(&(bar[1]), '=', bar_on);
  (void)memset(&(bar[1 + bar_on]), ' ', bar_off);
  bar[1 + bar_on + bar_off] = ']';
  bar[2 + bar_on + bar_off] = 0;
  
  (void)printf(BACK DEL, width);
//  (void)printf("%s\n", msg);
  if (mport_is_color_terminal()) {
    (void)printf("%s%s %3i/100%%%s", KCYN, bar, (int)(percent * 100), KNRM);
  } else {
    (void)printf("%s %3i/100%%", bar, (int)(percent * 100));
  }
  (void)fflush(stdout);
  
  free(bar);
  bar = NULL;
}

void mport_default_progress_free_cb(void) 
{
  (void)printf("\n");
  (void)fflush(stdout);
}

