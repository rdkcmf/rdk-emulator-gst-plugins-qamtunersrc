##########################################################################
# Copyright 2014 RDK Management
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation, version 2
# of the license.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
# Boston, MA 02110-1301, USA.
##########################################################################

SUBDIRS = 
AM_CPPFLAGS = -pthread -Wall
plugin_LTLIBRARIES = libgstqamtunersrc.la
libgstqamtunersrc_la_SOURCES = gstqamtunersrc.c
libgstqamtunersrc_la_CFLAGS =  $(GST_CFLAGS) -DINTEL_JEDI_LINUX_CANMORE -D__STDC_CONSTANT_MACROS -DGCC4_XXX -DPC_BUILD -DUSE_CC_GETTIMEOFDAY -DRMF_OSAL_FEATURE_IPV6 -DRMF_OSAL_LITTLE_ENDIAN -DXONE_STB -DRDK_EMULATOR -DUSE_HN_SINK -DRMF_STREAMER -DGENERATE_SI_CACHE_UTILITY -DRECORDER_SUPPORT
libgstqamtunersrc_la_LDFLAGS = $(GST_LIBS) -ltinyxml -luuid -lhdhomerun
libgstqamtunersrc_la_LDFLAGS += -module -avoid-version
