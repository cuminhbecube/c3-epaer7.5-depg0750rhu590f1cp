# ESP32-C3 + 7.5" E-Paper Clock & Image Display

Đồng hồ thời tiết + hiển thị ảnh tùy chỉnh trên màn hình E-Ink 7.5" 3 màu, chạy trên **ESP32-C3 DevKitM-1**.

---

## Phần cứng

| Linh kiện | Thông số |
|---|---|
| Vi điều khiển | ESP32-C3 DevKitM-1 |
| Màn hình | 7.5" 3-Color E-Paper (640×384, GxEPD2_750c) |
| Giao tiếp màn hình | SPI (tùy chỉnh) |
| Nút nhấn | GPIO 9 (nút BOOT tích hợp, pull-up nội) |

### Sơ đồ nối chân E-Paper

| Chân E-Paper | GPIO ESP32-C3 |
|---|---|
| SCK | 4 |
| MOSI | 6 |
| CS | 7 |
| DC | 5 |
| RST | 1 |
| BUSY | 0 |
| VCC | 3.3V |
| GND | GND |

---

## Tính năng

- **Màn hình đồng hồ + thời tiết**: Hiển thị giờ, ngày, nhiệt độ hiện tại và dự báo 4 ngày (nguồn: Open-Meteo API).
- **Chế độ ảnh tùy chỉnh**: Upload ảnh nhị phân (640×384, 2bpp 3 màu) qua Wi-Fi — hiển thị ngay lập tức, không nhấp nháy.
- **Cấu hình Wi-Fi qua WiFiManager**: Lần đầu bật nguồn, thiết bị tạo AP `ESP32C3-CLOCK` để kết nối và nhập tọa độ vị trí.
- **OTA Firmware Update**: Tự đọng cập nhật firmware từ URL GitHub khi có yêu cầu.
- **Lưu cấu hình**: Tọa độ địa lý (lat/lon) lưu vào LittleFS dưới dạng JSON.

---

## Cách sử dụng

### 1. Cấu hình Wi-Fi (lần đầu)

1. Bật nguồn thiết bị.
2. Thiết bị tạo hotspot tên **`ESP32C3-CLOCK`**.
3. Kết nối điện thoại/máy tính vào hotspot đó.
4. Trình duyệt tự mở trang cấu hình → nhập SSID, mật khẩu Wi-Fi và **Latitude / Longitude** của vị trí.
5. Lưu → thiết bị kết nối Wi-Fi và bắt đầu hiển thị thời tiết.

### 2. Upload ảnh tùy chỉnh

1. **Nhấn giữ nút BOOT (GPIO 9) khi bật nguồn** → thiết bị vào chế độ Image Mode.
2. Thiết bị tạo hotspot tên **`ESP32C3-IMG`**.
3. Kết nối vào hotspot → trình duyệt tự mở trang upload.
4. Chọn file ảnh nhị phân (`.bin`, 61440 bytes — 30720 bytes BW + 30720 bytes Red, 1bpp mỗi kênh).
5. Nhấn Upload → ảnh hiển thị ngay trên màn hình E-Ink.

> **Định dạng ảnh**: Raw binary, 640×384 pixels, 1bpp cho kênh đen-trắng và 1bpp cho kênh đỏ. Tổng 61440 bytes.

### 3. OTA Update

Chức năng OTA tự động tải firmware từ:
```
https://raw.githubusercontent.com/cuminhbecube/firmware-esp07s-epaper2-Eink8-wr-/main/firmware.bin
```
Có thể gọi endpoint `/ota` để kích hoạt thủ công.

---

## Cài đặt & Build

### Yêu cầu

- [VS Code](https://code.visualstudio.com/) + [PlatformIO IDE Extension](https://platformio.org/install/ide?install=vscode)

### Build và Flash

```bash
# Build
platformio run

# Flash firmware
platformio run --target upload --upload-port COM10

# Flash filesystem (LittleFS)
platformio run --target uploadfs --upload-port COM10

# Mở Serial Monitor
platformio device monitor --port COM10 --baud 115200
```

Thay `COM10` bằng cổng COM thực tế của thiết bị.

---

## Thư viện sử dụng

| Thư viện | Phiên bản | Mô tả |
|---|---|---|
| [GxEPD2](https://github.com/ZinggJM/GxEPD2) | latest | Driver E-Paper |
| [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) | latest | Vẽ font, hình |
| [ArduinoJson](https://arduinojson.org/) | ^7.4.2 | Parse JSON thời tiết |
| [WiFiManager](https://github.com/tzapu/WiFiManager) | ^2.0.17 | Cấu hình Wi-Fi qua AP |

---

## Cấu trúc file

```
c3epaper7.5/
├── driver_config.h          # Định nghĩa chân GPIO
├── platformio.ini           # Cấu hình build PlatformIO
├── src/
│   ├── main.cpp             # Firmware chính
│   └── ImagePage.h          # HTML trang upload ảnh (nhúng trong firmware)
└── include/
    └── weather_icons.h      # Icon thời tiết (bitmap)
```

---

## API Thời tiết

Sử dụng [Open-Meteo](https://open-meteo.com/) — miễn phí, không cần API key.

**Endpoint:**
```
https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}
  &current=temperature_2m,weather_code
  &daily=weather_code,temperature_2m_max,temperature_2m_min
  &timezone=Asia/Bangkok&forecast_days=5
```

---

## Ghi chú kỹ thuật

- **SPI tùy chỉnh**: `SPI.begin(SCK=4, MISO=-1, MOSI=6, SS=7)` — E-Paper là thiết bị write-only nên MISO để `-1`.
- **Không cần wdtFeed**: ESP32 không có phần cứng WDT như ESP8266; các thao tác file dài không gây reset.
- **Tối ưu render ảnh**: Toàn bộ 61440 bytes được `malloc()` vào RAM trước khi gọi `display.drawImage()` — không có file I/O trong lúc màn hình đang refresh.
- **LittleFS**: `LittleFS.begin(true)` — tham số `true` cho phép tự format nếu mount thất bại.
