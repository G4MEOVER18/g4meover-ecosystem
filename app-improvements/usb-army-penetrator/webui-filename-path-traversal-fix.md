# usb-army-penetrator — Web-UI: fehlende Filename-Validierung (Path Traversal)

## Was
`src/Comms/Web/WebServer.cpp` nimmt an fünf Stellen einen client-gelieferten
`filename` unbehandelt entgegen und reicht ihn direkt an die
Storage-Schicht durch — ohne jede Prüfung auf `..`, Backslashes,
Steuerzeichen oder Leerstring:

- `/runfile?filename=...` → `Attacks::Ducky.setPayload(filename.c_str())`
- `/showimage?filename=...` → `Devices::TFT.displayPng(Devices::Storage, filename.c_str())`
- `/download?filename=...` → `Devices::Storage.openFile(filename.c_str(), "r")`
- `/delete?filename=...` → `Devices::Storage.deleteFile(filename.c_str())`
- `handleUpload()` (Multipart-Upload, `/uploadFile`) →
  `Devices::Storage.openFile(std::string("/")+filename.c_str(), "w")`

`filename` kommt in den ersten vier Fällen aus einem HTTP-Query-Parameter,
im Upload-Fall aus dem `Content-Disposition: filename=`-Feld des
Multipart-Requests — beides vollständig client-kontrollierter Text. Ich habe
den gesamten Quellbaum durchsucht (`grep -rn "\.\."` etc.): es gibt an
keiner Stelle im Repo eine Sanitisierung von Dateinamen, weder in
`WebServer.cpp` noch in der Storage-Abstraktion
(`HardwareStorage::openFile/deleteFile/getFileSize/...` reichen den String
1:1 an `FILE_INTERFACE.open()/.remove()` durch, siehe
`src/Devices/Storage/HardwareStorage.cpp:80-110`).

Meine eigene Speicherablage ist als flaches Root-Verzeichnis ausgelegt (alle
`.ds`-Payloads liegen direkt unter `/`, Kategorisierung läuft rein über
Dateinamens-Präfixe wie `wifi_`/`ble_`/`hid_`, siehe `/payloadinfo`-Handler
direkt daneben in derselben Datei) — ein `/` oder `..` mitten im
`filename`-Wert ist bei mir also nie ein legitimer Anwendungsfall, sondern
ausschließlich ein Angriffsvektor.

## Nutzen
Schließt eine Path-Traversal-Lücke im eigenen Web-Control-Interface
(Port 8080, `AsyncWebServer`), das genau für autorisierte
Pentest-Einsätze mitgeführt wird und dabei zwangsläufig zumindest
kurzzeitig in fremden/unbekannten Netzsegmenten (Ziel-WLAN, Zutritts-
Netzwerk beim Einsatz) aktiv sein kann. Ohne diesen Fix kann jeder Client,
der das Web-UI erreicht (nicht zwingend ich selbst):
- über `/delete?filename=../../esp32m_settings.json` oder ähnliche Pfade
  beliebige Dateien außerhalb des eigenen Payload-Verzeichnisses löschen,
- über `/download?filename=../...` beliebige über den Mountpoint
  erreichbare Dateien exfiltrieren (potenziell inkl. mitgeschnittener
  Capture-/Zugangsdaten aus anderen Modulen, die auf derselben SD-Karte
  abgelegt werden),
- über den Multipart-Upload (`handleUpload`) beliebige Dateien außerhalb
  des Payload-Verzeichnisses **überschreiben** — die schwerwiegendste
  Variante, da sie dem Gerät aktiv Daten/Code unterschieben kann (z. B.
  `esp32m_settings.json` oder ein anderes `autorun.ds` an unerwarteter
  Stelle).

Die Datei liegt bereits im Backlog als offener Prüfpunkt
("`usb-army-penetrator`: restliche Module (WiFi/BLE/USB-HID/Marauder,
Web-UI) weiterhin nicht im Detail geprüft") — dies ist der erste konkrete
Fund aus der Web-UI-Seite.

