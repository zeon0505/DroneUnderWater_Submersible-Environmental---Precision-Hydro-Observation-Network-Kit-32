-- ============================================================
-- DATABASE UNDERWATER DRONE MONITORING SYSTEM
-- Sensor: pH + Turbidity
-- Jalankan script ini di phpMyAdmin
-- ============================================================

-- LANGKAH 1: Gunakan database hosting kamu
CREATE DATABASE IF NOT EXISTS underwat_daffa_water;
USE underwat_daffa_water;

-- LANGKAH 2: Buat tabel log sensor
-- JIKA TABEL SUDAH ADA SEBELUMNYA (dengan kolom udara),
-- jalankan perintah ALTER di bawah ini untuk menghapus kolom udara:
-- ALTER TABLE drone_logs DROP COLUMN udara;

CREATE TABLE IF NOT EXISTS drone_logs (
    id           INT AUTO_INCREMENT PRIMARY KEY,
    timestamp    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    kualitas_air DECIMAL(10, 2) COMMENT 'Nilai pH air (contoh: 7.20)',
    tahan        DECIMAL(10, 2) COMMENT 'Turbidity / Kekeruhan air dalam NTU',
    daya_listrik DECIMAL(10, 2) COMMENT 'Level baterai drone dalam persen (%)'
);

-- LANGKAH 3: (Opsional) Data dummy untuk testing awal
INSERT INTO drone_logs (kualitas_air, tahan, daya_listrik) VALUES
(7.20, 312.50, 100.00),
(7.15, 318.00,  98.00),
(7.30, 305.25,  95.00),
(6.95, 325.75,  92.00),
(7.10, 298.00,  89.00);