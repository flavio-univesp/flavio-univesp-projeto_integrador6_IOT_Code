#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include <time.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>

// ============================================================
// Pinos (ESP32-WROVER-KIT)
//   microSD onboard usa SDMMC 1-bit: CLK=14, CMD=15, D0=2.
//   Esses GPIOs ficam reservados ao periferico SD_MMC.
// ============================================================
#define RFID_SS_PIN    5
#define RFID_RST_PIN  21
#define WIFI_LED_PIN  32
#define GREEN_LED_PIN 33
#define RED_LED_PIN   25
#define BUZZER_PIN    26

// ============================================================
// Wi-Fi
// ============================================================
static const char* WIFI_SSID     = "Entre com o SSID";
static const char* WIFI_PASSWORD = "insira_a_senha_aqui";
static const unsigned long WIFI_RETRY_INTERVAL_MS = 10000;

// ============================================================
// Azure Storage (download de tags-autorizadas.json).
// Preencher SAS de leitura em ambientes com container privado.
// ============================================================
static const char* TAGS_BLOB_URL = "Insira_a_URL_do_blob_aqui";
static const char* TAGS_BLOB_SAS_QUERY = "Insira_a_SAS_de_leitura_aqui";

// ============================================================
// Azure IoT Hub (upload de logs via file upload REST).
// DEVICE_KEY: SharedAccessKey primaria do device em Base64.
// ============================================================
static const char* IOT_HUB_HOST       = "Insira_o_host_do_IoT_Hub_aqui";
static const char* IOT_HUB_DEVICE_ID  = "Insira_o_device_id_aqui";
static const char* IOT_HUB_DEVICE_KEY = "Insira_a_Chave_de_Acesso_Primaria_do_Device_em_Base64_aqui";
static const uint32_t IOT_HUB_SAS_TTL_SECONDS = 3600;

// ============================================================
// Layout do cartao microSD
// ============================================================
static const char* SD_MOUNT_POINT = "/sd";
static const char* SD_CONFIG_DIR  = "/config";
static const char* SD_LOGS_DIR    = "/logs";
static const char* SD_TAGS_FILE   = "/config/tags-autorizadas.json";

// ============================================================
// Tempos e limites
// ============================================================
static const uint32_t ACCESS_GRANT_MS     = 10000;
static const size_t   MAX_TAGS            = 256;
static const size_t   TAG_ID_MAX_LEN      = 20;
static const uint32_t NTP_TIMEOUT_MS      = 15000;
static const uint32_t BUZZER_FREQ_HZ      = 2000;
static const uint32_t BUZZER_BEEP_ON_MS   = 150;
static const uint32_t BUZZER_BEEP_OFF_MS  = 100;
static const uint8_t  BUZZER_BEEP_COUNT   = 3;
static const uint32_t BATCH_MAX_AGE_MS    = 30000;
static const uint8_t  BATCH_MAX_EVENTS    = 5;
static const char*    SD_BATCH_FILE       = "/logs/current-batch.ndjson";

// ============================================================
// Estado global
// ============================================================
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

wl_status_t previousWifiStatus = WL_IDLE_STATUS;
unsigned long lastWifiAttempt = 0;
bool sdReady = false;
bool tagsFileReady = false;
bool timeReady = false;
unsigned long accessGrantedUntilMs = 0;
unsigned long buzzerStartMs = 0;
bool buzzerActive = false;
unsigned long batchFirstEventMs = 0;
uint8_t batchEventCount = 0;

char authorizedTags[MAX_TAGS][TAG_ID_MAX_LEN + 1];
size_t authorizedTagCount = 0;

// ============================================================
// Fila e task de upload assincrono
// ============================================================
struct UploadItem {
  char localPath[128];
};
static const size_t UPLOAD_QUEUE_LEN = 32;
static QueueHandle_t uploadQueue = nullptr;

