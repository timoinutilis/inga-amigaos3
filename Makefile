CC = gcc
CFLAGS = -Wall -g -O2 -m68020-60 -msoft-float -ffast-math -fomit-frame-pointer
LDFLAGS = -noixemul -m68020-60 -s
LIBS = -lm

INGA_OBJS = kernel.o grafik.o animation.o textausgabe.o elem_felder.o elem_zierden.o \
	elem_objekte.o elem_personen.o vp.o inventar.o dialog.o sequenzen.o \
	menu.o cache.o ingasound.o ingaplayer.o ingaaudiocd.o device_info.o

INGA = Inga

SOUND_OBJS = sound.o

SOUND = IngaSound

all: $(INGA) $(SOUND)

$(INGA): $(INGA_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(INGA_OBJS) $(LIBS)

$(SOUND): $(SOUND_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SOUND_OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $*.c

clean:
	rm -f $(INGA) $(INGA_OBJS) $(SOUND) $(SOUND_OBJS)

