
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <math.h>
// #include "ml_classifier.h"


const char* ssid     = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const char* sheetURL = "SHEET_URL";


WebServer server(80);


int   g_aqi=0;
float g_pm25=0,g_pm10=0,g_pm1=0;
float g_co=0,g_o3=0,g_no2=0,g_so2=0,g_voc=0;
float g_temp=0,g_hum=0,g_pres=0;
String g_cat="N/A";
String g_location="Bosila";
String g_ml_class="N/A";
String g_dominant="PM2.5";
unsigned long lastLog=0,lastReceived=0,total_logs=0;
bool dataReady=false;
String inputBuffer="";


#define MAX_HISTORY 24
float aqi_history[MAX_HISTORY];
unsigned long time_history[MAX_HISTORY];
int history_count=0;


float calcAQI2(float C,float Cl,float Ch,float Il,float Ih){
  if(C<Cl)C=Cl;if(C>Ch)C=Ch;
  return((Ih-Il)/(Ch-Cl))*(C-Cl)+Il;
}
float aqiPM25(float c){
  if(c<=12.0)return calcAQI2(c,0,12,0,50);
  if(c<=35.4)return calcAQI2(c,12.1,35.4,51,100);
  if(c<=55.4)return calcAQI2(c,35.5,55.4,101,150);
  if(c<=150.4)return calcAQI2(c,55.5,150.4,151,200);
  if(c<=250.4)return calcAQI2(c,150.5,250.4,201,300);
  return calcAQI2(c,250.5,500.4,301,500);
}
float aqiPM10(float c){
  if(c<=54)return calcAQI2(c,0,54,0,50);
  if(c<=154)return calcAQI2(c,55,154,51,100);
  if(c<=254)return calcAQI2(c,155,254,101,150);
  if(c<=354)return calcAQI2(c,255,354,151,200);
  if(c<=424)return calcAQI2(c,355,424,201,300);
  return calcAQI2(c,425,604,301,500);
}
float aqiCO(float c){
  if(c<=4.4)return calcAQI2(c,0,4.4,0,50);
  if(c<=9.4)return calcAQI2(c,4.5,9.4,51,100);
  if(c<=12.4)return calcAQI2(c,9.5,12.4,101,150);
  if(c<=15.4)return calcAQI2(c,12.5,15.4,151,200);
  if(c<=30.4)return calcAQI2(c,15.5,30.4,201,300);
  return calcAQI2(c,30.5,50.4,301,500);
}
float aqiO3(float c){
  if(c<=54)return calcAQI2(c,0,54,0,50);
  if(c<=70)return calcAQI2(c,55,70,51,100);
  if(c<=85)return calcAQI2(c,71,85,101,150);
  if(c<=105)return calcAQI2(c,86,105,151,200);
  if(c<=200)return calcAQI2(c,106,200,201,300);
  return calcAQI2(c,201,504,301,500);
}
const char* category(int aqi){
  if(aqi<=50)return "Good";
  if(aqi<=100)return "Moderate";
  if(aqi<=150)return "Unhealthy_SG";
  if(aqi<=200)return "Unhealthy";
  if(aqi<=300)return "V.Unhealthy";
  return "Hazardous";
}
const char* advice(int aqi){
  if(aqi<=50)return "Air is clean. Enjoy outdoor activities.";
  if(aqi<=100)return "Acceptable. Sensitive people take care.";
  if(aqi<=150)return "Sensitive groups should limit outdoor activity.";
  if(aqi<=200)return "Reduce outdoor activity. Wear a mask.";
  if(aqi<=300)return "Avoid outdoors. Use mask if going out.";
  return "Stay indoors. Health emergency condition.";
}
const char* aqiColor(int aqi){
  if(aqi<=50)return "#00b050";
  if(aqi<=100)return "#92d050";
  if(aqi<=150)return "#ffc000";
  if(aqi<=200)return "#ff6600";
  if(aqi<=300)return "#c00000";
  return "#7030a0";
}


void findDominant(){
  float scores[6]={
    aqiPM25(g_pm25),aqiPM10(g_pm10),
    aqiCO(g_co),aqiO3(g_o3),
    calcAQI2(g_no2*532,0,53,0,50),
    calcAQI2(g_so2*381,0,35,0,50)
  };
  String names[6]={"PM2.5","PM10","CO","O3","NO2","SO2"};
  int best=0;
  for(int i=1;i<6;i++) if(scores[i]>scores[best]) best=i;
  g_dominant=names[best];
}


