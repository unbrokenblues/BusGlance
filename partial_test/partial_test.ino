/*
  Partial-refresh test for the WeAct 4.2" panel driven as B/W (GDEY042T81).
  One full refresh, then a counter updated by PARTIAL refresh every 3s.
  Watch the panel:
   - counter updates quietly, no full flash  -> PARTIAL WORKS
   - whole screen flashes each update / garbage -> partial not supported
*/
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSans18pt7b.h>

GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT>
  display(GxEPD2_420_GDEY042T81(/*CS=*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));

int n = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  display.init(115200);
  Serial.print("hasFastPartialUpdate = ");
  Serial.println(display.epd2.hasFastPartialUpdate ? "true" : "false");

  // base screen (one full refresh - a flash here is expected)
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSans18pt7b);
    display.setCursor(20, 40);  display.print("PARTIAL REFRESH TEST");
    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(20, 160); display.print("count:");
    display.drawRect(190, 120, 150, 60, GxEPD_BLACK);  // box marks the partial area
  } while (display.nextPage());
  Serial.println("base drawn");
}

void loop() {
  n++;
  // update ONLY the boxed area with a partial refresh
  display.setPartialWindow(191, 121, 148, 58);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(205, 165);
    display.print(n);
  } while (display.nextPage());
  Serial.print("partial update #"); Serial.println(n);
  delay(3000);
}
