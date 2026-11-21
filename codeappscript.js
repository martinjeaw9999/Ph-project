
const SPREADSHEET_ID = "1aWVSeVVVErQ1QOuIVYgLSpTI9gyDS7GULPCHAfns0Ao"; // ดูจาก URL: docs.google.com/spreadsheets/d/[ID_อยู่ตรงนี้]/edit
const SHEET_NAME = "Sheet1"; // เปลี่ยนเป็นชื่อแท็บชีตด้านล่าง (เช่น Sheet1 หรือ ชีต1)

// 1. รับค่าจาก ESP32 / Arduino (HTTP POST)
function doPost(e) {
  try {
    const ss = SpreadsheetApp.openById(SPREADSHEET_ID);
    const sheet = ss.getSheetByName(SHEET_NAME);
    
    const data = JSON.parse(e.postData.contents);
    
    // บันทึกลงแถวใหม่: [Timestamp, Sensor ID, ค่า pH]
    sheet.appendRow([
      new Date(),
      data.sensor_id || "unknown",
      data.ph
    ]);

    return createJsonResponse({
      status: "success",
      message: "Data saved",
      received_ph: data.ph
    });

  } catch (err) {
    return createJsonResponse({
      status: "error",
      message: err.toString()
    });
  }
}

// Format a timestamp as Thai-time text
function formatDate(value) {
  if (!value) return "-";
  const d = (value instanceof Date) ? value : new Date(value);
  if (isNaN(d)) return String(value);
  return Utilities.formatDate(d, "Asia/Bangkok", "dd/MM/yyyy HH:mm:ss");
}

// Choose the badge for a pH value (uses the existing CSS: .acid / .neutral / .alkaline)
function getPhBadge(ph) {
  if (isNaN(ph)) return `<span class="badge neutral">ไม่มีข้อมูล</span>`;
  if (ph < 6.5)  return `<span class="badge acid">กรด (Acidic)</span>`;
  if (ph > 8.5)  return `<span class="badge alkaline">ด่าง (Alkaline)</span>`;
  return `<span class="badge neutral">ปกติ (Neutral)</span>`;
}

