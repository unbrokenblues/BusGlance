/*
  Battery-sense pin finder for BusGlance.
  Reads every ADC1 pin and shows the voltage ON THE E-PAPER (serial is unreadable
  through this dongle). We're hunting for a pin that sits at ~half the battery
  voltage (~1.9-2.1 V) - that's the board's built-in VBAT divider, if it exists.
  A real VBAT pin will also CHANGE a little when you plug/unplug USB.
  Refreshes every ~20 s. Flash bus_display back when done.
*/
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT>
  display(GxEPD2_420_GDEY042T81(/*CS=*/5, /*DC=*/17, /*RST=*/16, /*BUSY=*/4));

const int  pins[]  = {32, 33, 34, 35, 36, 39};
const char* names[] = {"32", "33", "34", "35", "36/VP", "39/VN"};

uint32_t readAvg(int pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 32; i++) sum += analogReadMilliVolts(pin);
  return sum / 32;
}

void draw() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(8, 26);  display.print("BATTERY-SENSE SCAN");
    display.setFont(&FreeSans9pt7b);
    display.setCursor(8, 48);  display.print("Find a pin near ~2000mV (=half battery).");
    display.setCursor(8, 66);  display.print("It should shift when USB is un/plugged.");
    int y = 96;
    for (int i = 0; i < 6; i++) {
      uint32_t mv = readAvg(pins[i]);
      char buf[56];
      snprintf(buf, sizeof(buf), "GPIO %-6s  %4u mV   (x2 = %.2f V)", names[i], mv, mv * 2 / 1000.0);
      display.setFont(&FreeSans9pt7b);
      display.setCursor(8, y);
      display.print(buf);
      y += 30;
    }
  } while (display.nextPage());
}

void setup() {
  analogReadResolution(12);
  display.init();
  display.setRotation(0);
  draw();
}

void loop() {
  delay(20000);
  draw();
}
