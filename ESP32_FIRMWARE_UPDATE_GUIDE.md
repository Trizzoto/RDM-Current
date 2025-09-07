# ESP32 Firmware Update Integration Guide
## RDM-7 Dash OTA Update System

This guide explains how to integrate your ESP32 device with the RDM-7 firmware update portal for Over-the-Air (OTA) updates.

## 🌐 Firmware Portal Access

**Web Interface:** https://rdm-7-ota-firmware-updates.vercel.app
- Upload new firmware files (.bin)
- View available firmware versions
- Manual download links
- Version management

## 📡 API Endpoints for ESP32

### Check Latest Firmware
```cpp
GET https://rdm-7-ota-firmware-updates.vercel.app/api/firmware/latest
```

**Response Example:**
```json
{
  "version": "1.2.3",
  "downloadUrl": "https://rdm-7-ota-firmware-updates.vercel.app/firmware/rdm7_v1.2.3.bin",
  "size": "2.1 MB",
  "sizeBytes": 2097152,
  "date": "2025-01-20",
  "filename": "rdm7_v1.2.3.bin",
  "releaseNotes": "Firmware version 1.2.3"
}
```

### List All Firmware Versions
```cpp
GET https://rdm-7-ota-firmware-updates.vercel.app/api/firmware/list
```

## 🔧 ESP32 Implementation

### Required Libraries
```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
```

### Install via Arduino Library Manager:
- **ArduinoJson** by Benoit Blanchon
- **HTTPClient** (built-in with ESP32)

### Basic Integration Code

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>

// Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* firmwareServer = "https://rdm-7-ota-firmware-updates.vercel.app";
const String currentVersion = "1.0.0"; // YOUR CURRENT VERSION

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  connectToWiFi();
  
  // Check for firmware updates on startup
  checkForUpdates();
}

void loop() {
  // Your main application code here
  
  // Optional: Check for updates periodically
  static unsigned long lastUpdateCheck = 0;
  if (millis() - lastUpdateCheck > 3600000) { // Check every hour
    checkForUpdates();
    lastUpdateCheck = millis();
  }
  
  delay(1000);
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void checkForUpdates() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Skipping update check.");
    return;
  }
  
  Serial.println("Checking for firmware updates...");
  
  HTTPClient http;
  http.begin(String(firmwareServer) + "/api/firmware/latest");
  http.setTimeout(10000); // 10 second timeout
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse JSON response
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      http.end();
      return;
    }
    
    String latestVersion = doc["version"];
    String downloadUrl = doc["downloadUrl"];
    int fileSize = doc["sizeBytes"];
    
    Serial.println("=== Firmware Update Check ===");
    Serial.println("Current version: " + currentVersion);
    Serial.println("Latest version:  " + latestVersion);
    Serial.println("=============================");
    
    if (latestVersion != currentVersion) {
      Serial.println("🚀 New firmware available! Starting download...");
      downloadAndInstallFirmware(downloadUrl, fileSize);
    } else {
      Serial.println("✅ Firmware is up to date.");
    }
  } else if (httpCode == 404) {
    Serial.println("ℹ️  No firmware files available on server.");
  } else {
    Serial.printf("❌ Failed to check for updates. HTTP code: %d\n", httpCode);
  }
  
  http.end();
}

