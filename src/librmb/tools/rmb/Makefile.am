# Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/librmb
	
shlibs = \
	$(top_srcdir)/src/librmb/librmb.la
	
bin_PROGRAMS = rmb
rmb_SOURCES = mailbox_tools.cpp \
			  ls_cmd_parser.cpp \
			  rmb.cpp 
			  
rmb_LDADD = $(shlibs)
 