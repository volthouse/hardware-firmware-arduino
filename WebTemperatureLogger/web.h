// Web ****************************************************************
#include <avr/pgmspace.h>
#include "webpage.h"
#include "measure.h"

#define CHART_X_INTERVALS	  20
#define CHART_Y_INTERVALS	  13
#define CHART_TOP			      50
#define CHART_LEFT			    100
#define CHART_RIGHT			    600
#define CHART_BOTTOM		    350
#define CHART_X_STEP		    (CHART_RIGHT - CHART_LEFT) / CHART_X_INTERVALS
#define CHART_Y_STEP		    (CHART_BOTTOM - CHART_TOP) / (CHART_Y_INTERVALS - 1)
#define CHART_Y_ZERO 		250 + CHART_TOP

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 2, 86); // 192.168.2.86

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

class WebManager {
  private:
    int m_values[5];
    int m_temperatureLog[20];
    char m_ethBuffer[30];
    bool m_logState;
  public:
    void begin() {
      Rnd();
      
      memset(m_ethBuffer, 0, 10);
      
      m_logState = false;
      
      Ethernet.begin(mac, ip);
      server.begin();
    }
    
    void Rnd() {
      for(int i = 0; i < 5; i++) {
        m_values[i] = 0; //random(10) * 10;
        //Serial.println(m_values[i]);
      }
      
      for(int i = 0; i < 20; i++) {
        m_temperatureLog[i] = random(12) * 10 - 20;
      }
      
    }
    
    void sendPage1(EthernetClient* client) {
      
      // send a standard http response header
      client->println(F("HTTP/1.1 200 OK"));
      client->println(F("Content-Type: text/html"));
      client->println(F("Connection: close"));  // the connection will be closed after completion of the response
      //client->println("Refresh: 5");  // refresh the page automatically every 5 sec
      client->println();
      
      PGM_P page_pointer = Page1;
              
      for(;;) {
        unsigned char b = pgm_read_byte(page_pointer++);
        if (strncasecmp_P("%END",page_pointer,4)==0) {                  
          client->flush();
          client->stop();
          Rnd();
          break;							
        } else if (strncasecmp_P("%?", page_pointer, 2)==0) {
          page_pointer += 2;
          uint8_t idx = pgm_read_byte(page_pointer) - '0' - 1;

          char buffer[5] = { 0 };
          bin2asc(m_values[idx], buffer, 3);
          client->print(buffer);
          
          page_pointer++;
        } else if (strncasecmp_P("%LOGSTATE", page_pointer, 9)==0) {
          if(m_logState)
            client->print("checked");
          page_pointer += 9;
        } else if (strncasecmp_P("%FILES", page_pointer, 6)==0) {
          files(client);
          page_pointer += 6;          
        } else if (strncasecmp_P("%CHART", page_pointer, 6)==0) {
          page_pointer += 7;
          if (strncasecmp_P("HORIZONTAL_LINES", page_pointer, 16)==0) {
            chartHorizontalLines(client);
            page_pointer += 16;
          } else if (strncasecmp_P("VERTICAL_LINES", page_pointer, 6)==0) {
            chartVerticalLines(client);
            page_pointer += 14;
          } else if (strncasecmp_P("PATH", page_pointer, 4)==0) {
            chartPath(client);
            page_pointer += 4;
          } else if (strncasecmp_P("POINTS", page_pointer, 6)==0) {
            chartCircles(client);
            page_pointer += 6;
          } else if (strncasecmp_P("XLABELS", page_pointer, 7)==0) {
            chartXLabels(client);
            page_pointer += 7;
          } else if (strncasecmp_P("YLABELS", page_pointer, 7)==0) {
            chartYLabels(client);
            page_pointer += 7;
          }
        } else {                
          client->write(b);
        }
      }
    }
    
