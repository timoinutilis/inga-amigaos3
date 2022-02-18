// Prototypen
void StoppeReden();
void Fehler(UWORD num, STRPTR t)
#ifdef __GNUC__
__attribute__((noreturn))
#endif
;
void Meldung(STRPTR t);
void Maus();
void StandartMauszeiger();
void MausStatusEcke(BOOL w);
void MausStatusWarte(BOOL w);
void MausStatusSichtbar(BOOL w);
void MausTastaturStatus();
void Ende();
