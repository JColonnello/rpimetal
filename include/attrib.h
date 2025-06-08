#pragma once

#ifndef alias
/* Define ALIASNAME as a strong alias for NAME.  */
#define alias(name, aliasname) _alias(name, aliasname)
#define _alias(name, aliasname) extern __typeof(name) aliasname __attribute__((alias(#name)));
#endif

#define weak __attribute__((weak))
#define constructor __attribute__((constructor))
#define noreturn __attribute__((noreturn))
