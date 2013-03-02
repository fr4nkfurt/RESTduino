#include <aJSON.h>

#define DEBUG true

#include <SPI.h>
#include <Ethernet.h>
#include <Bounce.h>
#include <EEPROMEx.h>

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
byte ip[] = {
  192,168,1,128};
#if defined(ARDUINO) && ARDUINO >= 100
EthernetServer server(80);
#else
Server server(80);
#endif

int address = 10;
const int numberOfInputs = 2 ;
typedef struct
{
  int input;
  int output;
  int type;
  int state;
  char name[20];
}  
input_type;

input_type config[numberOfInputs];

Bounce *bouncer;

void loadConfig (){
  EEPROM.readBlock(address, config);
#if DEBUG
  Serial.println("Leyendo eprom."); 
  for (int i = 0; i < numberOfInputs; i++) {
    Serial.println(config[i].input); 
    Serial.println(config[i].output);
    Serial.println(config[i].type);
    Serial.println(config[i].state); 
    Serial.println(config[i].name);   
  }
#endif

}

void createConfig () {
  config[0].input = 4;
  config[0].output = 8;
  config[0].type = 0;
  config[0].state = 1;
  String nombre = "luz techo";
  nombre.toCharArray(config[0].name,20);

  config[1].input = 2;
  config[1].output = 3;
  config[1].type = 1;
  config[1].state = 0;
  nombre = "luz dimmer";
  nombre.toCharArray(config[1].name,20);

  
#if DEBUG
    Serial.println("Escribiendo eprom."); 
#endif

  EEPROM.writeBlock(address, config);
}

String configToJson() {
  String configStr = String();
  configStr += "{\"";
  configStr += "config\" : [";
  for (int i = 0; i < numberOfInputs; i++) {
    configStr += "{\"input\":";
    configStr += config[i].input;
    configStr += ",";
    configStr += "\"output\":";
    configStr += config[i].output;
    configStr += ",";
    configStr += "\"state\":";
    configStr += config[i].state;
    configStr += ",";
    configStr += "\"type\":";
    configStr += config[i].type;
    configStr += ",";
    configStr += "\"name\":\"";
    configStr += config[i].name;
    configStr += "\"}";
    if (i < (numberOfInputs - 1)){
        configStr += ",";
    }
  
}
  configStr += "]}";
  
#if DEBUG
  Serial.println("Config output: "); 
  Serial.println(configStr);
#endif
  return configStr;
}

void updateOutput(int pin, int state){

  for (int i = 0; i < numberOfInputs; i++) {
    if (config[i].output==pin){
      if (state){
        digitalWrite(pin, HIGH);
        config[i].state=1;
      }
      else{
        digitalWrite(pin, LOW);
        config[i].state=0;
      }      
    }
  }
  //esta es la linea que hace que se guarden los estados en la eprom
  //EEPROM.updateBlock(address, config);
}

void setup()
{
#if DEBUG
  Serial.begin(9600);
  Serial.println("---"); 
#endif
  //createConfig(); 
  loadConfig ();

  bouncer = (Bounce *) malloc(sizeof(Bounce) * numberOfInputs);

  //Rutina de inicializacion de estado y configuracion
  for (int i = 0; i < numberOfInputs; i++) {

    pinMode(config[i].input , INPUT); 
    //digitalWrite(config[i].input, HIGH); //se utilizara el pull up resistor de 20k interno
    pinMode(config[i].output, OUTPUT);  

    if (config[i].type==0){
      if (config[i].state){
        digitalWrite(config[i].output, HIGH); 

      } 
      else {
        digitalWrite(config[i].output, LOW); 

      }
      bouncer[i] = Bounce( config[i].input,100 ); 
    }  


  }
  //Fin inicializacion

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
#if DEBUG
  Serial.println("---"); 
#endif
}

//  url buffer size
#define BUFSIZE 128

// Toggle case sensitivity
#define CASESENSE true

