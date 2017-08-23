AM_CPPFLAGS = \
	-I$(top_srcdir)/src/librmb
	
bin_PROGRAMS = rmb
rmb_SOURCES = ls_cmd_parser.cpp \
			  rmb.cpp 
			  

rmb_CXXFLAGS = -fpermissive -std=c++11
rmb_LDADD = $(top_srcdir)/src/librmb/librmb.la

