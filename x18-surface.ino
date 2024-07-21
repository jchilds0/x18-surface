#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <OSCMessage.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define TEXT_SIZE     3
#define BUF_NAME_SIZE 10
#define LEVEL_OFFSET  5

#define TRUE          1 
#define FALSE         0

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

#define TIMEOUT    100
static int screen_timer = TIMEOUT;

typedef struct {
    int   value;
    int   led_enable;
    int   button_in;
    bool  pressed;
} Button;

typedef struct {
    char    name[BUF_NAME_SIZE];
    Button  solo;
    Button  mute;
    int     level;
    int     channel_id;
    int     fader;
    int     motor[2];
} Channel;

Channel ch1 = { "CH1",
    { FALSE, CHANNEL1_SOLO_LED, CHANNEL1_SOLO, FALSE }, 
    { FALSE, CHANNEL1_MUTE_LED, CHANNEL1_MUTE, FALSE }, 
    0, 1, CHANNEL1_SLIDER, {CHANNEL1_1, CHANNEL1_2}
};

void init_button(Button b) {
    pinMode(b.led_enable, OUTPUT);
    pinMode(b.button_in, INPUT_PULLUP);
}

void init_channel(Channel ch) {
    pinMode(ch.motor[0], OUTPUT);
    pinMode(ch.motor[1], OUTPUT);

    init_button(ch.solo);
    init_button(ch.mute);
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
    delay(500);
    Serial.println("Setup Complete");
}

void draw_char(const char *text) {
    display.clearDisplay();
    display.setTextSize(TEXT_SIZE);
    display.setTextColor(SSD1306_WHITE);
    display.cp437(TRUE);

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

    if (level < ch->level - LEVEL_OFFSET || level > ch->level + LEVEL_OFFSET) {
        set_channel_level(name, (float)level / 1024);
        ch->level = level;

        screen_timer = TIMEOUT;
    }

    int solo = digitalRead(ch->solo.button_in);
    if (solo == LOW && !ch->solo.pressed) {
        Serial.println("Solo click");
        ch->solo.pressed = 1;
        ch->solo.value = !ch->solo.value;
        digitalWrite(ch->solo.led_enable, ch->solo.value ? HIGH : LOW);

        char addr_solo[] = "/-stat/solosw/01";
        addr_solo[14] = name[0];
        addr_solo[15] = name[1];

        set_int(addr_solo, ch->solo.value);

        screen_timer = TIMEOUT;
    } else if (solo == HIGH) {
        ch->solo.pressed = FALSE;
    }

    int mute = digitalRead(ch->mute.button_in);
    if (mute == LOW && !ch->mute.pressed) {
        Serial.println("Mute click");
        ch->mute.pressed = TRUE;
        ch->mute.value = !ch->mute.value;
        digitalWrite(ch->mute.led_enable, ch->mute.value ? LOW : HIGH);

        char addr_mute[] = "/ch/01/mix/on";
        addr_mute[4] = name[0];
        addr_mute[5] = name[1];

        set_int(addr_mute, ch->mute.value);

        screen_timer = TIMEOUT;
    } else if (mute == HIGH) {
        ch->mute.pressed = FALSE;
    }

    if (screen_timer > 0) {
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
    } else {
        display.clearDisplay();
        display.display();
        memset(ch->name, '\0', BUF_NAME_SIZE);
    }
}

void loop() {
    update_channel(&ch1);
    if (screen_timer > 0) {
      screen_timer--;
    }
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