// ============================================================
// Utilidades
// ============================================================
static String urlEncode(const String& value) {
  String out;
  const char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out += c;
    } else {
      out += '%';
      out += hex[(c >> 4) & 0xF];
      out += hex[c & 0xF];
    }
  }
  return out;
}

static String formatTagUid(MFRC522::Uid uid) {
  String out;
  const char hex[] = "0123456789ABCDEF";
  for (byte i = 0; i < uid.size; i++) {
    out += hex[(uid.uidByte[i] >> 4) & 0xF];
    out += hex[uid.uidByte[i] & 0xF];
    if (i != uid.size - 1) out += ' ';
  }
  return out;
}

static String formatIsoUtc(time_t utcSeconds) {
  struct tm t;
  gmtime_r(&utcSeconds, &t);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
  return String(buf);
}

static String randomHex(size_t bytes) {
  String out;
  const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < bytes; i++) {
    uint8_t b = (uint8_t)esp_random();
    out += hex[(b >> 4) & 0xF];
    out += hex[b & 0xF];
  }
  return out;
}

// ============================================================
// LEDs de acesso
// ============================================================
static void updateAccessLeds() {
  bool granted = accessGrantedUntilMs != 0 && (long)(millis() - accessGrantedUntilMs) < 0;
  if (!granted && accessGrantedUntilMs != 0) accessGrantedUntilMs = 0;
  digitalWrite(GREEN_LED_PIN, granted ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, granted ? LOW : HIGH);
}

static void grantAccessWindow() {
  accessGrantedUntilMs = millis() + ACCESS_GRANT_MS;
  updateAccessLeds();
}

// ============================================================
// Buzzer de acesso negado (nao bloqueante)
// ============================================================
static void startDenialBuzzer() {
  buzzerActive = true;
  buzzerStartMs = millis();
}

static void updateBuzzer() {
  if (!buzzerActive) return;
  const uint32_t cycle = BUZZER_BEEP_ON_MS + BUZZER_BEEP_OFF_MS;
  const uint32_t total = cycle * BUZZER_BEEP_COUNT;
  unsigned long elapsed = millis() - buzzerStartMs;
  if (elapsed >= total) {
    ledcWriteTone(BUZZER_PIN, 0);
    buzzerActive = false;
    return;
  }
  uint32_t phase = elapsed % cycle;
  // Buzzer e LED vermelho piscam juntos durante o alerta de acesso negado.
  if (phase < BUZZER_BEEP_ON_MS) {
    ledcWriteTone(BUZZER_PIN, BUZZER_FREQ_HZ);
    digitalWrite(RED_LED_PIN, HIGH);
  } else {
    ledcWriteTone(BUZZER_PIN, 0);
    digitalWrite(RED_LED_PIN, LOW);
  }
}

