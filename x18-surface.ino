#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <OSCMessage.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define TEXT_SIZE     3
#define BUF_NAME_SIZE 10

#define LOAD  A2
#define CLK   A0
#define DIN   A1

#define UP    3
#define DOWN  4

#define CHANNEL1_1    4
#define CHANNEL1_2    5
#define CHANNEL2_1    6
#define CHANNEL2_2    7

#define CHANNEL1_SLIDER      A0
#define CHANNEL1_SOLO        30
#define CHANNEL1_MUTE        31
#define CHANNEL1_SOLO_LED    22
#define CHANNEL1_MUTE_LED    23

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(10, 1, 1, 30);
unsigned int local_port = 9000;

IPAddress mixer_ip(10, 1, 1, 197);
unsigned int mixer_port = 10024;

EthernetUDP udp;

typedef struct {
    char    name[BUF_NAME_SIZE];
    bool    solo;
    int     solo_in;
    int     solo_led;
    bool    mute;
    int     mute_in;
    int     mute_led;
    float   level;
    int     channel_id;
    int     fader;
    int     motor[2];
} Channel;

Channel ch1 = {
    "CH1",
    0, CHANNEL1_SOLO, CHANNEL1_SOLO_LED, 
    0, CHANNEL1_MUTE, CHANNEL1_MUTE_LED, 
    0.0, 1, CHANNEL1_SLIDER, {CHANNEL1_1, CHANNEL1_2}
};

void init_channel(Channel ch) {
    pinMode(ch.motor[0], OUTPUT);
    pinMode(ch.motor[1], OUTPUT);

    pinMode(ch.solo_led, OUTPUT);
    pinMode(ch.mute_led, OUTPUT);

    pinMode(ch.solo_in, INPUT_PULLUP);
    pinMode(ch.mute_in, INPUT_PULLUP);
}

void setup() {
    Serial.begin(9600);
    Ethernet.begin(mac, ip);
    udp.begin(local_port);

    init_channel(ch1);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        while (1);
    }

    display.display();
    delay(2000);

    draw_char(ch1.name);
    Serial.println("Setup Complete");
}

void draw_char(const char *text) {
    display.clearDisplay();
    display.setTextSize(TEXT_SIZE);
    display.setTextColor(SSD1306_WHITE);
    display.cp437(true);

    int x, y, width, height;
    display.getTextBounds(text, 0, 0, &x, &y, &width, &height);
    display.setCursor((SCREEN_WIDTH - width) / 2, (SCREEN_HEIGHT - height) / 2);

    int i = 0;
    while (text[i]) {
        display.write(text[i++]);
    }

    display.display();
}

void update_channel(Channel *ch) {
    char name[3];
    int level = analogRead(ch->fader);

    if (ch->channel_id < 10) {
        name[0] = '0';
        name[1] = ch->channel_id + '0';
    } else {
        name[0] = '1';
        name[1] = ch->channel_id - 10 + '0';
    }
    name[2] = '\0';

    set_channel_level(name, (float)level / 1024);

    if (digitalRead(ch->solo_in) == LOW) {
        Serial.println("Solo click");
        ch->solo = !ch->solo;
        digitalWrite(ch->solo_led, ch->solo ? HIGH : LOW);

        char addr_solo[] = "/-stat/solosw/01";
        addr_solo[14] = name[0];
        addr_solo[15] = name[1];

        set_int(addr_solo, ch->solo);
    }

    if (digitalRead(ch->mute_in) == LOW) {
        Serial.println("Mute click");
        ch->mute = !ch->mute;
        digitalWrite(ch->mute_led, ch->mute ? LOW : HIGH);

        char addr_mute[] = "/ch/01/mix/on";
        addr_mute[4] = name[0];
        addr_mute[5] = name[1];

        set_int(addr_mute, ch->mute);
    }

    char addr_name[] = "/ch/01/config/name";
    addr_name[4] = name[0];
    addr_name[5] = name[1];

    char buf[BUF_NAME_SIZE];
    memset(buf, '\0', sizeof buf);

    if (get_string(addr_name, buf, BUF_NAME_SIZE) < 0) {
        Serial.print(addr_name);
        Serial.println(" name is too long");
    }

    if (strncmp(buf, ch->name, BUF_NAME_SIZE)) {
        memcpy(ch->name, buf, BUF_NAME_SIZE);
        draw_char(ch->name);
    }
}

void loop() {
    update_channel(&ch1);
    delay(20);
}

float get_channel_level(const char *channel) {
    char addr[] = "/ch/01/mix/fader";
    addr[4] = channel[0];
    addr[5] = channel[1];

    return get_float(addr);
}

void set_channel_level(const char *channel, float level) {
    char addr[] = "/ch/01/mix/fader";
    addr[4] = channel[0];
    addr[5] = channel[1];

    set_float(addr, level);
}

void get_value(OSCMessage *msg) {
    if (!udp.beginPacket(mixer_ip, mixer_port)) {
        Serial.println("Error creating udp packet");
        return 0;
    }

    msg->send(udp);

    if (!udp.endPacket()) {
        Serial.println("Error sending udp packet");
        return 0;
    }

    msg->empty();
    delay(20);

    int packet_size = udp.parsePacket();
    if (!packet_size) {
        Serial.println("No packet recieved");
        return;
    }
    
    while (packet_size--) {
        msg->fill(udp.read());
    }
}

int get_int(const char *addr) {
    OSCMessage msg(addr);
    get_value(&msg);

    if (!msg.isInt(0)) {
        Serial.print(addr);
        Serial.println(" did not return an integer");
        return 0;
    }

    return msg.getInt(0);
}

float get_float(const char *addr) {
    OSCMessage msg(addr);
    get_value(&msg);

    if (!msg.isFloat(0)) {
        Serial.print(addr);
        Serial.println(" did not return a float");
        return 0;
    }
    return msg.getFloat(0);
}

int get_string(const char *addr, char *buf, int buf_size) {
    OSCMessage msg(addr);
    get_value(&msg);

    if (!msg.isString(0)) {
        Serial.print(addr);
        Serial.println(" did not return a string");
        return 0;
    }

    return msg.getString(0, buf, buf_size);
}

void set_int(const char *addr, int value) {
    OSCMessage msg(addr);
    msg.add(value);

    if (!udp.beginPacket(mixer_ip, mixer_port)) {
        Serial.println("Error creating udp packet");
        return 0;
    }

    msg.send(udp);

    if (!udp.endPacket()) {
        Serial.println("Error sending udp packet");
        return;
    }

    msg.empty();
}

void set_float(const char *addr, float value) {
    OSCMessage msg(addr);
    msg.add(value);

    if (!udp.beginPacket(mixer_ip, mixer_port)) {
        Serial.println("Error creating udp packet");
        return;
    }

    msg.send(udp);

    if (!udp.endPacket()) {
        Serial.println("Error sending udp packet");
        return;
    }

    msg.empty();
}


