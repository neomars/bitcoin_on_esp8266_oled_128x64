// Déclaration des bibliothèques utilisées
#include <Wire.h> // Librairie pour la communication I2C
#include <Adafruit_GFX.h> // Librairie graphique de base pour Adafruit
#include <Adafruit_SSD1306.h> // Librairie spécifique pour les écrans SSD1306
#include <pgmspace.h> // Pour l'utilisation de PROGMEM pour stocker le bitmap en mémoire flash

// Inclusion d'une police personnalisée pour supporter le symbole Bitcoin
#include <Fonts/FreeSans12pt7b.h> // Nécessite la bibliothèque Adafruit GFX Library installée avec les exemples de polices

#include <ESP8266WiFi.h> // Pour la gestion du WiFi sur l'ESP8266
#include <WiFiUdp.h>     // Nécessaire pour NTPClient
#include <NTPClient.h>   // Pour synchroniser l'heure via le protocole NTP
#include <ArduinoJson.h> // Pour parser les réponses JSON des APIs
#include <ESP8266HTTPClient.h>  // Pour effectuer des requêtes HTTP
#include <WiFiClientSecure.h> // POUR LES REQUÊTES HTTPS
#include <WiFiManager.h>      // Pour la gestion de la configuration WiFi

// --- Paramètres des APIs ---
// Pour CoinGecko (API Bitcoin):
// L'API est généralement gratuite pour un usage basique.
// URL pour les prix du Bitcoin, Ethereum, et Binance Coin en USD
const String COINGECKO_URL = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin%2Cethereum%2Cbinancecoin&vs_currencies=usd&include_24hr_change=true";


// --- Définitions pour l'écran OLED ---
#define SCREEN_WIDTH 128 // Largeur de l'écran OLED en pixels
#define SCREEN_HEIGHT 64 // Hauteur de l'écran OLED en pixels
#define OLED_RESET    -1 // Broche de Reset (ou -1 si partagée avec le reset de l'ESP)

// Création de l'objet display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Objets pour les services (déclarés globalement) ---
WiFiUDP ntpUDP; // Objet UDP nécessaire pour NTPClient
// NTPClient est initialisé avec un décalage UTC de 7200 secondes (pour l'heure de Paris).
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000); // Serveur NTP, décalage, intervalle de mise à jour
WiFiManager wifiManager; // Déclaration de l'objet WiFiManager

// --- Variables globales pour stocker les données ---
String timeStamp;      // Heure au format HH:mm:ss
float btcPrice = 0.0;
float btc24hChange = 0.0;
float ethPrice = 0.0;
float eth24hChange = 0.0;
float bnbPrice = 0.0;
float bnb24hChange = 0.0;

// Hauteur d'une ligne de texte en setTextSize(1) - Déclarée globalement
const int TEXT_HEIGHT_SIZE1 = 8; 

// --- Variables pour la gestion des pages et des mises à jour ---
unsigned long previousMillisPage = 0;
const long intervalPage = 5000; // Changer de page toutes les 5 secondes (au lieu de 2.5s)
int currentPage = 0; // Index de la page actuelle dans le tableau cryptoPages

unsigned long previousMillisDataUpdate = 0;
const long intervalDataUpdate = 300000; // Mettre à jour les données toutes les 5 minutes (300000 ms)

// --- Paramètres du bouton pour forcer le portail de configuration ---
#define BUTTON_PIN D3 // Broche GPIO à laquelle le bouton est connecté (D3 correspond à GPIO0)
#define LONG_PRESS_TIME 3000 // Temps en millisecondes pour un appui long (3 secondes)

unsigned long buttonPressStartTime = 0; // Pour détecter la durée de l'appui
bool buttonPressed = false; // Pour suivre l'état du bouton

// --- Fonctions d'affichage des pages (déclarations anticipées) ---
void drawTimePage();
void drawBitcoinPage();
void drawEthereumPage();
void drawBinanceCoinPage();

// --- Fonctions de dessin de symboles WiFi (MISES À JOUR) ---

