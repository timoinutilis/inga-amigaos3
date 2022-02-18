#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __GNUC__
#define __REG__(r, p) p __asm(#r)
#else
#define __REG__(r, p) register __ ## r p
#endif

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>

#include <exec/exec.h>
#include <dos/dos.h>
#include <devices/ahi.h>

#include <clib/alib_protos.h>

char *ver="$VER: Inga Sound-Plug-In Version 1.2";

struct SOUNDBASE {
	BOOL soundplugin;
	BOOL speech;
	BPTR conwin; // wird nicht mehr benutzt
};

struct SOUNDMESSAGE {
	struct Message ExecMessage;
	BOOL busy;
	BOOL cancel;
	UBYTE action;
	char file[256];
	UWORD id;
	UWORD vol;
	UWORD pan;
	struct SOUNDBASE *soundbase;
};

struct MsgPort *port;
struct SOUNDBASE *soundbase=NULL;
struct SOUNDMESSAGE *smsg=NULL;

struct Library *AHIBase;
struct MsgPort *AHImp=NULL;
struct AHIRequest *AHIio=NULL;
struct Hook SoundHook={{NULL, NULL}, NULL, NULL, NULL};
BOOL offen=FALSE;
BYTE sig=-1;


// AHI-Device öffnen
BOOL offneAHI() {
	if ((AHImp = CreateMsgPort())) {
		if ((AHIio = (struct AHIRequest *)CreateIORequest(AHImp, sizeof(struct AHIRequest)))) {
			AHIio->ahir_Version = 4;
			if (!OpenDevice(AHINAME, AHI_NO_UNIT, (struct IORequest *) AHIio, NULL)) {
				offen=TRUE;
				AHIBase=(struct Library *)AHIio->ahir_Std.io_Device;
				return(TRUE);
			}
		}
	}
	return(FALSE);
}

// AHI-Device schließen
void schliesseAHI() {
	if (offen) CloseDevice((struct IORequest *) AHIio);
	if (AHIio!=NULL) DeleteIORequest((struct IORequest *)AHIio);
	if (AHImp!=NULL) DeleteMsgPort(AHImp);
}

// Prefs-Datei von AHI öffnen und Standart-Mix-Frequenz
// herausfinden.
ULONG PrefsFreq() {
	BPTR file;
	APTR mem;
	ULONG freq=0;
	ULONG *adr;
	ULONG chunk, len;
	struct AHIUnitPrefs *unit;

	if ((file=Open("ENV:Sys/ahi.prefs", MODE_OLDFILE))) {
		if ((mem=AllocVec(254, MEMF_ANY | MEMF_CLEAR))) {
			Read(file, mem, 254);

			adr=mem;
			if (*adr==0x464F524D) {
				adr++; adr++;
				if (*adr==0x50524546) {
					adr++;

					do {
						chunk=*adr; adr++; len=*adr; adr++;
						if (chunk==ID_AHIU) {
							unit=(struct AHIUnitPrefs *)adr;
							if (unit->ahiup_Unit==AHI_NO_UNIT) {
								freq=unit->ahiup_Frequency;
								break;
							}
						}
						adr=(ULONG *)((ULONG)adr+len);
					} while ((ULONG)adr<(ULONG)mem+250);

				}
			}
			FreeVec(mem);
		}
		Close(file);
	}
	return(freq);
}

// Hook-Funktion, die AHI automatisch beim Starten jedes Playbacks ausführt.
// Dabei wird ein Signal geschickt.
ULONG SoundFunc(__REG__(a0, struct Hook *hook), __REG__(a2, struct AHIAudioCtrl *audioctrl), __REG__(a1, struct AHISoundMessage *smsg)) {
	Signal(audioctrl->ahiac_UserData, (1L<<sig));
	return(NULL);
}