// ============================================================
// Wi-Fi
// ============================================================
static void connectWifi() {
  Serial.print("Conectando Wi-Fi ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt = millis();
}

static void updateWifiStatus() {
  wl_status_t currentStatus = WiFi.status();
  digitalWrite(WIFI_LED_PIN, currentStatus == WL_CONNECTED ? HIGH : LOW);

  if (currentStatus != previousWifiStatus) {
    if (currentStatus == WL_CONNECTED) {
      Serial.print("Wi-Fi conectado. IP: ");
      Serial.println(WiFi.localIP());
    } else if (previousWifiStatus == WL_CONNECTED) {
      Serial.println("Wi-Fi desconectado.");
    }
    previousWifiStatus = currentStatus;
  }

  if (currentStatus != WL_CONNECTED &&
      millis() - lastWifiAttempt >= WIFI_RETRY_INTERVAL_MS) {
    WiFi.disconnect();
    connectWifi();
  }
}

static bool ensureWifi(uint32_t timeoutMs) {
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    updateWifiStatus();
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

// ============================================================
// Relogio (NTP em UTC)
// ============================================================
static bool syncTime() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  unsigned long start = millis();
  while (millis() - start < NTP_TIMEOUT_MS) {
    time_t now = time(nullptr);
    if (now > 1700000000) {
      timeReady = true;
      Serial.print("Relogio UTC: ");
      Serial.println(formatIsoUtc(now));
      return true;
    }
    delay(250);
  }
  Serial.println("Falha ao sincronizar NTP.");
  return false;
}

// ============================================================
// microSD
// ============================================================
static bool ensureDir(const char* path) {
  if (SD_MMC.exists(path)) return true;
  if (SD_MMC.mkdir(path)) return true;
  Serial.printf("Falha ao criar diretorio: %s\n", path);
  return false;
}

static bool mountSdCard() {
  // WROVER-KIT: SDMMC 1-bit (CLK=14, CMD=15, D0=2); format_if_mount_failed
  // formata em FAT toda a capacidade se a particao existente for ilegivel.
  if (!SD_MMC.begin(SD_MOUNT_POINT, true, true)) {
    Serial.println("Falha ao montar microSD apos tentativa de formatacao.");
    return false;
  }
  if (SD_MMC.cardType() == CARD_NONE) {
    Serial.println("Cartao microSD nao detectado.");
    return false;
  }
  Serial.printf("microSD montado. Tamanho=%llu MB Usado=%llu MB\n",
                SD_MMC.cardSize() / (1024ULL * 1024ULL),
                SD_MMC.usedBytes() / (1024ULL * 1024ULL));
  if (!ensureDir(SD_CONFIG_DIR) || !ensureDir(SD_LOGS_DIR)) return false;
  sdReady = true;
  return true;
}

// ============================================================
// Cache de TAGs autorizadas
// ============================================================
static bool loadAuthorizedTagsFromFile() {
  authorizedTagCount = 0;
  tagsFileReady = false;
  if (!sdReady || !SD_MMC.exists(SD_TAGS_FILE)) return false;

  File f = SD_MMC.open(SD_TAGS_FILE, FILE_READ);
  if (!f) return false;
  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("JSON local invalido: %s\n", err.c_str());
    return false;
  }
  JsonArray arr = doc["tags"].as<JsonArray>();
  for (JsonObject item : arr) {
    const char* tag = item["tagid"] | (const char*)nullptr;
    if (!tag) continue;
    size_t len = strlen(tag);
    if (len == 0 || len > TAG_ID_MAX_LEN) continue;
    if (authorizedTagCount >= MAX_TAGS) break;
    strncpy(authorizedTags[authorizedTagCount], tag, TAG_ID_MAX_LEN);
    authorizedTags[authorizedTagCount][TAG_ID_MAX_LEN] = '\0';
    authorizedTagCount++;
  }
  const char* version = doc["version"] | "";
  Serial.printf("TAGs autorizadas: %u (version=%s)\n",
                (unsigned)authorizedTagCount, version);
  tagsFileReady = authorizedTagCount > 0;
  return true;
}

static bool isTagAuthorized(const String& tagid) {
  for (size_t i = 0; i < authorizedTagCount; i++) {
    if (tagid.equalsIgnoreCase(authorizedTags[i])) return true;
  }
  return false;
}

static String readLocalVersion() {
  if (!sdReady || !SD_MMC.exists(SD_TAGS_FILE)) return String();
  File f = SD_MMC.open(SD_TAGS_FILE, FILE_READ);
  if (!f) return String();
  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, f)) { f.close(); return String(); }
  f.close();
  const char* v = doc["version"] | "";
  return String(v);
}

