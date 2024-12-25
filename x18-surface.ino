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

#define WITHIN(a, x, y)   ((x) <= (a) && (a) <= (y))
#define DIST(a, x, r)     WITHIN(a, (x) - (r), (x) + (r))

#define TRUE          1 
#define FALSE         0

#define CHANNEL1_1    46
#define CHANNEL1_2    47

#define CHANNEL1_SLIDER      A0
#define CHANNEL1_SOLO        30
#define CHANNEL1_MUTE        31
#define CHANNEL1_SOLO_LED    22
#define CHANNEL1_MUTE_LED    23

#define LOWER_LAYER_IN       40
#define LOWER_LAYER_LED      42

#define UPPER_LAYER_IN       41
#define UPPER_LAYER_LED      43

#define DIN     50 
#define CLK     51
#define LOAD    52

#define DECODE_MODE          0x09
#define INTENSITY            0x0a
#define SCAN_LIMIT           0x0b
#define SHUTDOWN             0x0c
#define DISPLAY_TEST         0x0f

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
    int     fader_level;
    int     fader_in;
    int     channel_id;
    int     fader_motor1;
    int     fader_motor2;
} Channel;

Channel ch1 = { "CH1",
    { FALSE, CHANNEL1_SOLO_LED, CHANNEL1_SOLO, FALSE }, 
    { FALSE, CHANNEL1_MUTE_LED, CHANNEL1_MUTE, FALSE }, 
    0, CHANNEL1_SLIDER, 1, CHANNEL1_1, CHANNEL1_2
};

Button lower = { TRUE, LOWER_LAYER_LED, LOWER_LAYER_IN, FALSE };
Button upper = { FALSE, UPPER_LAYER_LED, UPPER_LAYER_IN, FALSE };

void init_button(Button b) {
    pinMode(b.led_enable, OUTPUT);
    pinMode(b.button_in, INPUT_PULLUP);
}

void init_channel(Channel ch) {
    pinMode(ch.fader_motor1, OUTPUT);
    pinMode(ch.fader_motor2, OUTPUT);

    init_button(ch.solo);
    init_button(ch.mute);
}

void max7219_message(byte addr, byte data) {
    digitalWrite(LOAD, LOW);
    digitalWrite(CLK, LOW);
    shiftOut(DIN, CLK, MSBFIRST, addr);
    shiftOut(DIN, CLK, MSBFIRST, data);
    digitalWrite(LOAD, HIGH);
}

void setup() {
    Serial.begin(9600);
    Ethernet.begin(mac, ip);
    udp.begin(local_port);

    init_channel(ch1);
    init_button(lower);
    init_button(upper);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        while (1);
    }

    display.display();
    delay(500);
    Serial.println("Setup Complete");

    pinMode(DIN, OUTPUT);
    pinMode(LOAD, OUTPUT);
    pinMode(CLK, OUTPUT);

    delay(100);

    digitalWrite(LOAD, HIGH);
    max7219_message(SHUTDOWN, 0);

    delay(100);

    max7219_message(SCAN_LIMIT, 1);
    //max7219_message(DECODE_MODE, 0);
    max7219_message(SHUTDOWN, 1);
    //max7219_message(DISPLAY_TEST, 0);

    delay(20);

    //max7219_message(INTENSITY, 0x0F);

    for (int i = 1; i <= 8; i++) {
        max7219_message(i, 0x0);
    }

    Serial.println("cleared");

    delay(100);
    return;

    for (int i = 0; i < 255; i++) {
        max7219_message(0x01, i);
        Serial.println(i);
        delay(1000);
    }
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

void set_fader_level(Channel *ch, int level) {
    int fader_level = analogRead(ch->fader_in);

    while (!DIST(fader_level, level, LEVEL_OFFSET)) {
        if (fader_level > level) {
            digitalWrite(ch->fader_motor1, HIGH);
            digitalWrite(ch->fader_motor2, LOW);
        } else if (fader_level < level) {
            digitalWrite(ch->fader_motor1, LOW);
            digitalWrite(ch->fader_motor2, HIGH);
        }

        fader_level = analogRead(ch->fader_in);
    }

    digitalWrite(ch->fader_motor1, LOW);
    digitalWrite(ch->fader_motor2, LOW);
}

int update_button(Button *b) {
    int state = digitalRead(b->button_in);

    if (state == LOW && !b->pressed) {
        //Serial.println("Button press");
        b->pressed = TRUE;

        return TRUE;
    } else if (state == HIGH) {
        b->pressed = FALSE;
    }
    
    return FALSE;
}

void update_channel(Channel *ch) {
    char name[3];

    if (ch->channel_id < 10) {
        name[0] = '0';
        name[1] = ch->channel_id + '0';
    } else {
        name[0] = '1';
        name[1] = ch->channel_id - 10 + '0';
    }
    name[2] = '\0';

    int fader_level = analogRead(ch->fader_in);
    int digital_level = 1024 * get_channel_level(name);

    if (!DIST(fader_level, ch->fader_level, LEVEL_OFFSET)) {
        // physical fader has moved
        set_channel_level(name, (float)fader_level / 1024);
        ch->fader_level = fader_level;

        screen_timer = TIMEOUT;
    } else {
        if (!DIST(digital_level, ch->fader_level, LEVEL_OFFSET)) {
            // digital fader has moved
            set_fader_level(ch, digital_level);
            ch->fader_level = digital_level;
        }
    }

    char addr_solo[] = "/-stat/solosw/01";
    addr_solo[14] = name[0];
    addr_solo[15] = name[1];

    ch->solo.value = get_int(addr_solo);

    char addr_mute[] = "/ch/01/mix/on";
    addr_mute[4] = name[0];
    addr_mute[5] = name[1];

    ch->mute.value = !get_int(addr_mute);

    if (update_button(&ch->solo)) {
        //Serial.println("Solo click");
        ch->solo.value = !ch->solo.value;
        set_int(addr_solo, ch->solo.value);

        screen_timer = TIMEOUT;
    }

    if (update_button(&ch->mute)) {
        //Serial.println("Mute click");
        ch->mute.value = !ch->mute.value;

        set_int(addr_mute, !ch->mute.value);

        screen_timer = TIMEOUT;
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
    return;

    int upper_state = update_button(&upper);
    int lower_state = update_button(&lower);
    
    if (upper_state) {
        upper.value = TRUE;
        lower.value = FALSE;
        ch1.channel_id = 8;
    } else if (lower_state) {
        upper.value = FALSE;
        lower.value = TRUE;
        ch1.channel_id = 1;
    }

    digitalWrite(upper.led_enable, upper.value);
    digitalWrite(lower.led_enable, lower.value);
    
    update_channel(&ch1);

    digitalWrite(ch1.solo.led_enable, ch1.solo.value);
    digitalWrite(ch1.mute.led_enable, ch1.mute.value);

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