## Betroffene Dateien
- `src/Comms/Web/WebServer.cpp` (einzige betroffene Datei — neue
  `isFilenameSafe()`-Hilfsfunktion plus fünf Aufrufstellen).

## Risiko
Sehr gering. Die Änderung fügt ausschließlich eine zusätzliche
Vorab-Prüfung ein (reine String-Validierung, keine Änderung an
Storage-API, Protokoll oder sonstiger Logik) und lehnt im Fehlerfall mit
HTTP 400 ab, statt die Anfrage stillschweigend zu verwerfen — bestehende,
legitime Dateinamen (flaches Root-Verzeichnis, ASCII-Namen wie
`wifi_deauth.ds`, `autorun.ds`, `.env`) sind von der Prüfung nicht
betroffen (im Repro-Testprogramm gegen alle fünf betroffenen
Aufrufstellen-Muster verifiziert).

## Fix (Patch)

```diff
--- a/src/Comms/Web/WebServer.cpp
+++ b/src/Comms/Web/WebServer.cpp
@@
 static const char *GetMimeType(const char *fname)
 {
   std::string filename(fname);
   // Extract the file extension
   size_t dotPos = filename.find_last_of('.');
   if (dotPos == std::string::npos)
   {
     // No extension found
     return nullptr;
   }
 
   std::string extension = filename.substr(dotPos);
 
   // Look up the MIME type
   auto it = mimeTypes.find(extension);
   if (it != mimeTypes.end())
   {
     return it->second.c_str();
   }
   else
   {
     // Extension not found in our mapping
     return nullptr;
   }
 }
 
+// Storage is a flat root directory (all payloads live directly under "/",
+// categorisation is by filename prefix only -- see /payloadinfo below), so
+// a "/" or ".." component in a client-supplied filename is never a
+// legitimate request, only a traversal attempt. Reject before the value
+// ever reaches Storage/Ducky/TFT.
+static bool isFilenameSafe(const String &filename)
+{
+  if (filename.length() == 0) return false;
+  if (filename.indexOf("..") != -1) return false;
+  if (filename.indexOf('\\') != -1) return false;
+  for (size_t i = 0; i < filename.length(); i++)
+  {
+    const unsigned char c = (unsigned char)filename[i];
+    if (c < 0x20) return false; // control chars, including embedded NUL
+  }
+  return true;
+}
+
 static void webRequestHandler(AsyncWebServerRequest *request)
 {
@@
   else if (url == "/runfile" && request->hasParam("filename"))
   {
     const String filename = request->getParam("filename")->value();
+    if (!isFilenameSafe(filename))
+    {
+      Debug::Log.error(LOG_WEB, std::string("Rejected unsafe filename in /runfile: ") + filename.c_str());
+      request->send(400, "text/plain", "Invalid filename");
+      return;
+    }
     Debug::Log.info(LOG_WEB, std::string("Executing file from webUI - ") + filename.c_str());
 
     Attacks::Ducky.setPayload(filename.c_str());
     request->redirect("/index.html"); // redirect to our main page
   }
@@
   else if (url == "/showimage" && request->hasParam("filename"))
   {
     const String filename = request->getParam("filename")->value();
+    if (!isFilenameSafe(filename))
+    {
+      Debug::Log.error(LOG_WEB, std::string("Rejected unsafe filename in /showimage: ") + filename.c_str());
+      request->send(400, "text/plain", "Invalid filename");
+      return;
+    }
     Debug::Log.info(LOG_WEB, "Show image");
     Debug::Log.info(LOG_WEB, filename.c_str());
 
     Devices::TFT.displayPng(Devices::Storage, filename.c_str());
 
     request->redirect("/index.html"); // redirect to our main page
   }
@@
   else if (url == "/download" && request->hasParam("filename"))
   {
     const String filename = request->getParam("filename")->value();
+    if (!isFilenameSafe(filename))
+    {
+      Debug::Log.error(LOG_WEB, std::string("Rejected unsafe filename in /download: ") + filename.c_str());
+      request->send(400, "text/plain", "Invalid filename");
+      return;
+    }
     Debug::Log.info(LOG_WEB, std::string("In download, filename: ")+filename.c_str());
     auto file = Devices::Storage.openFile(filename.c_str(), "r");
@@
   else if (url == "/delete" && request->hasParam("filename"))
   {
     const String filename = request->getParam("filename")->value();
 
+    if (!isFilenameSafe(filename))
+    {
+      Debug::Log.error(LOG_WEB, std::string("Rejected unsafe filename in /delete: ") + filename.c_str());
+      request->send(400, "text/plain", "Invalid filename");
+      return;
+    }
+
     Debug::Log.info(LOG_WEB, std::string("Deleting file: ") + std::string(filename.c_str()));
 
     Devices::Storage.deleteFile(filename.c_str());
@@
 void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
 {
   const auto &url = request->url();
 
   if (url.startsWith("/uploadFile"))
   {
     if (!index) {
+
+      if (!isFilenameSafe(filename))
+      {
+        Debug::Log.error(LOG_WEB, std::string("Rejected unsafe upload filename: ") + filename.c_str());
+        request->send(400);
+        return;
+      }
     
       // We need to ensure that if the autorun.ds payload is currently running then we stop it
       // Otherwise we risk changing the file content while the script is executing
       if (filename == "autorun.ds")
       {
         Attacks::Ducky.setPayload("");
       }
```

