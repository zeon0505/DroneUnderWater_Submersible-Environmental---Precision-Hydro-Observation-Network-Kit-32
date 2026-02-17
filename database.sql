-- ═══════════════════════════════════════════════
-- 🌊 SeaPhonk - Underwater Drone Database Schema
-- ═══════════════════════════════════════════════
-- Jalankan di Query phpMyAdmin:
-- 1. Buat Database water_drone_db (jika lokal)
-- 2. Copy semua code ini lalu jalankan
-- ═══════════════════════════════════════════════

CREATE DATABASE IF NOT EXISTS water_drone_db;
USE water_drone_db;

CREATE TABLE IF NOT EXISTS drone_logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    kualitas_air DECIMAL(10, 2),  -- pH Level (skala 0–14)
    tahan DECIMAL(10, 2),         -- Turbidity / Kekeruhan (0–1000 NTU)
    udara DECIMAL(10, 2),         -- Suhu Air / Temperature (°C, dari DS18B20)
    daya_listrik DECIMAL(10, 2)   -- Battery / Daya Listrik (%)
);

-- ═══════════════════════════════════════════════
-- 📌 Mapping Field → Sensor ESP32:
-- ═══════════════════════════════════════════════
-- kualitas_air  → Sensor pH (Potentiometer GPIO 34)
-- tahan         → Sensor Turbidity (Potentiometer GPIO 35)
-- udara         → Sensor Suhu DS18B20 (GPIO 4)
-- daya_listrik  → Simulasi battery level
-- ═══════════════════════════════════════════════