dnl $Id$
dnl config.m4 for extension xlog

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(xlog, for xlog support,
dnl Make sure that the comment is aligned:
[  --with-xlog             Include xlog support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(xlog, whether to enable xlog support,
dnl Make sure that the comment is aligned:
dnl [  --enable-xlog           Enable xlog support])

if test "$PHP_XLOG" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-xlog -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/xlog.h"  # you most likely want to change this
  dnl if test -r $PHP_XLOG/$SEARCH_FOR; then # path given as parameter
  dnl   XLOG_DIR=$PHP_XLOG
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for xlog files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       XLOG_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$XLOG_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the xlog distribution])
  dnl fi

  dnl # --with-xlog -> add include path
  dnl PHP_ADD_INCLUDE($XLOG_DIR/include)

  dnl # --with-xlog -> check for lib and symbol presence
  dnl LIBNAME=xlog # you may want to change this
  dnl LIBSYMBOL=xlog # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $XLOG_DIR/lib, XLOG_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_XLOGLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong xlog lib version or lib not found])
  dnl ],[
  dnl   -L$XLOG_DIR/lib -lm
  dnl ])
  dnl
  dnl PHP_SUBST(XLOG_SHARED_LIBADD)

  PHP_NEW_EXTENSION(xlog, log.c redis.c mail.c common.c xlog.c profile.c,  $ext_shared)
fi
