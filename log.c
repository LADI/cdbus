/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * cdbus - libdbus helper library
 *
 * Copyright (C) 2023 Nedko Arnaudov
 *
 **************************************************************************
 * This file contains code that implements logging functionality
 **************************************************************************
 *
 * Licensed under the Academic Free License version 2.1
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

#include "log.h"

cdbus_log_function cdbus_log;

__attribute__((visibility("default")))
void cdbus_log_setup(cdbus_log_function logfn)
{
  cdbus_log = logfn;
}
