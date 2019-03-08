INCLUDES = -I./

##### Change the following for your environment:
COMPILE_OPTS =		$(INCLUDES) -I. $(EXTRA_LDFLAGS)
C =			c
C_COMPILER =		gcc
C_FLAGS =		$(COMPILE_OPTS)
OBJ =			o
LINK_OPTS =		-L.
CONSOLE_LINK_OPTS =	$(LINK_OPTS)
LIBRARY_LINK =		libtool -s -o 
LIBRARY_LINK_OPTS =	
LIB_SUFFIX =			a
LIBS_FOR_CONSOLE_APPLICATION =
LIBS_FOR_GUI_APPLICATION =
EXE =
##### End of variables to change
NAME = libmuxer
MUXER_LIB = $(NAME).$(LIB_SUFFIX)
ALL = $(MUXER_LIB)
all:	$(ALL)

.$(C).$(OBJ):
	$(C_COMPILER) -c $(C_FLAGS) $<

H264TSMUXER_OBJS = h264TsMuxer.$(OBJ) 

MUXER_LIB_OBJS = $(H264TSMUXER_OBJS)

$(MUXER_LIB): $(MUXER_LIB_OBJS) \
    $(PLATFORM_SPECIFIC_LIB_OBJS)
	$(LIBRARY_LINK)$@ $(LIBRARY_LINK_OPTS) \
		$(H264TSMUXER_OBJS) 

h264TsMuxer.$(C):		h264TsMuxer.h

clean:
	-rm -rf *.$(OBJ) $(ALL) core *.core *~ include/*~ 

##### Any additional, platform-specific rules come here:
