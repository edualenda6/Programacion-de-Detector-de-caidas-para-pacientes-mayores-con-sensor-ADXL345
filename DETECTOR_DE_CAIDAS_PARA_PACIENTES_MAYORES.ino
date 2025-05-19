#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Configuración WiFi y Telegram
const char* ssid = "";
const char* password = "";
const char* botToken = "";
const char* chatId = "";

// Umbral de impacto ajustado
const float UMBRAL_IMPACTO = 16.0; // m/s²

WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Estado del sistema
bool sistemaActivo = true;
unsigned long ultimaVerificacionMensajes = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n⚡ Sistema de alertas iniciado");

  // Conexión WiFi
  conectarWiFi();

  // Configuración cliente seguro
  secured_client.setInsecure();
  secured_client.setTimeout(10000);

  // Inicialización del sensor
  if (!accel.begin()) {
    Serial.println("❌ Error en sensor - Verifica conexiones");
    while (true);
  }
  accel.setRange(ADXL345_RANGE_16_G);
  Serial.println("✅ Sensor listo");

  // Mensaje inicial
  enviarNotificacion("🟢 *Sistema de alertas ACTIVO*\n\n"
                    "Comandos de Telegram:\n"
                    "/encender - Activa el sistema\n"
                    "/apagar - Desactiva el sistema");
}

void loop() {
  mantenerConexiones();
  verificarComandosTelegram();

  if (!sistemaActivo) {
    delay(500);
    return;
  }

  // Detección de impacto automático
  sensors_event_t lectura;
  accel.getEvent(&lectura);
  float fuerza = sqrt(
    lectura.acceleration.x * lectura.acceleration.x +
    lectura.acceleration.y * lectura.acceleration.y +
    lectura.acceleration.z * lectura.acceleration.z
  );

  if (fuerza > UMBRAL_IMPACTO) {
    Serial.println("¡Impacto detectado!");
    enviarAlertaImpacto(fuerza);
    delay(1000); // Evita múltiples detecciones
  }

  delay(100);
}

// ========== CONTROL POR TELEGRAM ==========
void verificarComandosTelegram() {
  if (millis() - ultimaVerificacionMensajes > 1000) {
    int nuevosMensajes = bot.getUpdates(bot.last_message_received + 1);

    while (nuevosMensajes) {
      for (int i = 0; i < nuevosMensajes; i++) {
        String texto = bot.messages[i].text;
        String chat_id = bot.messages[i].chat_id;

        if (texto == "/encender") {
          sistemaActivo = true;
          bot.sendMessage(chat_id, "✅ *Sistema ACTIVADO*", "Markdown");
          Serial.println("Sistema activado por Telegram");
        }
        else if (texto == "/apagar") {
          sistemaActivo = false;
          bot.sendMessage(chat_id, "🔴 *Sistema DESACTIVADO*", "Markdown");
          Serial.println("Sistema desactivado por Telegram");
        }
      }
      nuevosMensajes = bot.getUpdates(bot.last_message_received + 1);
    }
    ultimaVerificacionMensajes = millis();
  }
}

// ========== FUNCIONES AUXILIARES ==========
void conectarWiFi() {
  Serial.print("📡 Conectando a WiFi...");
  WiFi.begin(ssid, password);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi conectado");
  } else {
    Serial.println("\n❌ Error en conexión WiFi");
  }
}

void mantenerConexiones() {
  static unsigned long ultimaVerificacion = 0;
  if (millis() - ultimaVerificacion > 15000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("⚠️ Reconectando WiFi...");
      WiFi.reconnect();
    }
    ultimaVerificacion = millis();
  }
}

String clasificarImpacto(float fuerza) {
  if (fuerza < 20.0) return "LEVE";
  if (fuerza < 30.0) return "MODERADO";
  if (fuerza < 50.0) return "FUERTE";
  return "SEVERO";
}

void enviarAlertaImpacto(float fuerza) {
  String mensaje = "🚨 *ALERTA DE IMPACTO DETECTADO*\n\n";
  mensaje += "• *Nivel de impacto:* " + clasificarImpacto(fuerza) + "\n";
  mensaje += "• *Estado del sistema:* " + String(sistemaActivo ? "OPERATIVO" : "INACTIVO") + "\n\n";
  mensaje += "Equipos de emergencia en camino.";

  enviarNotificacion(mensaje);
}

bool enviarNotificacion(String mensaje) {
  Serial.println("📤 Enviando notificación...");

  for (int intento = 1; intento <= 3; intento++) {
    if (WiFi.status() != WL_CONNECTED) {
      delay(2000);
      continue;
    }

    if (bot.sendMessage(chatId, mensaje, "Markdown")) {
      Serial.println("✅ Notificación enviada");
      return true;
    }

    delay(3000);
  }

  Serial.println("❌ Fallo al enviar notificación");
  return false;
}
