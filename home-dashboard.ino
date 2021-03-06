// This #include statement was automatically added by the Particle IDE.
#include "SparkJson/SparkJson.h"

// This #include statement was automatically added by the Particle IDE.
#include "HttpClient/HttpClient.h"

// This #include statement was automatically added by the Particle IDE.
#include "LiquidCrystal_I2C_Spark/LiquidCrystal_I2C_Spark.h"

LiquidCrystal_I2C *lcd;

HttpClient http;
http_request_t request;
http_response_t response;
http_header_t headers[] = {
    { "Accept" , "*/*"},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};


int lastSecond = 0;

int8_t lastD2=0;
int8_t lastD3=0;
volatile int c=0;
int lastCount=0;
int lastD4=0;


void blinkDisplay();
Timer timer(500, blinkDisplay);

void setup(void)
{
  Serial.begin(9600);
  lcd = new LiquidCrystal_I2C(0x27, 16, 2);
  lcd->init();
  lcd->backlight();
  lcd->clear();
  
  
  lcd->print("Home Dashboard");
  
  pinMode(D2, INPUT);
  pinMode(D3, INPUT);
  pinMode(D4, INPUT);
  
  attachInterrupt(D2, encoderChangeD2, CHANGE);
  attachInterrupt(D3, encoderChangeD3, CHANGE);
  
  //degreesC
  { uint8_t rows[]={0x18,0x18,0x0,0xe,0x8,0xe,0x0,0x0};
    lcd->createChar(1, rows);
  }
  //pounds
  { uint8_t rows[]={0x6,0x9,0x8,0x1e,0x8,0x8,0x1f,0x0};
    lcd->createChar(2, rows);
  }
  //plug
  { uint8_t rows[]={0xa,0xa,0x1f,0x1f,0xe,0x4,0x4,0x0};
    lcd->createChar(3, rows);
  }
  //clock
  { uint8_t rows[]={0xe,0x15,0x15,0x17,0x11,0x11,0xe,0x0};
    lcd->createChar(4, rows);
  }
  //down arrow
  { //uint8_t rows[]={0x0,0x4,0x4,0x4,0x1f,0xe,0x4,0x0};
    uint8_t rows[]={ 0b00100,
	0b00100,
	0b11111,
	0b01110,
	0b00100,
	0b00000,
	0b11111,
	0b00000 };
  
    lcd->createChar(5, rows);
  }
  //warning
  { 
    uint8_t rows[]={ 0b11111,
	0b11011,
	0b11011,
	0b11011,
	0b11111,
	0b11011,
	0b11111,
	0b00000 };
  
    lcd->createChar(6, rows);
  }
  	
  
  
  
}

void encoderChangeD2()
{   lastD2=!digitalRead(D2);
    encoderChange();
}
void encoderChangeD3()
{   lastD3=!digitalRead(D3);
    encoderChange();
}
void encoderChange()
{   int8_t encoder=(lastD2&1) | ((lastD3&1)<<1);
    int8_t v=read_encoder(encoder);
    if (v==1) c++;
    if (v==-1) c--;
}

