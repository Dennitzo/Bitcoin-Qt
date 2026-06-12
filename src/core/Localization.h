#pragma once

#include <QString>

inline QString appText(const QString& language, const QString& key)
{
    const bool english = language.toLower().startsWith("en");

    if (key == "app.settings") return english ? "Settings" : "Einstellungen";
    if (key == "app.start") return english ? "Start" : "Start";
    if (key == "app.stop") return english ? "Stop" : "Stop";
    if (key == "app.save") return english ? "Save" : "Speichern";
    if (key == "app.select") return english ? "Select" : "Auswählen";

    if (key == "dashboard.title") return "Dashboard";
    if (key == "dashboard.blockHeight") return english ? "Block height" : "Blockhöhe";
    if (key == "dashboard.sync") return "Sync";
    if (key == "dashboard.storage") return english ? "Storage" : "Speicher";
    if (key == "dashboard.calculating") return english ? "Calculating" : "Berechne";
    if (key == "dashboard.unavailable") return english ? "Unavailable" : "Nicht verfügbar";
    if (key == "dashboard.peers") return "Peers";
    if (key == "dashboard.network") return english ? "Network" : "Netzwerk";
    if (key == "dashboard.offline") return "Offline";

    if (key == "state.stopped") return english ? "Stopped" : "Gestoppt";
    if (key == "state.starting") return english ? "Starting" : "Startet";
    if (key == "state.online") return "Online";
    if (key == "state.indexing") return english ? "Indexing" : "Indexiert";
    if (key == "state.synced") return english ? "Synced" : "Synchron";
    if (key == "state.error") return english ? "Error" : "Fehler";
    if (key == "state.unknown") return english ? "Unknown" : "Unbekannt";

    if (key == "settings.storage") return english ? "Storage" : "Speicher";
    if (key == "settings.dataDirectory") return english ? "Data directory" : "Datenverzeichnis";
    if (key == "settings.rpcUser") return english ? "RPC user" : "RPC Benutzer";
    if (key == "settings.rpcPassword") return english ? "RPC password" : "RPC Passwort";
    if (key == "settings.rpcPort") return "RPC Port";
    if (key == "settings.network") return english ? "Network" : "Netzwerk";
    if (key == "settings.services") return english ? "Services" : "Dienste";
    if (key == "settings.publicPoolAddress") return english ? "Public Pool address" : "Public Pool Adresse";
    if (key == "settings.app") return "App";
    if (key == "settings.theme") return "Theme";
    if (key == "settings.language") return english ? "Language" : "Sprache";
    if (key == "settings.autostart") return "Autostart";
    if (key == "settings.autostartText") return english ? "Start services automatically when the app starts" : "Dienste beim App-Start automatisch starten";
    if (key == "settings.savedTitle") return english ? "Settings saved" : "Einstellungen gespeichert";
    if (key == "settings.savedText") return english ? "Please restart Bitcoin-Qt so all changes can take effect." : "Bitte starte Bitcoin-Qt neu, damit alle Änderungen übernommen werden.";

    if (key == "logs.bitcoind") return english ? "Bitcoind Log" : "Bitcoind Log";
    if (key == "logs.electrs") return english ? "Electrs Log" : "Electrs Log";
    if (key == "web.mempoolWaiting") return english ? "Mempool will load once backend and frontend are ready." : "Mempool wird geladen, sobald Backend und Frontend bereit sind.";
    if (key == "web.publicPoolWaiting") return english ? "Public Pool will load once Stratum/API and UI are ready." : "Public Pool wird geladen, sobald Stratum/API und UI bereit sind.";

    if (key == "firstRun.windowTitle") return english ? "Set up storage location" : "Speicherort einrichten";
    if (key == "firstRun.title") return english ? "Bitcoin full node storage location" : "Bitcoin Full Node Speicherort";
    if (key == "firstRun.description") return english
        ? "Choose an internal or external drive. Bitcoin Core, electrs, Mempool, and Public Pool will store their data in this folder."
        : "Wähle eine interne oder externe Festplatte. Bitcoin Core, electrs, Mempool und Public Pool speichern ihre Daten vollständig in diesem Ordner.";
    if (key == "firstRun.placeholder") return english ? "/Volumes/BitcoinNode or /media/bitcoin-node" : "/Volumes/BitcoinNode oder /media/bitcoin-node";
    if (key == "firstRun.chooseDirectory") return english ? "Choose drive or folder" : "Datenträger oder Ordner auswählen";
    if (key == "firstRun.missingTitle") return english ? "Storage location missing" : "Speicherort fehlt";
    if (key == "firstRun.missingText") return english ? "Please choose a drive or folder first." : "Bitte wähle zuerst eine Festplatte oder einen Ordner aus.";
    if (key == "firstRun.unusableTitle") return english ? "Storage location unavailable" : "Speicherort nicht nutzbar";
    if (key == "firstRun.unusableText") return english ? "The folder could not be created." : "Der Ordner konnte nicht erstellt werden.";

    return key;
}

inline QString appServiceDetail(const QString& language, const QString& detail)
{
    const bool english = language.toLower().startsWith("en");
    if (!english) {
        return detail;
    }

    if (detail == "Gestoppt") return "Stopped";
    if (detail == "Wird beendet") return "Stopping";
    if (detail == "Startet") return "Starting";
    if (detail == "Läuft") return "Running";
    if (detail == "RPC nicht verfügbar") return "RPC unavailable";
    if (detail == "RPC verfügbar") return "RPC available";
    if (detail == "Warte auf Mempool DB") return "Waiting for Mempool DB";
    if (detail == "Warte auf Electrs") return "Waiting for Electrs";
    if (detail == "Warte auf Mempool Backend") return "Waiting for Mempool backend";
    if (detail == "Warte auf Mempool Frontend") return "Waiting for Mempool frontend";
    if (detail == "Warte auf Datenbank") return "Waiting for database";
    if (detail == "Warte auf Public Pool API") return "Waiting for Public Pool API";
    if (detail == "Warte auf Public Pool UI") return "Waiting for Public Pool UI";
    if (detail == "Stratum/API startet") return "Starting Stratum/API";
    if (detail == "Web UI startet") return "Starting web UI";
    if (detail == "Stratum und UI erreichbar") return "Stratum and UI reachable";
    if (detail == "Public Pool Prozessfehler") return "Public Pool process error";
    if (detail == "Warte auf Bitcoin Core RPC") return "Waiting for Bitcoin Core RPC";
    if (detail == "Indexiert Blockchain") return "Indexing blockchain";
    if (detail == "Bereit") return "Ready";
    if (detail == "Electrum bereit") return "Electrum ready";
    if (detail == "Frontend erreichbar") return "Frontend reachable";
    if (detail == "Datenbank erreichbar") return "Database reachable";
    if (detail == "Backend startet") return "Backend starting";
    if (detail == "Frontend startet") return "Frontend starting";
    if (detail == "Abgestürzt, Neustart wird vorbereitet") return "Crashed, preparing restart";

    QString translated = detail;
    translated.replace("MariaDB Fehler:", "MariaDB error:");
    translated.replace("Public Pool Runtime fehlt:", "Public Pool runtime missing:");
    translated.replace("Public Pool UI fehlt:", "Public Pool UI missing:");
    translated.replace("nicht gefunden oder nicht ausführbar:", "not found or not executable:");
    translated.replace("beendet mit Code", "exited with code");
    translated.replace("wurde beendet", "was stopped");
    return translated;
}
