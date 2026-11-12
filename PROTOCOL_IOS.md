# Wired XDisplay: protocollo USB iOS

## Evidenze e ambito

Specifica ricavata con **REA 6.9.0 / Ghidra 12.1.4** dal setup Windows fornito,
poi corretta e verificata con una cattura di una sessione Windows funzionante e
prove Linux reali. REA ha analizzato anche la cattura tramite TShark 4.7.4.

Client effettivamente provato: **iPad2,7, iOS 9.3.6, XDisplay 1.2.3 (84686)**,
bundle `com.splashtop.wiredxdisplayipad`. Non è stata verificata una macchina
con iOS 10. I risultati fisici sono in [docs/testing.md](docs/testing.md).

| Artefatto | SHA-256 |
|---|---|
| Setup Windows 1.5.8.3 | `fb7d9a5cf6ae1b52786827ab9357062ec5a7a9b49d1faaaaebc4b2e2b1c5f182` |
| Agent CAB `_8CFB2A6B0585CABF34C2023D32965904` | `cb9ab63be48947344e6d0e5e718e5522e707c64473dac6cc026149424964e52c` |
| `xdisplay-usbmux-20261112-230835.pcapng` | `2e57c0a9e99095aa77d2b089e131c21356653644137617d402b5882765957e52` |

Il setup è stato estratto staticamente tramite il CAB MSI incorporato, senza
eseguirlo. I nomi delle classi provengono dalle stringhe diagnostiche; gli
indirizzi sono VA del PE. Il pseudocodice REA conserva i limiti del decompilatore,
compresi argomenti impliciti e chiamate WinSock per ordinali.

La cattura comprende 4145 pacchetti loopback. Il tunnel rilevante è la connessione
TCP Windows `50635 → 27015`, con richiesta usbmux Connect verso porta iPad 6789.
I due flussi applicativi sono stati riassemblati senza gap o overlap conflittuali,
rimuovendo richiesta e risposta usbmux iniziali. Il flusso host contiene
**797 frame JPEG completi**; quello client contiene 20 eventi mouse (10 pressioni,
10 rilasci), oltre all'handshake. Nessun frame del desktop è distribuito nel repo.

Il riepilogo pubblico senza immagini e la trascrizione dell'handshake reale sono
in [docs/evidence](docs/evidence). Output REA completi, listing e catture restano
locali: possono contenere percorsi personali, codice decompilato o desktop.
L'analisi statica è ripetibile con gli script inclusi e un installer fornito dall'utente.

## Trasporto USB

La porta TCP **sull'iPad è 6789 (`0x1a85`)**.

- `0x410020`: inizializza il campo configurazione `+8` a `0x1a85`.
- `CProbeUsbmuxd::Run`, `0x430bb0`: enumera i device e passa handle/porta a
  `usbmuxd_connect`; la chiamata è a `0x430c9c`.
- La richiesta usbmux Connect nella cattura conferma questa porta.

