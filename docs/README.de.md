# Omarchy auf dem Amazon Fire HD 10 (2021)

Dieses Repository dokumentiert unseren nativen Linux-Port für das
**Fire HD 10 der 11. Generation, Codename Trona, MT8183**. Arch Linux ARM,
Hyprland und Omarchy laufen direkt auf der Hardware.

Enthalten sind Kernel-Patches, Device Tree, Treiberquellen, Tablet-Bedienung,
Kamera-App, Standby-Steuerung, Touch-Bildschirmsperre und Update-Schutz.
Die ausführlichen technischen Anleitungen sind auf Englisch:

1. [Hardware und offene Punkte](hardware.md)
2. [Kernel und Treiber bauen](build.md)
3. [Installation und Recovery](install.md)
4. [Updates](updates.md)
5. [Sicherheit und private Daten](security.md)
6. [Technischer Aufbau](architecture.md)
7. [Fehlerbehebung, einschließlich Browser-/WLAN-Problemen](troubleshooting.md)

**Es gibt jetzt einen experimentellen geführten Installer**, gestartet mit
`./install.sh`. Er prüft das Gerät, übernimmt die eigene Firmware, baut ein
frisches ARM-System samt Treibern und installiert es nach ausdrücklicher
Löschbestätigung. Die [Installer-Anleitung](installer.md) beschreibt alle Schritte.
Voraussetzungen: bereits entsperrtes **32-GB-Trona**, originales Partitionslayout,
TWRP 3.7.0_9-0, Python, ADB, ARM64-fähiges Docker und mindestens 25 GiB freier
Build-Speicher. Das Entsperren wird nicht automatisiert. Eine vollständige
Neuinstallation auf einem zweiten Tablet ist noch nicht am Gerät bestätigt.

Die Build-Schritte greifen nicht auf ein angeschlossenes Tablet zu. Firmware,
Kalibrierung, Zugangsdaten und Geräteabbilder sind bewusst nicht enthalten.
Für ein eigenes Gerät werden eigene Firmware-Dateien, Root-UUID und
Boot-/Recovery-Prüfsummen benötigt.

Im Alltag funktionieren auf dem Referenzgerät unter anderem Touch, automatische
Drehung, Tastatur, WLAN, Bluetooth-Tastatur, Lautsprecher, Lautstärketasten,
beide Kameras, SD-Karte, Standby und Touch-Entsperren. Optimierter
Firmware-Tiefschlaf und USB-A-Laden sind zurückgestellt. Andere USB-C-Netzteile,
Kopfhörerbuchse und Bluetooth-Audio brauchen eigene Tests.

Der abschließende Speicherumbau hat Android entfernt und Linux etwa 28,93 GiB
gegeben. Der Installer bietet diesen destruktiven Schritt erst nach Prüfung und
ausdrücklicher Bestätigung an. Android und interne Benutzerdaten werden gelöscht;
die SD-Karte wird nicht beschrieben. Private Installer-Dateien gehören niemals
in öffentliche Fehlerberichte oder Releases.
Die vorherige Installation als Datei in Androids Datenpartition benötigt ein
anderes Startabbild. Die Varianten dürfen nicht vermischt werden.

Das Projekt wurde von Stefan Maier mit Unterstützung durch OpenAI Codex
entwickelt und am Gerät geprüft. Die wesentlichen Vorarbeiten und ihre Autoren
stehen in [CREDITS.md](../CREDITS.md).