void loop()
{
  //·····································Comienzo codigo input handling·····································//  
  for (int i = 0; i < numberOfInputs; i++) {

    if (bouncer[i].update()){
#if DEBUG
      Serial.println("cambio");
#endif
      if (bouncer[i].read()){
        updateOutput(config[i].output, 1);     
      }
      else{
        updateOutput(config[i].output, 0);
      }
    }
  }


  //·····································Comienzo codigo restDuino·····································//
  char clientline[BUFSIZE];
  char paramsline[BUFSIZE];
  int index = 0;
  // listen for incoming clients
#if defined(ARDUINO) && ARDUINO >= 100
  EthernetClient client = server.available();
#else
  Client client = server.available();
#endif
  if (client) {
    memset(clientline, 0, BUFSIZE);
    memset(paramsline, 0, BUFSIZE);

    //  reset input buffer
    index = 0;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();

        //  fill url the buffer
        if(c != '\n' && c != '\r' && c != '?' && index < BUFSIZE){ // Reads until either an eol character is reached or the buffer is full
          clientline[index++] = c;
          continue;
        } 
        
       
       if (c == '?'){//HAY PARAMETROS
         clientline[index++] = ' '; // Se agrega un espacio al final porque este caracter se usa para cortar el string luego, solo aplica para cuando hay parametros, con ? en el url
         index = 0; 
         c = client.read();
         while (c != '\n' && c != '\r' && c != ' ' && index < BUFSIZE){   
           paramsline[index++] = c;
            c = client.read();
         }  
     }


#if DEBUG
        Serial.print("client available bytes before flush: "); 
        Serial.println(client.available());
        Serial.print("request = "); 
        Serial.println(clientline);
        Serial.print("params = "); 
        Serial.println(paramsline);
#endif

        // Flush any remaining bytes from the client buffer
        client.flush();

#if DEBUG
        // Should be 0
        Serial.print("client available bytes after flush: "); 
        Serial.println(client.available());
#endif

        //  convert clientline into a proper
        //  string for further processing
        String urlString = String(clientline);
        String urlStringParams = String(paramsline);
        
        //  extract the operation
        String op = urlString.substring(0,urlString.indexOf(' '));

        //  we're only interested in the first part...
        urlString = urlString.substring(urlString.indexOf('/'), urlString.indexOf(' ', urlString.indexOf('/')));
        
        
        //  se truncan los parametros pasados por get
        /*
        if (urlString.indexOf('?')!=-1){
          urlString=urlString.substring(urlString.indexOf('/'), urlString.indexOf('?', urlString.indexOf('/')));
        }*/

        //  put what's left of the URL back in client line
#if CASESENSE
        urlString.toUpperCase();
#endif
        urlString.toCharArray(clientline, BUFSIZE);

        //  get the first two parameters
        char *pin = strtok(clientline,"/");
        char *value = strtok(NULL,"/");

        //  this is where we actually *do something*!
        char outValue[10] = "MU";
        String jsonOut = String();
        if (urlString!="/FAVICON.ICO"){

        if (urlString!="/SAVECONFIG"){
          if (urlString!="/CONFIG"){
            if(pin != NULL){
#if DEBUG
        Serial.print("Selected pin: "); 
        Serial.println();
#endif
              if(value != NULL){
#if DEBUG
        Serial.print("Selected value: "); 
        Serial.println(value);
#endif

#if DEBUG
                //  set the pin value
                Serial.println("setting pin");
#endif

                //  select the pin
                int selectedPin = atoi (pin);
#if DEBUG
                Serial.println(selectedPin);
#endif
                //Revisar si esta bien que cambie los tipos de output input
                //  set the pin for output
                //pinMode(selectedPin, OUTPUT);

                //  determine digital or analog (PWM)
                if(strncmp(value, "HIGH", 4) == 0 || strncmp(value, "LOW", 3) == 0){

#if DEBUG
                  //  digital
                  Serial.println("digital");
#endif

                  if(strncmp(value, "HIGH", 4) == 0){
#if DEBUG
                    Serial.println("HIGH");
#endif
                    //digitalWrite(selectedPin, HIGH);
                    updateOutput(selectedPin, 1);
                  }

                  if(strncmp(value, "LOW", 3) == 0){
#if DEBUG
                    Serial.println("LOW");
#endif
                    //digitalWrite(selectedPin, LOW);
                    updateOutput(selectedPin, 0);
                  }

                } 
                else {

#if DEBUG
                  //  analog
                  Serial.println("analog");
#endif
                  //  get numeric value
                  int selectedValue = atoi(value);              
#if DEBUG
                  Serial.println(selectedValue);
#endif
                  analogWrite(selectedPin, selectedValue);

                }

                //  return status
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: text/html");
                client.println();

              } 
              else {
#if DEBUG
                //  read the pin value
                Serial.println("reading pin");
#endif

                //  determine analog or digital
                if(pin[0] == 'a' || pin[0] == 'A'){

                  //  analog
                  int selectedPin = pin[1] - '0';

#if DEBUG
                  Serial.println(selectedPin);
                  Serial.println("analog");
#endif

                  sprintf(outValue,"%d",analogRead(selectedPin));

#if DEBUG
                  Serial.println(outValue);
#endif

                } 
                else if(pin[0] != NULL) {

                  //  digital
                  int selectedPin = pin[0] - '0';

#if DEBUG
                  Serial.println(selectedPin);
                  Serial.println("digital");
#endif

                  //pinMode(selectedPin, INPUT);

                  int inValue = digitalRead(selectedPin);

                  if(inValue == 0){
                    sprintf(outValue,"%s","OFF");
                    //sprintf(outValue,"%d",digitalRead(selectedPin));
                  }

                  if(inValue == 1){
                    sprintf(outValue,"%s","ON");
                  }


                }

                //  assemble the json output
                /*jsonOut += "{\"";
                jsonOut += pin;
                jsonOut += "\":\"";*/
                jsonOut += "<state>";
                jsonOut += outValue;
                jsonOut += "</state>";
                /*jsonOut += "\"}";*/
#if DEBUG
                  Serial.println(outValue);
#endif
                //  return value with wildcarded Cross-origin policy
                client.println("HTTP/1.1 200 OK");
                client.println("Content-Type: application/xml");
                client.println("Access-Control-Allow-Origin: *");
                client.println();
                client.println(jsonOut);
              }
            } 
            else {

              //  error
#if DEBUG
              Serial.println("erroring");
#endif
              client.println("HTTP/1.1 404 Not Found");
              client.println("Content-Type: text/html");
              client.println();

            }
          }
          else{
            //se solicito /CONFIG
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Access-Control-Allow-Origin: *");
            client.println();
            client.println(configToJson());
          }
        }
        else{
          //se solicito /SAVECONFIG
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Access-Control-Allow-Origin: *");
          client.println();
          client.println("Configuracion guardada con exito");
          
          while(client.available())
          {
             Serial.write(client.read());
          }
        }
        }
        break;
      }
    }

    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    //client.stop();
    client.stop();
    while(client.status() != 0){
      delay(5);
    }
  }
}

