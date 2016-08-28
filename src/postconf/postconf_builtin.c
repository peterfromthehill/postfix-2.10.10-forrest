/*++
/* NAME
/*	postconf_builtin 3
/* SUMMARY
/*	built-in parameter support
/* SYNOPSIS
/*	#include <postconf.h>
/*
/*	void	register_builtin_parameters(procname, pid)
/*	const char *procname;
/*	pid_t	pid;
/* DESCRIPTION
/*	register_builtin_parameters() initializes the global parameter
/*	name space and adds all built-in parameter information.
/*
/*	Arguments:
/*.IP procname
/*	Provides the default value for the "process_name" parameter.
/*.IP pid
/*	Provides the default value for the "process_id" parameter.
/* DIAGNOSTICS
/*	Problems are reported to the standard error stream.
/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/

/* System library. */

#include <sys_defs.h>
#include <string.h>

#ifdef USE_PATHS_H
#include <paths.h>
#endif

/* Utility library. */

#include <msg.h>
#include <mymalloc.h>
#include <htable.h>
#include <vstring.h>
#include <get_hostname.h>
#include <stringops.h>

/* Global library. */

#include <mynetworks.h>
#include <mail_conf.h>
#include <mail_params.h>
#include <mail_version.h>
#include <mail_proto.h>
#include <mail_addr.h>
#include <inet_proto.h>
#include <server_acl.h>

/* Application-specific. */

#include <postconf.h>

 /*
  * Support for built-in parameters: declarations generated by scanning
  * actual C source files.
  */
#include "time_vars.h"
#include "bool_vars.h"
#include "int_vars.h"
#include "str_vars.h"
#include "raw_vars.h"
#include "nint_vars.h"
#include "nbool_vars.h"
#include "long_vars.h"

 /*
  * Support for built-in parameters: manually extracted.
  */
#include "install_vars.h"

 /*
  * Support for built-in parameters: lookup tables generated by scanning
  * actual C source files.
  */
static const CONFIG_TIME_TABLE time_table[] = {
#include "time_table.h"
    0,
};

static const CONFIG_BOOL_TABLE bool_table[] = {
#include "bool_table.h"
    0,
};

static const CONFIG_INT_TABLE int_table[] = {
#include "int_table.h"
    0,
};

static const CONFIG_STR_TABLE str_table[] = {
#include "str_table.h"
#include "install_table.h"
    0,
};

static const CONFIG_RAW_TABLE raw_table[] = {
#include "raw_table.h"
    0,
};

static const CONFIG_NINT_TABLE nint_table[] = {
#include "nint_table.h"
    0,
};

static const CONFIG_NBOOL_TABLE nbool_table[] = {
#include "nbool_table.h"
    0,
};

static const CONFIG_LONG_TABLE long_table[] = {
#include "long_table.h"
    0,
};

 /*
  * Legacy parameters for backwards compatibility.
  */
static const CONFIG_STR_TABLE legacy_str_table[] = {
    {"virtual_maps", ""},
    {"fallback_relay", ""},
    {"authorized_verp_clients", ""},
    {"smtpd_client_connection_limit_exceptions", ""},
    0,
};

 /*
  * Parameters whose default values are normally initialized by calling a
  * function. We direct the calls to our own versions of those functions
  * because the run-time conditions are slightly different.
  * 
  * Important: if the evaluation of a parameter default value has any side
  * effects, then those side effects must happen only once.
  */
static const char *pc_check_myhostname(void);
static const char *pc_check_mydomainname(void);
static const char *pc_mynetworks(void);

#include "str_fn_vars.h"

static const CONFIG_STR_FN_TABLE str_fn_table[] = {
#include "str_fn_table.h"
    0,
};

 /*
  * Parameters whose default values are normally initialized by ad-hoc code.
  * The AWK script cannot identify these parameters or values, so we provide
  * our own.
  * 
  * Important: if the evaluation of a parameter default value has any side
  * effects, then those side effects must happen only once.
  */
static CONFIG_STR_TABLE adhoc_procname = {VAR_PROCNAME};
static CONFIG_INT_TABLE adhoc_pid = {VAR_PID};

#define STR(x) vstring_str(x)

/* pc_check_myhostname - lookup hostname and validate */

static const char *pc_check_myhostname(void)
{
    static const char *name;
    const char *dot;
    const char *domain;

    /*
     * Use cached result.
     */
    if (name)
	return (name);

    /*
     * If the local machine name is not in FQDN form, try to append the
     * contents of $mydomain.
     */
    name = get_hostname();
    if ((dot = strchr(name, '.')) == 0) {
	if ((domain = mail_conf_lookup_eval(VAR_MYDOMAIN)) == 0)
	    domain = DEF_MYDOMAIN;
	name = concatenate(name, ".", domain, (char *) 0);
    }
    return (name);
}