// Dessine un symbole WiFi avec un certain niveau de signal
// x, y : position du coin supérieur gauche de la zone de l'icône (par ex. une boîte de 16x16 pixels)
// level: 0 (pas de signal), 1 (faible), 2 (moyen), 3 (fort)
void drawWifiSignal(int x, int y, int level) {
  // Coordonnées du point central (la base du signal)
  // Environ au milieu horizontalement et vers le bas verticalement pour une icône de ~16x16
  int dot_x = x + 7; 
  int dot_y = y + 15; 

  // Dessine le point central
  display.drawPixel(dot_x, dot_y, WHITE);

  // Dessine les arcs (formes de V inversées) pour représenter les niveaux de signal
  // Arc 1 (le plus interne)
  if (level >= 1) {
    display.drawLine(dot_x - 3, dot_y - 3, dot_x, dot_y - 5, WHITE);
    display.drawLine(dot_x, dot_y - 5, dot_x + 3, dot_y - 3, WHITE);
  }
  // Arc 2 (moyen)
  if (level >= 2) {
    display.drawLine(dot_x - 6, dot_y - 6, dot_x, dot_y - 10, WHITE);
    display.drawLine(dot_x, dot_y - 10, dot_x + 6, dot_y - 6, WHITE);
  }
  // Arc 3 (le plus externe)
  if (level >= 3) {
    display.drawLine(dot_x - 9, dot_y - 9, dot_x, dot_y - 15, WHITE);
    display.drawLine(dot_x, dot_y - 15, dot_x + 9, dot_y - 9, WHITE);
  }
}

// Dessine un symbole de point d'accès (AP) simple
// x, y : position du coin supérieur gauche du symbole
void drawWifiAPIcon(int x, int y) {
  // Rectangle représentant un routeur/AP
  display.drawRect(x + 2, y + 8, 12, 8, WHITE);
  // Petite antenne
  display.drawLine(x + 8, y + 8, x + 8, y + 4, WHITE);
  display.drawLine(x + 6, y + 4, x + 10, y + 4, WHITE);
}


// --- Structure pour les pages d'affichage ---
// Contient le nom de la page et un pointeur vers sa fonction d'affichage
typedef struct {
  const char* name;
  void (*drawFunction)();
} CryptoPage;

// Tableau des pages disponibles
CryptoPage displayPages[] = {
  {"Time", drawTimePage},
  {"Bitcoin", drawBitcoinPage},
  {"Ethereum", drawEthereumPage},
  {"Binance Coin", drawBinanceCoinPage}
};

// Callback pour afficher le message du portail de configuration
void configModeCallback (WiFiManager *myWiFiManager) {
  display.clearDisplay(); // Ajout de cette ligne pour effacer l'écran
  Serial.println("Entrée dans le mode de configuration du WiFiManager.");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.println(WiFi.softAPIP());

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Connectez-vous au WiFi:");
  display.setTextSize(1.5);
  display.setCursor(0,18);
  display.println(myWiFiManager->getConfigPortalSSID());
  display.setTextSize(1);
  display.setCursor(0,40);
  display.println("Accedez a 192.168.4.1");
  
  // Dessine le symbole AP en bas à droite
  drawWifiAPIcon(SCREEN_WIDTH - 20, SCREEN_HEIGHT - 20); 

  display.display();
}

// --- Fonctions de récupération des données ---

void updateTime() {
  timeClient.update(); // Met à jour le temps directement avec le décalage local
  timeStamp = timeClient.getFormattedTime(); // HH:mm:ss
  
  Serial.print("NTP Time Epoch: "); // Ajout pour le débogage de l'heure
  Serial.println(timeClient.getEpochTime()); // Ajout pour le débogage de l'heure
  Serial.println("Formatted Time: " + timeStamp); // Ajout pour le débogage de l'heure
}