void updateHistory(){
  if(history_count<MAX_HISTORY){
    aqi_history[history_count]=g_aqi;
    time_history[history_count]=millis()/1000;
    history_count++;
  } else {
    for(int i=0;i<MAX_HISTORY-1;i++){
      aqi_history[i]=aqi_history[i+1];
      time_history[i]=time_history[i+1];
    }
    aqi_history[MAX_HISTORY-1]=g_aqi;
    time_history[MAX_HISTORY-1]=millis()/1000;
  }
}


void handleRoot(){
  int aqi=g_aqi;
  String col=aqiColor(aqi);
  String textCol=(aqi<=100)?"#1a1a2e":"#fff";
  findDominant();


  String historyData="[";
  for(int i=0;i<history_count;i++){
    if(i>0)historyData+=",";
    historyData+=String(aqi_history[i],0);
  }
  historyData+="]";


  String timeLabels="[";
  for(int i=0;i<history_count;i++){
    if(i>0)timeLabels+=",";
    unsigned long ago_mins=(millis()/1000-time_history[i])/60;
    timeLabels+="\""+String(ago_mins)+"m\"";
  }
  timeLabels+="]";


  unsigned long ago=dataReady?(millis()-lastReceived)/1000:0;


  String html="<!DOCTYPE html><html><head>";
  html+="<meta charset='utf-8'>";
  html+="<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html+="<meta http-equiv='refresh' content='10'>";
  html+="<title>AQ-Sense Dashboard</title>";
  html+="<script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.9.1/chart.min.js'></script>";
  html+="<style>";
  html+="*{box-sizing:border-box;margin:0;padding:0;}";
  html+="body{font-family:'Segoe UI',Arial,sans-serif;background:#f0f4f8;color:#1a1a2e;}";
  html+=".hdr{background:#f8f9fa;color:#1a1a2e;padding:12px 20px;display:flex;align-items:center;justify-content:space-between;border-bottom:2px solid #e5e7eb;}";
  html+=".hdr-brand{display:flex;align-items:center;gap:10px;}";
  html+=".hdr-logo{width:36px;height:36px;background:"+col+";border-radius:8px;display:flex;align-items:center;justify-content:center;font-weight:800;font-size:13px;color:#fff;}";
  html+=".hdr h1{font-size:18px;font-weight:700;color:#1a1a2e;margin:0;}";
  html+=".hdr p{font-size:11px;color:#6b7280;margin:0;}";
  html+=".hdr-right{font-size:11px;color:#6b7280;text-align:right;}";
  html+=".hdr-right strong{color:#1a1a2e;}";
  html+=".container{max-width:900px;margin:0 auto;padding:16px;}";
  html+=".hero{background:"+col+";color:"+textCol+";border-radius:16px;padding:24px;margin-bottom:16px;display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:16px;box-shadow:0 4px 20px rgba(0,0,0,0.15);}";
  html+=".eyebrow{font-size:11px;text-transform:uppercase;letter-spacing:2px;opacity:0.8;}";
  html+=".aqi-big{font-size:88px;font-weight:900;line-height:1;letter-spacing:-2px;}";
  html+=".aqi-cat{font-size:20px;font-weight:600;margin-top:4px;}";
  html+=".aqi-advice{font-size:13px;opacity:0.9;margin-top:8px;max-width:260px;line-height:1.4;}";
  html+=".gauge-label{font-size:11px;opacity:0.8;margin-top:6px;text-align:center;}";
  html+=".grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:16px;}";
  html+=".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;margin-bottom:16px;}";
  html+=".card{background:#fff;border-radius:12px;padding:16px;box-shadow:0 1px 4px rgba(0,0,0,0.08);}";
  html+=".card-title{font-size:11px;font-weight:600;color:#6b7280;text-transform:uppercase;letter-spacing:0.8px;margin-bottom:12px;}";
  html+=".pollutant-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;}";
  html+=".poll-item{background:#f8fafc;border-radius:8px;padding:10px 12px;border-left:3px solid #e2e8f0;}";
  html+=".poll-item.hi{border-left-color:#ef4444;}";
  html+=".poll-item.md{border-left-color:#f59e0b;}";
  html+=".poll-item.lo{border-left-color:#10b981;}";
  html+=".poll-name{font-size:10px;color:#6b7280;font-weight:600;text-transform:uppercase;}";
  html+=".poll-val{font-size:20px;font-weight:700;color:#1a1a2e;margin:2px 0;}";
  html+=".poll-unit{font-size:10px;color:#9ca3af;}";
  html+=".poll-note{font-size:9px;color:#f59e0b;font-style:italic;margin-top:2px;}";
  html+=".poll-sub{font-size:10px;color:#6b7280;margin-top:2px;}";
  html+=".chart-wrap{position:relative;height:160px;margin-top:8px;}";
  html+=".ml-result{text-align:center;padding:8px;}";
  html+=".ml-class{font-size:22px;font-weight:700;color:#1a1a2e;margin:8px 0;}";
  html+=".ml-badge{display:inline-block;padding:4px 12px;border-radius:20px;font-size:11px;font-weight:600;color:#fff;margin-top:4px;}";
  html+=".env-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:4px;}";
  html+=".env-val{font-size:28px;font-weight:700;color:#1a1a2e;}";
  html+=".env-lbl{font-size:11px;color:#6b7280;margin-top:2px;}";
  html+=".status-item{display:flex;align-items:center;justify-content:space-between;padding:7px 0;border-bottom:1px solid #f1f5f9;}";
  html+=".status-item:last-child{border-bottom:none;}";
  html+=".status-label{font-size:12px;color:#6b7280;}";
  html+=".status-val{font-size:12px;font-weight:600;color:#1a1a2e;}";
  html+=".dot{width:8px;height:8px;border-radius:50%;display:inline-block;margin-right:4px;}";
  html+=".dot-green{background:#10b981;}";
  html+=".footer{text-align:center;font-size:11px;color:#9ca3af;padding:16px 0;margin-top:8px;line-height:1.8;}";
  html+="@media(max-width:600px){.grid,.grid-3{grid-template-columns:1fr;}.aqi-big{font-size:64px;}}";
  html+="</style></head><body>";


  html+="<div class='hdr'>";
  html+="<div class='hdr-brand'>";
  html+="<div class='hdr-logo'>💨</div>";
  html+="<div><h1>AQ-Sense</h1><p>Bangladesh Air Quality Monitor &middot; DoE AQI</p></div></div>";
  html+="<div class='hdr-right'><strong>"+g_location+"</strong><br>Updated "+String(ago)+"s ago</div>";
  html+="</div>";


  html+="<div class='container'>";


  int gAqi=min(aqi,300);
  float ang=180.0-(gAqi/300.0*180.0);
  float rad=ang*3.14159/180.0;
  int nx=80+(int)(60*cos(rad));
  int ny=80-(int)(60*sin(rad));


  html+="<div class='hero'>";
  html+="<div>";
  html+="<div class='eyebrow'>Current Air Quality Index</div>";
  html+="<div class='aqi-big'>"+String(aqi)+"</div>";
  html+="<div class='aqi-cat'>"+String(category(aqi))+"</div>";
  html+="<div class='aqi-advice'>"+String(advice(aqi))+"</div>";
  html+="</div>";
  html+="<div style='text-align:center'>";
  html+="<svg width='160' height='90' viewBox='0 0 160 90'>";
  html+="<path d='M10,80 A70,70 0 0,1 40,17' stroke='#00b050' stroke-width='14' fill='none' opacity='.7'/>";
  html+="<path d='M40,17 A70,70 0 0,1 80,10' stroke='#92d050' stroke-width='14' fill='none' opacity='.7'/>";
  html+="<path d='M80,10 A70,70 0 0,1 120,17' stroke='#ffc000' stroke-width='14' fill='none' opacity='.7'/>";
  html+="<path d='M120,17 A70,70 0 0,1 150,80' stroke='#ff6600' stroke-width='14' fill='none' opacity='.7'/>";
  html+="<line x1='80' y1='80' x2='"+String(nx)+"' y2='"+String(ny)+"' stroke='"+textCol+"' stroke-width='3' stroke-linecap='round'/>";
  html+="<circle cx='80' cy='80' r='7' fill='"+textCol+"'/>";
  html+="<text x='4' y='88' font-size='9' fill='"+textCol+"' opacity='.7'>0</text>";
  html+="<text x='65' y='7' font-size='9' fill='"+textCol+"' opacity='.7'>150</text>";
  html+="<text x='135' y='88' font-size='9' fill='"+textCol+"' opacity='.7'>300</text>";
  html+="</svg>";
  html+="<div class='gauge-label'>Dominant: <strong>"+g_dominant+"</strong></div>";
  html+="</div></div>";


  html+="<div class='card' style='margin-bottom:16px'>";
  html+="<div class='card-title'>Individual Pollutant Readings</div>";
  html+="<div class='pollutant-grid'>";


  String p25c=(g_pm25>55)?"hi":(g_pm25>12)?"md":"lo";
  html+="<div class='poll-item "+p25c+"'><div class='poll-name'>PM2.5</div><div class='poll-val'>"+String(g_pm25,0)+"</div><div class='poll-unit'>µg/m³</div><div class='poll-sub'>AQI sub: "+String((int)aqiPM25(g_pm25))+"</div></div>";


  String p10c=(g_pm10>155)?"hi":(g_pm10>55)?"md":"lo";
  html+="<div class='poll-item "+p10c+"'><div class='poll-name'>PM10</div><div class='poll-val'>"+String(g_pm10,0)+"</div><div class='poll-unit'>µg/m³</div><div class='poll-sub'>AQI sub: "+String((int)aqiPM10(g_pm10))+"</div></div>";


  html+="<div class='poll-item lo'><div class='poll-name'>PM1.0</div><div class='poll-val'>"+String(g_pm1,0)+"</div><div class='poll-unit'>µg/m³</div><div class='poll-sub'>Fine particles</div></div>";


  html+="<div class='poll-item'><div class='poll-name'>CO</div><div class='poll-val'>"+String(g_co,2)+"</div><div class='poll-unit'>V (raw)</div><div class='poll-note'>indicative</div></div>";


  html+="<div class='poll-item'><div class='poll-name'>O₃ (Ozone)</div><div class='poll-val'>"+String(g_o3,2)+"</div><div class='poll-unit'>V (raw)</div><div class='poll-note'>indicative</div></div>";


  html+="<div class='poll-item'><div class='poll-name'>NO₂</div><div class='poll-val'>"+String(g_no2,2)+"</div><div class='poll-unit'>V (raw)</div><div class='poll-note'>qualitative</div></div>";


  html+="<div class='poll-item'><div class='poll-name'>SO₂</div><div class='poll-val'>"+String(g_so2,2)+"</div><div class='poll-unit'>V (raw)</div><div class='poll-note'>qualitative</div></div>";


  html+="<div class='poll-item'><div class='poll-name'>VOC Index</div><div class='poll-val'>"+String(g_voc,2)+"</div><div class='poll-unit'>V (composite)</div><div class='poll-note'>MQ-135</div></div>";


  html+="</div></div>";


  html+="<div class='grid'>";
  html+="<div class='card'>";
  html+="<div class='card-title'>AQI Trend (Last "+String(history_count)+" readings)</div>";
  html+="<div class='chart-wrap'><canvas id='aqiChart'></canvas></div>";
  html+="</div>";


  html+="<div class='card'>";
  html+="<div class='card-title'>ML Classification Result</div>";
  html+="<div class='ml-result'>";
  html+="<div style='font-size:11px;color:#6b7280'>Predicted Air Quality Class</div>";
  html+="<div class='ml-class'>"+g_ml_class+"</div>";
  html+="<span class='ml-badge' style='background:"+col+"'>Decision Tree Model</span>";
  html+="<div style='font-size:11px;color:#6b7280;margin-top:12px'>Trained on Bangladesh AQI Dataset<br>2000-2025 &middot; 1,047,855 rows</div>";
  html+="</div></div>";
  html+="</div>";


  html+="<div class='grid-3'>";
  html+="<div class='card'>";
  html+="<div class='card-title'>AQI Categories</div>";
  struct { const char* range; const char* name; const char* color; int lo; int hi; } cats[]={
    {"0-50","Good","#00b050",0,50},
    {"51-100","Moderate","#92d050",51,100},
    {"101-150","Unhealthy SG","#ffc000",101,150},
    {"151-200","Unhealthy","#ff6600",151,200},
    {"201-300","Very Unhealthy","#c00000",201,300},
    {"301-500","Hazardous","#7030a0",301,500}
  };
  for(int i=0;i<6;i++){
    bool active=(aqi>=cats[i].lo&&aqi<=cats[i].hi);
    html+="<div style='display:flex;align-items:center;justify-content:space-between;padding:5px 8px;border-radius:6px;margin-bottom:3px;";
    if(active)html+="background:"+String(cats[i].color)+";color:#fff;font-weight:700;";
    else html+="background:#f8fafc;";
    html+="'><span style='font-size:11px'>"+String(cats[i].range)+"</span>";
    html+="<span style='font-size:11px;";
    if(!active)html+="color:"+String(cats[i].color)+";font-weight:600;";
    html+="'>"+String(cats[i].name)+"</span></div>";
  }
  html+="</div>";


  html+="<div class='card'>";
  html+="<div class='card-title'>Environment</div>";
  html+="<div class='env-grid'>";
  html+="<div><div class='env-val'>"+String(g_temp,1)+"°</div><div class='env-lbl'>Temperature (C)</div></div>";
  html+="<div><div class='env-val'>"+String(g_hum,0)+"%</div><div class='env-lbl'>Humidity (%RH)</div></div>";
  html+="</div>";
  html+="<div style='margin-top:12px;padding:10px;background:#f8fafc;border-radius:8px'>";
  html+="<div style='font-size:11px;color:#6b7280'>Atmospheric Pressure</div>";
  html+="<div style='font-size:22px;font-weight:700'>"+String(g_pres,0)+" <span style='font-size:12px;color:#9ca3af'>hPa</span></div>";
  html+="</div></div>";


  html+="<div class='card'>";
  html+="<div class='card-title'>Data Logging Status</div>";
  html+="<div class='status-item'><span class='status-label'>Google Sheets</span><span class='status-val'><span class='dot dot-green'></span>Connected</span></div>";
  html+="<div class='status-item'><span class='status-label'>Last Upload</span><span class='status-val'>"+String(ago)+"s ago</span></div>";
  html+="<div class='status-item'><span class='status-label'>Records Logged</span><span class='status-val'>"+String(total_logs)+"</span></div>";
  html+="<div class='status-item'><span class='status-label'>Location</span><span class='status-val'>"+g_location+"</span></div>";
  html+="<div class='status-item'><span class='status-label'>Sensor Board</span><span class='status-val'><span class='dot dot-green'></span>"+String(dataReady?"Online":"Waiting")+"</span></div>";
  html+="</div>";
  html+="</div>";


  html+="<div class='footer'>";
  html+="PM2.5/PM10 factory-calibrated (PMS5003). Gas sensors report raw voltage — indicative only.<br>";
  html+="AQI per <strong>Bangladesh DoE</strong> standards (USEPA breakpoints, NAQMP 2024-2030).<br>";
  html+="AQ-Sense &copy; 2026 &middot; Dept. of CSE, University of Liberal Arts Bangladesh";
  html+="</div>";
  html+="</div>";


  html+="<script>";
  html+="const ctx=document.getElementById('aqiChart').getContext('2d');";
  html+="new Chart(ctx,{type:'line',data:{labels:"+timeLabels+",datasets:[{";
  html+="label:'AQI',data:"+historyData+",borderColor:'"+col+"',";
  html+="backgroundColor:'"+col+"22',borderWidth:2,fill:true,tension:0.4,";
  html+="pointRadius:3,pointBackgroundColor:'"+col+"'}]},";
  html+="options:{responsive:true,maintainAspectRatio:false,";
  html+="plugins:{legend:{display:false}},";
  html+="scales:{y:{min:0,max:300,grid:{color:'#f1f5f9'},ticks:{font:{size:10}}},";
  html+="x:{grid:{display:false},ticks:{font:{size:9},maxRotation:0}}}}});";
  html+="</script>";
  html+="</body></html>";
  server.send(200,"text/html",html);
}