/* get_myhostname - look up and store my hostname */

static void get_myhostname(void)
{
    const char *name;

    if ((name = mail_conf_lookup_eval(VAR_MYHOSTNAME)) == 0)
	name = pc_check_myhostname();
    var_myhostname = mystrdup(name);
}

/* pc_check_mydomainname - lookup domain name and validate */

static const char *pc_check_mydomainname(void)
{
    static const char *domain;
    char   *dot;

    /*
     * Use cached result.
     */
    if (domain)
	return (domain);

    /*
     * Use the hostname when it is not a FQDN ("foo"), or when the hostname
     * actually is a domain name ("foo.com").
     */
    if (var_myhostname == 0)
	get_myhostname();
    if ((dot = strchr(var_myhostname, '.')) == 0 || strchr(dot + 1, '.') == 0)
	return (domain = DEF_MYDOMAIN);
    return (domain = mystrdup(dot + 1));
}

/* pc_mynetworks - lookup network address list */

static const char *pc_mynetworks(void)
{
    static const char *networks;
    INET_PROTO_INFO *proto_info;
    const char *junk;

    /*
     * Use cached result.
     */
    if (networks)
	return (networks);

    if (var_inet_interfaces == 0) {
	if ((cmd_mode & SHOW_DEFS)
	    || (junk = mail_conf_lookup_eval(VAR_INET_INTERFACES)) == 0)
	    junk = DEF_INET_INTERFACES;
	var_inet_interfaces = mystrdup(junk);
    }
    if (var_mynetworks_style == 0) {
	if ((cmd_mode & SHOW_DEFS)
	    || (junk = mail_conf_lookup_eval(VAR_MYNETWORKS_STYLE)) == 0)
	    junk = DEF_MYNETWORKS_STYLE;
	var_mynetworks_style = mystrdup(junk);
    }
    if (var_inet_protocols == 0) {
	if ((cmd_mode & SHOW_DEFS)
	    || (junk = mail_conf_lookup_eval(VAR_INET_PROTOCOLS)) == 0)
	    junk = DEF_INET_PROTOCOLS;
	var_inet_protocols = mystrdup(junk);
	proto_info = inet_proto_init(VAR_INET_PROTOCOLS, var_inet_protocols);
    }
    return (networks = mystrdup(mynetworks()));
}

/* convert_bool_parameter - get boolean parameter string value */

static const char *convert_bool_parameter(char *ptr)
{
    CONFIG_BOOL_TABLE *cbt = (CONFIG_BOOL_TABLE *) ptr;

    return (cbt->defval ? "yes" : "no");
}

/* convert_time_parameter - get relative time parameter string value */

static const char *convert_time_parameter(char *ptr)
{
    CONFIG_TIME_TABLE *ctt = (CONFIG_TIME_TABLE *) ptr;

    return (ctt->defval);
}

/* convert_int_parameter - get integer parameter string value */

static const char *convert_int_parameter(char *ptr)
{
    CONFIG_INT_TABLE *cit = (CONFIG_INT_TABLE *) ptr;

    return (STR(vstring_sprintf(param_string_buf, "%d", cit->defval)));
}

/* convert_str_parameter - get string parameter string value */

static const char *convert_str_parameter(char *ptr)
{
    CONFIG_STR_TABLE *cst = (CONFIG_STR_TABLE *) ptr;

    return (cst->defval);
}

/* convert_str_fn_parameter - get string-function parameter string value */

static const char *convert_str_fn_parameter(char *ptr)
{
    CONFIG_STR_FN_TABLE *cft = (CONFIG_STR_FN_TABLE *) ptr;

    return (cft->defval());
}

/* convert_raw_parameter - get raw string parameter string value */

static const char *convert_raw_parameter(char *ptr)
{
    CONFIG_RAW_TABLE *rst = (CONFIG_RAW_TABLE *) ptr;

    return (rst->defval);
}

/* convert_nint_parameter - get new integer parameter string value */

static const char *convert_nint_parameter(char *ptr)
{
    CONFIG_NINT_TABLE *rst = (CONFIG_NINT_TABLE *) ptr;

    return (rst->defval);
}

/* convert_nbool_parameter - get new boolean parameter string value */

static const char *convert_nbool_parameter(char *ptr)
{
    CONFIG_NBOOL_TABLE *bst = (CONFIG_NBOOL_TABLE *) ptr;

    return (bst->defval);
}

