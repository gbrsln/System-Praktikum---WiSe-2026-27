# MS01 – Teil A: TCP-Verbindungsaufbau

## 1. Zielsetzung

Dieser Teil des Systempraktikums implementiert den TCP-Verbindungsaufbau zwischen dem Reversi-Client und dem Gameserver der LMU München.

Die Implementierung stellt einen gültigen Socket-File-Descriptor bereit, der anschließend für die Protokollkommunikation in `performConnection()` verwendet werden kann.

## 2. Implementierte Funktionalitäten

- Auflösung des Hostnamens mittels `getaddrinfo()`
- Erzeugung eines TCP-Sockets mittels `socket()`
- Verbindungsaufbau mittels `connect()`
- Unterstützung von IPv4 und IPv6
- Iteration über verfügbare Netzwerkadressen
- Fehlerbehandlung bei fehlgeschlagenen Systemaufrufen
- Freigabe reservierter Adressinformationen mittels `freeaddrinfo()`
- Schließen nicht erfolgreich verbundener Sockets mittels `close()`
- Unterstützung alternativer Serverparameter für den Mock-Server

## 3. Dateistruktur

| Datei | Beschreibung |
|---|---|
| `connection.c` | Implementierung des TCP-Verbindungsaufbaus |
| `connection.h` | Deklaration der Netzwerkschnittstelle |
| `constants.h` | Definition der Serverkonstanten |
| `test_connection.c` | Eigenständiges Testprogramm für den Verbindungsaufbau |

## 4. Funktionsschnittstelle

```c
int connectToGameserver(void);
```

**Rückgabewerte:**

- `>= 0`: Gültiger Socket-File-Descriptor bei erfolgreichem Verbindungsaufbau.
- `-1`: Verbindungsaufbau fehlgeschlagen.

Der erfolgreich verbundene Socket wird an den Aufrufer zurückgegeben. Dieser ist für das anschließende Schließen des Sockets verantwortlich.

## 5. Serverkonfiguration

Die Standardkonfiguration befindet sich in `constants.h`.

```c
#ifndef CONSTANTS_H
#define CONSTANTS_H

#define GAMEKINDNAME "Reversi"

#ifndef PORTNUMBER
#define PORTNUMBER 1357
#endif

#ifndef HOSTNAME
#define HOSTNAME "sysprak.priv.lab.nm.ifi.lmu.de"
#endif

#endif
```

### Standardparameter

| Parameter | Wert |
|---|---|
| GAMEKINDNAME | Reversi |
| HOSTNAME | sysprak.priv.lab.nm.ifi.lmu.de |
| PORTNUMBER | 1357 |
| Transportprotokoll | TCP |

Durch die bedingten Präprozessordirektiven `#ifndef` und `#endif` können `HOSTNAME` und `PORTNUMBER` beim Kompilieren überschrieben werden, ohne den Quellcode verändern zu müssen.

## 6. Mock-Server-Kompatibilität

Zur Unterstützung automatisierter Tests kann der Client für einen lokalen Mock-Server kompiliert werden.

| Parameter | LMU-Gameserver | Mock-Server |
|---|---|---|
| HOSTNAME | sysprak.priv.lab.nm.ifi.lmu.de | 127.0.0.1 |
| PORTNUMBER | 1357 | 13570 |

Die Überschreibung erfolgt über die Compileroption `-D`.

```bash
-DHOSTNAME='"127.0.0.1"' -DPORTNUMBER=13570
```

Dadurch kann dieselbe Netzwerkimplementierung sowohl für den produktiven Gameserver als auch für den lokalen Mock-Server verwendet werden.

## 7. Kompilierung und Ausführung

### Voraussetzungen

- GCC oder Clang
- C11-Sprachstandard
- POSIX-kompatible Entwicklungsumgebung
- Netzwerkzugang zum LMU-Gameserver für reale Verbindungstests

### Standardkonfiguration

```bash
gcc -std=c17 -Wall -Wextra -Werror -g \
    connection.c test_connection.c \
    -o test_connection
```

Ausführung:

```bash
./test_connection
```

### Mock-Server-Konfiguration

```bash
gcc -std=c17 -Wall -Wextra -Werror -g \
    -DHOSTNAME='"127.0.0.1"' \
    -DPORTNUMBER=13570 \
    connection.c test_connection.c \
    -o test_connection_mock
```

Ausführung bei laufendem Mock-Server:

```bash
./test_connection_mock
```

Das Testprogramm überprüft ausschließlich den TCP-Verbindungsaufbau und nicht die vollständige Prolog-Kommunikation.

## 8. Fehlerbehandlung

Folgende Fehlerfälle werden berücksichtigt:

- Hostname kann nicht aufgelöst werden.
- Socket kann nicht erzeugt werden.
- Gameserver ist nicht erreichbar.
- Verbindungsaufbau wird verweigert.
- Keine der verfügbaren Netzwerkadressen ermöglicht einen erfolgreichen Verbindungsaufbau.

Die Fehlermeldungen werden mittels `perror()`, `fprintf()` und `gai_strerror()` ausgegeben.

## 9. Integration in das Gesamtprojekt

Das Netzwerkmodul stellt die Grundlage für die weitere Implementierung des Connectors bereit.

Vorgesehener Programmablauf:

```text
main()
    |
    v
connectToGameserver()
    |
    +-- getaddrinfo()
    +-- socket()
    +-- connect()
    +-- freeaddrinfo()
    |
    v
Socket-File-Descriptor
    |
    v
performConnection()
    |
    v
close()
```

Die Implementierung der Prolog-Phase, der Kommandozeilenparameter und der vollständigen Protokollverarbeitung erfolgt in den weiteren Modulen des Gruppenprojekts.

## 10. Teststatus

- [ ] Erfolgreiche Kompilierung ohne Warnungen
- [ ] Verbindung zum LMU-Gameserver
- [ ] Verbindung zum lokalen Mock-Server
- [ ] Fehlerbehandlung bei nicht laufendem Gameserver
- [ ] Integration mit `performConnection()`
- [ ] Automatisierte Mock-Server-Tests

## 11. Projektinformationen

- Veranstaltung: Systempraktikum
- Universität: Ludwig-Maximilians-Universität München
- Semester: WiSe 2026/27
- Gruppe: 03
- Projekt: Reversi-Client
- Meilenstein: MS01
- Programmiersprache: C17
- Branch: `MS01/Teil-A`