    void sendPage2(EthernetClient* client) {     
     
      client->println(F("HTTP/1.0 200 OK"));
      client->println(F("Content-Type: text/csv"));
      client->println(F("Connnection: close"));
      client->println(F("Content-disposition: attachment;filename=file.csv"));
      client->println();
      
      
      PGM_P page_pointer = Page2;
              
      for(;;) {
        unsigned char b = pgm_read_byte(page_pointer++);
        if (strncasecmp_P("%END",page_pointer,4)==0) {                  
          client->flush();
          client->stop();          
          break;							
        } else {                
          client->write(b);
        }
      }
    }
    
    void xmlResponse(EthernetClient* client) {
      client->println(F("HTTP/1.0 200 OK"));
      client->println("Content-Type: text/xml");
      client->println("Connection: keep-alive");
      client->println();
      // send XML file containing temperature
      
      client->print("<?xml version = \"1.0\" ?>");
      client->print("<inputs>");     
      client->print("<analog>");
      client->print(Measure.temperature);
      client->print("</analog>");
      client->print("</inputs>");
      client->flush();
      client->stop();
    }
    
    void dispatch() {
      EthernetClient client = server.available();
      if (client) {
        //Serial.println("new client");
        // an http request ends with a blank line
        static boolean currentLineIsBlank = true;
        if (client.connected()) {
          if (client.available()) {
            char c = client.read();
            Serial.write(c);
            
            m_ethBuffer[sizeof(m_ethBuffer) - 2] = c;
            
            if (strncasecmp_P(m_ethBuffer, PSTR("GET /file.txt"), 13)==0) {
              Serial.println("match file");
              sendPage2(&client);
            } else if (strncasecmp_P(m_ethBuffer, PSTR("GET /?SUB=Start+Log"), 19)==0) {
              m_logState = true;
            } else if (strncasecmp_P(m_ethBuffer, PSTR("GET /?SUB=Stopp+Log"), 19)==0) {
              m_logState = false;
            } else if (strncasecmp_P(m_ethBuffer, PSTR("GET /ajax_inputs"), 16)==0) {
              xmlResponse(&client);
            } else {
              // if you've gotten to the end of the line (received a newline
              // character) and the line is blank, the http request has ended,
              // so you can send a reply
              if (c == '\n' && currentLineIsBlank) {
                sendPage1(&client);
              }
              if (c == '\n') {
                // you're starting a new line
                currentLineIsBlank = true;
              }
              else if (c != '\r') {
                // you've gotten a character on the current line
                currentLineIsBlank = false;
              }
            }
            
            memmove(m_ethBuffer, m_ethBuffer + 1, sizeof(m_ethBuffer) - 2);
            
          }
        } else {
          // give the web browser time to receive the data
          //delay(1);
          // close the connection:
          client.stop();          
          Serial.println("client disconnected");
        }
      }
    }
    
    void chartVerticalLines(Stream* stream) {
      //<line x1 = '113' x2 = '113' y1 = '10' y2 = '380'></line>	
      uint16_t x = CHART_LEFT;
      for (uint8_t i = 0; i < CHART_X_INTERVALS; i++)
      {
        char buffer[100] = { 0 };        
        strcpy_P(buffer, PSTR("<line x1 = '????' x2 = '????' y1 = '????' y2 = '????'></line>"));
        bin2asc(x, &buffer[12], 4);
        bin2asc(x, &buffer[24], 4);
        
        bin2asc(CHART_TOP, &buffer[36], 4);
        bin2asc(CHART_BOTTOM, &buffer[48], 4);

        stream->println(buffer);

        x += CHART_X_STEP;
      }
    }

    void chartHorizontalLines(Stream* stream) {
      //<line x1 = '86' x2 = '1063' y1 = '40' y2 = '40'></line>

      uint16_t y = CHART_TOP;
      for (uint8_t i = 0; i < CHART_Y_INTERVALS; i++)
      {
        char buffer[100] = { 0 };        
        strcpy_P(buffer, PSTR("<line x1 = '????' x2 = '????' y1 = '????' y2 = '????'></line>"));
        bin2asc(CHART_LEFT, &buffer[12], 4);
        bin2asc(CHART_RIGHT - CHART_X_STEP, &buffer[24], 4);
        bin2asc(y, &buffer[36], 4);
        bin2asc(y, &buffer[48], 4);

        stream->println(buffer);

        y += CHART_Y_STEP;
      }
    }

