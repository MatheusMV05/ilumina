#pragma once

// Copie este arquivo para include/secrets.h e preencha com seus dados.
// include/secrets.h está no .gitignore — nunca versione senhas.
// Deixe WIFI_SSID vazio para operar somente em modo local.

#define WIFI_SSID "minha-rede"
#define WIFI_PASSWORD "minha-senha"

#define MQTT_ENABLED false
#define MQTT_HOST ""  // ex.: "192.168.0.10" ou "broker.exemplo.com"
#define MQTT_PORT 1883
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""

#define DEVICE_ID "poste-01"

// Dashboard web: o ESP32 cria esta rede; acesse http://192.168.4.1
#define DASHBOARD_ENABLED true
#define DASHBOARD_AP_SSID "Iluminacao-IoT"
#define DASHBOARD_AP_PASSWORD "troque-esta-senha"