// 2. ส่งข้อมูลไปแสดงบน Website (HTTP GET)
function doGet(e) {
  try {
    const ss = SpreadsheetApp.openById(SPREADSHEET_ID);
    const sheet = ss.getSheetByName(SHEET_NAME);
    const rows = sheet.getDataRange().getValues();
    // ถ้าเรียกด้วย ?format=json (หรือ ?json=1) ให้ตอบกลับเป็น JSON แทน HTML
    if (isJsonRequest(e)) {
      const limit = Math.max(1, Number((e && e.parameter && e.parameter.limit) || 50));
      const data = rows.slice(1).slice(-limit).reverse().map(row => ({
        timestamp: row[0] instanceof Date ? row[0].toISOString() : row[0],
        time: formatDate(row[0]),
        sensorId: row[1] || null,
        ph: row[2] === "" ? null : Number(row[2])
      }));
      return createJsonResponse({
        status: "success",
        count: data.length,
        latest: data[0] || null,
        data: data
      });
    }
    
    let tableRowsHtml = "";
    let latestPh = "-";
    let latestTime = "ยังไม่มีข้อมูล";
    let statusBadge = `<span class="badge neutral">รอข้อมูล</span>`;

    if (rows.length > 1) {
      // ดึงข้อมูล 50 รายการล่าสุด และกลับด้านให้ข้อมูลใหม่อยู่บนสุด
      const records = rows.slice(1).slice(-50).reverse();
      
      // ค่าล่าสุดสำหรับแสดงในการ์ดด้านบน
      const latest = records[0];
      latestPh = Number(latest[2]).toFixed(2);
      latestTime = formatDate(latest[0]);
      statusBadge = getPhBadge(Number(latest[2]));

      // สร้างแถวในตาราง <tr>
      records.forEach((row, index) => {
        const timeStr = formatDate(row[0]);
        const sensorId = row[1] || "-";
        const phVal = Number(row[2]).toFixed(2);
        const badge = getPhBadge(Number(row[2]));

        tableRowsHtml += `
          <tr>
            <td>${index + 1}</td>
            <td>${timeStr}</td>
            <td><code>${sensorId}</code></td>
            <td class="ph-cell">${phVal}</td>
            <td>${badge}</td>
          </tr>
        `;
      });
    } else {
      tableRowsHtml = `<tr><td colspan="5" style="text-align:center; padding: 24px; color: #64748b;">ยังไม่มีข้อมูลการวัดค่าในระบบ</td></tr>`;
    }

    // สร้างหน้าเว็บ HTML
    const htmlContent = `
      <!DOCTYPE html>
      <html lang="th">
      <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Water pH Monitor Dashboard</title>
        <style>
          * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
          body { background-color: #f1f5f9; color: #1e293b; margin: 0; padding: 20px; }
          .container { max-width: 900px; margin: 0 auto; }
          
          /* Header & Summary Card */
          .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
          .header h1 { font-size: 1.5rem; margin: 0; color: #0f172a; }
          .btn-refresh { background: #2563eb; color: white; border: none; padding: 10px 16px; border-radius: 8px; cursor: pointer; font-weight: 600; text-decoration: none; font-size: 0.9rem; }
          .btn-refresh:hover { background: #1d4ed8; }
          
          .summary-card { background: white; border-radius: 12px; padding: 24px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); margin-bottom: 24px; display: flex; justify-content: space-between; align-items: center; }
          .ph-big { font-size: 3rem; font-weight: 800; color: #0f172a; line-height: 1; margin: 8px 0; }
          .sub-text { color: #64748b; font-size: 0.9rem; }
          
          /* Table */
          .table-wrapper { background: white; border-radius: 12px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); overflow-x: auto; }
          table { width: 100%; border-collapse: collapse; text-align: left; }
          th { background-color: #f8fafc; color: #475569; font-weight: 600; font-size: 0.85rem; text-transform: uppercase; padding: 14px 16px; border-bottom: 1px solid #e2e8f0; }
          td { padding: 14px 16px; border-bottom: 1px solid #f1f5f9; font-size: 0.95rem; }
          tr:hover { background-color: #f8fafc; }
          .ph-cell { font-weight: 700; font-size: 1.05rem; }
          code { background: #f1f5f9; padding: 3px 6px; border-radius: 4px; font-size: 0.85rem; color: #334155; }
          
          /* Status Badges */
          .badge { padding: 4px 10px; border-radius: 999px; font-size: 0.8rem; font-weight: 600; display: inline-block; }
          .acid { background: #fee2e2; color: #b91c1c; }
          .neutral { background: #dcfce7; color: #15803d; }
          .alkaline { background: #e0e7ff; color: #1d4ed8; }
        </style>
      </head>
      <body>
        <div class="container">
          <div class="header">
            <h1>💧 Water pH Monitoring</h1>
            <button class="btn-refresh" onclick="window.top.location.reload();">↻ รีเฟรชข้อมูล</button>
          </div>

          <div class="summary-card">
            <div>
              <div class="sub-text">ค่า pH ล่าสุด</div>
              <div class="ph-big">${latestPh}</div>
              <div class="sub-text">อัปเดตเมื่อ: ${latestTime}</div>
            </div>
            <div>
              ${statusBadge}
            </div>
          </div>

          <div class="table-wrapper">
            <table>
              <thead>
                <tr>
                  <th style="width: 60px;">#</th>
                  <th>วัน-เวลา</th>
                  <th>Sensor ID</th>
                  <th>ค่า pH</th>
                  <th>สถานะน้ำ</th>
                </tr>
              </thead>
              <tbody>
                ${tableRowsHtml}
              </tbody>
            </table>
          </div>
        </div>
      </body>
      </html>
    `;

    return HtmlService.createHtmlOutput(htmlContent)
      .setTitle("Water pH Monitor")
      .setXFrameOptionsMode(HtmlService.XFrameOptionsMode.ALLOWALL);

  } catch (err) {
    if (isJsonRequest(e)) {
      return createJsonResponse({ status: "error", message: err.toString() });
    }
    return HtmlService.createHtmlOutput("<h3>เกิดข้อผิดพลาด:</h3><pre>" + err.toString() + "</pre>");
  }
}
// ฟังก์ชันเสริมสำหรับจัดรูปแบบ JSON Response
function createJsonResponse(payload) {
  return ContentService
    .createTextOutput(JSON.stringify(payload))
    .setMimeType(ContentService.MimeType.JSON);
}

// ตรวจว่า request ขอข้อมูลแบบ JSON หรือไม่ (?format=json หรือ ?json=1)
function isJsonRequest(e) {
  const p = (e && e.parameter) || {};
  return String(p.format || "").toLowerCase() === "json" || p.json === "1" || p.json === "true";
}
