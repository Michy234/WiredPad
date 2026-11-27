`evdi_lib.h` proviene da https://github.com/DisplayLink/evdi/blob/main/library/evdi_lib.h
(versione dichiarata 1.15.1), acquisito il 2026-11-12. Copyright DisplayLink (UK) Ltd.,
LGPL-2.1-or-later; testo in EVDI-LICENSE. Il demone carica dinamicamente la libreria
libevdi compilata e distribuita insieme a WiredPad; nessun codice binario Splashtop viene distribuito.

Il modulo kernel è distribuito separatamente come `evdi-wiredpad-dkms`,
con sorgenti DisplayLink GPL-2.0-only fissati al commit
8acd0b28230d271cc338bc3b2f201fb696dbd286. Il PKGBUILD scarica l'archivio
upstream e ne verifica SHA-256. La configurazione DKMS locale seleziona il
kernel di destinazione e abilita la ricompilazione automatica; il pacchetto
installa anche il caricamento all'avvio e `initial_device_count=1`.