// ============================================================
// Download condicional do tags-autorizadas.json
// ============================================================
static bool downloadTagsFile() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem Wi-Fi para baixar TAGs.");
    return false;
  }

  String url = String(TAGS_BLOB_URL);
  if (strlen(TAGS_BLOB_SAS_QUERY) > 0) {
    url += (url.indexOf('?') >= 0 ? "&" : "?");
    url += TAGS_BLOB_SAS_QUERY;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("Falha ao iniciar HTTPS para o Storage.");
    return false;
  }
  http.setUserAgent("ESP32-Portaria/1.0");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("Download tags-autorizadas.json falhou: HTTP %d\n", code);
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument remote(16384);
  DeserializationError err = deserializeJson(remote, body);
  if (err) {
    Serial.printf("JSON remoto invalido: %s\n", err.c_str());
    return false;
  }
  const char* remoteVersion = remote["version"] | "";
  String localVersion = readLocalVersion();
  if (localVersion.length() > 0 && strcmp(remoteVersion, localVersion.c_str()) <= 0) {
    Serial.printf("Copia local ja atualizada (version=%s).\n", localVersion.c_str());
    return true;
  }

  File f = SD_MMC.open(SD_TAGS_FILE, FILE_WRITE);
  if (!f) {
    Serial.println("Falha ao abrir arquivo local para escrita.");
    return false;
  }
  size_t written = f.print(body);
  f.close();
  if (written == 0) {
    Serial.println("Nada gravado no cartao.");
    return false;
  }
  Serial.printf("tags-autorizadas.json atualizado (version=%s, %u bytes).\n",
                remoteVersion, (unsigned)written);
  return true;
}

// ============================================================
// SAS token para IoT Hub Device REST API
// ============================================================
static String buildIotHubSasToken(uint32_t ttlSeconds) {
  String resourceUri = String(IOT_HUB_HOST) + "/devices/" + IOT_HUB_DEVICE_ID;
  String encodedResource = urlEncode(resourceUri);
  time_t expiry = time(nullptr) + ttlSeconds;
  String toSign = encodedResource + "\n" + String((unsigned long)expiry);

  uint8_t keyBuf[64];
  size_t keyLen = 0;
  if (mbedtls_base64_decode(keyBuf, sizeof(keyBuf), &keyLen,
                            (const unsigned char*)IOT_HUB_DEVICE_KEY,
                            strlen(IOT_HUB_DEVICE_KEY)) != 0) {
    Serial.println("DEVICE_KEY Base64 invalido.");
    return String();
  }

  uint8_t hmac[32];
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, md, 1);
  mbedtls_md_hmac_starts(&ctx, keyBuf, keyLen);
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)toSign.c_str(), toSign.length());
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);

  uint8_t sigB64[64];
  size_t sigB64Len = 0;
  mbedtls_base64_encode(sigB64, sizeof(sigB64), &sigB64Len, hmac, sizeof(hmac));
  String signature = urlEncode(String((char*)sigB64).substring(0, sigB64Len));

  return String("SharedAccessSignature sr=") + encodedResource +
         "&sig=" + signature +
         "&se=" + String((unsigned long)expiry);
}

