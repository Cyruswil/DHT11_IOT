#include "FS.h"   // Bibliothèque pour communiquer avec la carte SD
#include "SD.h"   // Bibliothèque pour gérer la carte SD
#include "SPI.h"  // Bibliothèque pour utiliser le protocole SPI

#include <DS1307.h>  // Bibliothèque pour utiliser le RTC DS1307
#include <RTClib.h>  // Bibliothèque pour faciliter l'utilisation des modules RTC

#include <YoupiLabESP32_IOT.h>  // Bibliothèque pour la connexion avec l'IOT de Youpilab

#include <DHT.h>    // Bibliothèque pour utiliser le capteur DHT11
#include <DHT_U.h>  // Bibliothèque complémentaire pour le DHT11

#include <LiquidCrystal_I2C.h>  // Bibliothèque pour utiliser l'écran LCD avec I2C

// Déclaration des informations de connexion WiFi
const char *ssid = "ALBAH";
const char *password = "26951525";

// Définition de la broche utilisée pour le capteur DHT11 et son type
#define DHTpin 0
#define DHTTYPE DHT11
DHT dht(DHTpin, DHTTYPE);  // Initialisation du capteur DHT

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Adresse I2C de l'écran LCD (0x27 ou 0x3F selon votre module)

// Identification et clé pour l'application Youpilab
String APP_ID = "dhte2507";
String APP_KEY = "6ab16498";
YoupiLabESP32_IOT esp32(APP_ID, APP_KEY);  // Initialisation de l'objet YoupiLabESP32_IOT

RTC_DS1307 rtc;  // Initialisation de l'objet RTC

// Tableau contenant les noms des jours de la semaine
char daysOfWeek[7][12] = {
  "Dimanche",
  "Lundi",
  "Mardi",
  "Mercredi",
  "Jeudi",
  "Vendredi",
  "Samedi"
};

// Déclaration des broches utilisées pour la communication SPI
int sck = 18;
int miso = 19;
int mosi = 23;
int cs = 5;

void setup() {
  Serial.begin(115200);                                    // Initialisation de la communication série à 115200 bauds
  if (esp32.veriyAndConnectToWifi(ssid, password) == 1) {  // Connexion au WiFi
    Serial.println("Connexion réussie");
  }

  dht.begin();  // Initialisation du capteur DHT

  // Initialisation de l'écran LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Temperature et ");
  lcd.setCursor(0, 1);
  lcd.print("Humidite ");
  lcd.clear();
  if (!rtc.begin()) {  // Initialisation du RTC
    Serial.println("RTC module is NOT found");
    Serial.flush();
    while (1)
      ;  // Boucle infinie si le RTC n'est pas trouvé
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Ajustement de la date en fonction de la date du PC

  while (!Serial) {
    delay(10);  // Attente de l'initialisation de la communication série
  }

#ifdef REASSIGN_PINS
  SPI.begin(sck, miso, mosi, cs);  // Initialisation du SPI avec les broches définies
  if (!SD.begin(cs)) {
#else
  if (!SD.begin()) {
#endif
    Serial.println("Card Mount Failed");
    return;
  }

  uint8_t cardType = SD.cardType();  // Obtention du type de carte SD

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);  // Obtention de la taille de la carte SD en MB
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  if (!SD.exists("/ESP32.csv")) {                                                  // Vérification de l'existence du fichier ESP32.csv
    writeFile(SD, "/ESP32.csv", "Date, Heure, Température (°C), Humidité (%)\n");  // Création du fichier si non existant
  }
}

void loop() {
  DateTime now = rtc.now();  // Obtention de la date et de l'heure actuelles
  Serial.print("Date Time: ");
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);

  float hum = dht.readHumidity();      // Lecture de l'humidité
  float temp = dht.readTemperature();  // Lecture de la température

  if (isnan(temp) || isnan(hum)) {  // Vérification des erreurs de lecture
    Serial.println("Erreur de lecture!");
  } else {
    Serial.print("Temperature= ");
    Serial.print(temp);
    Serial.println("°C");
    Serial.print("Humidity= ");
    Serial.print(hum);
    Serial.println("%");

    // Affichage des valeurs sur l'écran LCD
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temp);
    lcd.print(" C");

    lcd.setCursor(0, 1);
    lcd.print("Hum: ");
    lcd.print(hum);
    lcd.print(" %");
  }

  delay(1000);  // Attente de 1 seconde

  // Préparation des données à écrire
  String dataString = getTimeString(now) + "," + String(temp) + "," + String(hum) + "\n";
  Serial.print("Écriture des données : ");
  Serial.print(dataString);

  appendFile(SD, "/ESP32.csv", dataString.c_str());  // Ajout des données dans le fichier ESP32.csv

  if (esp32.send_Data(2, 2, temp, 2, hum) == 1) {  // Envoi des données à l'IOT de Youpilab
    Serial.println("Envoie réussi");
  }

  Serial.print("countData=");
  String n_data = esp32.countData();  // Comptage des données
  Serial.println(n_data);
  delay(500);  // Attente de 0,5 seconde
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);  // Ouverture du fichier en mode ajout
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();  // Fermeture du fichier
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Écriture dans le fichier: %s\n", path);

  File file = fs.open(path, FILE_WRITE);  // Ouverture du fichier en mode écriture
  if (!file) {
    Serial.println("Échec d'ouverture du fichier pour écriture");
    return;
  }
  if (file.print(message)) {
    Serial.println("Écriture réussie");
  } else {
    Serial.println("Échec de l'écriture");
  }
  file.close();  // Fermeture du fichier
}

String getTimeString(const DateTime &now) {
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second());  // Formatage de la date et de l'heure en chaîne de caractères
  return String(buffer);
}