`127.0.0.1:27015` è il servizio USB Apple su Windows, non la porta dell'app.
Il demone Linux usa libusbmuxd sul socket locale `/var/run/usbmuxd`.
[L'API libusbmuxd](https://github.com/libimobiledevice/libusbmuxd/blob/master/include/usbmuxd.h)
richiede `usbmuxd_connect(handle, 6789)` in host byte order, senza `htons`.
Sono selezionati solo dispositivi `CONNECTION_TYPE_USB`, con ProductType iPad,
e viene verificata la sessione lockdown tramite libimobiledevice.

L'UDID appartiene all'enumerazione usbmuxd: non è presente nell'HELLO applicativo.
Il pairing resta gestito dalle librerie con record in `/var/lib/lockdown`.
Il demone non accede direttamente ai segreti contenuti in quei record.

## Header applicativo

Header **4 byte**, seguito dal payload. Non esiste un magic fisso per ogni frame.

| Offset | Byte | Significato |
|---|---|---|
| 0 | 1 | tag |
| 1 | 3 | lunghezza unsigned 24 bit del solo payload |
| 4 | N | payload |

La lunghezza è little endian nel protocollo legacy e big endian nella modalità
network-order. Il tag resta sempre il primo byte. Non aggiungere padding, CRC,
sequence number o timestamp. Header/payload possono essere frammentati o
concatenati sul TCP. Il campo ammette `0xffffff`; il demone limita N a 8 MiB.

| Tag | Significato verificato o ricavato staticamente |
|---|---|
| `04` | probe/ack byte order, lunghezza zero |
| `01` / `05` | comandi legacy LE / network BE, tre u32 |
| `02` / `06` | dati video legacy / network; **il tag non identifica H.264** |
| `03` / `07` | input/controllo, tre u32 LE / BE |
| `20` | estensione JSON del protocollo v2 nel codice Windows; non osservata sul client provato |
| `11` | riconfigurazione video: payload `{1,width,height}` BE, da `0x42f220` |
| `12` | ramo video alternativo presente nel codice; non implementato né validato |

Fonti statiche: `DoHandshake` (`0x4303f0`), `Run` (`0x42f580`),
`ReceiveThread` (`0x42f840`), `SendReconfigVideo` (`0x42f220`).
La cattura funzionante non invia una configurazione `11` prima del video.

## Handshake reale

Tutti i messaggi usano lo stesso tunnel. In network-order, sul client provato:

```text
host: 04 00 00 00
ipad: 04 00 00 00
host: 05 00 00 0c  00 00 00 01  00 00 00 00  00 00 00 00
ipad: 05 00 00 0c  00 00 00 02  00 00 04 00  00 00 03 00
host: 05 00 00 0c  00 00 00 fe  00 00 00 00  00 00 00 00
ipad: 05 00 00 0c  00 00 00 ff  00 00 00 00  00 00 00 00
host: 05 00 00 0c  00 00 00 fc  05 00 00 00  00 00 00 00
ipad: 05 00 00 0c  00 00 00 fd  00 00 00 00  00 00 00 00
```

1. Probe `04`: il Windows host aspetta 4 byte per 5000 ms. Ack con primo byte 4
   abilita network-order; altrimenti l'host originale ripiega sul legacy.
   Il demone ripiega solo quando non arriva alcun byte e rifiuta risposte troncate
   o inattese. `--legacy` evita il probe e usa subito LE.
2. HELLO `{1,0,0}` → `{2,width,height}`: qui 1024×768.
3. READY `{0xfe,0,0}` → `{0xff,0,0}`. Il Windows host ignora gli ultimi due u32.
4. Capacità: comando `0xfc`, maschera 5, zero. **La maschera è LE anche dopo
   il probe network-order**, come osservato sia nel codice sia nella cattura.
5. Questo client risponde `{0xfd,0,0}`, senza JSON. L'host moderno attende la
   risposta extra per 500 ms e nel ramo v2 cerca la proprietà JSON `diff_jpeg`.
   Il demone consuma l'ack ma non abilita quel ramo non validato.

Width/height vengono arrotondate al multiplo di 4 superiore, come nell'agent.
Non compaiono UDID, nome dispositivo, identificatore codec o FPS nell'HELLO/READY.
Il pacing è locale: configurazione Windows `+0x28`, default 30 FPS. `--fps` cambia
il pacing Linux, non una capacità negoziata con l'iPad.

## Video effettivamente usato: JPEG

Il payload del tag **06 è un JPEG baseline completo**, per ogni frame:

```text
06 01 97 c1  ff d8 ff e0 00 10 4a 46 49 46 ... ff d9
^^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
TLV,104385 B       immagine JPEG indipendente
```

Tutti i 797 payload video della cattura hanno SOI `ff d8`, EOI `ff d9`, SOF0
`ff c0`, precisione 8 bit, risoluzione 1024×768, tre componenti e sampling
`22 11 11`: **YCbCr 4:2:0, full range** (`yuvj420p` in FFmpeg).
Le dimensioni dei payload sono 104385–105607 byte. Non ci sono prefissi per-NAL,
RTP, MP4, MPEG-TS o un header aggiuntivo prima del JPEG. Il TLV delimita il frame;
la concatenazione di queste immagini equivale a uno stream MJPEG.

La presenza di SRx264Wrapper/libx264 nel setup dimostra un encoder disponibile,
non il suo uso in questa sessione. L'analisi statica del wrapper ricostruisce
H.264 Annex B, ma **inviare quel bitstream sotto tag 06 a questo client produceva
schermo nero**. Sostituirlo con JPEG ha risolto il video nelle prove fisiche.

Il demone usa libavcodec MJPEG, YUVJ420P full range, qualità fissa qscale 4,
un thread, senza ritardo tra frame. Il livello di qualità non è un campo del
protocollo e non replica esattamente le tabelle di quantizzazione Windows.

## Input legacy osservato

Tag 07: payload di **12 byte**, tre u32 big endian; tag 03 è la variante LE.
La cattura Windows contiene:

| Primo u32 | Secondo u32 | Terzo u32 | Evento |
|---|---|---|---|
| 5 | x | y | pressione sinistra |
| 6 | x | y | rilascio sinistro |

Nella prova Linux di trascinamento sono osservati anche `{10,x,y}` per i
movimenti. Le coordinate sono pixel assoluti del display virtuale, non float
o valori normalizzati 0..65535. Esempio reale x=433, y=334:

```text
07 00 00 0c  00 00 00 05  00 00 01 b1  00 00 01 4e
07 00 00 0c  00 00 00 06  00 00 01 b1  00 00 01 4e
```

Il demone mantiene lo stato premuto durante i movimenti e applica il clamp
sulle dimensioni del display. Per questo percorso espone **WiredPad Pointer**
tramite uinput come penna assoluta: libinput/KWin possono associarla al solo
output virtuale e conservare click e trascinamento. Il primo tentativo di
tradurre questi comandi in tocchi diretti permetteva il tap ma non il drag.

Nella prova fisica con due dita, questa versione dell'app ha emesso **comando
11** con valori nel secondo u32, senza coppie di ID contatto. La semantica
completa del gesto non è stata validata e il demone non lo inietta. Non è una
prova di multitouch grezzo disponibile in questa modalità dell'app.

## Input HID multitouch ricavato staticamente

Il dispatcher Windows `0x440110` e `CInput::HandleHidMessage` (`0x440540`)
ricostruiscono un ulteriore percorso, non osservato nelle catture fisiche:

| Offset payload | Tipo | Campo |
|---|---|---|
| 0 | u32 | `16`: down/move; `17`: up |
| 4 | u32 | ID nel byte basso |
| 8 | u32 | x nei 16 bit bassi, y nei 16 bit alti |

Convertire prima l'intero u32 da BE per tag 07, poi separare gli halfword.
Il codice Windows interpreta x/y come int16, sottrae **1 solo a x**, aggiunge
l'origine del monitor e applica il clamp. `Control` (`0x43f360`) mantiene lo
stato per ID: primo down, move successivi, poi up. Inizializza al massimo dieci
contatti contemporanei. Non è presente un array di contatti per frame.

Il demone implementa questo parser e un dispositivo **WiredPad Touch**,
con dieci slot uinput e mappatura degli ID 0..255 agli slot liberi. Richiedere
256 slot è rifiutato dal kernel uinput, anche se l'ID del protocollo è un byte.
Il ramo è verificato con test sintetici di parsing e gestione slot; **non è
validato fisicamente come multitouch**. I bit alti dell'ID sono rifiutati.

## Limiti verificabili

Video, aggiornamenti del desktop, tap e trascinamento sono stati confermati
fisicamente su Plasma Wayland con il client sopra identificato. Non estendere
questa conclusione a iOS 10, X11, rotazione, multitouch HID o diff-JPEG.
Le richieste di riconfigurazione `0x70/0x71` provocano riconnessione.
Il mapping dei due dispositivi input all'output è configurato in KWin/XInput,
non incorporato in un offset fisso del desktop.