// ============================================================
// Upload do log via IoT Hub file upload
// ============================================================
// Upload do log via IoT Hub file upload
//   Reaproveita a mesma conexao TLS para init e notify no host
//   do IoT Hub, evitando um handshake extra por evento.
// ============================================================
static bool uploadAccessLogViaIotHub(const String& localPath, const String& blobFileName,
                                     const String& payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem Wi-Fi para IoT Hub.");
    return false;
  }
  if (!timeReady && !syncTime()) return false;

  String sas = buildIotHubSasToken(IOT_HUB_SAS_TTL_SECONDS);
  if (sas.length() == 0) return false;

  String blobPath = String("logs/") + IOT_HUB_DEVICE_ID + "/" + blobFileName;
  String initUrl = String("https://") + IOT_HUB_HOST +
                   "/devices/" + IOT_HUB_DEVICE_ID +
                   "/files?api-version=2020-03-13";
  String notifyUrl = String("https://") + IOT_HUB_HOST +
                     "/devices/" + IOT_HUB_DEVICE_ID +
                     "/files/notifications?api-version=2020-03-13";

  WiFiClientSecure hubClient;
  hubClient.setInsecure();
  HTTPClient hubHttp;
  hubHttp.setReuse(true);

  if (!hubHttp.begin(hubClient, initUrl)) return false;
  hubHttp.addHeader("Authorization", sas);
  hubHttp.addHeader("Content-Type", "application/json");
  hubHttp.addHeader("Accept", "application/json");

  DynamicJsonDocument initReq(256);
  initReq["blobName"] = blobPath;
  String initBody;
  serializeJson(initReq, initBody);

  int code = hubHttp.POST(initBody);
  String initResp = hubHttp.getString();
  if (code != HTTP_CODE_OK) {
    Serial.printf("IoT Hub /files falhou: HTTP %d %s\n", code, initResp.c_str());
    hubHttp.end();
    return false;
  }

  DynamicJsonDocument initDoc(1024);
  if (deserializeJson(initDoc, initResp)) {
    Serial.println("Resposta /files invalida.");
    hubHttp.end();
    return false;
  }
  String correlationId = String(initDoc["correlationId"] | "");
  String hostName      = String(initDoc["hostName"] | "");
  String container     = String(initDoc["containerName"] | "");
  String blobName      = String(initDoc["blobName"] | "");
  String sasToken      = String(initDoc["sasToken"] | "");
  if (correlationId.isEmpty() || hostName.isEmpty() || container.isEmpty()
      || blobName.isEmpty() || sasToken.isEmpty()) {
    Serial.println("Campos da resposta /files ausentes.");
    hubHttp.end();
    return false;
  }

  String putUrl = String("https://") + hostName + "/" + container + "/" + blobName + sasToken;
  WiFiClientSecure putClient;
  putClient.setInsecure();
  HTTPClient putHttp;
  if (!putHttp.begin(putClient, putUrl)) {
    hubHttp.end();
    return false;
  }
  putHttp.addHeader("x-ms-blob-type", "BlockBlob");
  putHttp.addHeader("Content-Type", "application/octet-stream");
  int putCode = putHttp.PUT((uint8_t*)payload.c_str(), payload.length());
  String putResp = putHttp.getString();
  putHttp.end();
  bool putOk = (putCode == 201);
  if (!putOk) {
    Serial.printf("PUT blob falhou: HTTP %d %s\n", putCode, putResp.c_str());
  }

  if (!hubHttp.begin(hubClient, notifyUrl)) return false;
  hubHttp.addHeader("Authorization", sas);
  hubHttp.addHeader("Content-Type", "application/json");

  DynamicJsonDocument notifyReq(512);
  notifyReq["correlationId"]     = correlationId;
  notifyReq["isSuccess"]         = putOk;
  notifyReq["statusCode"]        = putOk ? 200 : putCode;
  notifyReq["statusDescription"] = putOk ? "OK" : "Upload failed";
  String notifyBody;
  serializeJson(notifyReq, notifyBody);

  int notifyCode = hubHttp.POST(notifyBody);
  String notifyResp = hubHttp.getString();
  hubHttp.end();
  if (notifyCode != 204 && notifyCode != HTTP_CODE_OK) {
    Serial.printf("Notify falhou: HTTP %d %s\n", notifyCode, notifyResp.c_str());
    return false;
  }

  Serial.printf("Log enviado: %s -> %s/%s\n", localPath.c_str(), container.c_str(), blobName.c_str());
  return putOk;
}

// ============================================================
// ============================================================
// Registro de acesso (liberado ou negado) — acumula em batch
// ============================================================
static bool enqueueUpload(const String& localPath) {
  UploadItem item = {};
  strncpy(item.localPath, localPath.c_str(), sizeof(item.localPath) - 1);
  return uploadQueue != nullptr && xQueueSend(uploadQueue, &item, 0) == pdPASS;
}

static String uniqueBatchName() {
  time_t nowUtc = timeReady ? time(nullptr) : (time_t)(millis() / 1000);
  return String(SD_LOGS_DIR) + "/" + String(IOT_HUB_DEVICE_ID) + "-" +
         String((unsigned long)nowUtc) + "-" + randomHex(4) + ".ndjson";
}

