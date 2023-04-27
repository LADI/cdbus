/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*
 * LADI Session Handler (ladish)
 *
 * Copyright (C) 2008,2009,2010,2011,2012 Nedko Arnaudov <nedko@arnaudov.name>
 * Copyright (C) 2008 Juuso Alasuutari <juuso.alasuutari@gmail.com>
 *
 **************************************************************************
 * This file contains code of the D-Bus helpers
 **************************************************************************
 *
 * Licensed under the Academic Free License version 2.1
 *
 * LADI Session Handler is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * LADI Session Handler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LADI Session Handler. If not, see <http://www.gnu.org/licenses/>
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdbool.h>
#include <dbus/dbus.h>
#include <string.h>
#include <stdlib.h>

#include "helpers.h"
#include "method.h"
#include "../common.h"
#include "../common/klist.h"

/* D-Bus versions earlier than 1.4.12 dont define DBUS_TIMEOUT_INFINITE */
#if !defined(DBUS_TIMEOUT_INFINITE)
#define DBUS_TIMEOUT_INFINITE  ((int)0x7fffffff)
#endif

#define DBUS_CALL_DEFAULT_TIMEOUT 3000 // in milliseconds

DBusConnection * cdbus_g_dbus_connection;
DBusError cdbus_g_dbus_error;
static char * g_dbus_call_last_error_name;
static char * g_dbus_call_last_error_message;

struct cdbus_signal_hook_descriptor
{
  struct list_head siblings;
  char * object;
  char * interface;
  void * hook_context;
  const struct cdbus_signal_hook * signal_hooks;
};

struct cdbus_service_descriptor
{
  struct list_head siblings;
  char * service_name;
  void (* lifetime_hook_function)(bool appeared);
  struct list_head hooks;
};

static LIST_HEAD(g_dbus_services);


void cdbus_call_last_error_cleanup(void)
{
  free(g_dbus_call_last_error_name);
  g_dbus_call_last_error_name = NULL;

  free(g_dbus_call_last_error_message);
  g_dbus_call_last_error_message = NULL;
}

bool cdbus_call_last_error_is_name(const char * name)
{
  return g_dbus_call_last_error_name != NULL && strcmp(name, g_dbus_call_last_error_name) == 0;
}

const char * cdbus_call_last_error_get_message(void)
{
  return g_dbus_call_last_error_message != NULL ? g_dbus_call_last_error_message : "";
}

static void cdbus_call_last_error_set(void)
{
  cdbus_call_last_error_cleanup();

  if (cdbus_g_dbus_error.name != NULL)
  {
    g_dbus_call_last_error_name = strdup(cdbus_g_dbus_error.name);
  }

  if (cdbus_g_dbus_error.message != NULL)
  {
    g_dbus_call_last_error_message = strdup(cdbus_g_dbus_error.message);
  }
}

bool cdbus_iter_get_dict_entry(DBusMessageIter * iter_ptr, const char * key, void * value, int * type, int * size)
{
  DBusMessageIter dict_iter;
  DBusMessageIter entry_iter;
  DBusMessageIter variant_iter;
  const char * current_key;
  DBusMessageIter array_iter;
  int n;
  int detype;

  dbus_message_iter_recurse(iter_ptr, &dict_iter);

loop:
  detype = dbus_message_iter_get_arg_type(&dict_iter);

  if (detype == DBUS_TYPE_INVALID)
  {
    return false;
  }

  if (detype != DBUS_TYPE_DICT_ENTRY)
  {
    log_error("Iterator does not point to a dict entry container");
    return false;
  }

  dbus_message_iter_recurse(&dict_iter, &entry_iter);

  if (dbus_message_iter_get_arg_type(&entry_iter) != DBUS_TYPE_STRING)
  {
    log_error("Cannot find key in dict entry container");
    return false;
  }

  dbus_message_iter_get_basic(&entry_iter, &current_key);
  if (strcmp(current_key, key) != 0)
  {
    dbus_message_iter_next(&dict_iter);
    goto loop;
  }

  if (!dbus_message_iter_next(&entry_iter) || dbus_message_iter_get_arg_type(&entry_iter) != DBUS_TYPE_VARIANT)
  {
    log_error("Cannot find variant container in dict entry");
    return false;
  }

  dbus_message_iter_recurse(&entry_iter, &variant_iter);

  *type = dbus_message_iter_get_arg_type(&variant_iter);
  if (*type == DBUS_TYPE_INVALID)
  {
    log_error("Cannot find value in variant container");
    return false;
  }

  if (*type == DBUS_TYPE_ARRAY)
  {
    if (dbus_message_iter_get_element_type(&variant_iter) != DBUS_TYPE_BYTE)
    {
      log_error("Dict entry value is a non-byte array");
      return false;
    }
    *type = '-';

    dbus_message_iter_recurse(&variant_iter, &array_iter);
    dbus_message_iter_get_fixed_array(&array_iter, value, &n);

    if (size != NULL)
    {
      *size = n;
    }
  }
  else
  {
    dbus_message_iter_get_basic(&variant_iter, value);
  }

  return true;
}

