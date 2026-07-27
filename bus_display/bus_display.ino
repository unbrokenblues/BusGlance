/*
  BusGlance - Home Bus Arrival Display
  ESP32 Lite V1.0.0 + WeAct 4.2" E-Paper (400x300)

  WHAT THIS DOES:
  - Connects to WiFi
  - Every 60 seconds (5am - midnight only), fetches live bus arrival
    times from LTA DataMall for two bus stops
  - Draws them on the e-paper screen in black & white, using the panel's
    fast ~1.2s refresh (your stop on top, opposite stop below)
  - Outside 5am-midnight, it sleeps with WiFi off to save battery

  ============ BEFORE YOU UPLOAD ============
  1. Fill in your WiFi name/password below
  2. Your LTA API key is already filled in
  3. Fill in which bus SERVICE NUMBERS you actually want shown
     (e.g. "13", "52", "88") - up to 3 per stop
  4. Check the display driver line (search "DISPLAY DRIVER" below) -
     if the screen shows garbage/nothing, this is the first thing to
     change - tell me and I'll give you the right line.
  ============================================
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

// Private credentials live in secrets.h (git-ignored). Copy
// secrets.h.example to secrets.h and fill in your own values.
#include "secrets.h"

// ---------------- USER SETTINGS ----------------

const char* WIFI_SSID     = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;

const char* LTA_API_KEY = SECRET_LTA_API_KEY;

// Your two bus stops
const char* STOP_CODE_HOME     = "59399"; // your stop
const char* STOP_CODE_OPPOSITE = "59391"; // opposite stop

const char* STOP_LABEL_HOME     = "Home";
const char* STOP_LABEL_OPPOSITE = "Opposite";

// Which bus services to show, and in what order.
const char* SERVICES_HOME[4]     = {"807", "860", "806", "663"};
const int   NUM_SERVICES_HOME    = 4;

const char* SERVICES_OPPOSITE[2] = {"804", "807"};
const int   NUM_SERVICES_OPPOSITE = 2;

// Weather (Open-Meteo, free, no API key). Set to your area's coordinates.
const float LATITUDE  = 1.4299;    // your area (northern Singapore)
const float LONGITUDE = 103.8463;
const int   FORECAST_AHEAD_HOURS = 3;  // the "later" slot = now + this many hours

// Refresh interval while awake (seconds). Black & white fast refresh is
// ~1.2s with no 3-minute limit, so we can update often. 60s is a good
// balance of freshness vs battery (30s = fresher but ~half the runtime).
const int REFRESH_SECONDS = 60;

// How often to do a clean FULL (flashing) refresh vs a quiet PARTIAL one.
//   1  => full refresh EVERY cycle: crisp image, but a black flash each time.
//   15 => partial most cycles (no flash) + a full flash ~every 15 min.
// This 3-color panel driven as B/W renders partial refreshes as grey, ghosted
// mush, so we keep it at 1 (full every time). A TRUE B/W GDEY042T81 panel does
// partial cleanly + fast (~0.4s) - swap the panel, then set this to 15.
const int FULL_REFRESH_EVERY = 1;

// A bus this many minutes away (or less) gets the "arriving soon" black tag.
const int SOON_MINUTES = 4;

// DEBUG: while true, the board NEVER sleeps overnight - it keeps refreshing
// 24/7 so we can test past midnight. Set back to false before real use so it
// saves battery by sleeping 00:00-05:00.
const bool DEBUG_NO_NIGHT_SLEEP = false;

// Active hours (24h format) - outside this window the whole board sleeps.
// SG buses run ~5am to midnight, so we sleep 00:00-05:00 to save battery.
const int ACTIVE_START_HOUR = 5;
const int ACTIVE_START_MIN  = 0;
const int ACTIVE_END_HOUR   = 23;
const int ACTIVE_END_MIN    = 59;

// ---- Battery monitor ----
// Only works if your board wires the LiPo to an analog pin (many cheap boards
// do NOT). It's fail-safe: if the reading looks implausible, it stays silent
// instead of false-alarming. On first boot, check the Serial log to confirm
// the voltage is realistic. If your board can't sense it, set this to false.
const bool  BATTERY_MONITOR = true;
const int   BATTERY_ADC_PIN = 35;     // common VBAT-sense pin - CONFIRM for your board
const float BATTERY_DIVIDER = 2.0;    // board halves VBAT before the pin, so x2 to undo
const float LOW_BATT_VOLTS  = 3.45;   // warn below this (~15-20% left on a LiPo)

// Phone push for low battery via ntfy.sh (free, no account). Install the ntfy
// app, subscribe to YOUR topic below, and alerts arrive on your phone. Change
// this to something private and hard to guess.
const char* NTFY_TOPIC = SECRET_NTFY_TOPIC;

// 365 uplifting quotes - one for each day of the year, picked by date.
// A mix of scripture (public-domain KJV phrasing), affirmations,
// proverbs, and classic motivation. All short + ASCII so they fit.
// Add or edit freely; NUM_QUOTES updates itself.
const char* QUOTES[] = {
  "The Lord is my shepherd; I shall not want. - Psalm 23:1",
  "I can do all things through Christ who strengthens me. - Philippians 4:13",
  "Trust in the Lord with all your heart. - Proverbs 3:5",
  "The Lord is my light and my salvation; whom shall I fear? - Psalm 27:1",
  "Be strong and of a good courage. - Joshua 1:9",
  "Weeping may endure for a night, but joy cometh in the morning. - Psalm 30:5",
  "This is the day which the Lord hath made. - Psalm 118:24",
  "The Lord bless thee, and keep thee. - Numbers 6:24",
  "Let not your heart be troubled. - John 14:1",
  "Cast all your anxiety on him, for he cares for you. - 1 Peter 5:7",
  "Rejoice in the Lord always. - Philippians 4:4",
  "God is our refuge and strength. - Psalm 46:1",
  "Be still, and know that I am God. - Psalm 46:10",
  "Delight thyself also in the Lord. - Psalm 37:4",
  "Commit thy way unto the Lord. - Psalm 37:5",
  "The joy of the Lord is your strength. - Nehemiah 8:10",
  "Let all that you do be done in love. - 1 Corinthians 16:14",
  "Faith is the substance of things hoped for. - Hebrews 11:1",
  "And we know that all things work together for good. - Romans 8:28",
  "In everything give thanks. - 1 Thessalonians 5:18",
  "Pray without ceasing. - 1 Thessalonians 5:17",
  "Love is patient, love is kind. - 1 Corinthians 13:4",
  "Love never fails. - 1 Corinthians 13:8",
  "The Lord is good to all. - Psalm 145:9",
  "Wait on the Lord: be of good courage. - Psalm 27:14",
  "They that wait upon the Lord shall renew their strength. - Isaiah 40:31",
  "Fear not, for I am with thee. - Isaiah 41:10",
  "I have loved thee with an everlasting love. - Jeremiah 31:3",
  "For I know the thoughts that I think toward you. - Jeremiah 29:11",
  "Ask, and it shall be given you. - Matthew 7:7",
  "Come unto me, all ye that labour. - Matthew 11:28",
  "Let your light so shine before men. - Matthew 5:16",
  "Blessed are the pure in heart. - Matthew 5:8",
  "Blessed are the peacemakers. - Matthew 5:9",
  "Blessed are the merciful. - Matthew 5:7",
  "With God all things are possible. - Matthew 19:26",
  "Give, and it shall be given unto you. - Luke 6:38",
  "Peace I leave with you, my peace I give unto you. - John 14:27",
  "I am the light of the world. - John 8:12",
  "Ye shall know the truth, and the truth shall make you free. - John 8:32",
  "If God be for us, who can be against us? - Romans 8:31",
  "Be not overcome of evil, but overcome evil with good. - Romans 12:21",
  "Rejoice with them that do rejoice. - Romans 12:15",
  "The fruit of the Spirit is love, joy, peace. - Galatians 5:22",
  "Let us not be weary in well doing. - Galatians 6:9",
  "Be ye kind one to another. - Ephesians 4:32",
  "My God shall supply all your need. - Philippians 4:19",
  "Let the peace of God rule in your hearts. - Colossians 3:15",
  "Whatsoever ye do, do it heartily, as to the Lord. - Colossians 3:23",
  "God hath not given us the spirit of fear. - 2 Timothy 1:7",
  "Let us hold fast the profession of our faith. - Hebrews 10:23",
  "Jesus Christ the same yesterday, and to day, and for ever. - Hebrews 13:8",
  "Draw nigh to God, and he will draw nigh to you. - James 4:8",
  "Every good gift and every perfect gift is from above. - James 1:17",
  "We love him, because he first loved us. - 1 John 4:19",
  "Perfect love casteth out fear. - 1 John 4:18",
  "Thy word is a lamp unto my feet. - Psalm 119:105",
  "The Lord shall preserve thy going out and thy coming in. - Psalm 121:8",
  "I will lift up mine eyes unto the hills. - Psalm 121:1",
  "My help cometh from the Lord. - Psalm 121:2",
  "Bless the Lord, O my soul. - Psalm 103:1",
  "The Lord is merciful and gracious. - Psalm 103:8",
  "O give thanks unto the Lord; for he is good. - Psalm 107:1",
  "The Lord is nigh unto all them that call upon him. - Psalm 145:18",
  "He healeth the broken in heart. - Psalm 147:3",
  "A soft answer turneth away wrath. - Proverbs 15:1",
  "A merry heart doeth good like a medicine. - Proverbs 17:22",
  "In all thy ways acknowledge him. - Proverbs 3:6",
  "Trust in the Lord, and do good. - Psalm 37:3",
  "The name of the Lord is a strong tower. - Proverbs 18:10",
  "Iron sharpeneth iron. - Proverbs 27:17",
  "Train up a child in the way he should go. - Proverbs 22:6",
  "A good name is rather to be chosen than great riches. - Proverbs 22:1",
  "To every thing there is a season. - Ecclesiastes 3:1",
  "Two are better than one. - Ecclesiastes 4:9",
  "The Lord will fight for you. - Exodus 14:14",
  "Be strong and courageous. Do not be afraid. - Deuteronomy 31:6",
  "The eternal God is thy refuge. - Deuteronomy 33:27",
  "The Lord is my rock, and my fortress. - Psalm 18:2",
  "Thou wilt keep him in perfect peace. - Isaiah 26:3",
  "He giveth power to the faint. - Isaiah 40:29",
  "The Lord is my strength and my song. - Psalm 118:14",
  "Let everything that hath breath praise the Lord. - Psalm 150:6",
  "His compassions fail not; they are new every morning. - Lamentations 3:22",
  "Great is thy faithfulness. - Lamentations 3:23",
  "Seek, and ye shall find. - Matthew 7:7",
  "Blessed are they that mourn: for they shall be comforted. - Matthew 5:4",
  "The Lord is gracious, and full of compassion. - Psalm 111:4",
  "He restoreth my soul. - Psalm 23:3",
  "Thou shalt love thy neighbour as thyself. - Mark 12:31",
  "The Lord is good, a strong hold in the day of trouble. - Nahum 1:7",
  "Weeping endures for a night, but joy comes in the morning.",
  "Where focus goes, energy flows.",
  "What you think, you become.",
  "Act as if it were already yours.",
  "I am becoming who I choose to be.",
  "Small steps every day add up.",
  "Today is a fresh start.",
  "I am enough, just as I am.",
  "My mind is calm and my heart is open.",
  "I choose progress over perfection.",
  "Breathe in calm, breathe out doubt.",
  "I attract what I am ready for.",
  "Good things are on their way.",
  "I trust the timing of my life.",
  "I am stronger than my excuses.",
  "Every day I grow a little braver.",
  "I create my own sunshine.",
  "My potential is limitless.",
  "I am grateful for this moment.",
  "Peace begins with a single breath.",
  "I turn setbacks into comebacks.",
  "I am worthy of good things.",
  "My future is bright and open.",
  "I choose courage over comfort.",
  "Progress, not perfection.",
  "I plant seeds of kindness daily.",
  "I am the author of my story.",
  "Focus on the step in front of you.",
  "I release what I cannot control.",
  "My best is always good enough.",
  "I welcome change with open arms.",
  "Confidence grows each time I try.",
  "I am calm, capable, and ready.",
  "Every sunrise is a new chance.",
  "I speak kindly to myself.",
  "I am exactly where I need to be.",
  "Gratitude turns enough into plenty.",
  "I choose joy in the little things.",
  "My dreams are worth the effort.",
  "I rise by lifting others.",
  "Doubt kills more dreams than failure ever will.",
  "I am open to new possibilities.",
  "Today I choose to be present.",
  "I am proud of how far I have come.",
  "Courage starts with showing up.",
  "I water the seeds I want to grow.",
  "My energy is calm and steady.",
  "I am learning and I am growing.",
  "Every day is a chance to begin again.",
  "I let go and let good things flow.",
  "I am the calm in my own storm.",
  "My thoughts shape my world.",
  "I choose to see the good.",
  "I am patient with my progress.",
  "Kindness is never wasted.",
  "I believe in what I am building.",
  "One kind word can change a day.",
  "I trust myself to figure it out.",
  "Great things take a little time.",
  "I am resilient and I keep going.",
  "My heart is full of hope.",
  "I show up for myself today.",
  "Fear is just excitement without breath.",
  "I am grounded, grateful, and glad.",
  "Every effort counts, even the small ones.",
  "I choose thoughts that lift me up.",
  "My calm is my superpower.",
  "I am open to receive good today.",
  "Slow progress is still progress.",
  "I forgive myself and move forward.",
  "I am building a life I love.",
  "Today I plant hope, not worry.",
  "I am capable of amazing things.",
  "My kindness ripples outward.",
  "I meet challenges with a clear mind.",
  "I am worthy of rest and joy.",
  "Each morning offers a clean page.",
  "I choose faith over fear.",
  "My voice matters and I use it kindly.",
  "I am learning to trust the journey.",
  "Good energy flows to me and through me.",
  "I celebrate small wins today.",
  "I am becoming stronger every day.",
  "My patience is a quiet strength.",
  "I let my light shine gently.",
  "I am open, willing, and ready.",
  "Today I choose peace over pressure.",
  "I keep going, even when it is hard.",
  "My gratitude grows what is good.",
  "I am the calm I have been seeking.",
  "Every step forward is a victory.",
  "I trust that I am enough.",
  "I choose to bloom where I am.",
  "My mindset is my greatest asset.",
  "I am gentle with myself today.",
  "Hope lives in my next small step.",
  "I turn worry into wonder.",
  "I am ready for good things.",
  "My courage is bigger than my fear.",
  "I greet today with an open heart.",
  "I am grateful for another day.",
  "Calm mind, brave heart, steady steps.",
  "I choose growth over comfort.",
  "My dreams deserve my effort.",
  "I am at peace with what is.",
  "Today I move at my own pace.",
  "I am worthy of my own kindness.",
  "Little by little becomes a lot.",
  "I trust myself to begin.",
  "My heart leads with kindness.",
  "I am proud to keep trying.",
  "Fresh start, clear mind, open heart.",
  "I choose to rise, not sink.",
  "Every day I choose to grow.",
  "I am the calm within the noise.",
  "My gratitude opens new doors.",
  "I let good habits carry me.",
  "I am steady, strong, and sure.",
  "Today holds a quiet kind of hope.",
  "I keep my heart soft and my will strong.",
  "I am becoming my best self.",
  "I meet this day with gratitude.",
  "My peace is worth protecting.",
  "I choose kindness, always.",
  "Small brave steps change everything.",
  "I am open to joy today.",
  "My effort today builds my tomorrow.",
  "I trust the quiet work of growth.",
  "I am calm, focused, and free.",
  "Today I choose to begin again.",
  "I carry hope wherever I go.",
  "I bend so that I do not break.",
  "My worth is not up for debate.",
  "Every cloud has a silver lining.",
  "Rome was not built in a day.",
  "Where there is a will, there is a way.",
  "Actions speak louder than words.",
  "The early bird catches the worm.",
  "Fortune favors the bold.",
  "A journey of a thousand miles begins with a single step.",
  "Practice makes perfect.",
  "Slow and steady wins the race.",
  "Better late than never.",
  "Hope for the best, prepare for the worst.",
  "A stitch in time saves nine.",
  "Look before you leap.",
  "Many hands make light work.",
  "Honesty is the best policy.",
  "No pain, no gain.",
  "Patience is a virtue.",
  "What goes around comes around.",
  "Kindness is a language everyone understands.",
  "A smile is the best makeup.",
  "Laughter is the best medicine.",
  "Home is where the heart is.",
  "Every day is a second chance.",
  "Good things come to those who wait.",
  "If at first you don't succeed, try again.",
  "Two heads are better than one.",
  "Birds of a feather flock together.",
  "The grass is greener where you water it.",
  "A friend in need is a friend indeed.",
  "Make hay while the sun shines.",
  "Don't count your chickens before they hatch.",
  "The pen is mightier than the sword.",
  "Necessity is the mother of invention.",
  "When the going gets tough, the tough get going.",
  "Still waters run deep.",
  "You reap what you sow.",
  "A calm sea never made a skilled sailor.",
  "Little strokes fell great oaks.",
  "The best is yet to come.",
  "Every ending is a new beginning.",
  "Do unto others as you would have them do unto you.",
  "Kind words cost nothing.",
  "A good deed is never lost.",
  "Great oaks from little acorns grow.",
  "The darkest hour is just before the dawn.",
  "After a storm comes a calm.",
  "Where there is love, there is life.",
  "A problem shared is a problem halved.",
  "Measure twice, cut once.",
  "Well begun is half done.",
  "First things first.",
  "Easy does it.",
  "Live and let live.",
  "Better safe than sorry.",
  "Time heals all wounds.",
  "Absence makes the heart grow fonder.",
  "Give credit where credit is due.",
  "The apple never falls far from the tree.",
  "Strike while the iron is hot.",
  "Don't put off till tomorrow what you can do today.",
  "A penny saved is a penny earned.",
  "Waste not, want not.",
  "Every dog has its day.",
  "It is never too late to learn.",
  "Knowledge is power.",
  "Curiosity is the spark of learning.",
  "A leopard cannot change its spots.",
  "Don't judge a book by its cover.",
  "Beauty is in the eye of the beholder.",
  "The squeaky wheel gets the grease.",
  "When in Rome, do as the Romans do.",
  "There is no place like home.",
  "A chain is only as strong as its weakest link.",
  "The proof of the pudding is in the eating.",
  "All that glitters is not gold.",
  "An apple a day keeps the doctor away.",
  "Empty vessels make the most noise.",
  "Every man is the architect of his own fortune.",
  "God helps those who help themselves.",
  "Good fences make good neighbors.",
  "Half a loaf is better than none.",
  "He who hesitates is lost.",
  "It takes two to tango.",
  "Learn to walk before you run.",
  "Let sleeping dogs lie.",
  "Life is what you make it.",
  "Little things mean a lot.",
  "Make the most of every day.",
  "No news is good news.",
  "Nothing ventured, nothing gained.",
  "One good turn deserves another.",
  "Opportunity knocks but once.",
  "Every mistake is a lesson in disguise.",
  "Practice what you preach.",
  "Rain today, sunshine tomorrow.",
  "Seeing is believing.",
  "The sun will rise again tomorrow.",
  "There is strength in numbers.",
  "Together we are stronger.",
  "Variety is the spice of life.",
  "A grateful heart is a happy heart.",
  "Kindness always comes back around.",
  "Nothing great was ever achieved without enthusiasm. - Emerson",
  "The only way to have a friend is to be one. - Emerson",
  "Write it on your heart that every day is the best day. - Emerson",
  "Always do what you are afraid to do. - Emerson",
  "Do not go where the path may lead; go where there is no path. - Emerson",
  "Make the most of yourself, for that is all there is of you. - Emerson",
  "For every minute you are angry you lose sixty seconds of peace. - Emerson",
  "The person you are meant to be is the one you decide to be. - Emerson",
  "Go confidently in the direction of your dreams. - Thoreau",
  "Success comes to those too busy to be looking for it. - Thoreau",
  "It does not matter how slowly you go as long as you do not stop. - Confucius",
  "The greatest glory is in rising every time we fall. - Confucius",
  "Wheresoever you go, go with all your heart. - Confucius",
  "Real knowledge is to know the extent of one's ignorance. - Confucius",
  "Life is really simple, but we insist on making it complicated. - Confucius",
  "He who conquers himself is the mightiest warrior. - Confucius",
  "The man who moves a mountain begins by carrying small stones. - Confucius",
  "When I let go of what I am, I become what I might be. - Lao Tzu",
  "Nature does not hurry, yet everything is accomplished. - Lao Tzu",
  "Knowing others is wisdom, knowing yourself is enlightenment. - Lao Tzu",
  "He who knows that enough is enough will always have enough. - Lao Tzu",
  "Great acts are made up of small deeds. - Lao Tzu",
  "Your life is what your thoughts make it. - Marcus Aurelius",
  "Waste no more time arguing what a good man should be. Be one. - Marcus Aurelius",
  "Very little is needed to make a happy life. - Marcus Aurelius",
  "You have power over your mind, not outside events. - Marcus Aurelius",
  "Confine yourself to the present. - Marcus Aurelius",
  "Luck is what happens when preparation meets opportunity. - Seneca",
  "We suffer more often in imagination than in reality. - Seneca",
  "While we wait for life, life passes. - Seneca",
  "Difficulties strengthen the mind, as labor does the body. - Seneca",
  "He suffers more than necessary who suffers before it is necessary. - Seneca",
  "Believe you can and you're halfway there. - Theodore Roosevelt",
  "Do what you can, with what you have, where you are. - Theodore Roosevelt",
  "Keep your eyes on the stars, and your feet on the ground. - Theodore Roosevelt",
  "Nothing worth having comes easy. - Theodore Roosevelt",
  "Comparison is the thief of joy. - Theodore Roosevelt",
  "Our greatest weakness lies in giving up. - Edison",
  "There is no substitute for hard work. - Edison",
};
const int NUM_QUOTES = sizeof(QUOTES) / sizeof(QUOTES[0]);

// ---------------- DISPLAY DRIVER ----------------
// WeAct 4.2" 400x300. The physical panel is 3-color (GDEY042Z98), but we
// drive it in BLACK & WHITE with the GDEY042T81 driver on purpose: B/W mode
// unlocks the panel's fast ~1.2s refresh (red mode is stuck at ~15s and a
// 3-minute minimum). Same controller (SSD1683), so this drives it fine.
// If the screen shows garbage/nothing, this line is the first thing to change.
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT>
  display(GxEPD2_420_GDEY042T81(/*CS=*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));

// ---------------- INTERNAL STRUCTS ----------------

struct BusArrival {
  String service;
  int minsAway;    // next bus, minutes away. -1 means "no data"
  int minsAway2;   // the bus after that. -1 means "no data"
  int minsAway3;   // and the one after that. -1 means "no data"
};

BusArrival homeResults[4];
BusArrival oppResults[2];

// ---------------- WIFI ----------------

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;   // already up (same awake session)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi FAILED to connect within 15s");
  }
}

// ---------------- TIME (for active-hours check) ----------------

bool syncTime() {
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // UTC+8 = Singapore
  struct tm timeinfo;
  int attempts = 0;
  while (!getLocalTime(&timeinfo) && attempts < 10) {
    delay(500);
    attempts++;
  }
  return attempts < 10;
}

bool isWithinActiveHours(const struct tm &t) {
  if (DEBUG_NO_NIGHT_SLEEP) return true;   // debugging: always "active"
  int nowMinutes   = t.tm_hour * 60 + t.tm_min;
  int startMinutes = ACTIVE_START_HOUR * 60 + ACTIVE_START_MIN;
  int endMinutes   = ACTIVE_END_HOUR * 60 + ACTIVE_END_MIN;
  return (nowMinutes >= startMinutes && nowMinutes <= endMinutes);
}

// Deep sleep from now until the next ACTIVE_START (05:00). Does not return.
// WiFi stays off the whole time, which is where the overnight battery saving
// comes from. The ESP32's RTC keeps the clock running through deep sleep.
void deepSleepUntilActiveStart(const struct tm &now) {
  int nowSec   = now.tm_hour * 3600 + now.tm_min * 60 + now.tm_sec;
  int startSec = ACTIVE_START_HOUR * 3600 + ACTIVE_START_MIN * 60;
  long sleepSec = (nowSec < startSec) ? (startSec - nowSec)
                                      : (86400L - nowSec + startSec);
  if (sleepSec < 60) sleepSec = 60;

  Serial.print("Outside service hours. Sleeping (minutes): ");
  Serial.println(sleepSec / 60);
  Serial.flush();

  esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
  esp_deep_sleep_start();
}

// ---------------- LTA API ----------------

// Parses an LTA EstimatedArrival timestamp (e.g. 2026-07-25T14:32:10+08:00)
// into whole minutes from now. Returns -1 if empty/unparseable, 0 if in the past.
int minutesUntilArrival(const char* estArrival) {
  if (estArrival == nullptr || strlen(estArrival) == 0) return -1;
  struct tm arrivalTm = {};
  int y, mo, d, h, mi, s;
  if (sscanf(estArrival, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) return -1;
  arrivalTm.tm_year = y - 1900; arrivalTm.tm_mon = mo - 1; arrivalTm.tm_mday = d;
  arrivalTm.tm_hour = h; arrivalTm.tm_min = mi; arrivalTm.tm_sec = s;
  int m = (int)((mktime(&arrivalTm) - time(nullptr)) / 60);
  return (m < 0) ? 0 : m;
}

// Fetches arrival times for one stop, filling result[] with
// minutes-away for each service in wantedServices[].
void fetchBusArrivals(const char* stopCode, const char* wantedServices[], int count, BusArrival result[]) {
  for (int i = 0; i < count; i++) {
    result[i].service = wantedServices[i];
    result[i].minsAway = -1;
    result[i].minsAway2 = -1;
    result[i].minsAway3 = -1;
  }

  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://datamall2.mytransport.sg/ltaodataservice/v3/BusArrival?BusStopCode=";
  url += stopCode;

  http.begin(url);
  http.addHeader("AccountKey", LTA_API_KEY);
  http.addHeader("accept", "application/json");

  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.print("HTTP error for stop ");
    Serial.print(stopCode);
    Serial.print(": ");
    Serial.println(httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  // Response can be large depending on how many services stop here
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    return;
  }

  JsonArray services = doc["Services"].as<JsonArray>();

  for (JsonObject svc : services) {
    const char* serviceNo = svc["ServiceNo"];

    // check if this is one of the services we care about
    int slot = -1;
    for (int i = 0; i < count; i++) {
      if (wantedServices[i][0] != '\0' && strcmp(serviceNo, wantedServices[i]) == 0) {
        slot = i;
        break;
      }
    }
    if (slot == -1) continue;

    result[slot].minsAway  = minutesUntilArrival(svc["NextBus"]["EstimatedArrival"]);
    result[slot].minsAway2 = minutesUntilArrival(svc["NextBus2"]["EstimatedArrival"]);
    result[slot].minsAway3 = minutesUntilArrival(svc["NextBus3"]["EstimatedArrival"]);
  }
}

// ---------------- BATTERY ----------------

RTC_DATA_ATTR bool g_lowBattNotified = false;  // survives sleep: already pushed?
bool g_showLowBatt = false;                    // draw the icon this cycle?

// Reads battery volts, or -1 if monitoring is off or the reading is implausible
// (pin probably not wired to the battery - so we won't false-alarm).
float readBatteryVolts() {
  if (!BATTERY_MONITOR) return -1.0f;
  uint32_t mv = analogReadMilliVolts(BATTERY_ADC_PIN);
  float v = (mv / 1000.0f) * BATTERY_DIVIDER;
  if (v < 2.5f || v > 4.5f) return -1.0f;  // outside LiPo range -> not connected
  return v;
}

// Sends a one-off low-battery push to your phone via ntfy.sh.
void sendLowBatteryPush(float volts) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(String("https://ntfy.sh/") + NTFY_TOPIC);
  http.addHeader("Title", "BusGlance battery low");
  http.addHeader("Priority", "high");
  http.addHeader("Tags", "battery");
  char body[64];
  snprintf(body, sizeof(body), "Battery at %.2fV - please charge soon.", volts);
  http.POST((uint8_t*)body, strlen(body));
  http.end();
}

// ---------------- WEATHER ----------------
// Current + one "later" forecast from Open-Meteo (free, no API key).

// Three slots: [0] now, [1] now + AHEAD, [2] now + 2*AHEAD hours.
float g_temp[3] = {0, 0, 0};
int   g_rain[3] = {-1, -1, -1};
int   g_code[3] = {-1, -1, -1};
char  g_label[3][8] = {"NOW", "--:--", "--:--"};
bool  g_weatherOK = false;

// WMO weather code -> icon type: 0 sun, 1 sun+cloud, 2 cloud, 3 rain.
int weatherIconType(int code) {
  if (code < 0)  return 2;
  if (code == 0) return 0;
  if (code <= 2) return 1;
  if (code == 3 || code == 45 || code == 48) return 2;
  return 3;
}

void fetchWeather() {
  // NOTE: we do NOT clear g_weatherOK / the weather data up front. If this fetch
  // fails (network blip, Open-Meteo hiccup), we keep showing the last-good
  // weather instead of blanking the header. RAM survives light sleep, so the
  // previous values persist between refreshes. Only a success overwrites them.
  if (WiFi.status() != WL_CONNECTED) return;

  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += String(LATITUDE, 4);
  url += "&longitude=";
  url += String(LONGITUDE, 4);
  url += "&current=temperature_2m,weather_code";
  url += "&hourly=temperature_2m,precipitation_probability,weather_code";
  url += "&timezone=Asia%2FSingapore&forecast_days=2";

  HTTPClient http;
  http.begin(url);
  int code = http.GET();
  if (code != 200) { http.end(); return; }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return;

  g_temp[0] = doc["current"]["temperature_2m"] | 0.0f;
  g_code[0] = doc["current"]["weather_code"] | 0;

  struct tm now;
  if (getLocalTime(&now, 0)) {
    JsonArray temps = doc["hourly"]["temperature_2m"];
    JsonArray rains = doc["hourly"]["precipitation_probability"];
    JsonArray codes = doc["hourly"]["weather_code"];
    JsonArray times = doc["hourly"]["time"];

    int offs[3] = {0, FORECAST_AHEAD_HOURS, 2 * FORECAST_AHEAD_HOURS};
    for (int k = 0; k < 3; k++) {
      int idx = now.tm_hour + offs[k];   // forecast_days=2 gives 48 hourly points
      if (idx < 0 || idx >= (int)temps.size()) continue;

      g_rain[k] = rains[idx] | 0;
      if (k > 0) {                                  // slot 0 uses "current" above
        g_temp[k] = temps[idx] | 0.0f;
        g_code[k] = codes[idx] | 0;
        const char* t = times[idx];                 // e.g. "2026-07-25T20:00"
        if (t && strlen(t) >= 16) {
          g_label[k][0] = t[11]; g_label[k][1] = t[12]; g_label[k][2] = ':';
          g_label[k][3] = t[14]; g_label[k][4] = t[15]; g_label[k][5] = '\0';
        }
      }
    }
  }
  g_weatherOK = true;
}

// ---------------- DRAWING ----------------

// Outline weather icon centered at (cx, cy). type: 0 sun, 1 sun+cloud, 2 cloud, 3 rain.
// A uniform 2px-thick sun: outer ring + short solid rays. Uniform everywhere.
static void drawSun(int cx, int cy, int r) {
  display.fillCircle(cx, cy, r, GxEPD_BLACK);
  display.fillCircle(cx, cy, r - 2, GxEPD_WHITE);          // 2px ring
  display.fillRect(cx - 1, cy - (r + 7), 3, 5, GxEPD_BLACK);   // N
  display.fillRect(cx - 1, cy + (r + 2), 3, 5, GxEPD_BLACK);   // S
  display.fillRect(cx - (r + 7), cy - 1, 5, 3, GxEPD_BLACK);   // W
  display.fillRect(cx + (r + 2), cy - 1, 5, 3, GxEPD_BLACK);   // E
}

void drawWeatherIcon(int cx, int cy, int type) {
  if (type == 0) {                       // clear: sun only
    drawSun(cx, cy, 8);
    return;
  }
  if (type == 1) {                       // partly cloudy: small sun peeking, then cloud over it
    drawSun(cx - 12, cy - 8, 5);
  }
  // Cloud drawn as a UNIFORM outline: solid black silhouette, then the same
  // silhouette inset by 2px in white -> leaves an even 2px border all around.
  const int S = 2;
  display.fillCircle(cx - 9, cy + 3, 8,  GxEPD_BLACK);
  display.fillCircle(cx + 9, cy + 3, 9,  GxEPD_BLACK);
  display.fillCircle(cx,     cy - 4, 10, GxEPD_BLACK);
  display.fillRect(cx - 17, cy + 3, 35, 12, GxEPD_BLACK);
  display.fillCircle(cx - 9, cy + 3, 8  - S, GxEPD_WHITE);
  display.fillCircle(cx + 9, cy + 3, 9  - S, GxEPD_WHITE);
  display.fillCircle(cx,     cy - 4, 10 - S, GxEPD_WHITE);
  display.fillRect(cx - 17 + S, cy + 3, 35 - 2*S, 12 - S, GxEPD_WHITE);  // keep bottom edge
  if (type == 3) {                       // rain drops (short, so they clear the temp text)
    display.fillRect(cx - 9, cy + 15, 3, 5, GxEPD_BLACK);
    display.fillRect(cx - 1, cy + 15, 3, 5, GxEPD_BLACK);
    display.fillRect(cx + 7, cy + 15, 3, 5, GxEPD_BLACK);
  }
}

// Prints a string centered on x = cx at baseline y (uses current font).
void printCenteredAt(int cx, int y, const String &s) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  display.setCursor(cx - w / 2, y);
  display.print(s);
}

// One weather column centered at cx: label, outline icon, temp (deg), rain%.
void drawWeatherColumn(int cx, int k) {
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  printCenteredAt(cx, 16, String(g_label[k]));
  drawWeatherIcon(cx, 38, weatherIconType(g_code[k]));   // raised so rain drops clear the temp text

  // temp and rain% on ONE line, centered, so the icon can be bigger
  String ts = String((int)round(g_temp[k]));
  String rs = (g_rain[k] >= 0) ? ("  " + String(g_rain[k]) + "%") : String("  --");
  int16_t x1, y1;
  uint16_t wt, wr, h;
  display.getTextBounds(ts, 0, 0, &x1, &y1, &wt, &h);
  display.getTextBounds(rs, 0, 0, &x1, &y1, &wr, &h);
  int sx = cx - (wt + 6 + (int)wr) / 2;      // 6 = degree mark spacing
  display.setCursor(sx, 74);
  display.print(ts);
  int ex = display.getCursorX();
  display.drawCircle(ex + 3, 65, 2, GxEPD_BLACK);          // degree mark
  display.setCursor(ex + 6, 74);
  display.print(rs);
}

// Top header: date/day + time (left), three weather columns (right). No rule.
void drawHeader() {
  struct tm now;
  bool haveTime = getLocalTime(&now, 0);
  char dateStr[24] = "";
  char timeStr[8]  = "--:--";
  if (haveTime) {
    strftime(dateStr, sizeof(dateStr), "%a, %d %b %Y", &now);
    strftime(timeStr, sizeof(timeStr), "%H:%M", &now);
  }

  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(10, 20);
  display.print(dateStr);

  display.setFont(&FreeSansBold18pt7b);
  display.setCursor(10, 56);
  display.print(timeStr);

  if (g_weatherOK) {
    drawWeatherColumn(200, 0);
    drawWeatherColumn(276, 1);
    drawWeatherColumn(352, 2);
  }
}

// Prints one line of text horizontally centered on the 400px-wide screen.
void drawCenteredLine(const String &s, int baselineY) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, baselineY, &x1, &y1, &w, &h);
  int x = (400 - (int)w) / 2;
  if (x < 0) x = 0;
  display.setCursor(x, baselineY);
  display.print(s);
}

// Draws a quote in small italics, word-wrapped to at most two centered lines.
void drawQuote(const char* quote, int topY) {
  display.setFont(&FreeSans9pt7b);

  const int maxWidth  = 380;
  const int lineHeight = 20;

  String text = String(quote);
  String lines[2];
  int nLines = 0;
  String line = "";

  int from = 0;
  while (from <= (int)text.length() && nLines < 2) {
    int sp = text.indexOf(' ', from);
    String word = (sp == -1) ? text.substring(from) : text.substring(from, sp);

    String trial = (line.length() == 0) ? word : line + " " + word;
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(trial, 0, 0, &x1, &y1, &w, &h);

    if ((int)w > maxWidth && line.length() > 0) {
      lines[nLines++] = line;
      line = word;
    } else {
      line = trial;
    }

    if (sp == -1) break;
    from = sp + 1;
  }
  if (nLines < 2 && line.length() > 0) lines[nLines++] = line;

  for (int i = 0; i < nLines; i++) {
    drawCenteredLine(lines[i], topY + 14 + i * lineHeight);
  }
}

// Draws one bus: bold service number on the left, minutes hard-right.
// A bus SOON_MINUTES away or less (including NOW) gets a filled black tag with
// white text, so an imminent bus jumps out even in black & white.
void drawBus(int xLeft, int xRight, int baselineY, BusArrival &b) {
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSansBold24pt7b);
  display.setCursor(xLeft, baselineY);
  display.print(b.service);
  int numRight = display.getCursorX();     // right edge of the service number

  // Primary = next bus (big, with a black tag if imminent).
  String mins;
  bool soon = false;
  if (b.minsAway < 0) {
    mins = "-";
  } else if (b.minsAway == 0) {
    mins = "0";          // arriving now = 0 min (tag still makes it pop)
    soon = true;
  } else {
    mins = String(b.minsAway);      // just the number - "min" dropped
    soon = (b.minsAway <= SOON_MINUTES);
  }

  // Secondary = the next TWO buses after (small, raised), when we have them.
  String s2 = "";
  if (b.minsAway >= 0 && b.minsAway2 >= 0) {
    s2 = String(b.minsAway2);
    if (b.minsAway3 >= 0) s2 += " " + String(b.minsAway3);
  }
  int16_t sx, sy; uint16_t sw = 0, sh = 0;
  if (s2.length()) {
    display.setFont(&FreeSansBold9pt7b);
    display.getTextBounds(s2, 0, 0, &sx, &sy, &sw, &sh);
  }

  // Pick the primary font so number + primary + secondary all fit; else shrink.
  const int GAP = 10, SGAP = 12;
  int secW = s2.length() ? (SGAP + (int)sw) : 0;
  const GFXfont* pf = &FreeSansBold18pt7b;
  display.setFont(pf);
  int16_t mx, my; uint16_t mw, mh;
  display.getTextBounds(mins, 0, 0, &mx, &my, &mw, &mh);
  if (numRight + GAP + (int)mw + secW > xRight) {
    pf = &FreeSansBold12pt7b;
    display.setFont(pf);
    display.getTextBounds(mins, 0, 0, &mx, &my, &mw, &mh);
  }

  int minBase = baselineY - 5;
  // Right-align the [primary][secondary] group to xRight.
  int s2x = 0, s2base = minBase - 11;      // secondary raised like a superscript
  int groupRight = xRight;
  if (s2.length()) {
    s2x = xRight - (int)sw - sx;
    groupRight = s2x - SGAP;
  }
  int tx = groupRight - (int)mw - mx;
  if (tx < numRight + GAP) tx = numRight + GAP;   // never cross the number

  display.setFont(pf);
  if (soon) {                              // filled tag, snug + centered on the primary
    const int pad = 5;
    display.fillRect(tx + mx - pad, minBase + my - pad, (int)mw + 2*pad, (int)mh + 2*pad, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
  } else {
    display.setTextColor(GxEPD_BLACK);
  }
  display.setCursor(tx, minBase);
  display.print(mins);
  display.setTextColor(GxEPD_BLACK);

  if (s2.length()) {                       // the "after" bus, small + raised
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(s2x, s2base);
    display.print(s2);
  }
}

// Draws a centered uppercase label, a bold rule, then buses in a 2-column grid.
// 4 services -> 2x2 grid; 2 services -> one row of two.
void drawStopBlock(int yTop, const char* label, BusArrival results[], int count) {
  String heading = String(label);
  heading.toUpperCase();

  // Thick black header bar with white label, vertically centered in the bar
  const int barTop = yTop + 2, barH = 34;
  display.fillRect(10, barTop, 380, barH, GxEPD_BLACK);
  display.setFont(&FreeSansBold18pt7b);
  display.setTextColor(GxEPD_WHITE);
  int16_t hx, hy; uint16_t hw, hh;
  display.getTextBounds(heading, 0, 0, &hx, &hy, &hw, &hh);
  drawCenteredLine(heading, barTop + (barH - (int)hh) / 2 - hy);
  // tiny "MIN" legend on the RIGHT of the bar (the bare numbers below are minutes)
  display.setFont(&FreeSansBold9pt7b);
  int16_t lx, ly; uint16_t lw, lh;
  display.getTextBounds("MIN", 0, 0, &lx, &ly, &lw, &lh);
  display.setCursor(382 - (int)lw - lx, barTop + (barH - (int)lh) / 2 - ly);
  display.print("MIN");
  display.setTextColor(GxEPD_BLACK);

  const int colL[2]       = {15, 215};
  const int colR[2]       = {185, 385};
  const int firstBaseline = yTop + 72;
  const int rowGap        = 52;

  for (int i = 0; i < count; i++) {
    int col = i % 2;
    int row = i / 2;
    drawBus(colL[col], colR[col], firstBaseline + row * rowGap, results[i]);
  }
}

// Small low-battery icon, drawn white in the top-right of the black HOME bar.
void drawLowBattIcon() {
  int x = 356, y = 90;
  display.drawRect(x, y, 24, 14, GxEPD_WHITE);         // battery body
  display.fillRect(x + 24, y + 4, 3, 6, GxEPD_WHITE);  // positive nub
  display.fillRect(x + 2, y + 2, 4, 10, GxEPD_WHITE);  // low charge level
}

void updateDisplay(bool full) {
  display.setRotation(0);
  if (full) {
    display.setFullWindow();                                   // clean, flashes
  } else {
    display.setPartialWindow(0, 0, display.width(), display.height());  // quiet, no flash
  }
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    // Weather + date/time header (the black HOME bar below separates it)
    drawHeader();

    // Home stop: 4 services in a 2x2 grid
    drawStopBlock(80, STOP_LABEL_HOME, homeResults, NUM_SERVICES_HOME);
    if (g_showLowBatt) drawLowBattIcon();

    // Opposite stop: 2 services side by side
    drawStopBlock(214, STOP_LABEL_OPPOSITE, oppResults, NUM_SERVICES_OPPOSITE);

  } while (display.nextPage());
}

// ---------------- SETUP / LOOP ----------------

// Survives deep sleep (stored in RTC memory): true once we've synced the
// clock over the network at least once, so later wakes can trust the RTC.
RTC_DATA_ATTR bool g_haveSynced = false;

void setup() {
  Serial.begin(115200);
  delay(200);

  // The TZ env var is wiped on every deep-sleep wake, so set it each boot.
  // The RTC clock itself keeps running through sleep, so once we've synced
  // once we can read the time without going online.
  setenv("TZ", "SGT-8", 1);   // Singapore, UTC+8
  tzset();

  struct tm now;
  bool timeKnown = g_haveSynced && getLocalTime(&now, 0);

  // Fast path: we already know the time and it's the middle of the night ->
  // sleep straight through to 5am without ever powering up WiFi.
  if (timeKnown && !isWithinActiveHours(now)) {
    deepSleepUntilActiveStart(now);   // does not return
  }

  // Otherwise we need the network (first boot, or it's daytime).
  connectWiFi();
  if (syncTime()) {
    g_haveSynced = true;
    getLocalTime(&now, 0);
    timeKnown = true;
  }

  // Re-check with the freshly synced time in case we woke right at a boundary.
  if (timeKnown && !isWithinActiveHours(now)) {
    deepSleepUntilActiveStart(now);   // does not return
  }

  // Service hours: init the panel ONCE. Keeping it initialised (never
  // hibernated) between updates is what lets the later refreshes be partial.
  display.init();
  doRefreshCycle(true);   // first draw of the session is a clean FULL refresh
  sleepUntilNextRefresh();
}

// Fetch fresh data + battery status, then repaint. full=true does a clean
// flashing refresh; full=false does a quiet partial refresh (no black flash).
void doRefreshCycle(bool full) {
  connectWiFi();                         // reconnect after the sleep (no-op if up)
  fetchBusArrivals(STOP_CODE_HOME, SERVICES_HOME, NUM_SERVICES_HOME, homeResults);
  fetchBusArrivals(STOP_CODE_OPPOSITE, SERVICES_OPPOSITE, NUM_SERVICES_OPPOSITE, oppResults);
  fetchWeather();

  // Battery check: show an on-screen icon while low, and push to phone once.
  float vbat = readBatteryVolts();
  Serial.print("Battery volts: ");
  Serial.println(vbat);
  g_showLowBatt = (vbat > 0 && vbat < LOW_BATT_VOLTS);
  if (g_showLowBatt && !g_lowBattNotified) {
    sendLowBatteryPush(vbat);
    g_lowBattNotified = true;
  } else if (vbat > LOW_BATT_VOLTS + 0.15f) {
    g_lowBattNotified = false;  // recovered after charging -> re-arm alerts
  }

  updateDisplay(full);
}

// Turn WiFi off (save power) and LIGHT-sleep until the next refresh. Light
// sleep keeps RAM + the panel's state alive, so partial refresh keeps working;
// the RTC clock and TZ also survive it. Execution resumes at the next loop().
void sleepUntilNextRefresh() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("Light-sleeping until next refresh...");
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)REFRESH_SECONDS * 1000000ULL);
  esp_light_sleep_start();
}

void loop() {
  static int cycle = 0;

  // If we've crossed into the overnight window, power right down until 5am.
  struct tm now;
  if (getLocalTime(&now, 0) && !isWithinActiveHours(now)) {
    deepSleepUntilActiveStart(now);   // does not return; setup() runs at 5am
  }

  cycle++;
  bool full = (cycle % FULL_REFRESH_EVERY == 0);   // periodic ghost-clearing flash
  doRefreshCycle(full);
  sleepUntilNextRefresh();
}
