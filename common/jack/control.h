/* -*- Mode: C ; c-basic-offset: 4 -*- */
/* JACK server control API */
/* SPDX-FileCopyrightText:  2008-2023 Nedko Arnaudov */
/* SPDX-License-Identifier: ( GPL-2.0-or-later OR LGPL-2.1-or-later OR MIT OR AFL-2.1 ) *//**
 * @file   jack/control.h
 * @ingroup publicheader
 * @brief  JACK control API
 *
 */

#ifndef JACKCTL_H__2EEDAD78_DF4C_4B26_83B7_4FF1A446A47E__INCLUDED
#define JACKCTL_H__2EEDAD78_DF4C_4B26_83B7_4FF1A446A47E__INCLUDED

#include <jack/jslist.h>

#ifdef WIN32
typedef unsigned long sigset_t;
#endif

/** Parameter types, intentionally similar to jack_driver_param_type_t */
typedef enum
{
    JackParamInt = 1,			/**< @brief value type is a signed integer */
    JackParamUInt,				/**< @brief value type is an unsigned integer */
    JackParamChar,				/**< @brief value type is a char */
    JackParamString,			/**< @brief value type is a string with max size of ::JACK_PARAM_STRING_MAX+1 chars */
    JackParamBool,				/**< @brief value type is a boolean */
} jackctl_param_type_t;

/** @brief Max value that jackctl_param_type_t type can have */
#define JACK_PARAM_MAX (JackParamBool + 1)

/** @brief Max length of string parameter value, excluding terminating nul char */
#define JACK_PARAM_STRING_MAX  63

/** @brief Type for parameter value */
/* intentionally similar to jack_driver_param_value_t */
union jackctl_parameter_value
{
    uint32_t ui;				/**< @brief member used for ::JackParamUInt */
    int32_t i;					/**< @brief member used for ::JackParamInt */
    char c;						/**< @brief member used for ::JackParamChar */
    char str[JACK_PARAM_STRING_MAX + 1]; /**< @brief member used for ::JackParamString */
    bool b;				/**< @brief member used for ::JackParamBool */
};

/** opaque type for server object */
typedef struct jackctl_server jackctl_server_t;

/** opaque type for driver object */
typedef struct jackctl_driver jackctl_driver_t;

/** opaque type for parameter object */
typedef struct jackctl_parameter jackctl_parameter_t;

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* Adjust editor indent */
#endif

/** 
 * Call this function to setup process signal handling. As a general
 * rule, it is required for proper operation for the server object.
 * 
 * @param flags signals setup flags, use 0 for none. Currently no
 * flags are defined
 *
 * @return the configurated signal set.
 */
sigset_t
jackctl_setup_signals(
    unsigned int flags);

/** 
 * Call this function to wait on a signal set.
 * 
 * @param signals signals set to wait on
 */

void
jackctl_wait_signals(
    sigset_t signals);

/**
 * Call this function to create server object.
 *
 * @param on_device_acquire - Optional callback to be called before device is acquired. If false is returned, device usage will fail
 * @param on_device_release - Optional callback to be called after device is released.
 *
 * @return server object handle, NULL if creation of server object
 * failed. Successfully created server object must be destroyed with
 * paired call to ::jackctl_server_destroy
 */
jackctl_server_t *
jackctl_server_create(
    bool (* on_device_acquire)(const char * device_name),
    void (* on_device_release)(const char * device_name));

/** 
 * Call this function to destroy server object.
 * 
 * @param server server object handle to destroy
 */
void
jackctl_server_destroy(
	jackctl_server_t * server);

/** 
 * Call this function to get list of available drivers. List node data
 * pointers is a driver object handle (::jackctl_driver_t).
 * 
 * @param server server object handle to get drivers for
 *
 * @return Single linked list of driver object handles. Must not be
 * modified. Always same for same server object.
 */
const JSList *
jackctl_server_get_drivers_list(
	jackctl_server_t * server);

/** 
 * Call this function to start JACK server
 * 
 * @param server server object handle
 * @param driver driver to use
 * 
 * @return success status: true - success, false - fail
 */
bool
jackctl_server_start(
    jackctl_server_t * server,
    jackctl_driver_t * driver);

/** 
 * Call this function to stop JACK server
 * 
 * @param server server object handle
 * 
 * @return success status: true - success, false - fail
 */
bool
jackctl_server_stop(
	jackctl_server_t * server);

/** 
 * Call this function to get list of server parameters. List node data
 * pointers is a parameter object handle (::jackctl_parameter_t).
 * 
 * @param server server object handle to get parameters for
 *
 * @return Single linked list of parameter object handles. Must not be
 * modified. Always same for same server object.
 */
const JSList *
jackctl_server_get_parameters(
	jackctl_server_t * server);

/** 
 * Call this function to get name of driver.
 * 
 * @param driver driver object handle to get name of
 *
 * @return driver name. Must not be modified. Always same for same
 * driver object.
 */
const char *
jackctl_driver_get_name(
	jackctl_driver_t * driver);

/** 
 * Call this function to get list of driver parameters. List node data
 * pointers is a parameter object handle (::jackctl_parameter_t).
 * 
 * @param driver driver object handle to get parameters for
 *
 * @return Single linked list of parameter object handles. Must not be
 * modified. Always same for same driver object.
 */
const JSList *
jackctl_driver_get_parameters(
	jackctl_driver_t * driver);

/** 
 * Call this function to get parameter name.
 * 
 * @param parameter parameter object handle to get name of
 *
 * @return parameter name. Must not be modified. Always same for same
 * parameter object.
 */
const char *
jackctl_parameter_get_name(
	jackctl_parameter_t * parameter);