bool cdbus_iter_get_dict_entry_string(DBusMessageIter * iter_ptr, const char * key, const char ** value)
{
  int type;

  if (!cdbus_iter_get_dict_entry(iter_ptr, key, value, &type, NULL))
  {
    return false;
  }

  if (type != DBUS_TYPE_STRING)
  {
    log_error("value of the dict entry '%s' is not a string", key);
    return false;
  }

  return true;
}

/*
 * Append a variant type to a D-Bus message.
 * Return false if something fails, true otherwise.
 */
bool cdbus_iter_append_variant(DBusMessageIter * iter, int type, const void * arg)
{
  DBusMessageIter sub_iter;
  char s[2];

  s[0] = (char)type;
  s[1] = '\0';

  /* Open a variant container. */
  if (!dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, (const char *)s, &sub_iter))
    return false;

  /* Append the supplied value. */
  if (!dbus_message_iter_append_basic(&sub_iter, type, arg))
  {
    dbus_message_iter_close_container(iter, &sub_iter);
    return false;
  }

  /* Close the container. */
  if (!dbus_message_iter_close_container(iter, &sub_iter))
    return false;

  return true;
}

static __inline__ bool cdbus_iter_append_variant_raw(DBusMessageIter * iter, const void * buf, int len)
{
  DBusMessageIter variant_iter, array_iter;

  /* Open a variant container. */
  if (!dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT,
                                        "ay", &variant_iter))
    return false;

  /* Open an array container. */
  if (!dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY,
                                        "y", &array_iter))
    goto fail;

  /* Append the supplied data. */
  if (!dbus_message_iter_append_fixed_array(&array_iter, DBUS_TYPE_BYTE, buf, len)) {
    dbus_message_iter_close_container(&variant_iter, &array_iter);
    goto fail;
  }

  /* Close the containers. */
  if (!dbus_message_iter_close_container(&variant_iter, &array_iter))
    goto fail;
  else if (!dbus_message_iter_close_container(iter, &variant_iter))
    return false;

  return true;

fail:
  dbus_message_iter_close_container(iter, &variant_iter);
  return false;
}

bool cdbus_iter_append_dict_entry(DBusMessageIter * iter, int type, const char * key, const void * value, int length)
{
  DBusMessageIter dict_iter;

  if (!dbus_message_iter_open_container(iter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_iter))
    return false;

  if (!dbus_message_iter_append_basic(&dict_iter, DBUS_TYPE_STRING, &key))
    goto fail;

  if (type == '-')
  {
    if (!cdbus_iter_append_variant_raw(&dict_iter, value, length))
      goto fail;
  }
  else if (!cdbus_iter_append_variant(&dict_iter, type, value))
  {
    goto fail;
  }

  if (!dbus_message_iter_close_container(iter, &dict_iter))
    return false;

  return true;

fail:
  dbus_message_iter_close_container(iter, &dict_iter);
  return false;
}