/* convert_long_parameter - get long parameter string value */

static const char *convert_long_parameter(char *ptr)
{
    CONFIG_LONG_TABLE *clt = (CONFIG_LONG_TABLE *) ptr;

    return (STR(vstring_sprintf(param_string_buf, "%ld", clt->defval)));
}

/* register_builtin_parameters - add built-ins to the global name space */

void    register_builtin_parameters(const char *procname, pid_t pid)
{
    const char *myname = "register_builtin_parameters";
    const CONFIG_TIME_TABLE *ctt;
    const CONFIG_BOOL_TABLE *cbt;
    const CONFIG_INT_TABLE *cit;
    const CONFIG_STR_TABLE *cst;
    const CONFIG_STR_FN_TABLE *cft;
    const CONFIG_RAW_TABLE *rst;
    const CONFIG_NINT_TABLE *nst;
    const CONFIG_NBOOL_TABLE *bst;
    const CONFIG_LONG_TABLE *lst;

    /*
     * Sanity checks.
     */
    if (param_table != 0)
	msg_panic("%s: global parameter table is already initialized", myname);

    /*
     * Initialize the global parameter table.
     */
    param_table = PC_PARAM_TABLE_CREATE(1000);

    /*
     * Add the built-in parameters to the global name space. The class
     * (built-in) is tentative; some parameters are actually service-defined,
     * but they have their own default value.
     */
    for (ctt = time_table; ctt->name; ctt++)
	PC_PARAM_TABLE_ENTER(param_table, ctt->name, PC_PARAM_FLAG_BUILTIN,
			     (char *) ctt, convert_time_parameter);
    for (cbt = bool_table; cbt->name; cbt++)
	PC_PARAM_TABLE_ENTER(param_table, cbt->name, PC_PARAM_FLAG_BUILTIN,
			     (char *) cbt, convert_bool_parameter);
    for (cit = int_table; cit->name; cit++)
	PC_PARAM_TABLE_ENTER(param_table, cit->name, PC_PARAM_FLAG_BUILTIN,
			     (char *) cit, convert_int_parameter);
    for (cst = str_table; cst->name; cst++)
	PC_PARAM_TABLE_ENTER(param_table, cst->name, PC_PARAM_FLAG_BUILTIN,
			     (char *) cst, convert_str_parameter);
    for (cft = str_fn_table; cft->name; cft++)
	PC_PARAM_TABLE_ENTER(param_table, cft->name, PC_PARAM_FLAG_BUILTIN,
			     (char *) cft, convert_str_fn_parameter);
    for (rst = raw_table; rst->name; rst++)
	PC_PARAM_TABLE_ENTER(param_table, rst->name,
			     PC_PARAM_FLAG_BUILTIN | PC_PARAM_FLAG_RAW,
			     (char *) rst, convert_raw_parameter);
    for (nst = nint_table; nst->name; nst++)
	PC_PARAM_TABLE_ENTER(param_table, nst->name, PC_PARAM_FLAG_BUILTIN,
			     (char *) nst, convert_nint_parameter);
    for (bst = nbool_table; bst->name; bst++)
	PC_PARAM_TABLE_ENTER(param_table, bst->name, PC_PARAM_FLAG_BUILTIN,
			     (char *) bst, convert_nbool_parameter);
    for (lst = long_table; lst->name; lst++)
	PC_PARAM_TABLE_ENTER(param_table, lst->name, PC_PARAM_FLAG_BUILTIN,
			     (char *) lst, convert_long_parameter);

    /*
     * Register legacy parameters (used as a backwards-compatible migration
     * aid).
     */
    for (cst = legacy_str_table; cst->name; cst++)
	PC_PARAM_TABLE_ENTER(param_table, cst->name, PC_PARAM_FLAG_LEGACY,
			     (char *) cst, convert_str_parameter);

    /*
     * Register parameters whose default value is normally initialized by
     * ad-hoc code.
     */
    adhoc_procname.defval = mystrdup(procname);
    PC_PARAM_TABLE_ENTER(param_table, adhoc_procname.name,
			 PC_PARAM_FLAG_BUILTIN | PC_PARAM_FLAG_READONLY,
			 (char *) &adhoc_procname, convert_str_parameter);
    adhoc_pid.defval = pid;
    PC_PARAM_TABLE_ENTER(param_table, adhoc_pid.name,
			 PC_PARAM_FLAG_BUILTIN | PC_PARAM_FLAG_READONLY,
			 (char *) &adhoc_pid, convert_int_parameter);
}