/** 
 * Call this function to get parameter short description.
 * 
 * @param parameter parameter object handle to get short description of
 *
 * @return parameter short description. Must not be modified. Always
 * same for same parameter object.
 */
const char *
jackctl_parameter_get_short_description(
	jackctl_parameter_t * parameter);

/** 
 * Call this function to get parameter long description.
 * 
 * @param parameter parameter object handle to get long description of
 *
 * @return parameter long description. Must not be modified. Always
 * same for same parameter object.
 */
const char *
jackctl_parameter_get_long_description(
	jackctl_parameter_t * parameter);

/** 
 * Call this function to get parameter type.
 * 
 * @param parameter parameter object handle to get type of
 *
 * @return parameter type. Always same for same parameter object.
 */
jackctl_param_type_t
jackctl_parameter_get_type(
	jackctl_parameter_t * parameter);

/** 
 * Call this function to get parameter character.
 * 
 * @param parameter parameter object handle to get character of
 *
 * @return character. 
 */
char
jackctl_parameter_get_id(
	jackctl_parameter_t * parameter);

/** 
 * Call this function to check whether parameter has been set, or its
 * default value is being used.
 * 
 * @param parameter parameter object handle to check
 *
 * @return true - parameter is set, false - parameter is using default
 * value.
 */
bool
jackctl_parameter_is_set(
	jackctl_parameter_t * parameter);

/** 
 * Call this function to reset parameter to its default value.
 * 
 * @param parameter parameter object handle to reset value of
 *
 * @return success status: true - success, false - fail
 */
bool
jackctl_parameter_reset(
	jackctl_parameter_t * parameter);

/** 
 * Call this function to get parameter value.
 * 
 * @param parameter parameter object handle to get value of
 *
 * @return parameter value.
 */
union jackctl_parameter_value
jackctl_parameter_get_value(
	jackctl_parameter_t * parameter);

/** 
 * Call this function to set parameter value.
 * 
 * @param parameter parameter object handle to get value of
 * @param value_ptr pointer to variable containing parameter value
 *
 * @return success status: true - success, false - fail
 */
bool
jackctl_parameter_set_value(
	jackctl_parameter_t * parameter,
	const union jackctl_parameter_value * value_ptr);

/** 
 * Call this function to get parameter default value.
 * 
 * @param parameter parameter object handle to get default value of
 *
 * @return parameter default value.
 */
union jackctl_parameter_value
jackctl_parameter_get_default_value(
	jackctl_parameter_t * parameter);

/** 
 * Call this function check whether parameter has range constraint.
 * 
 * @param parameter object handle of parameter to check
 *
 * @return whether parameter has range constraint.
 */
bool
jackctl_parameter_has_range_constraint(
	jackctl_parameter_t * parameter_ptr);

/** 
 * Call this function check whether parameter has enumeration constraint.
 * 
 * @param parameter object handle of parameter to check
 *
 * @return whether parameter has enumeration constraint.
 */
bool
jackctl_parameter_has_enum_constraint(
	jackctl_parameter_t * parameter_ptr);

/** 
 * Call this function get how many enumeration values parameter has.
 * 
 * @param parameter object handle of parameter
 *
 * @return number of enumeration values
 */
uint32_t
jackctl_parameter_get_enum_constraints_count(
	jackctl_parameter_t * parameter_ptr);

/** 
 * Call this function to get parameter enumeration value.
 * 
 * @param parameter object handle of parameter
 * @param index index of parameter enumeration value
 *
 * @return enumeration value.
 */
union jackctl_parameter_value
jackctl_parameter_get_enum_constraint_value(
	jackctl_parameter_t * parameter_ptr,
	uint32_t index);

/** 
 * Call this function to get parameter enumeration value description.
 * 
 * @param parameter object handle of parameter
 * @param index index of parameter enumeration value
 *
 * @return enumeration value description.
 */
const char *
jackctl_parameter_get_enum_constraint_description(
	jackctl_parameter_t * parameter_ptr,
	uint32_t index);

/** 
 * Call this function to get parameter range.
 * 
 * @param parameter object handle of parameter
 * @param min_ptr pointer to variable receiving parameter minimum value
 * @param max_ptr pointer to variable receiving parameter maximum value
 */
void
jackctl_parameter_get_range_constraint(
	jackctl_parameter_t * parameter_ptr,
	union jackctl_parameter_value * min_ptr,
	union jackctl_parameter_value * max_ptr);

/** 
 * Call this function to check whether parameter constraint is strict,
 * i.e. whether supplying non-matching value will not work for sure.
 * 
 * @param parameter parameter object handle to check
 *
 * @return whether parameter constraint is strict.
 */
bool
jackctl_parameter_constraint_is_strict(
	jackctl_parameter_t * parameter_ptr);

/** 
 * Call this function to check whether parameter has fake values,
 * i.e. values have no user meaningful meaning and only value
 * description is meaningful to user.
 * 
 * @param parameter parameter object handle to check
 *
 * @return whether parameter constraint is strict.
 */
bool
jackctl_parameter_constraint_is_fake_value(
	jackctl_parameter_t * parameter_ptr);

/** 
 * Call this function to log an error message.
 * 
 * @param format string
 */
void
jack_error(
	const char *format,
	...);

/** 
 * Call this function to log an information message.
 * 
 * @param format string
 */
void
jack_info(
	const char *format,
	...);

/** 
 * Call this function to log an information message but only when
 * verbose mode is enabled.
 * 
 * @param format string
 */
void
jack_log(
	const char *format,
	...);

#if 0
{ /* Adjust editor indent */
#endif
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* #ifndef JACKCTL_H__2EEDAD78_DF4C_4B26_83B7_4FF1A446A47E__INCLUDED */