bool cdbus_maybe_add_dict_entry_string(DBusMessageIter *dict_iter_ptr, const char * key, const char * value)
{
  DBusMessageIter dict_entry_iter;

  if (value == NULL)
  {
    return true;
  }

  if (!dbus_message_iter_open_container(dict_iter_ptr, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry_iter))
  {
    return false;
  }

  if (!dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, (const void *) &key))
  {
    dbus_message_iter_close_container(dict_iter_ptr, &dict_entry_iter);
    return false;
  }

  cdbus_iter_append_variant(&dict_entry_iter, DBUS_TYPE_STRING, &value);

  if (!dbus_message_iter_close_container(dict_iter_ptr, &dict_entry_iter))
  {
    return false;
  }

  return true;
}

bool cdbus_add_dict_entry_uint32(DBusMessageIter * dict_iter_ptr, const char * key, dbus_uint32_t value)
{
  DBusMessageIter dict_entry_iter;

  if (!dbus_message_iter_open_container(dict_iter_ptr, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry_iter))
  {
    return false;
  }

  if (!dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, (const void *) &key))
  {
    dbus_message_iter_close_container(dict_iter_ptr, &dict_entry_iter);
    return false;
  }

  cdbus_iter_append_variant(&dict_entry_iter, DBUS_TYPE_UINT32, &value);

  if (!dbus_message_iter_close_container(dict_iter_ptr, &dict_entry_iter))
  {
    return false;
  }

  return true;
}

bool cdbus_add_dict_entry_bool(DBusMessageIter * dict_iter_ptr, const char * key, dbus_bool_t value)
{
  DBusMessageIter dict_entry_iter;

  if (!dbus_message_iter_open_container(dict_iter_ptr, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry_iter))
  {
    return false;
  }

  if (!dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, (const void *) &key))
  {
    dbus_message_iter_close_container(dict_iter_ptr, &dict_entry_iter);
    return false;
  }

  cdbus_iter_append_variant(&dict_entry_iter, DBUS_TYPE_BOOLEAN, &value);

  if (!dbus_message_iter_close_container(dict_iter_ptr, &dict_entry_iter))
  {
    return false;
  }

  return true;
}

DBusMessage *
cdbus_call_raw(
  unsigned int timeout,
  DBusMessage * request_ptr)
{
  DBusMessage * reply_ptr;

  if (timeout == 0)
  {
    timeout = DBUS_CALL_DEFAULT_TIMEOUT;
  }

  reply_ptr = dbus_connection_send_with_reply_and_block(
    cdbus_g_dbus_connection,
    request_ptr,
    timeout,
    &cdbus_g_dbus_error);
  if (reply_ptr == NULL)
  {
    cdbus_call_last_error_set();
    dbus_error_free(&cdbus_g_dbus_error);
  }

  return reply_ptr;
}

static
DBusMessage *
cdbus_new_method_call_message_valist(
  const char * service,
  const char * object,
  const char * iface,
  const char * method,
  const char * input_signature,
  va_list * vargs_ptr)
{
  DBusMessage * msg_ptr;
  DBusSignatureIter sig_iter;
  int type;
  void * parameter_ptr;
  DBusMessageIter iter;

  if (!dbus_signature_validate(input_signature, NULL))
  {
    log_error("input signature '%s' is invalid", input_signature);
    goto fail;
  }

  dbus_signature_iter_init(&sig_iter, input_signature);

  msg_ptr = dbus_message_new_method_call(service, object, iface, method);
  if (msg_ptr == NULL)
  {
    log_error("dbus_message_new_method_call() failed.");
    goto fail;
  }

  dbus_message_iter_init_append(msg_ptr, &iter);

  while (*input_signature != '\0')
  {
    type = dbus_signature_iter_get_current_type(&sig_iter);
    if (!dbus_type_is_basic(type))
    {
      log_error("non-basic input parameter '%c' (%d)", *input_signature, type);
      goto unref;
    }

    parameter_ptr = va_arg(*vargs_ptr, void *);

    if (!dbus_message_iter_append_basic(&iter, type, parameter_ptr))
    {
      log_error("dbus_message_iter_append_basic() failed.");
      goto unref;
    }

    dbus_signature_iter_next(&sig_iter);
    input_signature++;
  }

  return msg_ptr;
unref:
  dbus_message_unref(msg_ptr);
fail:
  return NULL;
}

