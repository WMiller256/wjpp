#Name of program
MAIN     = wjpp

ABS      = ./
BIN      = ~/bin
BUILD    = ./
RM       = /bin/rm -f
MV       = /bin/mv -f
CFLAGS   = -isystem /usr/local/include/opencv4/ -I /usr/local/include/eigen3/ -I /usr/lib/boost -Wno-deprecated-declarations -g -std=c++17 -rdynamic \
           -pthread -O3 -fopenmp
CC       = /usr/bin/g++ $(CFLAGS)

           # OpenCV
LIBS =     -lopencv_gapi -lopencv_stitching -lopencv_aruco -lopencv_bgsegm -lopencv_bioinspired    \
           -lopencv_ccalib -lopencv_cvv -lopencv_dnn_objdetect -lopencv_dpm -lopencv_face          \
           -lopencv_freetype -lopencv_fuzzy -lopencv_hdf -lopencv_hfs -lopencv_img_hash            \
           -lopencv_line_descriptor -lopencv_quality -lopencv_reg -lopencv_rgbd -lopencv_saliency  \
           -lopencv_sfm -lopencv_stereo -lopencv_structured_light -lopencv_superres                \
           -lopencv_surface_matching -lopencv_tracking -lopencv_videostab -lopencv_xfeatures2d     \
           -lopencv_xobjdetect -lopencv_xphoto -lopencv_shape -lopencv_datasets -lopencv_plot      \
           -lopencv_text -lopencv_dnn -lopencv_highgui -lopencv_ml -lopencv_phase_unwrapping       \
           -lopencv_optflow -lopencv_ximgproc -lopencv_video -lopencv_videoio -lopencv_imgcodecs   \
           -lopencv_objdetect -lopencv_calib3d -lopencv_features2d -lopencv_flann -lopencv_photo   \
           -lopencv_imgproc -lopencv_core                                                          \
           -lavutil -lavcodec -lavformat -lavdevice -lavfilter -lswscale                           \
           -lboost_program_options -lncurses -lstdc++fs
                    
LFLAGS   = -Wall -Wl,-rpath,/usr/local/lib
LIBDIRS   = $(LFLAGS) -L/usr/local/lib/ -L/usr/lib/boost

#Output coloring
GREEN   = \033[1;32m
CYAN    = \033[36m
BLUE    = \033[1;34m
BRIGHT  = \033[1;37m
WHITE   = \033[0;m
MAGENTA = \033[35m
YELLOW  = \033[33m
RED     = \033[91m

#Source files
OBJS   = $(BUILD)/wjpp.o \
         $(BUILD)/iocustom.o

SRCS   = wjpp.c++ \
         iocustom.c++

#Builds
all: $(OBJS)
	@printf "[$(CYAN)Linking $(WHITE)]   $(BRIGHT)$(MAIN)$(WHITE) - $(MAGENTA)Binary$(WHITE)\n"
	cd $(ABS); $(CC) $(OBJS) $(LIBDIRS) -o $(BIN)/$(MAIN) $(LIBS)
	@printf "[$(GREEN)Linked  $(WHITE)]   $(BRIGHT)$(MAIN)$(WHITE) - $(MAGENTA)Binary$(WHITE)\n"

$(BUILD)/%.o: %.c++
	@printf "[$(CYAN)Building$(WHITE)]   $(BRIGHT)$<$(WHITE) - $(MAGENTA)Object$(WHITE)\n"
	cd $(ABS); $(CC) -c -o $@ $<
	@printf "[$(GREEN) Built  $(WHITE)]   $(BRIGHT)$<$(WHITE) - $(MAGENTA)Object$(WHITE)\n"

$(MAIN).o: $(MAIN).c++
	@printf "[$(CYAN)Building$(WHITE)]   $(BRIGHT)$<$(WHITE) - $(MAGENTA)Object$(WHITE)\n"
	cd $(ABS); $(CC) -c $(MAIN).c++ -o $(MAIN).o
	@printf "[$(GREEN) Built  $(WHITE)]   $(BRIGHT)$<$(WHITE) - $(MAGENTA)Object$(WHITE)\n"

clean:
	$(RM) *.core $(BUILD)/*.o *.d *.stackdump

#Disable command echoing, reenabled with make verbose=1
ifndef verbose
.SILENT:
endif