int main() {
	BOOL end=FALSE;
	struct AHIAudioCtrl *audioctrl=NULL;
	struct AHISampleInfo info1={0, NULL, 0};
	struct AHISampleInfo info2={0, NULL, 0};
	BPTR file;
	APTR addr;
	WORD sound;
	LONG len;
	BOOL ok=FALSE;
	struct AHISampleInfo ton[8];
	char tondatei[8][256];
	BYTE n;
	ULONG loaderror;
	char dat[300];
	char config[64];
	ULONG panning=FALSE, pan;

	LONG pufgr;
	LONG freq=22050, f;
	ULONG typ=AHIST_M16S;
	BYTE faktor=1;
	UWORD offset=54;

	if ((port=CreatePort("IngaSoundPort", 40))) {
		// Sample-Format aus der Datei "Sound.config" erfahren
		if ((file=Open("Sound.config", MODE_OLDFILE))) {
			for (n=0; n<64; n++) config[n]=0;
			Read(file, config, 64);
			Close(file);
			for (n=0; n<64; n++) {
				if (config[n]==0) break;
				if (config[n]=='\n') config[n]=0;
			}
			freq=atol(config); n=strlen(config)+1;
			if (strcmp(&config[n], "M8")==0) {typ=AHIST_M8S; faktor=0;}
			if (strcmp(&config[n], "M16")==0) {typ=AHIST_M16S; faktor=1;}
			n=n+strlen(&config[n])+1;
			offset=atol(&config[n]);
		}

		// Acht Sample-Plätze initialisieren
		for (n=0; n<8; n++) {
			ton[n].ahisi_Type=typ;
			ton[n].ahisi_Address=NULL;
		}

		pufgr=freq/2;

		if (offneAHI()) {


			SoundHook.h_Entry=(HOOKFUNC)SoundFunc;

			f=PrefsFreq();
			// Mix-Frequenz soll höchstens Sample-Frequenz sein
			if (f>freq) f=freq;

			// 10 AHI-Soundplätze erzeugen
			if ((audioctrl=AHI_AllocAudio(
				AHIA_AudioID, AHI_DEFAULT_ID,
				AHIA_MixFreq, f,
				AHIA_Channels, 2,
				AHIA_Sounds, 10,
				AHIA_SoundFunc, (ULONG)&SoundHook,
				AHIA_UserData, (ULONG)FindTask(NULL),
				TAG_DONE))) {

				AHI_GetAudioAttrs(AHI_INVALID_ID, audioctrl, AHIDB_Panning, (ULONG)&panning, TAG_DONE);

				// Sample-Plätze für Sprachausgabe initialisieren mit Double-Buffer
				info1.ahisi_Address=AllocVec(pufgr, MEMF_ANY | MEMF_PUBLIC | MEMF_CLEAR);
				info2.ahisi_Address=AllocVec(pufgr, MEMF_ANY | MEMF_PUBLIC);
				info1.ahisi_Type=typ;
				info2.ahisi_Type=typ;
				info1.ahisi_Length=pufgr>>faktor;
				info2.ahisi_Length=pufgr>>faktor;

				if ((info1.ahisi_Address) && (info2.ahisi_Address)) {
					if ((!AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &info1, audioctrl)) &&
						(!AHI_LoadSound(1, AHIST_DYNAMICSAMPLE, &info2, audioctrl))) {

						sig=AllocSignal(-1);
						if (sig>=0) ok=TRUE;
					}
				}
			}
		}

		do {
			// Auf Nachricht von "Inga" warten
			WaitPort(port);
			while ((smsg=(struct SOUNDMESSAGE *)GetMsg(port))) {

				// Aktion: "IngaSound" schließen
				if (smsg->action==101) end=TRUE;

				// Aktion: "IngaSound" initialisieren
				if (smsg->action==100) {
					soundbase=smsg->soundbase;
					if (ok) {
						// "Inga" bescheid geben, dass "IngaSound" aktiv ist
						soundbase->soundplugin=TRUE;
						AHI_ControlAudio(audioctrl, AHIC_Play, TRUE, TAG_DONE);
					}
				}

				if (ok) {

					// Aktion: Einen Sample in eine der acht Sound-Plätze laden
					if (smsg->action==2) {
						n=smsg->id;
						if (n<8) {

							if (!((ton[n].ahisi_Address!=NULL) && (stricmp(tondatei[n], smsg->file)==0))) {

								strcpy(tondatei[n], smsg->file);

								if (ton[n].ahisi_Address!=NULL) {
									AHI_UnloadSound(2+n, audioctrl);
									FreeVec(ton[n].ahisi_Address);
								}

								strcpy(dat, "Sounds/");
								strcat(dat, smsg->file);
								strcat(dat, ".aiff");
								if ((file=Open(dat, MODE_OLDFILE))) {
									Seek(file, 0, OFFSET_END);
									len=Seek(file, offset, OFFSET_BEGINNING)-offset;
									if ((ton[n].ahisi_Address=AllocVec(len, MEMF_ANY | MEMF_PUBLIC))) {
										Read(file, ton[n].ahisi_Address, len);
										ton[n].ahisi_Length=len>>faktor;

										loaderror=AHI_LoadSound(2+n, AHIST_DYNAMICSAMPLE, &ton[n], audioctrl);
										if (loaderror) {
											FreeVec(ton[n].ahisi_Address);
											ton[n].ahisi_Address=NULL;
										}

									}
									Close(file);
								}
							}
						}
					}

					// Aktion: Einen Sound-Platz freigeben
					if (smsg->action==3) {
						n=smsg->id;
						if (n<8) {
							if (ton[n].ahisi_Address!=NULL) {
								AHI_UnloadSound(2+n, audioctrl);
								FreeVec(ton[n].ahisi_Address);
								ton[n].ahisi_Address=NULL;
							}
						}
					}

					// Aktion: Einen Sound abspielen
					if (smsg->action==4) {
						n=smsg->id;
						if (n<8) {
							if (ton[n].ahisi_Address) {
								if (panning) pan=(ULONG)((FLOAT)smsg->pan/100*0x10000); else pan=0x8000;
								AHI_Play(audioctrl,
									AHIP_BeginChannel, 0,
									AHIP_Freq, freq,
									AHIP_Vol, (ULONG)((FLOAT)smsg->vol/100*0x10000),
									AHIP_Pan, pan,
									AHIP_Sound, 2+n,
									AHIP_Offset, 0,
									AHIP_Length, 0,
									AHIP_LoopSound, AHI_NOSOUND,
									AHIP_EndChannel, NULL,
									TAG_DONE);
							}
						}
					}

					// Aktion: Einen Sound in einer Endlosschleife abspielen
					if (smsg->action==5) {
						n=smsg->id;
						if (n<8) {
							if (ton[n].ahisi_Address) {
								if (panning) pan=(ULONG)((FLOAT)smsg->pan/100*0x10000); else pan=0x8000;
								AHI_Play(audioctrl,
									AHIP_BeginChannel, 1,
									AHIP_Freq, freq,
									AHIP_Vol, (ULONG)((FLOAT)smsg->vol/100*0x10000),
									AHIP_Pan, pan,
									AHIP_Sound, 2+n,
									AHIP_Offset, 0,
									AHIP_Length, 0,
									AHIP_LoopSound, 2+n,
									AHIP_EndChannel, NULL,
									TAG_DONE);
							}
						}
					}

					// Aktion: Bricht geloopten Sound ab
					if (smsg->action==6) {
						AHI_SetSound(1, AHI_NOSOUND, 0, 0, audioctrl, AHISF_IMM);
					}

					// Aktion: Ändert Lautstärke und Panning des aktuellen, geloopten Sounds
					if (smsg->action==7) {
						if (panning) pan=(ULONG)((FLOAT)smsg->pan/100*0x10000); else pan=0x8000;
						AHI_SetVol(1, (ULONG)((FLOAT)smsg->vol/100*0x10000), pan, audioctrl, AHISF_IMM);
					}

					// Aktion: Sprachausgabe (Double-Buffer Playback direkt von HD)
					if (smsg->action==1) {
						strcpy(dat, "Sounds/");
						strcat(dat, smsg->file);
						if ((file=Open(dat, MODE_OLDFILE))) {

							SetSignal(0L, 1L<<sig);

							// Sample-Datei Header überspringen
							Seek(file, offset, OFFSET_BEGINNING);
							// Ersten Teil laden
							addr=info1.ahisi_Address; sound=0;
							len=Read(file, addr, pufgr);

							if (panning) pan=(ULONG)((FLOAT)smsg->pan/100*0x10000); else pan=0x8000;
							// Ersten Buffer abspielen
							AHI_Play(audioctrl,
								AHIP_BeginChannel, 0,
								AHIP_Freq, freq,
								AHIP_Vol, 0x10000,
								AHIP_Pan, pan,
								AHIP_Sound, 0,
								AHIP_Offset, 0,
								AHIP_Length, len>>faktor,
								AHIP_EndChannel, NULL,
								TAG_DONE);

							// Solange der Buffer vollgeladen werden kann...
							while (len==pufgr) {
								// Double-Buffer wechseln
								if (sound==0) {
									sound=1; addr=info2.ahisi_Address;
								} else {
									sound=0; addr=info1.ahisi_Address;
								}
								// Falls Sprachausgabe von "Inga" abgebrochen wird
								if (smsg->cancel) break;
								// Warten, bis Ausgabe beginnt...
								Wait(1L<<sig);
								// Nächsten Buffer laden
								len=Read(file, addr, pufgr);
								// Nächsten Buffer abspielen, sobald der vorherige beendet ist
								AHI_SetSound(0, sound, 0, len>>faktor, audioctrl, NULL);

							}
							// Auf Ende warten und dann Soundausgabe beenden
							Wait(1L<<sig);
							AHI_SetSound(0, AHI_NOSOUND, 0, 0, audioctrl, NULL);
							Wait(1L<<sig);

							Close(file);
						}

						// "Inga" bescheid geben, dass Sprachausgabe beendet ist
						soundbase->speech=FALSE;

						smsg->cancel=FALSE;
					}

				}
				smsg->busy=FALSE;
			}
		} while (!end);
		// "IngaSound" beenden...

		if (ok) AHI_ControlAudio(audioctrl, AHIC_Play, FALSE, TAG_DONE);

		FreeVec(info1.ahisi_Address);
		FreeVec(info2.ahisi_Address);
		if (ok) {
			for (n=0; n<8; n++) {
				AHI_UnloadSound(2+n, audioctrl);
				FreeVec(ton[n].ahisi_Address);
			}
		}
		if (audioctrl) AHI_FreeAudio(audioctrl);
		if (sig>=0) FreeSignal(sig);

		schliesseAHI();
		DeletePort(port);
	}
	exit(0);
}