DBusMessage *
cdbus_new_method_call_message(
  const char * service,
  const char * object,
  const char * iface,
  const char * method,
  const char * input_signature,
  ...)
{
  va_list vargs;
  DBusMessage * msg_ptr;

  va_start(vargs, input_signature);
  msg_ptr = cdbus_new_method_call_message_valist(service, object, iface, method, input_signature, &vargs);
  va_end(vargs);

  return msg_ptr;
}

bool
cdbus_call(
  unsigned int timeout,
  const char * service,
  const char * object,
  const char * iface,
  const char * method,
  const char * input_signature,
  ...)
{
  DBusMessageIter iter;
  DBusMessage * request_ptr;
  DBusMessage * reply_ptr;
  const char * output_signature;
  const char * reply_signature;
  va_list ap;
  bool ret;
  void * parameter_ptr;

  //log_info("dbus_call('%s', '%s', '%s', '%s')", service, object, iface, method);

  ret = false;
  va_start(ap, input_signature);

  if (input_signature != NULL)
  {
    request_ptr = cdbus_new_method_call_message_valist(service, object, iface, method, input_signature, &ap);
    if (request_ptr == NULL)
    {
      goto fail;
    }
  }
  else
  {
    request_ptr = va_arg(ap, DBusMessage *);
  }

  output_signature = va_arg(ap, const char *);

  reply_ptr = cdbus_call_raw(timeout, request_ptr);

  if (input_signature != NULL)
  {
    dbus_message_unref(request_ptr);
  }

  if (reply_ptr == NULL)
  {
    goto fail;
  }

  if (output_signature != NULL)
  {
    reply_signature = dbus_message_get_signature(reply_ptr);

    if (strcmp(reply_signature, output_signature) != 0)
    {
      log_error("reply signature is '%s' but expected signature is '%s'", reply_signature, output_signature);
    }

    dbus_message_iter_init(reply_ptr, &iter);

    while (*output_signature++ != '\0')
    {
      ASSERT(dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_INVALID); /* we've checked the signature, this should not happen */
      parameter_ptr = va_arg(ap, void *);
      dbus_message_iter_get_basic(&iter, parameter_ptr);
      dbus_message_iter_next(&iter);
    }

    ASSERT(dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INVALID); /* we've checked the signature, this should not happen */
    dbus_message_unref(reply_ptr);
  }
  else
  {
    parameter_ptr = va_arg(ap, DBusMessage **);
    *(DBusMessage **)parameter_ptr = reply_ptr;
  }

  ret = true;

fail:
  va_end(ap);
  return ret;
}

struct cdbus_async_call_context
{
  void * context;
  void (* callback)(void * context, void * cookie, DBusMessage * reply_ptr);
  dbus_uint64_t cookie[0];
};

#define ctx_ptr ((struct cdbus_async_call_context *)user_data)

static void cdbus_async_call_reply_handler(DBusPendingCall * pending_call_ptr, void * user_data)
{
	DBusMessage * reply_ptr;

  reply_ptr = dbus_pending_call_steal_reply(pending_call_ptr);
  if (reply_ptr == NULL)
  {
    log_error("pending call notify called but reply is NULL");
  }
  else
  {
    ctx_ptr->callback(ctx_ptr->context, ctx_ptr->cookie, reply_ptr);
    ctx_ptr->callback = NULL;   /* mark that callback is already called */

    dbus_message_unref(reply_ptr);
  }
}

static void cdbus_async_call_reply_context_free(void * user_data)
{
  if (ctx_ptr->callback != NULL)
  {
    ctx_ptr->callback(ctx_ptr->context, ctx_ptr->cookie, NULL);
  }

  free(ctx_ptr);
}

#undef ctx_ptr

