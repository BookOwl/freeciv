#ifndef FC_CONFIG_H
#define FC_CONFIG_H
@TOP@

/*
 *   Defaults for configuration settings
 *   by Falk H�ffner <falk.hueffner@student.uni-tuebingen.de>
 *   and others
 *
 *   Need to mention HAVE_lib here when using FC_CHECK_X_LIB
 *   instead of AC_CHECK_LIB:  --dwp
 *
 *   Note PACKAGE and VERSION are now done automatically by
 *   autoheader  --dwp
 *
 *   ALWAYS_ROOT and following correspond to entries used by
 *   hand-written config.h files, and are not set by configure.
 *   (Exception:  Some, however, may be set in the platform-
 *    specific parts of configure.)
 */

#undef MAJOR_VERSION
#undef MINOR_VERSION
#undef PATCH_VERSION
#undef VERSION_LABEL
#undef IS_DEVEL_VERSION
#undef IS_BETA_VERSION
#undef VERSION_STRING
#undef HAVE_LIBICE
#undef HAVE_LIBSM
#undef HAVE_LIBX11
#undef HAVE_LIBXAW
#undef HAVE_LIBXAW3D
#undef HAVE_LIBXEXT
#undef HAVE_LIBXMU
#undef HAVE_LIBXPM
#undef HAVE_LIBXT
#undef ENABLE_NLS
#undef HAVE_CATGETS
#undef HAVE_GETTEXT
#undef HAVE_LC_MESSAGES
#undef HAVE_STPCPY
#undef LOCALEDIR
#undef DEFAULT_DATA_PATH
#undef HAVE_SIGPIPE
#undef XPM_H_NO_X11
#undef FUNCPROTO
#undef NARROWPROTO
#undef HAVE_LIBREADLINE
#undef NONBLOCKING_SOCKETS
#undef HAVE_FCNTL
#undef HAVE_IOCTL
#undef ALWAYS_ROOT
#undef WIN32_NATIVE
#undef HAVE_WINSOCK
#undef SOCKET_ZERO_ISNT_STDIN
#undef STRICT_WINDOWS
#undef GENERATING_MAC
#undef HAVE_OPENTRANSPORT
#undef PATH_SEPARATOR
#undef OPTION_FILE_NAME

@BOTTOM@

#endif /* FC_CONFIG_H */