void updateCryptoData() { // Renommé pour être plus générique
  Serial.println("Mise à jour des données crypto...");
  if (WiFi.status() != WL_CONNECTED) { // Vérifie la connexion WiFi avant de tenter une requête
    Serial.println("WiFi non connecte, impossible de récupérer les données crypto.");
    btcPrice = 0.0; btc24hChange = 0.0;
    ethPrice = 0.0; eth24hChange = 0.0;
    bnbPrice = 0.0; bnb24hChange = 0.0;
    return; // Sort de la fonction si pas connecté
  }

  WiFiClientSecure client; // Utilise WiFiClientSecure pour HTTPS
  
  // NECESSAIRE POUR HTTPS: DÉSACTIVER LA VALIDATION DES CERTIFICATS POUR LES TESTS.
  // ATTENTION: CE N'EST PAS SÉCURISÉ POUR UNE UTILISATION EN PRODUCTION.
  // Pour une solution sécurisée, utilisez client.setCACert(cert, cert_len)
  // ou client.setFingerprint("XX:XX:XX:...")
  client.setInsecure(); 

  HTTPClient http;

  // Utilise la nouvelle syntaxe begin(WiFiClient, url) avec le client sécurisé
  http.begin(client, COINGECKO_URL);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println(payload);

    // Allouer un buffer JSON dynamique
    DynamicJsonDocument doc(1024); // Augmenté pour contenir plus de données crypto
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      btcPrice = 0.0; btc24hChange = 0.0;
      ethPrice = 0.0; eth24hChange = 0.0;
      bnbPrice = 0.0; bnb24hChange = 0.0;
    } else {
      btcPrice = doc["bitcoin"]["usd"].as<float>();
      btc24hChange = doc["bitcoin"]["usd_24h_change"].as<float>();
      ethPrice = doc["ethereum"]["usd"].as<float>();
      eth24hChange = doc["ethereum"]["usd_24h_change"].as<float>();
      bnbPrice = doc["binancecoin"]["usd"].as<float>();
      bnb24hChange = doc["binancecoin"]["usd_24h_change"].as<float>();

      Serial.print("Prix BTC: "); Serial.println(btcPrice);
      Serial.print("Variation 24h: "); Serial.println(btc24hChange);
      Serial.print("Prix ETH: "); Serial.println(ethPrice);
      Serial.print("Variation 24h: "); Serial.println(eth24hChange);
      Serial.print("Prix BNB: "); Serial.println(bnbPrice);
      Serial.print("Variation 24h: "); Serial.println(bnb24hChange);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    btcPrice = 0.0; btc24hChange = 0.0;
    ethPrice = 0.0; eth24hChange = 0.0;
    bnbPrice = 0.0; bnb24hChange = 0.0;
  }
  http.end();
}


// --- Fonctions d'affichage des pages ---

void drawTimePage() { // Heure seule
  display.clearDisplay();
  display.setTextColor(WHITE);

  // Heure centrée horizontalement et verticalement
  display.setFont(NULL); // Utilise la police par défaut (8x8 pixels par caractère pour setTextSize(1))
  display.setTextSize(2); // Taille 2 pour une meilleure visibilité de l'heure
  int16_t x1, y1;
  uint16_t w, h;
  // Calcule les dimensions du texte pour le centrer
  display.getTextBounds(timeStamp, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2); 
  display.print(timeStamp);

  display.display();
}