bool
cdbus_call_async(
  DBusMessage * request_ptr,
  void * context,
  void * cookie,
  size_t cookie_size,
  void (* callback)(void * context, void * cookie, DBusMessage * reply_ptr))
{
  bool ret;
  DBusPendingCall * pending_call_ptr;
  struct cdbus_async_call_context * ctx_ptr;

  ret = false;

  if (!dbus_connection_send_with_reply(cdbus_g_dbus_connection, request_ptr, &pending_call_ptr, DBUS_TIMEOUT_INFINITE))
  {
    log_error("dbus_connection_send_with_reply() failed.");
    goto exit;
  }

  if (pending_call_ptr == NULL)
  {
    log_error("dbus_connection_send_with_reply() returned NULL pending call object pointer.");
    goto exit;
  }

  ctx_ptr = malloc(sizeof(struct cdbus_async_call_context) + cookie_size);
  if (ctx_ptr == NULL)
  {
    log_error("malloc() failed to allocate cdbus_async_call_context struct with cookie size of %zu", cookie_size);
    goto unref;
  }

  ctx_ptr->context = context;
  ctx_ptr->callback = callback;
  memcpy(ctx_ptr->cookie, cookie, cookie_size);

  ret = dbus_pending_call_set_notify(pending_call_ptr, cdbus_async_call_reply_handler, ctx_ptr, cdbus_async_call_reply_context_free);
  if (!ret)
  {
    log_error("dbus_pending_call_set_notify() failed.");
    goto free;
  }

  goto unref;

free:
  free(ctx_ptr);
unref:
  dbus_pending_call_unref(pending_call_ptr);
exit:
  return ret;
}

static
const char *
cdbus_compose_signal_match(
  const char * service,
  const char * object,
  const char * iface,
  const char * signal)
{
  static char rule[1024];
  snprintf(rule, sizeof(rule), "type='signal',sender='%s',path='%s',interface='%s',member='%s'", service, object, iface, signal);
  return rule;
}

static const char * cdbus_compose_name_owner_match(const char * service)
{
  static char rule[1024];
  snprintf(rule, sizeof(rule), "type='signal',interface='"DBUS_INTERFACE_DBUS"',member=NameOwnerChanged,arg0='%s'", service);
  return rule;
}

bool
cdbus_register_object_signal_handler(
  DBusConnection * connection,
  const char * service,
  const char * object,
  const char * iface,
  const char * const * signals,
  DBusHandleMessageFunction handler,
  void * handler_data)
{
  const char * const * signal;

  for (signal = signals; *signal != NULL; signal++)
  {
    dbus_bus_add_match(connection, cdbus_compose_signal_match(service, object, iface, *signal), &cdbus_g_dbus_error);
    if (dbus_error_is_set(&cdbus_g_dbus_error))
    {
      log_error("Failed to add D-Bus match rule: %s", cdbus_g_dbus_error.message);
      dbus_error_free(&cdbus_g_dbus_error);
      return false;
    }
  }

  dbus_connection_add_filter(cdbus_g_dbus_connection, handler, handler_data, NULL);

  return true;
}

bool
cdbus_unregister_object_signal_handler(
  DBusConnection * connection,
  const char * service,
  const char * object,
  const char * iface,
  const char * const * signals,
  DBusHandleMessageFunction handler,
  void * handler_data)
{
  const char * const * signal;

  for (signal = signals; *signal != NULL; signal++)
  {
    dbus_bus_remove_match(connection, cdbus_compose_signal_match(service, object, iface, *signal), &cdbus_g_dbus_error);
    if (dbus_error_is_set(&cdbus_g_dbus_error))
    {
      log_error("Failed to remove D-Bus match rule: %s", cdbus_g_dbus_error.message);
      dbus_error_free(&cdbus_g_dbus_error);
      return false;
    }
  }

  dbus_connection_remove_filter(cdbus_g_dbus_connection, handler, handler_data);

  return true;
}

static
struct cdbus_signal_hook_descriptor *
find_signal_hook_descriptor(
  struct cdbus_service_descriptor * service_ptr,
  const char * object,
  const char * interface)
{
  struct list_head * node_ptr;
  struct cdbus_signal_hook_descriptor * hook_ptr;

  list_for_each(node_ptr, &service_ptr->hooks)
  {
    hook_ptr = list_entry(node_ptr, struct cdbus_signal_hook_descriptor, siblings);
    if (strcmp(hook_ptr->object, object) == 0 &&
        strcmp(hook_ptr->interface, interface) == 0)
    {
      return hook_ptr;
    }
  }

  return NULL;
}

#define service_ptr ((struct cdbus_service_descriptor *)data)