void handleData(){
  String j="{\"aqi\":"+String(g_aqi)+",\"cat\":\""+g_cat+"\",";
  j+="\"pm25\":"+String(g_pm25,0)+",\"pm10\":"+String(g_pm10,0)+",";
  j+="\"temp\":"+String(g_temp,1)+",\"hum\":"+String(g_hum,0)+",";
  j+="\"ml\":\""+g_ml_class+"\",\"dominant\":\""+g_dominant+"\"";
  j+="}";
  server.send(200,"application/json",j);
}


void logToSheet(){
  if(!dataReady) return;


  // Reconnect WiFi if disconnected
  if(WiFi.status()!=WL_CONNECTED){
    Serial.println("WiFi reconnecting...");
    WiFi.reconnect();
    unsigned long t=millis();
    while(WiFi.status()!=WL_CONNECTED&&millis()-t<10000){
      delay(300);
    }
  }


  if(WiFi.status()!=WL_CONNECTED){
    Serial.println("WiFi failed, skip log");
    return;
  }


  String cat=g_cat;      cat.replace(" ","%20");
  String loc=g_location; loc.replace(" ","%20");
  String ml=g_ml_class;  ml.replace(" ","%20");


  String url=String(sheetURL);
  url+="?aqi="+String(g_aqi);
  url+="&cat="+cat;
  url+="&pm25="+String(g_pm25,0);
  url+="&pm10="+String(g_pm10,0);
  url+="&pm1="+String(g_pm1,0);
  url+="&co="+String(g_co,2);
  url+="&o3="+String(g_o3,2);
  url+="&no2="+String(g_no2,2);
  url+="&so2="+String(g_so2,2);
  url+="&voc="+String(g_voc,2);
  url+="&temp="+String(g_temp,1);
  url+="&hum="+String(g_hum,0);
  url+="&pres="+String(g_pres,0);
  url+="&loc="+loc;
  url+="&ml="+ml;


  Serial.println("Logging...");
  Serial.println(url);


  HTTPClient http;
  http.begin(url);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  int code=http.GET();
  if(code>0){
    total_logs++;
    Serial.print("Sheet: HTTP "); Serial.println(code);
  } else {
    Serial.print("Sheet error: "); Serial.println(code);
  }
  http.end();
}


