AC_DEFUN([FC_CHECK_SOUND],[
 AC_ARG_ENABLE([sdl-mixer],
   AS_HELP_STRING([--enable-sdl-mixer], [use the SDL mixer [sdl]]),
  [case "${enableval}" in
   yes|sdl) USE_SOUND_SDL=sdl ;;
   no)  USE_SOUND_SDL=no ;;
   sdl2) USE_SOUND_SDL=sdl2 ;;
   *)   AC_MSG_ERROR([bad value ${enableval} for --enable-sdl-mixer]) ;;
   esac], [USE_SOUND_SDL=maybe])

 if test "x$USE_SOUND_SDL" != "xno" && test "x$USE_SOUND_SDL" != "xsdl2" ; then
  dnl Add SDL support to client
  SDL_VERSION=1.0.0
  AM_PATH_SDL($SDL_VERSION, SDL=yes, SDL=no)
  if test "x$SDL" != "xno"; then
    PKG_CHECK_MODULES([SDLMIXER], [SDL_mixer],
[
    SOUND_CFLAGS="$SOUND_CFLAGS $SDLMIXER_CFLAGS"
    SOUND_LIBS="$SOUND_LIBS $SDLMIXER_LIBS"
    AC_DEFINE([AUDIO_SDL], [1], [SDL_Mixer support])
    AC_DEFINE([AUDIO_SDL1_2], [1], [SDL-1.2 Mixer in use])
    SDL_mixer=sdl
    SOUND_SDL_OK=true
], [
    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
    CFLAGS="$CFLAGS $SDL_CFLAGS"
    LIBS="$LIBS $SDL_LIBS"
    AC_CHECK_HEADER([SDL_mixer.h], [SDL_mixer_h=1], [SDL_mixer_h=0])
    AC_CHECK_LIB([SDL_mixer], [Mix_OpenAudio], [SDL_mixer=sdl])
    CPPFLAGS="$ac_save_CPPFLAGS"
    CFLAGS="$ac_save_CFLAGS"
    LIBS="$ac_save_LIBS"

    AC_MSG_CHECKING([building SDL_mixer support])
    if test "x$SDL_mixer_h" = "x1"; then
      if test "x$SDL_mixer" = "xsdl"; then
        SOUND_CFLAGS="$SOUND_CFLAGS $SDL_CFLAGS"
        SOUND_LIBS="$SOUND_LIBS -lSDL_mixer $SDL_LIBS"
        AC_DEFINE([AUDIO_SDL], [1], [SDL_Mixer support])
        AC_DEFINE([AUDIO_SDL1_2], [1], [SDL-1.2 Mixer in use])
        AC_MSG_RESULT([yes])
        SOUND_SDL_OK=true
      else
        AC_MSG_RESULT([no SDL_mixer library found, install from http://www.libsdl.org/projects/SDL_mixer/index.html ])
      fi
    else
      AC_MSG_RESULT([no SDL_mixer headers found, install from http://www.libsdl.org/projects/SDL_mixer/index.html])
      SDL_mixer=no
    fi ])
  fi
elif test "x$USE_SOUND_SDL" = "xsdl2" ; then
  dnl Add SDL support to client
  SDL2_VERSION=2.0.0
  AM_PATH_SDL2($SDL2_VERSION, SDL2=yes, SDL2=no)
  if test "x$SDL2" != "xno"; then
    PKG_CHECK_MODULES([SDL2MIXER], [SDL2_mixer],
[
    SOUND_CFLAGS="$SOUND_CFLAGS $SDL2MIXER_CFLAGS"
    SOUND_LIBS="$SOUND_LIBS $SDL2MIXER_LIBS"
    AC_DEFINE([AUDIO_SDL], [1], [SDL Mixer support])
    AC_DEFINE([AUDIO_SDL2], [1], [SDL-2 Mixer in use])
    SDL_mixer=sdl2
    SOUND_SDL_OK=true
], [
    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CPPFLAGS="$CPPFLAGS $SDL2_CFLAGS"
    CFLAGS="$CFLAGS $SDL2_CFLAGS"
    LIBS="$LIBS $SDL2_LIBS"
    AC_CHECK_HEADER([SDL_mixer.h], [SDL_mixer_h=1], [SDL_mixer_h=0])
    AC_CHECK_LIB([SDL2_mixer], [Mix_OpenAudio], [SDL_mixer=sdl2])
    CPPFLAGS="$ac_save_CPPFLAGS"
    CFLAGS="$ac_save_CFLAGS"
    LIBS="$ac_save_LIBS"

    AC_MSG_CHECKING([building SDL2_mixer support])
    if test "x$SDL_mixer_h" = "x1"; then
      if test "x$SDL_mixer" = "xsdl2"; then
        SOUND_CFLAGS="$SOUND_CFLAGS $SDL2_CFLAGS"
        SOUND_LIBS="$SOUND_LIBS -lSDL2_mixer $SDL2_LIBS"
        AC_DEFINE([AUDIO_SDL], [1], [SDL Mixer support])
        AC_DEFINE([AUDIO_SDL2], [1], [SDL-2 Mixer in use])
        AC_MSG_RESULT([yes])
        SOUND_SDL_OK=true
      else
        AC_MSG_RESULT([no SDL2_mixer library found, install from http://www.libsdl.org/projects/SDL_mixer/index.html ])
      fi
    else
      AC_MSG_RESULT([no SDL2_mixer headers found, install from http://www.libsdl.org/projects/SDL_mixer/index.html])
      SDL_mixer=no
    fi ])
  fi
fi
if test "x$USE_SOUND_SDL" = "xsdl" || test "x$USE_SOUND_SDL" = "xsdl2" ;then
  if test "x$SOUND_SDL_OK" != "xtrue" ; then
     AC_MSG_ERROR([SDL mixer support requested, but cannot be compiled in])
  fi
fi
if test "x$USE_SOUND_SDL" = "xmaybe" && test "x$SOUND_SDL_OK" != "xtrue" ; then
  feature_sound=missing
fi
if test "x$SDL_mixer" = "x" ; then
  SDL_mixer=no
fi
])
