# Cattura di riferimento Windows

Serve una cattura **funzionante** dell'agent originale con lo stesso iPad e la
stessa app 1.2.3. La prima cattura ha identificato JPEG e risolto lo schermo nero del prototipo.
Questa procedura resta utile per confrontare versioni del client ancora non provate.

1. Su Windows verifica prima che l'agent originale mostri effettivamente il
   desktop sull'iPad. Se anche Windows resta nero, annotalo: non avremmo ancora
   una sessione di riferimento valida.
2. Installa [Wireshark con Npcap](https://www.wireshark.org/download.html), se
   non è presente. Serve la cattura **loopback** Npcap, non l'adattatore USB.
3. Esci dall'agent XDisplay nell'area di notifica Windows. Lascia l'iPad
   collegato e l'app aperta. Chiudi iTunes e altri programmi che comunicano
   con il dispositivo, per isolare il traffico della prova.
4. Copia [capture-windows.ps1](../scripts/capture-windows.ps1) su Windows e apri
   PowerShell nella sua directory. Esegui:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\capture-windows.ps1 -Seconds 30
   ```

   L'opzione ExecutionPolicy si applica solo a quell'invocazione. Se Npcap è
   configurato per i soli amministratori, usa PowerShell come amministratore.
5. Appena parte la cattura, avvia l'agent Windows. Quando compare l'immagine,
   muovi una finestra, tocca i quattro angoli e prova due dita contemporaneamente.
   La registrazione termina da sola e scrive `xdisplay-usbmux-*.pcapng` sul Desktop.
6. Riporta il file in questa conversazione insieme alla versione dell'agent
   Windows e alla conferma che l'immagine fosse visibile durante la cattura.

Lo script usa `dumpcap`, seleziona l'interfaccia Npcap loopback e applica il filtro
`tcp port 27015`. Non installa servizi, non modifica il pairing e non cambia
la configurazione permanente di PowerShell. Non è stato eseguito su Windows
in questa sessione. Il traffico contiene comunicazioni locali verso il servizio
USB Apple: conserva la cattura privata per questa analisi.

Se la selezione automatica non riesce, usa il numero indicato nell'elenco:

```powershell
.\capture-windows.ps1 -Interface 3 -Seconds 30
```

Alternativa grafica: in Wireshark seleziona **Adapter for loopback traffic capture**,
usa il capture filter `tcp port 27015`, inizia a registrare prima di avviare
l'agent e salva come pcapng dopo circa 30 secondi. Nessun proxy deve essere
inserito nel percorso: si osserva la sessione originale.

Riferimenti: [opzioni dumpcap](https://www.wireshark.org/docs/man-pages/dumpcap.html),
[cattura loopback Npcap](https://npcap.com/guide/npcap-users-guide.html).