/* returns change in encoder state (-1,0,1) */
int8_t read_encoder(int8_t v)
{
  static uint8_t old_AB = 0;
  int8_t enc_states[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
  /**/
  old_AB <<= 2;                   //remember previous state
  old_AB |= ( v & 0x03 );  //add current state
  return ( enc_states[( old_AB & 0x0f )]);
}

const int SOURCE_LENGTH=200;
char jsonSource[SOURCE_LENGTH];
void renderElectricity(JsonObject &root);
void renderOther();
void renderSensorError(char* msg);
void renderError(char *msg);
JsonObject& getDashboardState(StaticJsonBuffer<500> &);
volatile int lcdBusy=0;

int mode=0;
const int MODE_ELEC=0;
const int MODE_OTHER=1;
const int MODE_MAX=2;
const int MODE_ERROR=3;

int lastRefresh=Time.now();

    
void loop(void)
{
    int d4=!digitalRead(D4);
    
    char s[64];
    sprintf(s,"mode: %d ",mode);
    Serial.println(s);
    
    int click=0;
    if (d4!=lastD4)
    {   lastD4=d4;
        if (d4)
        {   click=1;
            Serial.println("click");
            
            if (mode==MODE_ERROR )
            {   mode=MODE_ELEC;
                timer.stop();
                lcd->clear();
                lcd->backlight();
            }
        }
    }
    
    if (lastCount!=c && mode!=MODE_ERROR)
    {
        int diff=(c-lastCount)/4;
        if (diff!=0)
        {
            lastCount=c;
            int newMode=(mode+diff)%MODE_MAX;
            if (newMode<0) newMode+=MODE_MAX;
            mode=newMode;
            lastRefresh=0;
            lcd->clear();
        }
    }
    
    
    
    int now=Time.now();
    if (now-lastRefresh>10)
    {
        lastRefresh=now;
        
        StaticJsonBuffer<500> jsonBuffer;
        JsonObject& root = getDashboardState(jsonBuffer);
        if (!root.success())
        {   
            renderError("\x06 Connecting...");
        }
        else
        {   static int lastError=0;
            static char lastErrorMsg[128];
            const char* errorMsg=root["error"];
            int error=strlen(errorMsg)>0;
            if (error && !lastError)
            {   strcpy(lastErrorMsg,errorMsg);
                mode=MODE_ERROR;
                timer.start();
            }
            if (mode==MODE_ERROR && ( (lastError && !error) ))
            {   lastError=error;
                mode=MODE_ELEC;
                timer.stop();
                lcd->backlight();
            }
            lastError=error;

            if (mode==MODE_ELEC)
            {   renderElectricity(root);
            }
            else if (mode==MODE_OTHER)
            {   renderOther();
            }
            else if (mode==MODE_ERROR)
            {   renderSensorError(lastErrorMsg);
            }
        }

    }
    
    delay(100);
    
}

JsonObject& getDashboardState(StaticJsonBuffer<500> &jsonBuffer)
{
    request.hostname = "192.168.1.1";
    request.port = 8080;
    request.path = "/polestar/scripts/execute/C6E124D6C8125C6A";
    http.get(request, response, headers);
    String json=response.body;
    
    json.toCharArray(jsonSource,SOURCE_LENGTH);
    JsonObject& root = jsonBuffer.parseObject(jsonSource);
    return root;
}

void renderOther()
{   renderError("Other!");
}

void renderSensorError(char* msg)
{
    if (!lcdBusy)
    {   lcdBusy=1;
    
        lcd->setCursor(0,0);
        lcd->print("\x06 Error on:        ");
        lcd->setCursor(0,1);
        char s[128];
        sprintf(s,"%.16s",msg);
        lcd->print(s);
        
        lcdBusy=0;
    }
}

void renderError(char *msg)
{
    if (!lcdBusy)
    {   lcdBusy=1;
        lcd->clear();
        lcd->setCursor(0,0);
        lcd->print(msg);
        lcdBusy=0;
    }
    
}


void renderElectricity(JsonObject &root)
{
    JsonObject& elec = root["elec"];
    long c=elec["current"];
    long b=elec["baseline"];
    long avg=elec["lastHour"];
    double te=elec["energyToday"];
    double tc=elec["costToday"];

    char s[64];
    sprintf(s,"\x03%dW\x7E%dW   ",c,avg);
    lcd->setCursor(0,0);
    lcd->print(s);
        
    sprintf(s,"\x05%3dW ",b);
    lcd->setCursor(11,0);
    lcd->print(s);

    sprintf(s,"\x04%5.2fkWh  \x02%0.2f",te,tc);
    lcd->setCursor(0,1);
    lcd->print(s);
}

void blinkDisplay()
{   static int toggle;
    if (!lcdBusy)
    {   lcdBusy=1;
        toggle=!toggle;
    
        if (toggle)
        {   lcd->noBacklight();
        }
        else
        {   lcd->backlight();
        }
        lcdBusy=0;
    }
}
    
