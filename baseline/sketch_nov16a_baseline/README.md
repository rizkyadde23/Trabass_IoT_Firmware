# Baseline Firmware — Milestone 1

Backup dari `sketch_nov16a/sketch_nov16a.ino` sebelum integrasi dimulai.

- Tanggal backup  : 2026-11-16
- File asli       : `sketch_nov16a/sketch_nov16a.ino`
- File backup     : `baseline/sketch_nov16a_baseline/sketch_nov16a_baseline.ino`
- SHA-256         : `4910CDB3C39B551DEAC675E8E4CBE1A490F5B40F7EA5A4304A1AD8AFE871FE48`
- Ukuran          : 10737 byte

## Tujuan

1. Menjaga titik kembali jika perubahan integrasi merusak fungsi tes existing.
2. Menjadi referensi wiring dan pin saat kode dipecah menjadi modul.
3. Menjadi bahan regresi: menu serial `a`, `1`–`7`, `s`, `h`, dan `AT...` harus tetap berfungsi setelah dipindah ke modul `self_test`.

## Aturan

- **Jangan mengubah file di folder ini.** File ini bersifat read-only secara administratif.
- Perubahan integrasi hanya dilakukan di folder `sketch_nov16a/`.
- Jika perlu restore, salin file backup ke `sketch_nov16a/sketch_nov16a.ino`, lalu verifikasi SHA-256 sebelum upload.
- Setiap milestone baru sebaiknya membuat baseline timestamp baru di folder ini.
