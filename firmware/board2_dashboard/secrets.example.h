// =====================================================================
// AQ-Sense — Board 2 secrets template
// Copy this file to `secrets.h` in the same folder and fill in the real
// values. `secrets.h` is gitignored and must NEVER be committed.
// =====================================================================

#ifndef SECRETS_H
#define SECRETS_H

// Your WiFi network
#define WIFI_SSID     "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

// Google Apps Script deployment URL for the Sheets logger.
// Deploy your Apps Script as a Web App ("Anyone" access) and paste the
// /exec URL here. Treat this URL as a secret — anyone with it can POST
// to your sheet.
#define SHEET_URL     "https://script.google.com/macros/s/XXXXXXXXXXXXXX/exec"

#endif