static void flushBatch() {
  if (!sdReady) return;
  if (batchEventCount == 0 && !SD_MMC.exists(SD_BATCH_FILE)) return;
  String finalPath = uniqueBatchName();
  if (!SD_MMC.rename(SD_BATCH_FILE, finalPath.c_str())) {
    Serial.printf("Falha ao renomear batch para %s\n", finalPath.c_str());
    return;
  }
  Serial.printf("Batch fechado: %s (%u eventos).\n",
                finalPath.c_str(), (unsigned)batchEventCount);
  batchEventCount = 0;
  batchFirstEventMs = 0;
  if (!enqueueUpload(finalPath)) {
    Serial.println("Fila cheia; batch aguardara reenvio no proximo boot.");
  }
}

static void maybeFlushBatch() {
  if (batchEventCount == 0) return;
  if (batchEventCount >= BATCH_MAX_EVENTS ||
      (millis() - batchFirstEventMs) >= BATCH_MAX_AGE_MS) {
    flushBatch();
  }
}

static void promoteStaleBatch() {
  if (!sdReady) return;
  if (!SD_MMC.exists(SD_BATCH_FILE)) return;
  String finalPath = uniqueBatchName();
  if (SD_MMC.rename(SD_BATCH_FILE, finalPath.c_str())) {
    Serial.printf("Batch pendente promovido a %s.\n", finalPath.c_str());
  }
}

static void registerAccess(const String& tagid, bool liberado) {
  if (!sdReady) {
    Serial.println("microSD indisponivel; evento nao registrado.");
    return;
  }
  if (!timeReady && !syncTime()) {
    Serial.println("Sem horario UTC confiavel; log nao gerado.");
    return;
  }

  time_t nowUtc = time(nullptr);
  String iso = formatIsoUtc(nowUtc);
  String eventoId = String(IOT_HUB_DEVICE_ID) + "-" +
                    String((unsigned long)nowUtc) + "-" + randomHex(4);

  DynamicJsonDocument record(512);
  record["eventoId"]      = eventoId;
  record["dispositivoId"] = IOT_HUB_DEVICE_ID;
  record["tagid"]         = tagid;
  record["acesso"]        = iso;
  record["liberacao"]     = liberado;
  String line;
  serializeJson(record, line);
  line += "\n";

  File f = SD_MMC.open(SD_BATCH_FILE, FILE_APPEND);
  if (!f) {
    Serial.printf("Falha ao abrir batch %s\n", SD_BATCH_FILE);
    return;
  }
  f.print(line);
  f.close();

  if (batchEventCount == 0) batchFirstEventMs = millis();
  batchEventCount++;
  Serial.printf("Evento no batch: %u/%u.\n",
                (unsigned)batchEventCount, (unsigned)BATCH_MAX_EVENTS);

  if (batchEventCount >= BATCH_MAX_EVENTS) flushBatch();
}

// ============================================================
// Reenvio dos NDJSON pendentes no boot
// ============================================================
static bool uploadAndClearIfOk(const String& localPath, const String& fileName) {
  File f = SD_MMC.open(localPath.c_str(), FILE_READ);
  if (!f) return false;
  String payload = f.readString();
  f.close();
  if (payload.length() == 0) return false;
  if (!uploadAccessLogViaIotHub(localPath, fileName, payload)) return false;
  SD_MMC.remove(localPath.c_str());
  return true;
}

