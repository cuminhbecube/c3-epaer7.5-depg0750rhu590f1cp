# ESP32-C3 + 7.5" E-Paper Clock & Image Display

Đồng hồ thời tiết + hiển thị ảnh tùy chỉnh trên màn hình E-Ink 7.5" 3 màu, chạy trên **ESP32-C3 DevKitM-1**.

---

## Mục lục

1. [Tổng quan](#tổng-quan)
2. [Phần cứng & Sơ đồ nối chân](#phần-cứng)
3. [Cài đặt lần đầu — Kết nối WiFi](#1-cài-đặt-lần-đầu--kết-nối-wifi)
4. [Màn hình đồng hồ & thời tiết](#2-màn-hình-đồng-hồ--thời-tiết)
5. [Chế độ hiển thị ảnh](#3-chế-độ-hiển-thị-ảnh)
6. [Tải ảnh lên thiết bị](#4-tải-ảnh-lên-thiết-bị)
7. [Chức năng nút nhấn (bảng tóm tắt)](#5-chức-năng-nút-nhấn)
8. [Cập nhật firmware OTA](#6-cập-nhật-firmware-ota)
9. [Đặt lại cấu hình WiFi](#7-đặt-lại-cấu-hình-wifi)
10. [Thông số kỹ thuật](#thông-số-kỹ-thuật)
11. [Build & Flash](#build--flash-cho-developer)
12. [Thư viện sử dụng](#thư-viện-sử-dụng)
13. [Cấu trúc file](#cấu-trúc-file)
14. [Ghi chú kỹ thuật](#ghi-chú-kỹ-thuật)

---

## Tổng quan

Thiết bị hiển thị:
- **Đồng hồ** (giờ, ngày tháng) — màu đỏ
- **Thời tiết hiện tại** và dự báo 5 ngày tới (lấy từ [Open-Meteo](https://open-meteo.com/), miễn phí, không cần API key)
- **Nhiệt độ & độ ẩm** từ cảm biến SHT30 gắn trực tiếp
- **Ảnh tùy chỉnh** — tối đa 5 ảnh, có chế độ slideshow tự động

Màn hình E-Ink 3 màu (trắng / đen / đỏ) không cần điện để giữ hình → tiết kiệm năng lượng.

---

## Phần cứng

| Linh kiện | Thông số |
|---|---|
| Vi điều khiển | ESP32-C3 DevKitM-1 |
| Màn hình | 7.5" 3-Color E-Paper (640×384, DEPG0750RHU590F1CP) |
| Driver IC | GxEPD2_750c |
| Giao tiếp màn hình | SPI |
| Cảm biến | SHT30 (nhiệt độ + độ ẩm, I2C) |
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

### Sơ đồ nối chân SHT30

| Chân SHT30 | GPIO ESP32-C3 |
|---|---|
| SDA | 3 |
| SCL | 2 |
| VCC | 3.3V |
| GND | GND |

---

## Hướng dẫn sử dụng

### 1. Cài đặt lần đầu — Kết nối WiFi

Khi thiết bị khởi động lần đầu (hoặc chưa có WiFi đã lưu):

1. Màn hình hiển thị thông báo **"WIFI SETUP"**
2. Trên điện thoại/máy tính, kết nối WiFi tên **`BECUBE-CLOCK`**
3. Trình duyệt sẽ tự mở trang cấu hình (hoặc truy cập `192.168.4.1`)
4. Điền thông tin:
   - **WiFi SSID & mật khẩu** — mạng WiFi nhà bạn
   - **Vĩ độ (Latitude)** — ví dụ: `21.02` (Hà Nội)
   - **Kinh độ (Longitude)** — ví dụ: `105.83` (Hà Nội)
   - **Rotation** — góc xoay màn hình: `0` / `90` / `180` / `270`
5. Nhấn **Save** → thiết bị tự kết nối WiFi và bắt đầu hiển thị

> **Lưu ý:** Portal WiFi tự đóng sau **5 phút** nếu không có thao tác. Nếu không kết nối được WiFi trong thời gian này, thiết bị chuyển sang **chế độ offline** — tự động slideshow nếu có ảnh đã lưu, hoặc hiển thị màn hình đồng hồ (không có dữ liệu thời tiết) nếu chưa có ảnh.

---

### 2. Màn hình đồng hồ & thời tiết

Sau khi kết nối WiFi thành công, màn hình hiển thị:

| Vùng | Nội dung |
|---|---|
| Giờ (đỏ) | HH:MM, đồng bộ NTP tự động |
| Ngày (đỏ) | Thứ, ngày/tháng/năm |
| Thời tiết hiện tại | Icon + nhiệt độ + mô tả |
| Dự báo 5 ngày | Icon + nhiệt độ cao/thấp từng ngày |
| Cảm biến SHT30 | Nhiệt độ & độ ẩm trong nhà (nếu có kết nối) |

**Tự động cập nhật:**
- Đồng hồ: **mỗi 60 giây**
- Thời tiết: **mỗi 1 giờ**
- Cảm biến SHT30: **mỗi 10 giây** (không gây refresh màn hình)

> Màn hình E-Ink mất khoảng **15–50 giây** để vẽ lại hoàn toàn — đây là đặc tính bình thường của công nghệ E-Paper.

---

### 3. Chế độ hiển thị ảnh

Thiết bị hỗ trợ lưu tối đa **5 ảnh** và hiển thị thay cho màn hình đồng hồ.

**Bật/tắt chế độ ảnh:**
- **Nhấn nút 1 lần (< 3 giây)** khi đang ở màn hình đồng hồ → chuyển sang hiển thị ảnh đầu tiên
- **Nhấn nút 1 lần** khi đang xem ảnh → quay về màn hình đồng hồ

**Chuyển ảnh tiếp theo (Double-press):**
- **Nhấn nút 2 lần nhanh** (trong vòng 400ms) → hiển thị ảnh kế tiếp
- Chỉ hoạt động khi đang ở chế độ ảnh và có ≥ 2 ảnh

**Slideshow tự động:**
- Nếu có ≥ 2 ảnh, thiết bị tự động chuyển ảnh mỗi **5 phút**

---

### 4. Tải ảnh lên thiết bị

Có hai cách vào chế độ tải ảnh:

#### Cách 1 — Giữ nút khi cắm điện
1. Giữ nút nhấn → cắm điện vào thiết bị
2. Màn hình hiện **"IMAGE MODE"** kèm địa chỉ IP
3. Kết nối máy tính/điện thoại vào WiFi **`BECUBE-IMG`**
4. Mở trình duyệt, truy cập `192.168.4.1`

#### Cách 2 — Nhấn giữ nút 3–8 giây
1. Nhấn và giữ nút **3–8 giây** khi thiết bị đang chạy bình thường
2. Màn hình hiện **"IMAGE MODE"**
3. Kết nối WiFi **`BECUBE-IMG`** → truy cập `192.168.4.1`

#### Giao diện web tải ảnh

Trang web hiển thị:
- **Chọn ảnh** từ máy tính (JPEG, PNG, v.v.)
- **Preview** ngay trên trình duyệt (đúng tỷ lệ màn hình E-Paper)
- **Chọn slot** lưu (0–4, tối đa 5 ảnh)
- **Nhấn Upload** → ảnh được chuyển đổi và gửi lên thiết bị

> Sau khi upload thành công, thiết bị tự thoát chế độ IMAGE MODE, kết nối lại WiFi và hiển thị ảnh vừa tải.

**Lưu ý khi chọn ảnh:**
- Ảnh sẽ được tự động co/kéo vừa màn hình 640×384 (hoặc 384×640 nếu xoay dọc)
- Màn hình E-Ink 3 màu: ảnh sẽ được chuyển thành **đen / trắng / đỏ** (dithering tự động)
- Khuyên dùng ảnh có độ tương phản cao, ít gradient mịn

---

### 5. Chức năng nút nhấn

| Thao tác | Điều kiện | Kết quả |
|---|---|---|
| **Nhấn nhanh** (< 3s) | Đang xem đồng hồ, có ảnh trong bộ nhớ | Chuyển sang hiển thị ảnh |
| **Nhấn nhanh** (< 3s) | Đang xem đồng hồ, không có ảnh | Cập nhật thời tiết ngay |
| **Nhấn nhanh** (< 3s) | Đang xem ảnh | Quay về đồng hồ |
| **Double-press** (2 lần ≤ 400ms) | Đang xem ảnh, có ≥ 2 ảnh | Chuyển ảnh tiếp theo |
| **Nhấn giữ 3–8s** | Bất kỳ | Vào chế độ IMAGE MODE (tải ảnh) |
| **Nhấn giữ 8–15s** | Bất kỳ | Cập nhật firmware OTA |
| **Nhấn giữ > 15s** | Bất kỳ | Xóa cấu hình WiFi, khởi động lại |
| **5 lần nhấn nhanh** | Đang online | Xóa trắng màn hình + vào deep sleep |
| **Giữ khi cắm điện** | Boot | Vào IMAGE MODE ngay từ đầu |

---

### 6. Cập nhật firmware OTA

Thiết bị hỗ trợ cập nhật firmware qua WiFi (Over The Air):

1. Đảm bảo thiết bị đang kết nối WiFi
2. **Nhấn giữ nút 8–15 giây** → màn hình hiện thông báo OTA
3. Thiết bị tự tải firmware mới về và khởi động lại

> **URL firmware:**
> ```
> https://raw.githubusercontent.com/cuminhbecube/c3-epaer7.5-depg0750rhu590f1cp/main/firmware-ota/firmware.bin
> ```
> Để phát hành firmware mới: build → copy `.pio/build/esp32-c3-devkitm-1/firmware.bin` vào folder `firmware-ota/` → git commit & push.

---

### 7. Đặt lại cấu hình WiFi

Nếu cần đổi mạng WiFi hoặc thay đổi vị trí địa lý:

1. **Nhấn giữ nút > 15 giây** → thiết bị xóa toàn bộ cấu hình WiFi
2. Thiết bị khởi động lại và mở portal cấu hình lại từ đầu
3. Làm theo hướng dẫn ở [Bước 1](#1-cài-đặt-lần-đầu--kết-nối-wifi)

---

## Thông số kỹ thuật

| Thông số | Giá trị |
|---|---|
| Vi điều khiển | ESP32-C3 (RISC-V, 160MHz) |
| Bộ nhớ flash | 4MB |
| Màn hình | 7.5" BWR E-Paper, 640×384 px |
| Màu hiển thị | Trắng / Đen / Đỏ |
| Thời gian refresh màn hình | ~15–50 giây (full refresh) |
| Nguồn điện | 5V qua cổng USB-C |
| WiFi | 2.4GHz 802.11 b/g/n |
| Lưu trữ ảnh | LittleFS, tối đa 5 ảnh (~30KB/ảnh) |
| Dữ liệu thời tiết | Open-Meteo API (miễn phí) |
| Múi giờ mặc định | UTC+7 (Việt Nam) |

---

## Build & Flash (cho developer)

```bash
# Cài PlatformIO
pip install platformio

# Build
pio run

# Upload (thay COM10 bằng cổng thực tế)
pio run --target upload --upload-port COM10

# Upload filesystem (LittleFS)
pio run --target uploadfs --upload-port COM10
```

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
├── firmware-ota/
│   └── firmware.bin         # Firmware binary cho OTA (cập nhật khi release)
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
