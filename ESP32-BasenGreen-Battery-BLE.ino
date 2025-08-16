#include <NimBLEDevice.h>

// Configuración BMS
static NimBLEAddress bmsAddress("A5:C2:37:57:E5:FF", BLE_ADDR_PUBLIC); // Change to your own MAC Here
static NimBLEUUID serviceUUID("0000ff00-0000-1000-8000-00805f9b34fb");
static NimBLEUUID charNotifyUUID("0000ff01-0000-1000-8000-00805f9b34fb");
static NimBLEUUID charWriteUUID("0000ff02-0000-1000-8000-00805f9b34fb");

NimBLERemoteCharacteristic* pWriteChar;

// Variables públicas con los datos del BMS
float bms_voltage = 0.0;
float bms_current = 0.0;
int bms_soc = 0;
bool bms_data_ready = false;

// Buffer para frames fragmentados
uint8_t frameBuffer[64];
int bufferPos = 0;
bool frameStarted = false;

void bms_notify_callback(NimBLERemoteCharacteristic* pRC, uint8_t* pData, size_t length, bool isNotify) {
    // Detectar inicio de frame
    if (length >= 4 && pData[0] == 0xDD && pData[1] == 0x03) {
        frameStarted = true;
        bufferPos = 0;
    }
    
    // Acumular datos si hay frame iniciado
    if (frameStarted) {
        for (size_t i = 0; i < length && bufferPos < sizeof(frameBuffer); i++) {
            frameBuffer[bufferPos++] = pData[i];
        }
        
        // Procesar cuando tengamos frame completo (41 bytes)
        if (bufferPos >= 41) {
            bms_voltage = ((frameBuffer[4] << 8) | frameBuffer[5]) / 100.0;
            bms_current = ((int16_t)((frameBuffer[6] << 8) | frameBuffer[7])) / 100.0;
            bms_soc = frameBuffer[23];
            bms_data_ready = true;
            frameStarted = false;
        }
    }
}

bool bms_init() {
    NimBLEDevice::init("");
    
    NimBLEClient* pClient = NimBLEDevice::createClient();
    if (!pClient->connect(bmsAddress)) return false;
    
    NimBLERemoteService* pService = pClient->getService(serviceUUID);
    if (!pService) return false;
    
    NimBLERemoteCharacteristic* pNotifyChar = pService->getCharacteristic(charNotifyUUID);
    pWriteChar = pService->getCharacteristic(charWriteUUID);
    
    if (pNotifyChar && pNotifyChar->canNotify()) {
        pNotifyChar->subscribe(true, bms_notify_callback);
    }
    
    return (pWriteChar != nullptr);
}

void bms_request_data() {
    if (pWriteChar) {
        uint8_t cmd[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
        pWriteChar->writeValue(cmd, sizeof(cmd), false);
    }
}

// Funciones para obtener los datos
float bms_get_voltage() { return bms_voltage; }
float bms_get_current() { return bms_current; }
int bms_get_soc() { return bms_soc; }
bool bms_has_new_data() { 
    if (bms_data_ready) {
        bms_data_ready = false;
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    
    if (bms_init()) {
        Serial.println("BMS conectado");
        bms_request_data();
    } else {
        Serial.println("Error conectando BMS");
    }
}

void loop() {
    // Solicitar datos cada 5 segundos
    static unsigned long lastRequest = 0;
    if (millis() - lastRequest > 5000) {
        bms_request_data();
        lastRequest = millis();
    }
    
    // Mostrar datos cuando estén listos
    if (bms_has_new_data()) {
        Serial.printf("Voltaje: %.2f V, Corriente: %.2f A, SOC: %d%%\n", 
                     bms_get_voltage(), bms_get_current(), bms_get_soc());
    }
    
    delay(100);
}