void parseData(String line){
  while(line.length()>0&&(line[0]<'0'||line[0]>'9'))
    line=line.substring(1);


  int idx=0;
  String f[13];
  for(int i=0;i<13;i++){
    int s=line.indexOf('|',idx);
    if(s==-1)s=line.length();
    f[i]=line.substring(idx,s);
    f[i].trim();
    idx=s+1;
  }


  int   v_aqi  = f[0].toInt();
  float v_pm25 = f[2].toFloat();
  float v_pm10 = f[3].toFloat();
  float v_pm1  = f[4].toFloat();
  float v_co   = f[5].toFloat();
  float v_o3   = f[6].toFloat();
  float v_no2  = f[7].toFloat();
  float v_so2  = f[8].toFloat();
  float v_voc  = f[9].toFloat();
  float v_temp = f[10].toFloat();
  float v_hum  = f[11].toFloat();
  float v_pres = f[12].toFloat();


  // Minimal validation
 


  g_pm25 = v_pm25;
  g_pm10 = v_pm10;
  g_pm1  = v_pm1;
  g_co   = v_co;
  g_o3   = v_o3;
  g_no2  = v_no2;
  g_so2  = v_so2;
  g_voc  = v_voc;
  g_temp = v_temp;
  g_hum  = v_hum;
  g_pres = v_pres;


  // Trust board 1 AQI, recalculate category cleanly
  g_aqi = v_aqi;
  g_cat = String(category(g_aqi));


  dataReady=true;
  lastReceived=millis();
  updateHistory();
  findDominant();
  // g_ml_class=String(mlClassify(g_pm25,g_pm10,g_co,g_no2,g_so2,g_o3));


  Serial.print("VALID AQI:"); Serial.print(g_aqi);
  Serial.print(" PM25:"); Serial.print(g_pm25);
  Serial.print(" PM10:"); Serial.print(g_pm10);
  Serial.print(" Cat:"); Serial.println(g_cat);
}


