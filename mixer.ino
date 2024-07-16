#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <OSCMessage.h>

#define LOAD  A2
#define CLK   A0
#define DIN   A1

#define UP    3
#define DOWN  4

#define IN1   4
#define IN2   5
#define IN3   6
#define IN4   7

#define FORWARD 8
#define BACKWARD 9 

#define DECODE        0x09                        
#define INTENSITY     0x0a                       
#define SCAN_LIMIT    0x0b                       
#define SHUTDOWN      0x0c                      
#define DISPLAY_TEST  0x0f  

#define SLIDER_1      A0

typedef union {
    int32_t value;
    char bytes[4];
} IntBytes;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(10, 1, 1, 30);
unsigned int local_port = 9000;

IPAddress mixer_ip(10, 1, 1, 197);
unsigned int mixer_port = 10024;

EthernetUDP udp;

void setup() {
    Serial.begin(9600);
    Ethernet.begin(mac, ip);
    udp.begin(local_port);

    pinMode(FORWARD, INPUT_PULLUP);
    pinMode(BACKWARD, INPUT_PULLUP);

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    Serial.println("Setup Complete");
}

char packet_buffer[UDP_TX_PACKET_MAX_SIZE];

float get_channel_level(const char *channel) {
    char addr[] = "/ch/01/mix/fader";
    addr[4] = channel[0];
    addr[5] = channel[1];
    OSCMessage msg(addr);

    if (!udp.beginPacket(mixer_ip, mixer_port)) {
        Serial.println("Error creating udp packet");
        return 0;
    }

    msg.send(udp);

    if (!udp.endPacket()) {
        Serial.println("Error sending udp packet");
        return 0;
    }

    msg.empty();
    delay(20);

    int packet_size = udp.parsePacket();
    if (!packet_size) {
        Serial.println("No packet recieved");
        return;
    }
    
    while (packet_size--) {
        msg.fill(udp.read());
    }

    if (!msg.isFloat(0)) {
        Serial.print("Channel ");
        Serial.print(channel);
        Serial.println(" did not return a float level");
        return 0;
    }
    return msg.getFloat(0);
}

void set_channel_level(const char *channel, float level) {
    char addr[] = "/ch/01/mix/fader";
    addr[4] = channel[0];
    addr[5] = channel[1];

    OSCMessage msg(addr);
    msg.add(level);

    if (!udp.beginPacket(mixer_ip, mixer_port)) {
        Serial.println("Error creating udp packet");
        return 0;
    }

    msg.send(udp);

    if (!udp.endPacket()) {
        Serial.println("Error sending udp packet");
        return 0;
    }

    msg.empty();
}

void write_data(byte addr, byte data) {
    digitalWrite(LOAD, LOW);
    shiftOut(DIN, CLK, MSBFIRST, addr);
    shiftOut(DIN, CLK, MSBFIRST, data);
    digitalWrite(LOAD, HIGH);
}

void loop() {
    int slider1 = analogRead(SLIDER_1);
    set_channel_level("01", (float)slider1 / 1024);

    if (digitalRead(FORWARD) == LOW) {
        digitalWrite(IN1, HIGH);
    } else {
        digitalWrite(IN1, LOW);
    }

    if (digitalRead(BACKWARD) == LOW) {
        digitalWrite(IN2, HIGH);
    } else {
        digitalWrite(IN2, LOW);
    }

    delay(20);
}
