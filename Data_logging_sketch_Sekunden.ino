// Biblothek einbinden.
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>

// Erstellen des Objektes "rtc", das den PCF8523-Chip repräsentiert.
RTC_PCF8523 rtc;

// Sensor-Objekt erstellen
SensirionI2cScd4x scd4x;

// Array mt den Wochentagen
char daysOfTheWeek[7][12] = { "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag" };

int lastSecond = -1;  // Speichert die Sekunde des letzten ausgelösten Events
// Intervall in Sekunden. Da der SCD-41 nur alle 5 Sekunden Daten bereitstellt, muss das Interval > 5 Sekunden sein.
const int interval = 10;
unsigned long sumOfIntervals = 0;  // Speichert die Summe der vergangenen Intervalle

// Für das Data Logging Shield nutzen wir Digital-Pin 10 für Chip Select.
const int chipSelect = 10;
// Die Logdatei
File logfile;

void setup() {
  /*
  Startet die serielle Kommunikation mit einer Geschwindigkeit von 57600 Baud. Muss mit Bautrate am
  seriellen Monitor übereinstimmen. 
  */
  Serial.begin(9600);
  // Geht in eine Endlosschleife, wenn keine Verbindung zum RTC aufgebaut werden kann
  if (!rtc.begin()) {
    Serial.println("RTC nicht gefunden!");
    Serial.flush();
    while (1) delay(10);
  }
  Serial.println("RTC gestartet...");
  /*
  Die folgende Zeile setzt den RTC nach der Uhrzeit am Computer, von dem der
  Sketch hochgeladen wird. Danach die folgende Zeile auskommentieren und den
  Sketch noch einmal hochladen. Damit ist der RTC gesetzt, bis nach etwa 5 
  Jahren die Batterie gewechselt werden muss.
  */
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // Startet den RTC
  rtc.start();
  // Kalibrierung des RTCs. Drift = 43 bedeutet zum Beispiel, dass der RTC 43 Sekunden pro Woche vorgeht.
  // Geht er nach, wird ein negativer Wert verwendet.
  float drift = 43;
  float period_sec = (7 * 86400);
  float deviation_ppm = (drift / period_sec * 1000000);
  float drift_unit = 4.34;
  int offset = round(deviation_ppm / drift_unit);

  // SD-Karte initialisieren
  Serial.println("Initialisiere SD-Karte...");
  if (!SD.begin(chipSelect)) {
    Serial.println("Karte fehlgeschlagen oder nicht vorhanden.");
    while (1)
      ;
  }
  Serial.println("Karte initialisiert.");

  // I2C-Bus initialisieren
  Wire.begin();
  // Sensor mit dem I2C-Bus und der Standard-Adresse (0x62) starten
  scd4x.begin(Wire, 0x62);
  // Eine Zeile zur Funktionsprüfung (gibt true zurück, wenn kein Fehler auftritt)
  bool sensorFunktioniert = (scd4x.stopPeriodicMeasurement() == 0);
  if (sensorFunktioniert) {
    Serial.println("Gassensor SCD41 funktioniert einwandfrei!");
  } else {
    Serial.println("KRITISCHER FEHLER: Gassensor SCD41 antwortet nicht. Programm gestoppt!");
    while (1)
      ;  // Hält den Arduino hier für immer an (Endlosschleife)
  }
  scd4x.stopPeriodicMeasurement();
  // Die kontinuierliche Messung starten (Sensor misst nun alle 5 Sekunden selbstständig)
  scd4x.startPeriodicMeasurement();

  // CSV-Datei öffnen. Wenn noch nicht vorhanden, wird Datei automatisch erstellt.
  logfile = SD.open("data.csv", FILE_WRITE);
  if (!logfile) {
    Serial.println("Fehler beim Öffnen von data.csv");
    while (1)
      ;
  }
  // Falls die Datei nagelneu und leer ist, Spaltenüberschriften schreiben
  if (logfile.size() == 0) {
    logfile.println("Zeit [s]; [CO2 ppm]");
    logfile.flush();
    Serial.println("Kopfzeile in data.csv geschrieben.");
  }
}

void loop() {
  // Tag, Datum und Uhrzeit im seriellen Monitor anzeigen.
  DateTime now = rtc.now();
  // Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  // Serial.print(", der ");
  // Serial.print(now.day(), DEC);
  // Serial.print('.');
  // Serial.print(now.month(), DEC);
  // Serial.print('.');
  // Serial.println(now.year(), DEC);
  // if(now.hour() < 10) Serial.print('0');
  // Serial.print(now.hour(), DEC);
  //  Serial.print(':');
  // if(now.minute() < 10) Serial.print('0');
  // Serial.print(now.minute(), DEC);
  // Serial.print(':');
  // if(now.second() < 10) Serial.print('0');
  // Serial.println(now.second(), DEC);
  // Serial.println();
  // delay(3000);
  int currentSecond = now.second();
  // 1. Prüfen, ob eine neue Sekunde angefangen hat.
  // 2. Prüfen, ob die Sekunde ohne Rest durch das Intervall teilbar ist.
  if (currentSecond != lastSecond && currentSecond % interval == 0) {
    lastSecond = currentSecond;  // Verhindert mehrfaches Auslösen in derselben Sekunde
    // Das Intervall zur Gesamtsumme dazurechnen.
    sumOfIntervals += interval;
    // Variablen für die Sensordaten
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    // Daten vom SCD41 auslesen
    uint16_t error = scd4x.readMeasurement(co2, temperature, humidity);
    if (!error && co2 > 0) {
      // Daten im CSV-Format auf die SD-Karte schreiben
      if (logfile) {
        logfile.print(sumOfIntervals);
        logfile.print(";");
        logfile.println(co2);
        // Daten sofort physisch auf der Karte sichern
        logfile.flush();
      }
      // Gleichzeitige Kontroll-Ausgabe auf dem seriellen Monitor
      Serial.print("Zeit [s]: ");
      Serial.print(sumOfIntervals);
      Serial.print("; ");
      Serial.print("CO2 [ppm]: ");
      Serial.println(co2);
    } else {
      Serial.println("Fehler beim Lesen der Sensordaten oder Sensor noch in Aufwärmphase.");
    }
  }
}