void setup(){
  Serial.begin(115200);
  Serial.println("=== BOARD 2 ===");


  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  Serial.print("WiFi");
  unsigned long t=millis();
  while(WiFi.status()!=WL_CONNECTED&&millis()-t<15000){delay(300);Serial.print(".");}
  if(WiFi.status()==WL_CONNECTED){
    Serial.println();
    Serial.println("============================");
    Serial.print("  http://"); Serial.println(WiFi.localIP());
    Serial.println("============================");
  } else {
    Serial.println("\nWiFi failed");
  }


  server.on("/",handleRoot);
  server.on("/data",handleData);
  server.begin();


  delay(2000);
  Serial2.begin(9600,SERIAL_8N1,16,-1);
  Serial.println("Ready.");
}


void loop(){
  server.handleClient();
  while(Serial2.available()){
    char c=Serial2.read();
    if(c=='\n'){
      inputBuffer.trim();
      if(inputBuffer.length()>5&&inputBuffer[0]>='0'&&inputBuffer[0]<='9'){
        parseData(inputBuffer);
      }
      inputBuffer="";
    } else if(c>=32&&c<127){
      inputBuffer+=c;
    }
  }
  if(dataReady&&millis()-lastLog>60000){
    logToSheet();
    lastLog=millis();
  }
}

