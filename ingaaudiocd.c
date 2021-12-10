#include <stdio.h>

#include <exec/exec.h>
#include <libraries/cdplayer.h>
#include <dos/dos.h>
#include <devices/timer.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>

#include "kernel.h"
#include "device_info.h"

//Prototypen
BYTE CDPlay(ULONG, ULONG, struct IOStdReq *);
BYTE CDStop(struct IOStdReq *);
BYTE CDTitleTime(struct CD_Time *, struct IOStdReq *);
BYTE CDReadTOC(struct CD_TOC *, struct IOStdReq *);

//Systemzeiger
struct Library *CDPlayerBase = NULL;
struct MsgPort	*cdport = NULL;
struct IOStdReq *cdreq = NULL;
BYTE cderr = -1;

//Programmsystemvariablen
extern FLOAT frame20;
char cddrive[30] = "CD0:";
extern struct timeval systime;

//CD intern
UWORD cdtrack = 0;
struct timeval cdtrende;
struct CD_TOC cdc;
struct CD_Time cdt;
	

/*=======================================================*/

void StarteAudioCDTreiber() {
	struct device_info_s *dev;
	//Treiber starten und alles initialisieren.

	if (CDPlayerBase = OpenLibrary("cdplayer.library", 0)) {
		if (dev = get_device_info(cddrive)) {
			if (cdport = CreateMsgPort()) {
				if (cdreq = (struct IOStdReq *)CreateIORequest(cdport, sizeof(struct IOStdReq))) {
					cderr = OpenDevice(dev->device, dev->unit, (struct IORequest *)cdreq, 0);
				}
			}
			free_device_info(dev);
		}
		if (cderr) Meldung("Entweder liegt keine CD im Laufwerk,\noder die Piktogramm-Eigenschaften\nsind falsch konfiguriert.\nCD-Musik ist deaktiviert.");
	} else Meldung("Konnte cdplayer.library nicht öffnen.\nCD-Musik ist deaktiviert.");
}

void EntferneAudioCDTreiber() {
	//Erst überprüfen, ob Treiber überhaupt gestartet ist!

	if (!cderr) {
		CDStop(cdreq);
		CloseDevice((struct IORequest *)cdreq);
	}
	if (cdreq) DeleteIORequest(cdreq);
	if (cdport) DeleteMsgPort(cdport);

	CloseLibrary(CDPlayerBase);

}

void TesteAudioCD() {
	//Falls Lied beendet, neu beginnen! (repeat)

	if (!cderr) {
		if (CmpTime(&cdtrende, &systime) == 1) {
			CDReadTOC(&cdc, cdreq);
			CDPlay(cdtrack, cdc.cdc_NumTracks, cdreq);
			CDTitleTime(&cdt, cdreq);
			GetSysTime(&cdtrende);
			cdtrende.tv_secs += (ULONG)(cdt.cdt_TrackRemainBase / 75) - 2;
		}
	}
}

//========Befehle===========

void SpieleCDTrack(UWORD num) {
	if (num != cdtrack) {
		cdtrack = num;
	
		//Starte Lied von CD!
		if (!cderr) {
			CDReadTOC(&cdc, cdreq);
			CDPlay(cdtrack, cdc.cdc_NumTracks, cdreq);
			CDTitleTime(&cdt, cdreq);
			GetSysTime(&cdtrende);
			cdtrende.tv_secs += (ULONG)(cdt.cdt_TrackRemainBase / 75) - 2;
		}
	}
}

void StoppeCD() {
	if (cdtrack > 0) {
	
		//Stoppe CD!
		if (!cderr) CDStop(cdreq);
		cdtrack = 0;
	}
}
