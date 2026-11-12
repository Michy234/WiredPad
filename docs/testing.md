# Prove reali del 2026-11-12

## Esito attuale

L'utente ha confermato su **Plasma Wayland**:

- pattern JPEG con barra bianca in movimento;
- desktop virtuale e aggiornamenti delle finestre;
- tap nel punto corretto;
- trascinamento delle finestre, dopo la correzione dell'iniezione del puntatore.

Dispositivo: **iPad2,7, iOS 9.3.6**. App `com.splashtop.wiredxdisplayipad`,
**1.2.3 (84686)**. Non è stata provata una macchina iOS 10.
Host Arch: kernel **7.2.11-zen1-1-zen**, Plasma Wayland.

## Differenza identificata dalla cattura Windows

File `xdisplay-usbmux-20261112-230835.pcapng`, SHA-256
`2e57c0a9e99095aa77d2b089e131c21356653644137617d402b5882765957e52`.
Il file è stato analizzato con REA 6.9.0, TShark/capinfos 4.7.4 e riassemblaggio
TCP con verifica di gap e overlap. Gli esiti senza immagini sono versionati in
`docs/evidence/windows-capture-summary.json` e nelle evidenze REA locali associate.

La sessione funzionante usa la porta iPad 6789 e lo stesso handshake Linux:
probe, HELLO 1024×768, READY e capacità `0xfc/0xfd`. Il flusso host comprende
797 payload tag 06, **tutti JPEG baseline completi**, 1024×768, YCbCr 4:2:0.
Il video iniziale H.264, scelto erroneamente dalla presenza del wrapper x264 nel
setup, lasciava lo schermo nero. Passare al codec MJPEG di FFmpeg ha risolto
il problema sul dispositivo. La specifica finale è in [protocol notes](../PROTOCOL_IOS.md).

La trascrizione separata dell'handshake Linux reale è in
`docs/evidence/hardware-ipad2-7-handshake.json`. Non contiene record lockdown.
La cattura e i JPEG del desktop Windows restano locali, ignorati da Git.

## Output EVDI

EVDI **1.15.1**, commit `8acd0b28230d271cc338bc3b2f201fb696dbd286`, compilata
localmente contro gli header **7.2.11.zen1-1** corrispondenti al kernel in uso.
Gli header correnti del repository erano 7.2.15: non sono stati installati e
il kernel non è stato aggiornato. Materiali in `.research/evdi` e `.research/zen-headers`.
La generazione BTF è stata disabilitata nella build locale perché pahole è assente.

Il modulo è caricato per questa sessione con `initial_device_count=1`, senza
configurazione permanente di modprobe o servizi. La libreria è caricata dalla
build locale. `scripts/start.sh` la riusa automaticamente se presente.

Output **DVI-I-1**, `/dev/dri/card0`, 1024×768@30, scala 1, origine 1536,0.
Il pannello eDP-1 è rimasto primario, 1920×1200, scala 1.25, origine 0,0.
KWin fornisce **ARGB8888**: è stato corretto il rifiuto iniziale del formato,
che ammetteva solo XRGB8888. Entrambi hanno disposizione B,G,R,A/X su questo host.
La cattura restituisce pixel del desktop e l'utente ne ha confermato la visione
sull'iPad dopo il passaggio a JPEG.

## Input

La cattura Windows contiene pressioni/rilasci `{5,x,y}` / `{6,x,y}`. Durante
il drag Linux sono stati verificati anche movimenti `{10,x,y}` tra down e up.

Due correzioni derivate dai test fisici:

1. uinput rifiutava 256 slot; ora gli ID sono mappati a **dieci slot** riutilizzabili.
2. Tradurre i comandi legacy in touch diretto permetteva il tap ma non il drag.
   Ora vengono iniettati da **WiredPad Pointer**, una penna assoluta uinput,
   mappata da KWin a DVI-I-1. Tap e drag sono stati confermati dall'utente.

**WiredPad Touch** resta disponibile per il parser HID 16/17, con dieci slot.
Entrambi i dispositivi sono associati a DVI-I-1 tramite proprietà outputName di KWin.
Lo script `wiredpad-map-input DVI-I-1` applica e verifica il mapping
soltanto ai due dispositivi XDisplay.

Nella prova con due dita sono arrivati 15 messaggi tag 07, **comando 11**, senza
ID multipli o comandi HID 16/17. Il comando 11 non viene iniettato: non sono stati
validati scorrimento o gesti, né un flusso multitouch grezzo di questa app.

## Verifiche automatiche e limiti

`make check` verifica parser LE/BE, frammentazione, concatenazione, troncamenti,
limiti, handshake/capacità, input legacy e HID, allocazione/rilascio dei dieci slot,
EDID e encoding/decoding di JPEG completi. L'EDID 2048×1536@30 è stato inoltre
validato con edid-decode. Queste verifiche non sostituiscono il collaudo fisico.

Non provati fisicamente: iOS 10, X11, rotazione, multitouch HID, diff-JPEG e
riconnessione dopo un riavvio del sistema. Non sono state misurate latenza o
qualità comparata con Windows.

Per la sessione corrente il demone può usare modulo e libreria già predisposti;
dopo il riavvio serve nuovamente un modulo EVDI compatibile caricato. Non è stata
installata una configurazione DKMS permanente. Le istruzioni di cattura Windows
restano in [windows-capture.md](windows-capture.md) per future versioni del client.

## Packaging follow-up

WiredPad now includes a Qt desktop app, an Arch PKGBUILD, desktop/AppStream
metadata and an optional user service. The app starts and stops its own host
and maps the renamed input devices on Plasma. The packaged source excludes
all raw captures, installers and decompiled evidence. The host and app each
prevent duplicate instances.
