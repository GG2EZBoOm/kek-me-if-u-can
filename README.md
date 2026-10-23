# Kek Me If You Can

Ein Prop-Hunt-Spiel mit **eigener C++-Engine**, ohne externe Bibliotheken: Geometrie, Materialien, Klänge und Musik
werden beim Start prozedural erzeugt. Am Ende steht eine einzelne EXE. Die einzige Datei neben dem Code ist das
Programmsymbol, eine Dose mit verschmitztem Blick. Das „can“ im Namen ist wörtlich gemeint.

## Herunterladen

Die fertigen EXEs gibt es unter [Releases](https://github.com/GG2EZBoOm/kek-me-if-u-can/releases/latest):
`KekMeIfYouCan.exe` laden und starten, eine Installation ist nicht nötig. Windows SmartScreen kann beim ersten Start
warnen, weil die Datei nicht signiert ist („Weitere Informationen“ → „Trotzdem ausführen“).

### Automatische Updates

Beim Start fragt das Spiel im Hintergrund das neueste Release auf GitHub ab. Gibt es eine neuere Version, lädt es die
EXEs, prüft Größe und SHA-256-Prüfsumme und tauscht die Dateien aus. Kommt das Update direkt nach dem Start an,
startet das Spiel sofort neu, sonst zeigt das Hauptmenü „Update ist bereit“ mit einem Neustart-Knopf. Während eines
Spiels wird nichts unterbrochen. In den Einstellungen lässt sich das abschalten oder von Hand prüfen.
`--no-update` überspringt die Prüfung für einen Start.

## Selbst bauen

```bash
build.bat
```

Danach `KekMeIfYouCan.exe` starten. Voraussetzung: Visual Studio 2022/2026 mit „Desktopentwicklung mit C++“
(liefert Compiler, CMake und Ninja mit).

## Spielprinzip

- **Props** verwandeln sich in Gegenstände der Karte und verstecken sich. Wer bis zum Rundenende überlebt, gewinnt.
- **Jäger** sind während der Versteckphase blind. Danach suchen sie, aber **jeder Treffer auf ein falsches Objekt kostet Lebenspunkte**.
- Nach jeder Runde rotieren die Teams, sodass jeder einmal jagt.

### Was dieses Prop Hunt besser macht

| Feature | Beschreibung |
|---|---|
| Exakte Trefferzonen | Schüsse treffen die echten Einzelteile eines Objekts: Durch die Lücke zwischen Stuhlbeinen schießt man vorbei. |
| Faire Tarnung | Echte und verwandelte Objekte werden identisch gerendert und ausgeblendet. Es gibt keine optischen Unterschiede, die Spieler verraten. |
| Ausrichtung übernehmen | Beim Verwandeln übernimmt man die Drehung des Vorbilds und sperrt sie automatisch, damit man sich sauber in Reihen einfügt. |
| Drehung mit Kollision | Große Objekte lassen sich nicht in Wände drehen. Ein Sofa muss sich seitlich durch die Tür schieben. |
| Taunts mit Punkten | 16 Taunts (Präsidentenglocke, Applaus, Kuckuck, Quietscheente …). Längere Taunts bringen mehr Punkte. |
| Zwangs-Taunt | Wer zu lange still ist, tauntet automatisch: alle 45 s, im Endspurt alle 20 s. |
| Köder | Props stellen zwei Kopien ihrer Tarnung auf. Treffer darauf kosten die Jäger Lebenspunkte. |
| Sonar | Jäger lösen einen Ping aus, auf den nahe Props hörbar antworten. |
| Granate | Eine pro Runde, für verdächtige Gruppen. |
| 3D-Klang mit Verdeckung | Geräusche hinter Wänden klingen gedämpft. So lassen sich Taunts orten. |
| Bots für beide Teams | Prop-Bots suchen sich passende Gruppen gleicher Objekte, richten sich aus, verteilen sich über die Karte und fliehen bei Treffern. Jäger-Bots gehen Geräuschen nach, nutzen Sonar und geben Testschüsse ab. |
| Infektionsmodus | Gefundene Props jagen mit (optional). |
| Lebendige Karten | Fahnen wehen im Wind, Riesenrad und Kettenflieger drehen sich. |
| Freie Durchgänge | Gegenstände, die eine Tür versperren würden, werden beim Kartenbau automatisch weggelassen. Ein Test prüft jede Tür. |
| Mehrspieler übers Internet | Bis zu 16 Spieler. Einladungscode, automatische Portfreigabe am Router (UPnP) und Discord-Einladungen (siehe unten). |

## Karten

Die Karte wählst du im Hauptmenü über die Kartenkarte unten rechts oder beim Spielstart.

### Bundestag

Das Reichstagsgebäude auf drei Ebenen plus Dach:

- **Plenarsaal** mit 232 blauen Abgeordnetensitzen auf Sitzstufen, Rednerpult, Präsidium, Regierungs- und
  Bundesratsbank, Bundesadler und Besucher- und Pressetribünen
- **Glaskuppel** mit zwei begehbaren Spiralrampen, Aussichtsplattform und Spiegelkegel. Durch den Glasboden
  sieht man in den Plenarsaal.
- **Westportal** mit Säulen und der Inschrift „DEM DEUTSCHEN VOLKE“, Eingangshalle mit Sicherheitsschleuse und
  großer Treppe
- Fraktionssäle, Sitzungssaal, Präsidialbüro, Pressezentrum, Lounge, Bibliothek, Poststelle, Kantine mit
  Currywurst-Theke, Küche, Lager, vier Ecktürme mit Treppenhäusern, Dachterrasse mit Dachrestaurant
- **Platz der Republik** mit Rasen, wehenden Fahnen, Besucherzentrum, Straße mit Polizeiwagen und der Spree.
  In der Ferne stehen Fernsehturm und Brandenburger Tor.

### Oktoberfest

Ein Festzelt auf der Theresienwiese in der Abenddämmerung:

- **Festzelt** mit Bierzeltgarnituren, Musikpodium in der Mitte, umlaufender **Empore mit Boxen**, Ausschank mit
  Fässern und Hendlküche, Wagenrad-Leuchtern und blau-weißem Himmel
- **Biergarten** mit blau-weißen Schirmen, Kastanien und Maibaum
- **Wirtsbudenstraße** mit Lichterketten und Ständen: Lebkuchen, Schießbude, Hendl, Mandeln, Steckerlfisch,
  Souvenirs, Zuckerwatte, Brezn und Lose
- **Fahrgeschäfte**: drehendes Riesenrad, Kettenflieger, Autoscooter-Halle und Kinderkarussell
- **Hinterhof** mit Toilettenhäuschen, Strohballen und Fässern. Im Hintergrund stehen Bavaria, Ruhmeshalle und
  die Türme der Frauenkirche.

### Nudelfabrik

Eine Backsteinfabrik mit Werkshof und zwei Produktionslinien:

- **Produktionshalle** unter einem verglasten Sheddach, mit drehenden Teigmischern, Nudelpresse, Schneidwerk,
  laufendem Nudelband und hängenden Spaghetti auf einer Stangenbahn
- **Laufstege auf vier Metern Höhe**, erreichbar über zwei Stahltreppen und mit Zugang zum Meisterbüro
- **Trockenraum**, Mehllager mit Hochregalen und Gabelstapler sowie eine Werkstatt
- **Verpackung** mit laufendem Kartonband, Paletten und einem begehbaren Lkw-Anhänger an der Laderampe
- **Bürotrakt** mit Kantine, Umkleide, Meisterbüro, Labor und Archiv
- **Außengelände** mit drei Mehlsilos, Parkplatz, Pförtnerhaus, Hinterhof und Schornstein

Für einen dedizierten Server: `KekMeIfYouCanCLI.exe --server --map nudelfabrik --bots 8`.

### Gegenstände

139 verschiedene Gegenstände. Im Bundestag gibt es unter anderem Aktenordner, Wasserglas, Laptop, Mate-Flasche,
Currywurst, Wahlurne, Präsidentenglocke, Büste und Ficus. Auf dem Oktoberfest kommen Maßkrug, Wiesnbrezn,
Lebkuchenherz, Bierfass, Hendl, Radi, Trachtenhut, Tuba, Teddybär, Gartenzwerg, Karussellpferd, Autoscooter und
viele mehr dazu. Die Nudelfabrik ergänzt 32 Gegenstände, darunter Mehlsack, Europalette, Spaghetti-Packung,
Tomatendose, Parmesanlaib, Trockenwagen, Hubwagen, Schutzhelm, Werkzeugkiste und Mikroskop.

## Steuerung

| | Allgemein | Jäger | Props |
|---|---|---|---|
| Linke Maustaste | | Schießen | |
| Rechte Maustaste | | Zielen | |
| E | | | In anvisiertes Objekt verwandeln |
| R | | Nachladen | Drehung sperren/lösen |
| Q | | Sonar | Köder platzieren |
| F | | | Zufälliger Taunt |
| T | | | Taunt-Auswahl |
| 1 / 2 / 3 | | MP / Schrotflinte / Granate | Taunt wählen (bei offener Liste) |
| W A S D, Leertaste, Umschalt | Laufen, Springen, Sprinten | | |
| Strg / C | | Ducken | |
| Tab / Enter / Esc / F1 / F11 | Punkte / Chat / Pause / Hilfe / Vollbild | | |
| F8 / F9 | Discord-Beitrittsanfrage annehmen / ablehnen | | |

## Mehrspieler

### Freunde einladen

1. Wähle **Server erstellen**. Die Windows-Firewall fragt beim ersten Mal nach einer Freigabe, die du erlauben musst.
2. Das Spiel gibt den **UDP-Port 27015** automatisch am Router frei (UPnP). Im **Esc-Menü** steht rechts die Karte
   **„Freunde einladen“** mit
   - einem **Einladungscode** wie `KEK-TE294-3VBJZ`. Er enthält deine IPv4-Adresse und den Port.
   - **Einladung kopieren**: Das kopiert eine fertige Nachricht für Discord oder einen anderen Chat.
   - **Öffentliche IP**: Diese Schaltfläche erscheint, wenn der Router die Adresse nicht verrät. Die Adresse wird
     erst auf Klick bei api.ipify.org nachgefragt.
3. Deine Freunde wählen **Server beitreten** und fügen den Code, die IP-Adresse oder gleich die ganze Nachricht ein
   (Einfügen-Knopf neben dem Feld). Server im selben Netzwerk erscheinen automatisch in der Liste.
4. Freie Plätze werden mit Bots aufgefüllt. Tritt ein Mensch bei, macht ein Bot Platz.

Wenn das automatische Freigeben nicht klappt, gib im Router UDP-Port 27015 für deinen PC frei. Das Esc-Menü zeigt
dafür die passende lokale Adresse an. Bei Anschlüssen ohne eigene öffentliche IPv4 (Carrier-NAT/DS-Lite) ist ein
Internetspiel nur per VPN möglich. Das Spiel erkennt diesen Fall und weist darauf hin.

### Discord

Läuft Discord auf dem PC, zeigt dein Profil Karte, Phase, Runde und Rolle. Freunde sehen bei einem Online-Spiel
**„Beitreten“** und landen mit einem Klick auf deinem Server. Ist das Spiel bei ihnen noch zu, startet Discord es.
Fragt jemand per Discord an, erscheint rechts ein Hinweis, den du mit **F8** annimmst oder mit **F9** ablehnst.

Damit Discord das Spielsymbol anzeigt, lade im [Discord Developer Portal](https://discord.com/developers/applications)
bei der Anwendung (ID `1563117727370055710`) unter **Rich Presence → Art Assets** die Datei `res/icon_1024.png` mit dem
Namen **`logo`** hoch. Der Name der Anwendung dort ist der Spielname, den Discord anzeigt.

### Dedizierter Server

```bash
KekMeIfYouCanCLI.exe --server --map oktoberfest --bots 8 --rounds 6 --name "Mein Server"
```

Der Server schreibt beim Start seinen Einladungscode fürs Heimnetz ins Fenster. Er aktualisiert sich nicht von
selbst. Ein Update holt dieser Befehl:

```bash
KekMeIfYouCanCLI.exe --update
```

## Für Entwickler

```bash
KekMeIfYouCanCLI.exe --test 3
```

```bash
KekMeIfYouCanCLI.exe --nettest
```

```bash
KekMeIfYouCan.exe --gameshot build\game
```

`--test` prüft Einladungscodes, Updater, Bewegung, Türen und Wegfindung auf allen Karten. Für die Nudelfabrik werden
zusätzlich alle Startplätze, die Förderband-Umläufe und die Sperre der Spaghetti-Bahn geprüft. Danach spielt es auf jeder
Karte ein komplettes Bot-Match im Zeitraffer (etwa 1000- bis 2000-fache Echtzeit). Zwei Prüfungen gelten der Grafik und
der Fairness:

- **Flackern:** Kein Flächenpaar darf in derselben Ebene liegen (bis 5 mm Abstand) und dabei unterschiedlich aussehen.
  Solche Flächen flackern, weil der Tiefenpuffer sie nicht trennen kann.
- **Blattwerk:** An keiner begehbaren Stelle darf man in Büschen oder Hecken stecken, die nur Optik sind.

`--nettest` testet Server und Client über Loopback. `--gameshot` klickt sich automatisch durch Menüs und Spiel und
speichert Screenshots, auch vom Einladungsmenü. Dabei werden weder der Router noch Discord angesprochen.

Einzelne Stellen einer Karte rendern (Kamera, Blickpunkt). `--hunters` stellt am Blickpunkt vier Jäger in
verschiedenen Posen auf:

```bash
KekMeIfYouCanCLI.exe --shots build\cam --map bundestag --cam -50 0.3 1.6 -50 -0.1 0 --hunters
```

Das Programmsymbol erzeugt `tools/make_icon.py` (benötigt Pillow) als `res/icon.ico` und `res/icon_1024.png`.

### Neue Version veröffentlichen

1. Version in `src/game/Version.h` erhöhen (z. B. `1.0.1`) und alles committen.
2. `release.bat` ausführen. Es pusht den Stand und das Tag `v1.0.1`.
3. GitHub Actions (`.github/workflows/release.yml`) baut, führt `--test` und `--nettest` aus und erstellt das
   Release mit beiden EXEs. Ab dann bekommen alle Spieler die neue Version beim nächsten Start.

Das Update testen, ohne eine neue Version zu bauen: Die EXEs in einen leeren Ordner kopieren und dort Folgendes
ausführen. Der Befehl gibt eine ältere eigene Version vor und installiert dann das aktuelle Release.

```bash
KekMeIfYouCanCLI.exe --update --pretend-version 0.0.1
```

### Aufbau

```
src/engine   Plattform (Win32/WGL), eigener GL-Loader, Renderer (HDR, kaskadierte Schatten, SSAO, Bloom, FXAA,
             prozedurale Materialien), UI mit Milchglas und GDI-Schriftatlas, Audio-Mixer (waveOut),
             UDP-Netzwerk, UPnP-Portfreigabe
src/game     Prop-Katalog, Karten-Baukasten, Karten (maps/), Kollision und Bewegung, Simulation,
             Navigationsgitter und Bots, Netzwerkprotokoll, Einladungscodes, Discord-Anbindung (IPC ohne SDK),
             Auto-Update (WinHTTP + SHA-256), Darstellung, Klangsynthese, Menüs und HUD
res          Programmsymbol und Versionsinfo
tools        Symbol-Generator
.github      Release-Workflow (bauen, testen, veröffentlichen)
```
