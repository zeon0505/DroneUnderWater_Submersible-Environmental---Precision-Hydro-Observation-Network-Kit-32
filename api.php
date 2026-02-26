<?php
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');

// 1. Koneksi Database
$servername = "localhost";
$username = "underwat_seaphonk";
$password = "YPUXnmZjyH8LSamBvcrp";
$dbname = "water_drone_db"; // Sesuai dengan database.sql

$conn = new mysqli($servername, $username, $password, $dbname);

if ($conn->connect_error) {
    die(json_encode(["status" => "error", "message" => "Connection failed"]));
}

// 2. LOGIKA PENERIMAAN DATA (DARI ESP32)
if (isset($_GET['kualitas_air']) && isset($_GET['tahan'])) {
    $ph = $_GET['kualitas_air'];
    $turbidity = $_GET['tahan'];
    $suhu = $_GET['udara'];
    $baterai = $_GET['daya_listrik'];

    $sql = "INSERT INTO drone_logs (kualitas_air, tahan, udara, daya_listrik) 
            VALUES ('$ph', '$turbidity', '$suhu', '$baterai')";

    if ($conn->query($sql) === TRUE) {
        echo json_encode(["status" => "success", "message" => "Data inserted successfully"]);
    } else {
        echo json_encode(["status" => "error", "message" => $conn->error]);
    }
}

// 3. LOGIKA AMBIL DATA TERBARU (UNTUK DASHBOARD)
else if (isset($_GET['get_latest'])) {
    $result = $conn->query("SELECT * FROM drone_logs ORDER BY id DESC LIMIT 1");
    if ($result->num_rows > 0) {
        echo json_encode($result->fetch_assoc());
    } else {
        echo json_encode(["status" => "empty"]);
    }
}

// 4. LOGIKA LAPORAN (UNTUK GRAFIK & PDF)
else if (isset($_GET['get_reports'])) {
    $period = $_GET['period'] ?? 'daily';
    
    // Query untuk Summary
    $summaryQuery = "SELECT 
                        AVG(kualitas_air) as avg_kualitas_air, 
                        MAX(tahan) as max_tahan, 
                        COUNT(*) as total_logs 
                     FROM drone_logs";
    
    // Jika period daily, filter data 24 jam terakhir
    if($period == 'daily') {
        $summaryQuery .= " WHERE timestamp >= NOW() - INTERVAL 1 DAY";
    }

    $summaryRes = $conn->query($summaryQuery);
    $summary = $summaryRes->fetch_assoc();

    // Ambil 10 data terakhir untuk tabel riwayat
    $historyRes = $conn->query("SELECT * FROM drone_logs ORDER BY id DESC LIMIT 10");
    $history = [];
    while($row = $historyRes->fetch_assoc()) {
        $history[] = $row;
    }

    echo json_encode([
        "status" => "success",
        "summary" => $summary,
        "history" => $history
    ]);
}

$conn->close();
?>