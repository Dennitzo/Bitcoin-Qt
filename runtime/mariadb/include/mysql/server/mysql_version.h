/* Copyright Abandoned 1996,1999 TCX DataKonsult AB & Monty Program KB
   & Detron HB, 1996, 1999-2004, 2007 MySQL AB.
   This file is public domain and comes with NO WARRANTY of any kind
*/

/* Version numbers for protocol & mysqld */

#ifndef _mysql_version_h
#define _mysql_version_h
#ifdef _CUSTOMCONFIG_
#include <custom_conf.h>
#else
#define PROTOCOL_VERSION		10
#define MYSQL_SERVER_VERSION		"12.3.2-MariaDB"
#define MYSQL_BASE_VERSION		"mysqld-12.3"
#define MARIADB_BASE_VERSION		"mariadb-12.3"
#define MARIADB_MAJOR_VERSION		"mariadb-12"
#define MARIADBD_BASE_VERSION		"mariadbd-12.3"
#define MARIADBD_MAJOR_VERSION		"mariadbd-12"
#define MYSQL_SERVER_SUFFIX_DEF		""
#define FRM_VER				6
#define MYSQL_VERSION_ID		120302
#define MARIADB_PORT                    3306
#define MYSQL_PORT_DEFAULT		0
#define MARIADB_UNIX_ADDR               "/tmp/mysql.sock"
#define MYSQL_CONFIG_NAME		"my"
#define MYSQL_COMPILATION_COMMENT	"Homebrew"
#define SERVER_MATURITY_LEVEL           MariaDB_PLUGIN_MATURITY_STABLE

#define MYSQL_PORT                      MARIADB_PORT
#define MYSQL_UNIX_ADDR                 MARIADB_UNIX_ADDR

#ifdef WITH_WSREP
#define WSREP_PATCH_VERSION             "wsrep_26.22"
#endif

/* mysqld compile time options */
#endif /* _CUSTOMCONFIG_ */

#ifndef LICENSE
#define LICENSE				GPL
#endif /* LICENSE */

#endif /* _mysql_version_h */