    void chartCircles(Stream* stream) {
      //<circle cx='113' cy='40' data-value='7.2' r='5'></circle>

      uint16_t x = CHART_LEFT;
      for (uint8_t i = 0; i < CHART_X_INTERVALS; i++)
      {        
        char buffer[100] = { 0 };        
        strcpy_P(buffer, PSTR("<circle id = 'dot????' cx='????' cy='????' data-value='7.2' r='5'></circle>"));
        int y = m_temperatureLog[i] * -2.5 + 250 + CHART_TOP;

        bin2asc(i, &buffer[17], 4);
        bin2asc(x, &buffer[27], 4);
        bin2asc(y, &buffer[37], 4);

        stream->println(buffer);

        x += CHART_X_STEP;
      }
    }

    void chartXLabels(Stream* stream) {
      //<text x='113' y='400'>1</text>

      uint16_t x = CHART_LEFT;
      for (uint8_t i = 0; i < CHART_X_INTERVALS; i++)
      {		
        char buffer[100] = { 0 };        
        strcpy_P(buffer, PSTR("<text x='????' y='????'>??</text>"));
        bin2asc(x, &buffer[9], 4);
        bin2asc(CHART_BOTTOM + 25, &buffer[18], 4);
        bin2asc(i + 1, &buffer[24], 2);
  
        stream->println(buffer);
        x += CHART_X_STEP;
      }
    }

    void chartYLabels(Stream* stream) {
    //<text x='80' y='45'>80</text>

      uint16_t y = CHART_TOP + 5;
      for (uint8_t i = 0; i < CHART_Y_INTERVALS; i++)
      {		
        char buffer[100] = { 0 };        
        strcpy_P(buffer, PSTR("<text x='????' y='????'>???</text>"));
        bin2asc(80, &buffer[9], 4);
        bin2asc(y, &buffer[18], 4);
        bin2asc(100 - i * 10, &buffer[24], 3);

        stream->println(buffer);

        y += CHART_Y_STEP;
      }
    }

    void chartPath(Stream* stream) {
      /*<path class = 'first_set' d = '
        M113, 360
        
        L113, 40
        
        L1063, 325
        L1063, 360
        Z'
      < / path>*/
      
      uint16_t x = CHART_LEFT;
   
      char mbuffer[15] = { 0 };        
      strcpy_P(mbuffer, PSTR("M????, ????"));
      
      bin2asc(x, &mbuffer[1], 4);
      bin2asc(CHART_Y_ZERO, &mbuffer[7], 4);
      stream->println(mbuffer);
      
      for (uint8_t i = 0; i < CHART_X_INTERVALS; i++)
      {        
        char buffer[15] = { 0 };        
        strcpy_P(buffer, PSTR("L????, ????"));
        int y = m_temperatureLog[i] * -2.5 + 250 + CHART_TOP;

        bin2asc(x, &buffer[1], 4);
        bin2asc(y, &buffer[7], 4);

        stream->println(buffer);

        x += CHART_X_STEP;
      }
      
      char lbuffer[15] = { 0 };        
      strcpy_P(lbuffer, PSTR("L????, ????"));
      bin2asc(CHART_RIGHT - CHART_X_STEP, &lbuffer[1], 4);
      bin2asc(CHART_Y_ZERO, &lbuffer[7], 4);
      stream->println(lbuffer);
    }
    
    void files(Stream* stream) {
    //<option>file.csv</option>      
      for (uint8_t i = 0; i < 10; i++)
      {		
        char buffer[50] = { 0 };        
        strcpy_P(buffer, PSTR("<option>????.csv</option>"));
        bin2asc(i, &buffer[8], 4);        
        stream->println(buffer);
      }
    }
};

WebManager Web;
