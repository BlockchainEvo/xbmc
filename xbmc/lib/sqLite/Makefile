INCLUDES=-I. -I../../ -I../../linux
SRCS=dataset.cpp qry_dat.cpp sqlitedataset.cpp mysqldataset.cpp
LIB=sqllite.a

include ../../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
