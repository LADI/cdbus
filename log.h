/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * cdbus - libdbus helper library
 *
 * Copyright (C) 2008-2023 Nedko Arnaudov
 * Copyright (C) 2008 Juuso Alasuutari <juuso.alasuutari@gmail.com>
 * Copyright (C) 2002 Robert Ham <rah@bash.sh>
 *
 **************************************************************************
 * This file contains cdbus_log() prototype and related log macros
 **************************************************************************
 *
 * cdbus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * cdbus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cdbus. If not, see <http://www.gnu.org/licenses/>
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __CDBUS_LOG_H__
#define __CDBUS_LOG_H__

#define ANSI_BOLD_ON    "\033[1m"
#define ANSI_BOLD_OFF   "\033[22m"
#define ANSI_COLOR_RED  "\033[31m"
#define ANSI_COLOR_YELLOW "\033[33m"
#define ANSI_RESET      "\033[0m"

#include <stdio.h>

#include "config.h"

/* fallback for old gcc versions,
   http://gcc.gnu.org/onlinedocs/gcc/Function-Names.html */
#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
#  define __func__ __FUNCTION__
# else
#  define __func__ "<unknown>"
# endif
#endif

#ifdef __cplusplus
extern "C"
#endif
void
cdbus_log(
  unsigned int level,
  const char * file,
  unsigned int line,
  const char * func,
  const char * format,
  ...)
#if defined (__GNUC__)
  __attribute__((format(printf, 5, 6)))
#endif
  ;

#define CDBUS_LOG_LEVEL_DEBUG        0
#define CDBUS_LOG_LEVEL_INFO         1
#define CDBUS_LOG_LEVEL_WARN         2
#define CDBUS_LOG_LEVEL_ERROR        3
#define CDBUS_LOG_LEVEL_ERROR_PLAIN  4

#define cdbus_log_debug(fmt, args...)       cdbus_log(CDBUS_LOG_LEVEL_DEBUG,       __FILE__, __LINE__, __func__, fmt, ## args)
#define cdbus_log_info(fmt, args...)        cdbus_log(CDBUS_LOG_LEVEL_INFO,        __FILE__, __LINE__, __func__, fmt, ## args)
#define cdbus_log_warn(fmt, args...)        cdbus_log(CDBUS_LOG_LEVEL_WARN,        __FILE__, __LINE__, __func__, fmt, ## args)
#define cdbus_log_error(fmt, args...)       cdbus_log(CDBUS_LOG_LEVEL_ERROR,       __FILE__, __LINE__, __func__, fmt, ## args)
#define cdbus_log_error_plain(fmt, args...) cdbus_log(CDBUS_LOG_LEVEL_ERROR_PLAIN, __FILE__, __LINE__, __func__, fmt, ## args)

#endif /* __CDBUS_LOG__ */
