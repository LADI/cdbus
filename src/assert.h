/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * cdbus - libdbus helper library
 *
 * Copyright (C) 2009-2023 Nedko Arnaudov
 *
 **************************************************************************
 * This file contains assert macros
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

#ifndef CDBUS_ASSERT_H__F236CB4F_D812_4636_958C_C82FD3781EC8__INCLUDED
#define CDBUS_ASSERT_H__F236CB4F_D812_4636_958C_C82FD3781EC8__INCLUDED

#include <cdbus/log.h>

#include <assert.h>

#define CDBUS_ASSERT(expr)                                            \
  do                                                                  \
  {                                                                   \
    if (!(expr))                                                      \
    {                                                                 \
      cdbus_log_error("ASSERT(" #expr ") failed. function %s in %s:%4u\n",  \
                __FUNCTION__,                                         \
                __FILE__,                                             \
                __LINE__);                                            \
      assert(false);                                                  \
    }                                                                 \
  }                                                                   \
  while(false)

#define CDBUS_ASSERT_NO_PASS                                          \
  do                                                                  \
  {                                                                   \
    cdbus_log_error("Code execution taboo point. function %s in %s:%4u\n",  \
              __FUNCTION__,                                           \
              __FILE__,                                               \
              __LINE__);                                              \
    assert(false);                                                    \
  }                                                                   \
  while(false)

#endif /* #ifndef CDBUS_ASSERT_H__F236CB4F_D812_4636_958C_C82FD3781EC8__INCLUDED */