static void retryPendingLogs() {
  if (!sdReady) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (!timeReady && !syncTime()) return;

  const size_t MAX_PENDING = 64;
  String pending[MAX_PENDING];
  size_t count = 0;

  File dir = SD_MMC.open(SD_LOGS_DIR);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return;
  }
  File entry = dir.openNextFile();
  while (entry && count < MAX_PENDING) {
    String name = String(entry.name());
    entry.close();
    String localPath = name.startsWith("/") ? name : (String(SD_LOGS_DIR) + "/" + name);
    if (localPath.endsWith(".ndjson")) {
      pending[count++] = localPath;
    }
    entry = dir.openNextFile();
  }
  dir.close();

  if (count == 0) return;
  Serial.printf("Reenvio de logs pendentes: %u arquivo(s) em fila.\n", (unsigned)count);

  size_t enqueued = 0;
  for (size_t i = 0; i < count; i++) {
    UploadItem item = {};
    strncpy(item.localPath, pending[i].c_str(), sizeof(item.localPath) - 1);
    if (uploadQueue != nullptr && xQueueSend(uploadQueue, &item, 0) == pdPASS) enqueued++;
  }
  Serial.printf("Reenvio agendado: %u de %u enfileirados.\n",
                (unsigned)enqueued, (unsigned)count);
}

// ============================================================
// Task worker de upload (roda no core 0, sem bloquear o loop)
// ============================================================
static void uploadWorkerTask(void* /*param*/) {
  UploadItem item;
  for (;;) {
    if (xQueueReceive(uploadQueue, &item, portMAX_DELAY) != pdPASS) continue;
    String localPath = String(item.localPath);
    int slash = localPath.lastIndexOf('/');
    String fileName = (slash >= 0) ? localPath.substring(slash + 1) : localPath;
    if (!uploadAndClearIfOk(localPath, fileName)) {
      Serial.printf("Upload em background falhou; %s permanece para reenvio.\n",
                    localPath.c_str());
    }
  }
}

// ============================================================
// Setup e loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  // Vermelho aceso por padrao: portaria bloqueada.
  digitalWrite(RED_LED_PIN, HIGH);

  // Buzzer via LEDC (resolucao 8 bits). Auto-teste imediato: 2 bipes de 150 ms
  // confirmam cabeamento antes mesmo do Wi-Fi subir.
  if (!ledcAttach(BUZZER_PIN, BUZZER_FREQ_HZ, 8)) {
    Serial.println("Falha ao inicializar buzzer no LEDC.");
  } else {
    Serial.println("Auto-teste do buzzer...");
    for (uint8_t i = 0; i < 2; i++) {
      ledcWriteTone(BUZZER_PIN, BUZZER_FREQ_HZ);
      delay(150);
      ledcWriteTone(BUZZER_PIN, 0);
      delay(100);
    }
  }

  uploadQueue = xQueueCreate(UPLOAD_QUEUE_LEN, sizeof(UploadItem));
  if (uploadQueue == nullptr) {
    Serial.println("Falha ao criar fila de upload.");
  } else {
    xTaskCreatePinnedToCore(uploadWorkerTask, "upload", 8192, nullptr, 1, nullptr, 0);
  }

  WiFi.mode(WIFI_STA);
  connectWifi();
  ensureWifi(20000);
  if (WiFi.status() == WL_CONNECTED) syncTime();

  if (!mountSdCard()) {
    Serial.println("microSD indisponivel; sistema seguira somente com RFID.");
  } else {
    downloadTagsFile();
    loadAuthorizedTagsFromFile();
    promoteStaleBatch();
    retryPendingLogs();
  }

  SPI.begin();
  rfid.PCD_Init();
  Serial.print("MFRC522 VersionReg=0x");
  Serial.println(rfid.PCD_ReadRegister(MFRC522::VersionReg), HEX);
  rfid.PCD_DumpVersionToSerial();
  Serial.println("Aproxime o cartao do leitor.");
}

void loop() {
  updateWifiStatus();
  updateAccessLeds();
  updateBuzzer();
  maybeFlushBatch();

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String tagid = formatTagUid(rfid.uid);
  Serial.printf("TAG lida: %s\n", tagid.c_str());

  if (tagsFileReady && isTagAuthorized(tagid)) {
    Serial.println("Acesso liberado.");
    grantAccessWindow();
    registerAccess(tagid, true);
  } else {
    Serial.println("Acesso negado.");
    startDenialBuzzer();
    registerAccess(tagid, false);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
