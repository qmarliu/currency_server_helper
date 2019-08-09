TARGET  := currency_server_helper.exe
INCS = -I ./network -I ./utils
LIBS = -L ./utils -lutils -L ./network -lnetwork -lev -ljansson -lm -lpthread -ldl
include ./makefile.inc
