#define TWITTER_OAUTH "___insert_your_oauth_string_here___"
#define LIB_DOMAIN "arduino-tweet.appspot.com"


#pragma SPARK_NO_PREPROCESSOR

// In case we're compiling locally:
#include "application.h"

void next_alarm_tweet();
void next_calm_tweet() ;
bool can_we_tweet();
int sendTweet(const char *msg);
void arming_tick(unsigned int now, bool circuitClosed);
void disarming_tick(unsigned int now, bool circuitClosed);



static const char* CALM_MSG1 = "Sure is quiet down here. Almost...too quiet...";
static const char* CALM_MSG2 = "I'm not sure I feel good about this, what if I actually catch something?";
static const char* CALM_MSG3 = "\"The supreme art of war is to subdue the enemy without fighting.\"― Sun Tzu, The Art of War";
static const char* CALM_MSG4 = "Still nothing, I guess no news is good news, right?";

static const char* ALARM_MSG1 = "Oh Wow. This is just awful. Why would you do this? This is terrible, just terrible. Does your mother know you built me?";
static const char* ALARM_MSG2 = "Mission Accomplished! - Come check on me!";
static const char* ALARM_MSG3 = "My only regret is that I have but one life to give. --\"The mouse I just killed\"";
static const char* ALARM_MSG4 = "All war is a symptom of man's failure as a thinking animal. ― John Steinbeck - Also I think I just caught a mouse.";


uint8_t calm_index = 0;
uint8_t calm_message_count = 4;
static const char* calm_messages[] = { CALM_MSG1, CALM_MSG2, CALM_MSG3, CALM_MSG4 };


uint8_t alarm_index = 0;
uint8_t alarm_message_count = 4;
static const char* alarm_messages[] = { ALARM_MSG1, ALARM_MSG2, ALARM_MSG3, ALARM_MSG4 };


int armed = 0;
int blink_led = 0;
int last_blink = 1;
unsigned int lastTweet = 0;
TCPClient client;

//it's easy when arming the trap for the wires to come apart / together, so we'll
//wait to make sure it's been at least ~10 seconds before really 'arming'
unsigned int arming_started = 0;
unsigned int arm_delay = 10000;

unsigned int disarming_started = 0;
unsigned int disarming_delay = 5000;

int alarm_red_value = 0;
int alarm_red_step = 5;


//lets not tweet more than once every six hours,
//we don't want to get too repetitive, 
//and we want to be good Twitter citizens
unsigned int minimumTweetDelay = 1000 * 60 * 60 * 6; 


void setup() {
    pinMode(D2, INPUT);
    pinMode(D7, OUTPUT);
    
    Serial.begin(9600);
}



void loop() {
    unsigned int now = millis();
    bool circuitClosed = (digitalRead(D2) == HIGH);

    //our arming / disarming clocks
    if (!armed && !circuitClosed) {
        arming_started = 0;
    }
    else if (armed && circuitClosed) {
        disarming_started = 0;
        RGB.color(0, 0, 0); //stealth!
    }

    //our truth table:
    if (circuitClosed && !armed) {
        arming_tick(now, circuitClosed);
    }
    else if (!circuitClosed && armed) {
        disarming_tick(now, circuitClosed);
    }
    else if (circuitClosed && armed) {
        next_calm_tweet();
    }
    else if (!circuitClosed && !armed) {
        next_alarm_tweet();
    }



    if (blink_led) {
        last_blink = !last_blink;

        //"breathe" the red led
        alarm_red_value += alarm_red_step;
        if (alarm_red_value >= 255) {
            alarm_red_step = -5;
        }
        else if (alarm_red_value <= 0) {
            alarm_red_step = 5;
        }

        digitalWrite(D7, (last_blink) ? HIGH : LOW);
        RGB.color(alarm_red_value, 0, 0);
        delay(100);
    }
}



void arming_tick(unsigned int now, bool circuitClosed) {
    if (arming_started == 0) {
        arming_started = now;
    }
    else if ((now - arming_started) > arm_delay) {
        
        //The trap was armed, and we waited at least 10 seconds before going 'live'
        Serial.println("Mousetrap is armed!");
        Spark.publish("Mousetrap/Status", "Armed!");
        armed = 1;
        blink_led = 0;
        arming_started = 0;
        digitalWrite(D7, HIGH);

        //stealth mode!
        RGB.control(true);
        RGB.color(0, 0, 0);
    }
    else if (circuitClosed) {
        uint8_t secondsLeft = (arm_delay - (now - arming_started)) / 1000;
        Serial.println("Arming in ... " + String(secondsLeft) + " seconds ");
        delay(1000);
    }
}

void disarming_tick(unsigned int now, bool circuitClosed) {
    //the trap has been opened, lets wait 5 seconds before notifying, in case it was a setup mistake
    if (disarming_started == 0) {
        disarming_started = now;
    }
    else if ((now - disarming_started) > disarming_delay) {
        //The trap was armed, and we waited at least 5 seconds before going 'live'

        // - Mousetrap was opened while armed!
        Spark.publish("Mousetrap/Status", "Triggered!");
        lastTweet = 0;
        armed = 0;
        blink_led = 1;
        disarming_started = 0;
        digitalWrite(D7, HIGH);

        //send the tweet
        next_alarm_tweet();
    }
    else if (!circuitClosed) {
        uint8_t secondsLeft = (disarming_delay - (now - disarming_started)) / 1000;
        Serial.println("Alarm will trigger in ... " + String(secondsLeft) + " seconds ");
        delay(1000);
    }
}

void next_alarm_tweet() {
    if (!can_we_tweet()) {
        return;
    }
    
    Serial.println("Alarm!");
    if (alarm_index > alarm_message_count) {
        alarm_index = 0;
    }
    
    sendTweet(alarm_messages[alarm_index]);
    alarm_index++;
}

void next_calm_tweet() {
    if (!can_we_tweet()) {
        return;
    }
    
    if (calm_index > calm_message_count) {
        calm_index = 0;
    }
    
    sendTweet(calm_messages[calm_index]);
    calm_index++;
}


//if it's been calm for more than XX hours, then we can tweet
bool can_we_tweet() {
    unsigned int now = millis();

    if (lastTweet == 0) {
        return true;
    }
    else {
        return ((now - lastTweet) >= minimumTweetDelay);
    }
}


int sendTweet(const char *msg) {
    if (!can_we_tweet()) {
        return 0;
    }

    Serial.println("Trying to Tweet...");

    //turn green when tweeting
    RGB.color(0, 0, 255);
  

    
    bool connected = client.connect(LIB_DOMAIN, 80);
    if (connected) {
        Serial.println("Connection success");
    }
    else {
        Serial.println("Connection failed");
        //client.stop();
        delay(1000);
        return 0;
    }
    
    client.flush();
    
    client.print("POST http://");
    client.print(LIB_DOMAIN);
    client.println("/update HTTP/1.0");
    client.println("Connection: close");
    client.println("Accept: text/html, text/plain");
    
    client.print("Content-Length: ");
    client.println(strlen(msg)+strlen(TWITTER_OAUTH)+14);
    client.println();
    client.print("token=");
    client.print(TWITTER_OAUTH);
    client.print("&status=");
    client.println(msg);
    
    delay(100);
    client.flush();
    delay(50);
    client.stop();
    delay(1);

    lastTweet = millis();
    
    Serial.println("Done tweeting...");
    Spark.publish("Mousetrap/Tweet", msg);
    
    //Serial.println("Done reading the result....");
    return 1;
}