void drawBitcoinPage() { // Bitcoin
  display.clearDisplay();
  display.setTextColor(WHITE);

  // --- LOGO BITCOIN DESSINÉ PAR PIXELS ---
  // Offsets ajustés pour que le coin supérieur gauche du logo soit à (0,0)
  // Bounding box originale du logo Bitcoin : (14,18) à (23,33)
  // Largeur originale: 23-14+1 = 10px, Hauteur originale: 33-18+1 = 16px (approx)
  // On décale les points pour que le point (14,18) se retrouve à (0,0)
  int offsetX_btc = -14; 
  int offsetY_btc = -18; 

  display.drawPixel(14 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 27 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 26 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 26 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 28 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 28 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 29 + offsetY_btc, WHITE);
  display.drawPixel(14 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(15 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(15 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 26 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 26 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 27 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 28 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 28 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 29 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(15 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(23 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 17 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 16 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 16 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 17 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 16 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 32 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 33 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 33 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 32 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 33 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 33 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(15 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(15 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 24 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 25 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 26 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 26 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 28 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 28 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 27 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 27 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 27 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 29 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 31 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(13 + offsetX_btc, 18 + offsetY_btc, WHITE);
  display.drawPixel(15 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 19 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 20 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 22 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 21 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 23 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 26 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 27 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 28 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 28 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 28 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 28 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 29 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 29 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 29 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 29 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(22 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(15 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(17 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(16 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(18 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(19 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(21 + offsetX_btc, 30 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 17 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 17 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 16 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 32 + offsetY_btc, WHITE);
  display.drawPixel(20 + offsetX_btc, 33 + offsetY_btc, WHITE);
  // --- FIN LOGO BITCOIN ---

  // Abréviation BTC/usd (à droite du logo, alignée verticalement)
  display.setFont(NULL);
  display.setTextSize(1);
  int logo_width_btc = 10; // Largeur approximative du logo BTC
  int logo_height_btc = 16; // Hauteur approximative du logo BTC
  display.setCursor(logo_width_btc + 2, (logo_height_btc - TEXT_HEIGHT_SIZE1) / 2); 
  display.println("BTC/usd"); // Changé pour afficher "/usd"

  // Prix du Bitcoin (taille 2)
  display.setTextSize(2); 
  char priceBuffer[15];
  snprintf(priceBuffer, sizeof(priceBuffer), "%.2f", btcPrice); // "$" supprimé
  int16_t x1, y1;
  uint16_t w_price, h_price;
  display.getTextBounds(priceBuffer, 0, 0, &x1, &y1, &w_price, &h_price);
  display.setCursor((SCREEN_WIDTH - w_price) / 2, 26); 
  display.println(priceBuffer);

  // Variation journalière (taille 1)
  display.setTextSize(1);
  char changeBuffer[20];
  snprintf(changeBuffer, sizeof(changeBuffer), "Var. 24h: %+.2f%%", btc24hChange); // %+.2f pour forcer le signe
  uint16_t w_change, h_change;
  display.getTextBounds(changeBuffer, 0, 0, &x1, &y1, &w_change, &h_change);
  display.setCursor((SCREEN_WIDTH - w_change) / 2, 52); 
  display.println(changeBuffer);

  display.display();
}

void drawEthereumPage() { // Ethereum
  display.clearDisplay();
  display.setTextColor(WHITE);

  // --- LOGO ETHEREUM DESSINÉ PAR PIXELS ---
  // Offsets ajustés pour que le coin supérieur gauche du logo soit à (0,0)
  // Bounding box originale du logo Ethereum: (19,32) à (25,44)
  // Largeur originale: 25-19+1 = 7px, Hauteur originale: 44-32+1 = 13px
  // On décale les points pour que le point (19,32) se retrouve à (0,0)
  int offsetX_eth = -19; 
  int offsetY_eth = -32; 

  display.drawPixel(22 + offsetX_eth, 44 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 43 + offsetY_eth, WHITE);
  display.drawPixel(20 + offsetX_eth, 42 + offsetY_eth, WHITE);
  display.drawPixel(23 + offsetX_eth, 43 + offsetY_eth, WHITE);
  display.drawPixel(24 + offsetX_eth, 42 + offsetY_eth, WHITE);
  display.drawPixel(19 + offsetX_eth, 41 + offsetY_eth, WHITE);
  display.drawPixel(25 + offsetX_eth, 41 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 43 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 42 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 42 + offsetY_eth, WHITE);
  display.drawPixel(23 + offsetX_eth, 42 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 40 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 40 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 40 + offsetY_eth, WHITE);
  display.drawPixel(20 + offsetX_eth, 39 + offsetY_eth, WHITE);
  display.drawPixel(19 + offsetX_eth, 39 + offsetY_eth, WHITE);
  display.drawPixel(23 + offsetX_eth, 40 + offsetY_eth, WHITE);
  display.drawPixel(24 + offsetX_eth, 39 + offsetY_eth, WHITE);
  display.drawPixel(25 + offsetX_eth, 39 + offsetY_eth, WHITE);
  display.drawPixel(25 + offsetX_eth, 38 + offsetY_eth, WHITE);
  display.drawPixel(24 + offsetX_eth, 37 + offsetY_eth, WHITE);
  display.drawPixel(24 + offsetX_eth, 38 + offsetY_eth, WHITE);
  display.drawPixel(24 + offsetX_eth, 36 + offsetY_eth, WHITE);
  display.drawPixel(23 + offsetX_eth, 36 + offsetY_eth, WHITE);
  display.drawPixel(23 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(23 + offsetX_eth, 34 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 33 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 32 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 34 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 34 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 34 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 34 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 34 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 34 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 35 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 36 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 36 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 36 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 36 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 36 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 36 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 37 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 37 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 37 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 36 + offsetY_eth, WHITE);
  display.drawPixel(23 + offsetX_eth, 37 + offsetY_eth, WHITE);
  display.drawPixel(23 + offsetX_eth, 39 + offsetY_eth, WHITE);
  display.drawPixel(23 + offsetX_eth, 38 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 38 + offsetY_eth, WHITE);
  display.drawPixel(22 + offsetX_eth, 39 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 39 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 38 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 38 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 37 + offsetY_eth, WHITE);
  display.drawPixel(21 + offsetX_eth, 34 + offsetY_eth, WHITE);
  display.drawPixel(20 + offsetX_eth, 38 + offsetY_eth, WHITE);
  display.drawPixel(20 + offsetX_eth, 37 + offsetY_eth, WHITE);
  display.drawPixel(20 + offsetX_eth, 36 + offsetY_eth, WHITE);
  display.drawPixel(19 + offsetX_eth, 38 + offsetY_eth, WHITE);
  // --- FIN LOGO ETHEREUM ---

  // Abréviation ETH/usd (à droite du logo, alignée verticalement)
  display.setFont(NULL);
  display.setTextSize(1);
  int logo_width_eth = 7; // Largeur approximative du logo ETH
  int logo_height_eth = 13; // Hauteur approximative du logo ETH
  display.setCursor(logo_width_eth + 2, (logo_height_eth - TEXT_HEIGHT_SIZE1) / 2);
  display.println("ETH/usd"); // Changé pour afficher "/usd"

  // Prix de l'Ethereum (taille 2)
  display.setTextSize(2); 
  char priceBuffer[15];
  snprintf(priceBuffer, sizeof(priceBuffer), "%.2f", ethPrice); // "$" supprimé
  int16_t x1, y1;
  uint16_t w_price, h_price;
  display.getTextBounds(priceBuffer, 0, 0, &x1, &y1, &w_price, &h_price);
  display.setCursor((SCREEN_WIDTH - w_price) / 2, 26); 
  display.println(priceBuffer);

  // Variation journalière (taille 1)
  display.setTextSize(1);
  char changeBuffer[20];
  snprintf(changeBuffer, sizeof(changeBuffer), "Var. 24h: %+.2f%%", eth24hChange); // %+.2f pour forcer le signe
  uint16_t w_change, h_change;
  display.getTextBounds(changeBuffer, 0, 0, &x1, &y1, &w_change, &h_change);
  display.setCursor((SCREEN_WIDTH - w_change) / 2, 52); 
  display.println(changeBuffer);

  display.display();
}

void drawBinanceCoinPage() { // Binance Coin
  display.clearDisplay();
  display.setTextColor(WHITE);

  // --- LOGO BINANCE COIN DESSINÉ PAR PIXELS ---
  // Offsets ajustés pour que le coin supérieur gauche du logo soit à (0,0)
  // Bounding box originale du logo Binance Coin: (8,17) à (30,40)
  // Largeur originale: 30-8+1 = 23px, Hauteur originale: 40-17+1 = 24px
  // On décale les points pour que le point (8,17) se retrouve à (0,0)
  int offsetX_bnb = -8; 
  int offsetY_bnb = -17;

  display.drawPixel(19 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 30 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 31 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 30 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 27 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 30 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 26 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 21 + offsetY_bnb, WHITE);
  display.drawPixel(26 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(27 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(27 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(26 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 21 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 21 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 21 + offsetY_bnb, WHITE);
  display.drawPixel(27 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(28 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(29 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 20 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 20 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 20 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 20 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 20 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 20 + offsetY_bnb, WHITE);
  display.drawPixel(30 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(26 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(27 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(28 + offsetX_bnb, 27 + offsetY_bnb, WHITE);
  display.drawPixel(28 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(28 + offsetX_bnb, 30 + offsetY_bnb, WHITE);
  display.drawPixel(28 + offsetX_bnb, 31 + offsetY_bnb, WHITE);
  display.drawPixel(28 + offsetX_bnb, 31 + offsetY_bnb, WHITE);
  display.drawPixel(27 + offsetX_bnb, 30 + offsetY_bnb, WHITE);
  display.drawPixel(29 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(29 + offsetX_bnb, 30 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 27 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 26 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 21 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 26 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 19 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 19 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 19 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 19 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 18 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 18 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 17 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 17 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 19 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 18 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 19 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 21 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 20 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 21 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 19 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 20 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 20 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 21 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 21 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 21 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 26 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 26 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(12 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(12 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(12 + offsetX_bnb, 26 + offsetY_bnb, WHITE);
  display.drawPixel(12 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(12 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(12 + offsetX_bnb, 27 + offsetY_bnb, WHITE);
  display.drawPixel(11 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(11 + offsetX_bnb, 27 + offsetY_bnb, WHITE);
  display.drawPixel(11 + offsetX_bnb, 27 + offsetY_bnb, WHITE);
  display.drawPixel(11 + offsetX_bnb, 26 + offsetY_bnb, WHITE);
  display.drawPixel(11 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(12 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(12 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(11 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(11 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(10 + offsetX_bnb, 27 + offsetY_bnb, WHITE);
  display.drawPixel(10 + offsetX_bnb, 27 + offsetY_bnb, WHITE);
  display.drawPixel(10 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(10 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(11 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(11 + offsetX_bnb, 30 + offsetY_bnb, WHITE);
  display.drawPixel(11 + offsetX_bnb, 30 + offsetY_bnb, WHITE);
  display.drawPixel(10 + offsetX_bnb, 30 + offsetY_bnb, WHITE);
  display.drawPixel(10 + offsetX_bnb, 31 + offsetY_bnb, WHITE);
  display.drawPixel(9 + offsetX_bnb, 30 + offsetY_bnb, WHITE);
  display.drawPixel(9 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(9 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(9 + offsetX_bnb, 28 + offsetY_bnb, WHITE);
  display.drawPixel(8 + offsetX_bnb, 29 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 22 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 26 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 27 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 23 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 24 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 26 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 25 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 26 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 27 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(12 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 31 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(12 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(13 + offsetX_bnb, 35 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(14 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(15 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 39 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 39 + offsetY_bnb, WHITE);
  display.drawPixel(18 + offsetX_bnb, 39 + offsetY_bnb, WHITE);
  display.drawPixel(16 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(17 + offsetX_bnb, 39 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 40 + offsetY_bnb, WHITE);
  display.drawPixel(19 + offsetX_bnb, 40 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 38 + offsetY_bnb, WHITE);
  display.drawPixel(20 + offsetX_bnb, 39 + offsetY_bnb, WHITE);
  display.drawPixel(22 + offsetX_bnb, 37 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 36 + offsetY_bnb, WHITE);
  display.drawPixel(25 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  display.drawPixel(26 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(26 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(26 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(26 + offsetX_bnb, 33 + offsetY_bnb, WHITE);
  display.drawPixel(24 + offsetX_bnb, 31 + offsetY_bnb, WHITE);
  display.drawPixel(23 + offsetX_bnb, 32 + offsetY_bnb, WHITE);
  display.drawPixel(21 + offsetX_bnb, 34 + offsetY_bnb, WHITE);
  // --- FIN LOGO BINANCE COIN ---

  // Abréviation BNB/usd (à droite du logo, alignée verticalement)
  display.setFont(NULL);
  display.setTextSize(1);
  int logo_width_bnb = 23; // Largeur approximative du logo BNB
  int logo_height_bnb = 24; // Hauteur approximative du logo BNB
  display.setCursor(logo_width_bnb + 2, (logo_height_bnb - TEXT_HEIGHT_SIZE1) / 2);
  display.println("BNB/usd"); // Changé pour afficher "/usd"

  // Prix du Binance Coin (taille 2)
  display.setTextSize(2); 
  char priceBuffer[15];
  snprintf(priceBuffer, sizeof(priceBuffer), "%.2f", bnbPrice); // "$" supprimé
  int16_t x1, y1;
  uint16_t w_price, h_price;
  display.getTextBounds(priceBuffer, 0, 0, &x1, &y1, &w_price, &h_price);
  display.setCursor((SCREEN_WIDTH - w_price) / 2, 26); 
  display.println(priceBuffer);

  // Variation journalière (taille 1)
  display.setTextSize(1);
  char changeBuffer[20];
  snprintf(changeBuffer, sizeof(changeBuffer), "Var. 24h: %+.2f%%", bnb24hChange); // %+.2f pour forcer le signe
  uint16_t w_change, h_change;
  display.getTextBounds(changeBuffer, 0, 0, &x1, &y1, &w_change, &h_change);
  display.setCursor((SCREEN_WIDTH - w_change) / 2, 52); 
  display.println(changeBuffer);

  display.display();
}


// --- Setup ---
void setup() {
  Serial.begin(115200); // Vitesse de communication série plus rapide pour ESP8266
  Serial.println(F("Démarrage du système..."));

  // Initialise la broche du bouton en INPUT_PULLUP.
  // Cela signifie que la broche est normalement HIGH (5V) et passe à LOW quand le bouton est appuyé (relié à la masse).
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.print(F("Bouton de configuration sur GPIO"));
  Serial.print(digitalPinToInterrupt(BUTTON_PIN)); // Affiche le numéro GPIO réel
  Serial.println(F(" initialisé."));


  // --- Initialisation de l'écran OLED avec les broches trouvées (SDA=GPIO4, SCL=GPIO5) ---
  Wire.begin(4, 5); // Initialise le bus I2C avec SDA sur GPIO4 et SCL sur GPIO5
  Wire.setClock(400000); // Règle la vitesse I2C à 400 kHz (standard et plus rapide)

  // Tente d'initialiser l'écran OLED avec 0x3C, puis 0x3D si 0x3C échoue
  Serial.print(F("Initialisation OLED... tentative adresse 0x3C: "));
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("ÉCHEC."));
    Serial.print(F("Tentative adresse 0x3D: "));
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(F("ÉCHEC. Aucune adresse I2C OLED fonctionnelle trouvée."));
      Serial.println(F("Vérifiez le câblage et l'alimentation de l'écran."));
      // Message d'erreur sur l'écran si l'initialisation OLED échoue
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0,0);
      display.println("OLED ERROR!");
      display.setCursor(0,10);
      display.println("Check Wiring/Address");
      display.display();
      for (;;); // Boucle infinie si l'écran ne répond pas
    } else {
      Serial.println(F("SUCCÈS sur 0x3D."));
      // Message de confirmation sur l'OLED
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(WHITE);
      display.setCursor(0,0);
      display.println("OLED OK!");
      display.setCursor(0,20);
      display.println("Addr: 0x3D");
      display.display();
      delay(2000); // Afficher pendant 2 secondes
      display.clearDisplay(); // Nettoyer pour la suite
    }
  } else {
    Serial.println(F("SUCCÈS sur 0x3C."));
    // Message de confirmation sur l'OLED
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("OLED OK!");
    display.setCursor(0,20);
    display.println("Addr: 0x3C");
    display.display();
    delay(2000); // Afficher pendant 2 secondes
    display.clearDisplay(); // Nettoyer pour la suite
    }
  Serial.println(F("OLED initialisé."));


  // --- Message de bienvenue "Bonjour Mars ;)" pendant 5 secondes ---
  display.clearDisplay();
  display.setTextSize(2); // Taille du texte
  display.setTextColor(WHITE); // Couleur du texte
  display.setCursor(10, 15); // Positionnement approximatif pour centrer
  display.println("Bonjour");
  display.setCursor(30, 40);
  display.println("Mars ;)");
  display.display(); // Afficher le message
  delay(2000); // Attendre 2 secondes

  // --- Connexion WiFi via WiFiManager ---
  display.clearDisplay(); // Effacer le message de bienvenue
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println("Connexion WiFi...");
  display.display(); // Affiche le message de connexion

  // Définit le callback pour le mode de configuration
  wifiManager.setAPCallback(configModeCallback);

  // Tentative de connexion WiFi automatique ou lancement du portail de configuration
  // Le SSID du point d'accès de configuration sera "AutoConnectAP" et le mot de passe "password"
  // Vous pouvez les changer selon vos besoins
  // La méthode autoConnect() démarre le portail AP si la connexion échoue
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("Échec de la connexion et temporisation du portail de configuration.");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("WiFi ECHEC !");
    display.setCursor(0,10);
    display.println("Redemarrage...");
    display.display();
    delay(3000);
    ESP.restart(); // Redémarre l'ESP si le WiFi échoue
  }
  
  Serial.println("\nWiFi connecte !");
  
  // Afficher le message de succès WiFi sur OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("WiFi connecte:");
  display.setCursor(0,10);
  display.setTextSize(2);
  display.println(WiFi.SSID()); // Nom du réseau WiFi connecté
  display.setCursor(0,30);
  display.setTextSize(1);
  display.print("IP: ");
  display.println(WiFi.localIP());
  
  // Dessine le symbole WiFi plein signal en bas à droite
  drawWifiSignal(SCREEN_WIDTH - 20, SCREEN_HEIGHT - 20, 3); // Position et niveau de signal 3 (fort)

  display.display();
  delay(3000); // Afficher pendant 3 secondes
  Serial.print("Adresse IP: ");
  Serial.println(WiFi.localIP());

  // --- Initialisation NTPClient ---
  display.clearDisplay(); // Effacer le message WiFi
  display.setCursor(0,0);
  display.setTextSize(1);
  display.println("Synchronisation Heure...");
  display.display();

  timeClient.begin();
  // Le décalage horaire pour Paris (UTC+2 en heure d'été) est appliqué directement ici.
  timeClient.setTimeOffset(7200); 

  // Tente de mettre à jour l'heure plusieurs fois au démarrage
  int timeAttempts = 0;
  // Utilise timeClient.forceUpdate() pour les tentatives initiales car update() peut ne pas retenter
  while (!timeClient.forceUpdate() && timeAttempts < 10) { 
    Serial.print("T"); // Indique une tentative de synchro
    delay(500);
    timeAttempts++;
  }

  // Si l'heure a été reçue (epoch time non nul), afficher la première page directement.
  // Le message "Heure Reçue !" est supprimé ici.
  if (timeClient.getEpochTime() > 0) { 
    Serial.println("Heure synchronisée avec succès.");
  } else {
    // Message d'erreur Heure sur OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("Erreur Heure!");
    display.setCursor(0,10);
    display.println("Heure non synchro.");
    display.display();
    delay(2000);
  }
  
  // Première mise à jour des données au démarrage
  updateTime(); // Mise à jour après la synchro NTP
  updateCryptoData(); // Appelle la fonction générique de mise à jour des données crypto

  // Afficher la première page immédiatement après le message de bienvenue et la récupération des données
  displayPages[currentPage].drawFunction(); // Appelle la fonction d'affichage de la page 0 (heure)
  previousMillisPage = millis(); // Initialiser le timer de page
  previousMillisDataUpdate = millis(); // Initialiser le timer de mise à jour des données
}

// --- Loop ---
void loop() {
  unsigned long currentMillis = millis();

  // --- Vérification du bouton pour forcer le mode de configuration ---
  // Un bouton poussoir est généralement câblé entre la broche GPIO et la masse (GND).
  // Avec INPUT_PULLUP, la broche est normalement HIGH (5V) et passe à LOW quand le bouton est appuyé.
  if (digitalRead(BUTTON_PIN) == LOW) { // Le bouton est appuyé
    if (!buttonPressed) { // Si c'est le début de l'appui
      buttonPressStartTime = currentMillis;
      buttonPressed = true;
      Serial.println("Bouton appuye. Maintenez pour reconfigurer le WiFi...");
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0,0);
      display.println("Bouton appuye.");
      display.setCursor(0,10);
      display.println("Maintenez " + String(LONG_PRESS_TIME/1000) + "s");
      display.println("pour config. WiFi");
      display.display();
    } else if (currentMillis - buttonPressStartTime >= LONG_PRESS_TIME) {
      // Appui long détecté !
      Serial.println("Appui long detecte. Forcage du portail de configuration WiFiManager.");
      
      // Nettoie l'écran et démarre le portail de configuration.
      // Le configModeCallback s'occupera d'afficher les messages du portail.
      display.clearDisplay(); // IMPORTANT: efface l'écran avant que le portail ne s'active
      display.display(); // S'assure que l'écran est bien vide avant de passer la main
      
      wifiManager.startConfigPortal("AutoConnectAP", "password");
      
      // Après la configuration (ou l'échec), redémarrage pour revenir au fonctionnement normal
      Serial.println("Configuration terminee. Redemarrage...");
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0,0);
      display.println("Configuration faite.");
      display.setCursor(0,10);
      display.println("Redemarrage...");
      display.display();
      delay(2000);
      ESP.restart(); // Redémarre pour se connecter au nouveau réseau
    }
  } else { // Le bouton n'est pas appuyé
    if (buttonPressed) { // Si le bouton vient d'être relâché (après un appui court)
      buttonPressed = false;
      Serial.println("Bouton relache.");
      // Réaffiche la page actuelle après le message du bouton
      // Ceci est important car le message "Bouton appuye" a effacé la page précédente.
      display.clearDisplay(); // Effacer à nouveau au cas où il y aurait du texte résiduel
      displayPages[currentPage].drawFunction();
    }
  }


  // --- Gestion du changement de page (toutes les 5 secondes) ---
  if (currentMillis - previousMillisPage >= intervalPage) {
    previousMillisPage = currentMillis;

    // Changer de page
    currentPage++;
    if (currentPage >= sizeof(displayPages) / sizeof(displayPages[0])) { // Utilise la taille du tableau
      currentPage = 0;
    }

    // Appelle la fonction d'affichage de la page actuelle
    displayPages[currentPage].drawFunction();
  }

  // --- Gestion de la mise à jour des données (toutes les 5 minutes) ---
  if (currentMillis - previousMillisDataUpdate >= intervalDataUpdate) {
    previousMillisDataUpdate = currentMillis;
    Serial.println("Mise à jour des données...");
    updateTime();
    updateCryptoData(); // Appelle la fonction générique de mise à jour des données crypto
    // Après la mise à jour, réafficher la page actuelle pour montrer les nouvelles données
    displayPages[currentPage].drawFunction();
  }

  // Mettre à jour l'heure en continu pour la page "Time" (si elle est affichée)
  // Cela permet une mise à jour de l'heure à la seconde près sans attendre le cycle de page
  if (strcmp(displayPages[currentPage].name, "Time") == 0) { // Compare le nom de la page
    updateTime(); // Met à jour timeStamp
    displayPages[currentPage].drawFunction(); // Réaffiche la page avec la nouvelle heure
  }
  delay(10); // Petite pause pour la stabilité du système
}