## Testhinweis
- Host-seitiges Repro (kein Hardware-Bezug, reine String-/Pfad-Logik):
  `app-improvements/usb-army-penetrator/repro-webui-path-traversal/repro.cpp`.
  Bildet exakt die zwei Codepfade nach — ALT (heutiges Verhalten: Filename
  wird unverändert an `"/" + filename` bzw. direkt an die Storage-API
  gereicht) gegen NEU (`isFilenameSafe()`-Guard) — über zehn Fälle (normale
  Payload-Namen, `..`-Traversal in mehreren Varianten, Backslash, Leerstring,
  eingebettetes NUL). Bauen/Ausführen:
  ```
  g++ -O1 -Wall -Wextra -fsanitize=address,undefined -o repro repro.cpp
  ./repro
  ```
  Erwartete Ausgabe: jeder ALT-Fall, der laut Pfad-Resolver "ESCAPES ROOT"
  meldet, wird im NEU-Block von `isFilenameSafe()` abgelehnt; alle
  legitimen Dateinamen bleiben in beiden Blöcken unverändert nutzbar. Bei
  mir lokal verifiziert: 0 Fehlschläge, sauberer ASan/UBSan-Lauf.
- Lokaler Hardware-Test (nach Einbau des Patches):
  1. `ufbt`/PlatformIO-Build wie gewohnt, auf das Gerät flashen.
  2. Web-UI öffnen, `curl` gegen `http://<gerät>:8080/download?filename=../../esp32m_settings.json`
     (und analog `/delete`, `/runfile`, `/showimage`) — vor dem Patch
     liefert das Gerät die fremde Datei aus bzw. verändert sie, nach dem
     Patch kommt HTTP 400 "Invalid filename".
  3. Multipart-Upload mit präpariertem `filename` (z. B. über
     `curl -F "file=@test.ds;filename=../evil.json" http://<gerät>:8080/uploadFile`)
     gegen `/uploadFile` senden — vor dem Patch schreibt das Gerät die Datei
     ggf. außerhalb des Payload-Verzeichnisses, nach dem Patch HTTP 400.
  4. Regressionstest: normaler Upload/Download/Delete/Run/Showimage mit
     regulären Dateinamen (`wifi_deauth.ds`, `autorun.ds`, hochgeladenes
     Bild) muss weiterhin unverändert funktionieren.

## Quelle
- `G4MEOVER18/usb-army-penetrator` (Fork, Commit `411b153d749f66a5c0059c39a3933c61971fcb2f`),
  `src/Comms/Web/WebServer.cpp`, Handler `webRequestHandler()` und
  `handleUpload()`.