void downloadAndInstallFirmware(String url, int expectedSize) {
  Serial.println("Starting firmware download from: " + url);
  
  HTTPClient http;
  http.begin(url);
  http.setTimeout(30000); // 30 second timeout for download
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    
    Serial.printf("Content length: %d bytes\n", contentLength);
    
    if (contentLength != expectedSize) {
      Serial.printf("⚠️  Content length mismatch! Expected: %d, Got: %d\n", expectedSize, contentLength);
      http.end();
      return;
    }
    
    if (contentLength > 0) {
      bool canBegin = Update.begin(contentLength);
      
      if (canBegin) {
        Serial.println("📥 Starting firmware update...");
        
        WiFiClient* client = http.getStreamPtr();
        size_t written = Update.writeStream(*client);
        
        if (written == contentLength) {
          Serial.println("📥 Download completed successfully!");
          
          if (Update.end()) {
            if (Update.isFinished()) {
              Serial.println("🎉 Update successful! Rebooting in 3 seconds...");
              delay(3000);
              ESP.restart();
            } else {
              Serial.println("❌ Update not finished. Something went wrong.");
            }
          } else {
            Serial.printf("❌ Update error: %d\n", Update.getError());
          }
        } else {
          Serial.printf("❌ Download incomplete. Written: %d of %d bytes\n", written, contentLength);
        }
      } else {
        Serial.println("❌ Not enough space for OTA update");
      }
    } else {
      Serial.println("❌ Invalid content length");
    }
  } else {
    Serial.printf("❌ Download failed. HTTP code: %d\n", httpCode);
  }
  
  http.end();
}
```

## 📋 Implementation Checklist

### Before Using This Code:

1. **✅ Update Configuration**
   ```cpp
   const char* ssid = "YOUR_ACTUAL_WIFI_SSID";
   const char* password = "YOUR_ACTUAL_WIFI_PASSWORD";
   const String currentVersion = "YOUR_CURRENT_VERSION"; // e.g., "1.0.0"
   ```

2. **✅ Install Required Libraries**
   - Open Arduino IDE
   - Go to Tools > Manage Libraries
   - Search and install "ArduinoJson" by Benoit Blanchon

3. **✅ Set Partition Scheme**
   - In Arduino IDE: Tools > Partition Scheme
   - Choose "Minimal SPIFFS" or "No OTA" for more app space
   - Or choose "Default 4MB with spiffs" for OTA support

4. **✅ Test WiFi Connection First**
   - Verify your ESP32 can connect to WiFi
   - Check the serial monitor for connection status

## 📱 Firmware File Naming Convention

When uploading firmware to the portal, use this naming pattern:
- `rdm7_v1.0.0.bin` ✅
- `firmware_v2.1.3.bin` ✅
- `esp32_rdm7_v1.2.0.bin` ✅

The system automatically extracts version numbers for comparison.

## 🔧 Advanced Configuration

### Custom Update Intervals
```cpp
// Check for updates every 6 hours
if (millis() - lastUpdateCheck > 21600000) {
  checkForUpdates();
  lastUpdateCheck = millis();
}
```

### Version Comparison Logic
```cpp
bool isNewerVersion(String current, String latest) {
  // Implement custom version comparison logic
  // Current implementation uses simple string comparison
  return current != latest;
}
```

### Error Handling & Retry Logic
```cpp
int updateRetries = 0;
const int maxRetries = 3;

void checkForUpdatesWithRetry() {
  if (updateRetries < maxRetries) {
    if (!checkForUpdates()) {
      updateRetries++;
      delay(5000); // Wait 5 seconds before retry
      checkForUpdatesWithRetry();
    } else {
      updateRetries = 0; // Reset on success
    }
  }
}
```

## 🐛 Troubleshooting

### Common Issues:

1. **WiFi Connection Failed**
   - Check SSID and password
   - Ensure 2.4GHz network (ESP32 doesn't support 5GHz)

2. **HTTP Request Timeout**
   - Check internet connectivity
   - Verify server URL is correct
   - Increase timeout values

3. **JSON Parsing Error**
   - Server might be returning HTML error page
   - Check HTTP response code first

4. **OTA Update Failed**
   - Ensure sufficient flash memory
   - Check partition scheme
   - Verify .bin file is valid ESP32 firmware

5. **Update Successful but Device Won't Boot**
   - Firmware might be corrupted
   - Check compiler settings match your ESP32 model
   - Verify the uploaded .bin file works

### Debug Mode:
Add this for detailed logging:
```cpp
Serial.setDebugOutput(true);
```

## 📞 Support

- **Portal Issues**: Check the web interface at the URL above
- **ESP32 Code Issues**: Review the troubleshooting section
- **Firmware Upload**: Use the web interface to upload new .bin files

## 🔒 Security Notes

- Currently no authentication required (development mode)
- All firmware files are publicly accessible
- Use HTTPS for production deployments
- Consider adding firmware signing for production use

---

**Ready to implement?** Copy the code above, update the configuration values, and start testing your OTA update system! 