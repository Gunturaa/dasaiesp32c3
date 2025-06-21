# Dasai Mochi ESP32-C3 SuperMini Project

Proyek ini adalah implementasi animasi OLED, jam digital, dan tampilan cuaca berbasis ESP32-C3 SuperMini. Dilengkapi juga dengan tombol fisik dan buzzer untuk alarm dan interaksi. Semua data cuaca diambil dari OpenWeatherMap API.

---

## Fitur Utama 🚀

* 🔁 **Animasi looping bitmap** dari frame SPIFFS
* 🕒 **Jam digital otomatis** tersinkronisasi via NTP
* 🌤️ **Tampilan cuaca real-time** (lokasi: Yogyakarta)
* 🔘 **Tombol** untuk berpindah layar (Clock / Weather)
* 🔊 **Buzzer** sebagai alarm & efek suara
* 📶 **Koneksi WiFi** via webserver konfigurasi
* 🎵 **Melodi sederhana** dengan buzzer saat event khusus
* 🌐 **Webserver** untuk setting WiFi, alarm, dll

---

## Wiring Diagram ⚡

| Komponen       | ESP32-C3 SuperMini Pin     |
| -------------- | -------------------------- |
| OLED 0.96" I2C | SDA = GPIO 8, SCL = GPIO 9 |
| Tombol Jam     | GPIO 2                     |
| Tombol Cuaca   | GPIO 3                     |
| Buzzer         | GPIO 4                     |

> **Catatan:** OLED menggunakan alamat I2C `0x3C`. Pastikan modul OLED sesuai dan mendukung I2C.

---

## Struktur Proyek 🗂️

```
ESP32_DasaiMochi/
├── data/                     # Berisi frame animasi dalam format .bin
│   ├── frame_00000.bin
│   ├── ...
│   └── frame_00773.bin
├── include/
│   └── LocalTimeHelper.h     # Helper NTP untuk waktu lokal
├── src/
│   └── main.cpp              # File utama semua logika dijalankan
├── platformio.ini           # Konfigurasi proyek PlatformIO
└── README.md                # Dokumentasi ini
```

---

## Cara Penggunaan 🛠️

1. **Upload file animasi** ke SPIFFS menggunakan PlatformIO > Data Upload
2. **Upload sketch (main.cpp)** ke ESP32
3. **ESP32 akan membuat AP (Access Point)** bernama `GantengEsp`
4. **Konek ke WiFi tersebut**, lalu buka IP `192.168.4.1` untuk mengatur WiFi rumah
5. Setelah konek internet, ESP akan:

   * Ambil waktu lokal dari NTP
   * Fetch cuaca dari OpenWeatherMap
   * Jalankan animasi secara otomatis
6. Tekan **Tombol 1 (GPIO 2)** untuk jam, **Tombol 2 (GPIO 3)** untuk cuaca

---

## API Endpoint Webserver 🌐

* `POST /save` → Simpan SSID dan password WiFi
* `GET /toggleClock` → Tampilkan/sembunyikan jam
* `GET /toggleWeather` → Tampilkan cuaca
* `POST /setAlarm` → Atur alarm `hour`, `minute`
* `GET /playMusicFrame` → Mainkan frame 773 + melodi
* `GET /status` → JSON info waktu, suhu, IP

---

## API Cuaca

Menggunakan OpenWeatherMap:

* Endpoint: `http://api.openweathermap.org/data/2.5/weather`
* Lokasi: Yogyakarta
* API Key: (disediakan dalam kode `main.cpp`)

---

## License 📄

MIT License. Silakan gunakan, modifikasi, dan bagikan!

---

## Author 👨‍💻

Guntur Tri Atmaja

📱 GitHub: (https://github.com/Gunturaa)

---

> Proyek ini didedikasikan untuk pembelajaran dan eksplorasi IoT dengan ESP32.
> Disklaimer proyek ini masih dalam percobaan (Beta) belum fix masih ingin menambah fitur yang lain
