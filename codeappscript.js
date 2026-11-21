
const SPREADSHEET_ID = "1aWVSeVVVErQ1QOuIVYgLSpTI9gyDS7GULPCHAfns0Ao"; // ดูจาก URL: docs.google.com/spreadsheets/d/[ID_อยู่ตรงนี้]/edit
const SHEET_NAME = "Sheet1"; // เปลี่ยนเป็นชื่อแท็บชีตด้านล่าง (เช่น Sheet1 หรือ ชีต1)
const CONFIG_SHEET_NAME = 'config'
// 1. รับค่าจาก ESP32 / Arduino (HTTP POST)
function doPost(e) {
  try {
    const ss = SpreadsheetApp.openById(SPREADSHEET_ID);
    const sheet = ss.getSheetByName(SHEET_NAME);
    
    const data = JSON.parse(e.postData.contents);
    if (e && e.parameter && e.parameter.config === "true") {
      return setConfig(data);
    }
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
    if (e && e.parameter && e.parameter.config === "true") {
      return getConfig();
    }

    const ss = SpreadsheetApp.openById(SPREADSHEET_ID);
    const sheet = ss.getSheetByName(SHEET_NAME);
    
    // ดึงข้อมูลทั้งหมดในชีต
    const rows = sheet.getDataRange().getValues();
    
    // หากไม่มีข้อมูล (มีแค่หัวตารางหรือว่างเปล่า)
    if (rows.length <= 1) {
      return createJsonResponse({ status: "success", data: [] });
    }

    // แปลงข้อมูลแถวที่ 2 เป็นต้นไปให้เป็น Array of Objects (ข้ามแถวที่ 1 ที่เป็นหัวตาราง)
    const records = rows.slice(1).map(row => ({
      timestamp: row[0],
      sensor_id: row[1],
      ph: row[2],
      location: row[3]
    }));

    // (ตัวเลือกเสริม) หากส่งพารามิเตอร์ ?latest=true มา จะส่งกลับเฉพาะค่าล่าสุดค่าเดียว
    if (e && e.parameter && e.parameter.latest === "true") {
      const latestRecord = records[records.length - 1];
      return createJsonResponse({
        status: "success",
        data: latestRecord
      });
    }

    // ส่งข้อมูลทั้งหมดกลับไป (จำกัด 50 รายการล่าสุดเพื่อไม่ให้โหลดช้า)
    return createJsonResponse({
      status: "success",
      total: records.length,
      data: records.slice(-50)
    });

  } catch (err) {
    return createJsonResponse({
      status: "error",
      message: err.toString()
    });
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
function setConfig(data){
  const ss = SpreadsheetApp.openById(SPREADSHEET_ID);
  const sheet = ss.getSheetByName(CONFIG_SHEET_NAME);

  const cell1 = sheet.getRange("B1");
  cell1.setValue(data.status);

  const cell2 = sheet.getRange("B2");
  cell2.setValue(data.sensor_id);

  return createJsonResponse({
    status: "success",
    message: "Data saved",
  });
}

function getConfig(){
  try {
    const ss = SpreadsheetApp.openById(SPREADSHEET_ID);
    const sheet = ss.getSheetByName(CONFIG_SHEET_NAME);
    
    // ดึงข้อมูลทั้งหมดในชีต
    const rows = sheet.getDataRange().getValues();
    const configs = {}
    rows.map(row => (configs[row[0]] = row[1]));

    // ส่งข้อมูลทั้งหมดกลับไป (จำกัด 50 รายการล่าสุดเพื่อไม่ให้โหลดช้า)
    return createJsonResponse({
      status: "success",
      data: configs
    });

  } catch (err) {
    return createJsonResponse({
      status: "error",
      message: err.toString()
    });
  }
}