static
DBusHandlerResult
cdbus_signal_handler(
  DBusConnection * UNUSED(connection_ptr),
  DBusMessage * message_ptr,
  void * data)
{
  const char * object_path;
  const char * interface;
  const char * signal_name;
  const char * object_name;
  const char * old_owner;
  const char * new_owner;
  struct cdbus_signal_hook_descriptor * hook_ptr;
  const struct cdbus_signal_hook * signal_ptr;

  /* Non-signal messages are ignored */
  if (dbus_message_get_type(message_ptr) != DBUS_MESSAGE_TYPE_SIGNAL)
  {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  interface = dbus_message_get_interface(message_ptr);
  if (interface == NULL)
  {
    /* Signals with no interface are ignored */
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  object_path = dbus_message_get_path(message_ptr);

  signal_name = dbus_message_get_member(message_ptr);
  if (signal_name == NULL)
  {
    log_error("Received signal with NULL member");
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  log_debug("'%s' sent signal '%s'::'%s'", object_path, interface, signal_name);

  /* Handle session bus signals to track service alive state */
  if (strcmp(interface, DBUS_INTERFACE_DBUS) == 0)
  {
    if (strcmp(signal_name, "NameOwnerChanged") != 0)
    {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (service_ptr->lifetime_hook_function == NULL)
    {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    //log_info("NameOwnerChanged signal received");

    dbus_error_init(&cdbus_g_dbus_error);
    if (!dbus_message_get_args(
          message_ptr,
          &cdbus_g_dbus_error,
          DBUS_TYPE_STRING, &object_name,
          DBUS_TYPE_STRING, &old_owner,
          DBUS_TYPE_STRING, &new_owner,
          DBUS_TYPE_INVALID))
    {
      log_error("Cannot get message arguments: %s", cdbus_g_dbus_error.message);
      dbus_error_free(&cdbus_g_dbus_error);
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (strcmp(object_name, service_ptr->service_name) != 0)
    {
      return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if (old_owner[0] == '\0')
    {
      service_ptr->lifetime_hook_function(true);
    }
    else if (new_owner[0] == '\0')
    {
      service_ptr->lifetime_hook_function(false);
    }

    return DBUS_HANDLER_RESULT_HANDLED;
  }

  /* Handle object interface signals */
  if (object_path != NULL)
  {
    hook_ptr = find_signal_hook_descriptor(service_ptr, object_path, interface);
    if (hook_ptr != NULL)
    {
      for (signal_ptr = hook_ptr->signal_hooks; signal_ptr->signal_name != NULL; signal_ptr++)
      {
        if (strcmp(signal_name, signal_ptr->signal_name) == 0)
        {
          signal_ptr->hook_function(hook_ptr->hook_context, message_ptr);
          return DBUS_HANDLER_RESULT_HANDLED;
        }
      }
    }
  }

  /* Let everything else pass through */
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

#undef service_ptr

static struct cdbus_service_descriptor * find_service_descriptor(const char * service_name)
{
  struct list_head * node_ptr;
  struct cdbus_service_descriptor * descr_ptr;

  list_for_each(node_ptr, &g_dbus_services)
  {
    descr_ptr = list_entry(node_ptr, struct cdbus_service_descriptor, siblings);
    if (strcmp(descr_ptr->service_name, service_name) == 0)
    {
      return descr_ptr;
    }
  }

  return NULL;
}

static struct cdbus_service_descriptor * find_or_create_service_descriptor(const char * service_name)
{
  struct cdbus_service_descriptor * descr_ptr;

  descr_ptr = find_service_descriptor(service_name);
  if (descr_ptr != NULL)
  {
    return descr_ptr;
  }

  descr_ptr = malloc(sizeof(struct cdbus_service_descriptor));
  if (descr_ptr == NULL)
  {
    log_error("malloc() failed to allocate struct cdbus_service_descriptor");
    return NULL;
  }

  descr_ptr->service_name = strdup(service_name);
  if (descr_ptr->service_name == NULL)
  {
    log_error("strdup() failed for service name '%s'", service_name);
    free(descr_ptr);
    return NULL;
  }

  descr_ptr->lifetime_hook_function = NULL;
  INIT_LIST_HEAD(&descr_ptr->hooks);

  list_add_tail(&descr_ptr->siblings, &g_dbus_services);

  dbus_connection_add_filter(cdbus_g_dbus_connection, cdbus_signal_handler, descr_ptr, NULL);

  return descr_ptr;
}

static void free_service_descriptor_if_empty(struct cdbus_service_descriptor * service_ptr)
{
  if (service_ptr->lifetime_hook_function != NULL)
  {
    return;
  }

  if (!list_empty(&service_ptr->hooks))
  {
    return;
  }

  dbus_connection_remove_filter(cdbus_g_dbus_connection, cdbus_signal_handler, service_ptr);

  list_del(&service_ptr->siblings);
  free(service_ptr->service_name);
  free(service_ptr);
}

bool
cdbus_register_object_signal_hooks(
  DBusConnection * connection,
  const char * service_name,
  const char * object,
  const char * iface,
  void * hook_context,
  const struct cdbus_signal_hook * signal_hooks)
{
  struct cdbus_service_descriptor * service_ptr;
  struct cdbus_signal_hook_descriptor * hook_ptr;
  const struct cdbus_signal_hook * signal_ptr;

  if (connection != cdbus_g_dbus_connection)
  {
    log_error("multiple connections are not implemented yet");
    ASSERT_NO_PASS;
    goto fail;
  }

  service_ptr = find_or_create_service_descriptor(service_name);
  if (service_ptr == NULL)
  {
    log_error("find_or_create_service_descriptor() failed.");
    goto fail;
  }

  hook_ptr = find_signal_hook_descriptor(service_ptr, object, iface);
  if (hook_ptr != NULL)
  {
    log_error("refusing to register two signal monitors for '%s':'%s':'%s'", service_name, object, iface);
    ASSERT_NO_PASS;
    goto maybe_free_service;
  }

  hook_ptr = malloc(sizeof(struct cdbus_signal_hook_descriptor));
  if (hook_ptr == NULL)
  {
    log_error("malloc() failed to allocate struct cdbus_signal_hook_descriptor");
    goto maybe_free_service;
  }

  hook_ptr->object = strdup(object);
  if (hook_ptr->object == NULL)
  {
    log_error("strdup() failed for object name");
    goto free_hook;
  }

  hook_ptr->interface = strdup(iface);
  if (hook_ptr->interface == NULL)
  {
    log_error("strdup() failed for interface name");
    goto free_object_name;
  }

  hook_ptr->hook_context = hook_context;
  hook_ptr->signal_hooks = signal_hooks;

  list_add_tail(&hook_ptr->siblings, &service_ptr->hooks);

  for (signal_ptr = signal_hooks; signal_ptr->signal_name != NULL; signal_ptr++)
  {
    dbus_bus_add_match(connection, cdbus_compose_signal_match(service_name, object, iface, signal_ptr->signal_name), &cdbus_g_dbus_error);
    if (dbus_error_is_set(&cdbus_g_dbus_error))
    {
      log_error("Failed to add D-Bus match rule: %s", cdbus_g_dbus_error.message);
      dbus_error_free(&cdbus_g_dbus_error);

      while (signal_ptr != signal_hooks)
      {
        ASSERT(signal_ptr > signal_hooks);
        signal_ptr--;

        dbus_bus_remove_match(connection, cdbus_compose_signal_match(service_name, object, iface, signal_ptr->signal_name), &cdbus_g_dbus_error);
        if (dbus_error_is_set(&cdbus_g_dbus_error))
        {
          log_error("Failed to remove D-Bus match rule: %s", cdbus_g_dbus_error.message);
          dbus_error_free(&cdbus_g_dbus_error);
        }
      }

      goto remove_hook;
    }
  }

  return true;

remove_hook:
  list_del(&hook_ptr->siblings);
  free(hook_ptr->interface);
free_object_name:
  free(hook_ptr->object);
free_hook:
  free(hook_ptr);
maybe_free_service:
  free_service_descriptor_if_empty(service_ptr);
fail:
  return false;
}

void
cdbus_unregister_object_signal_hooks(
  DBusConnection * connection,
  const char * service_name,
  const char * object,
  const char * iface)
{
  struct cdbus_service_descriptor * service_ptr;
  struct cdbus_signal_hook_descriptor * hook_ptr;
  const struct cdbus_signal_hook * signal_ptr;

  if (connection != cdbus_g_dbus_connection)
  {
    log_error("multiple connections are not implemented yet");
    ASSERT_NO_PASS;
    return;
  }

  service_ptr = find_service_descriptor(service_name);
  if (service_ptr == NULL)
  {
    log_error("find_service_descriptor() failed.");
    ASSERT_NO_PASS;
    return;
  }

  hook_ptr = find_signal_hook_descriptor(service_ptr, object, iface);
  if (hook_ptr == NULL)
  {
    log_error("cannot unregister non-existing signal monitor for '%s':'%s':'%s'", service_name, object, iface);
    ASSERT_NO_PASS;
    return;
  }

  for (signal_ptr = hook_ptr->signal_hooks; signal_ptr->signal_name != NULL; signal_ptr++)
  {
    dbus_bus_remove_match(connection, cdbus_compose_signal_match(service_name, object, iface, signal_ptr->signal_name), &cdbus_g_dbus_error);
    if (dbus_error_is_set(&cdbus_g_dbus_error))
    {
      if (dbus_error_is_set(&cdbus_g_dbus_error))
      {
        log_error("Failed to remove D-Bus match rule: %s", cdbus_g_dbus_error.message);
        dbus_error_free(&cdbus_g_dbus_error);
      }
    }
  }

  list_del(&hook_ptr->siblings);

  free(hook_ptr->interface);
  free(hook_ptr->object);
  free(hook_ptr);

  free_service_descriptor_if_empty(service_ptr);
}

bool
cdbus_register_service_lifetime_hook(
  DBusConnection * connection,
  const char * service_name,
  void (* hook_function)(bool appeared))
{
  struct cdbus_service_descriptor * service_ptr;

  if (connection != cdbus_g_dbus_connection)
  {
    log_error("multiple connections are not implemented yet");
    ASSERT_NO_PASS;
    goto fail;
  }

  service_ptr = find_or_create_service_descriptor(service_name);
  if (service_ptr == NULL)
  {
    log_error("find_or_create_service_descriptor() failed.");
    goto fail;
  }

  if (service_ptr->lifetime_hook_function != NULL)
  {
    log_error("cannot register two lifetime hooks for '%s'", service_name);
    ASSERT_NO_PASS;
    goto maybe_free_service;
  }

  service_ptr->lifetime_hook_function = hook_function;

  dbus_bus_add_match(connection, cdbus_compose_name_owner_match(service_name), &cdbus_g_dbus_error);
  if (dbus_error_is_set(&cdbus_g_dbus_error))
  {
    log_error("Failed to add D-Bus match rule: %s", cdbus_g_dbus_error.message);
    dbus_error_free(&cdbus_g_dbus_error);
    goto clear_hook;
  }

  return true;

clear_hook:
  service_ptr->lifetime_hook_function = NULL;
maybe_free_service:
  free_service_descriptor_if_empty(service_ptr);
fail:
  return false;
}

void
cdbus_unregister_service_lifetime_hook(
  DBusConnection * connection,
  const char * service_name)
{
  struct cdbus_service_descriptor * service_ptr;

  if (connection != cdbus_g_dbus_connection)
  {
    log_error("multiple connections are not implemented yet");
    ASSERT_NO_PASS;
    return;
  }

  service_ptr = find_service_descriptor(service_name);
  if (service_ptr == NULL)
  {
    log_error("find_service_descriptor() failed.");
    return;
  }

  if (service_ptr->lifetime_hook_function == NULL)
  {
    log_error("cannot unregister non-existent lifetime hook for '%s'", service_name);
    ASSERT_NO_PASS;
    return;
  }

  service_ptr->lifetime_hook_function = NULL;

  dbus_bus_remove_match(connection, cdbus_compose_name_owner_match(service_name), &cdbus_g_dbus_error);
  if (dbus_error_is_set(&cdbus_g_dbus_error))
  {
    log_error("Failed to remove D-Bus match rule: %s", cdbus_g_dbus_error.message);
    dbus_error_free(&cdbus_g_dbus_error);
  }

  free_service_descriptor_if_empty(service_ptr);
}
