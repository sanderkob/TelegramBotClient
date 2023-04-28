/**
    \file JsonWebClient.cpp
    \brief Implementation of a simple web client receiving json
           uses an underlying implementation of Client interface.
           It implements a pseudo background behavior by providing a loop()
           method that can be polled and calls callback on receiving valid data.

    Part of TelegramBotClient (https://github.com/schlingensiepen/TelegramBotClient)
    Jörn Schlingensiepen <joern@schlingensiepen.com>
*/


#include "JsonWebClient.h"
unsigned long markWdt = 0;

JsonWebClient::JsonWebClient (
  Client* netClient,
  String host,
  int port,
  void* callBackObject,
  JWC_CALLBACK_MESSAGE_SIGNATURE,
  JWC_CALLBACK_ERROR_SIGNATURE)
{
  DOUT (("New JsonWebClient"));
  this->NetClient = netClient;
  this->State = JwcClientState::Unconnected;
  this->Host = host;
  this->Port = port;
  this->CallBackObject = callBackObject;
  this->callbackSuccess = callbackSuccess;
  this->callbackError = callbackError;
}
void JsonWebClient::reConnect()
{
  DOUT (("reConnect"));
#ifdef ESP8266
  DOUTKV(("ESP.getFreeHeap()"), ESP.getFreeHeap());
#endif
//  if (NetClient->connected())
//  {
    DOUT (("stop"));
    stop();
//  }
  DOUT (("connecting ..."));
            //  this->State =
            //    (NetClient->connect(this->Host.c_str(), this->Port) == 1)
            //    ? JwcClientState::Connected
            //    : JwcClientState::Unconnected;
  if (NetClient->connect(this->Host.c_str(), this->Port) == 1)
  {
    this->State = JwcClientState::Connected;
    DOUT (("connected"));
    long ContentLength = JWC_BUFF_SIZE;
    bool HttpStatusOk = false;    
  }
  else
  {
    this->State = JwcClientState::Unconnected;
    NetClient->stop();
    DOUT (("not connected (yet)")); 
  }
}

bool JsonWebClient::stop()
{
  DOUT(("stop"));
  NetClient->stop();
  State = JwcClientState::Unconnected;
  return true;
}

bool JsonWebClient::processHeader()
{
  String header = NetClient->readStringUntil('\n');
  DOUTKV(("Got header"), header);
  if (header.startsWith("Content-Length:"))
  {
    ContentLength = header.substring(15).toInt(); //TODO check for error
    DOUTKV (("ContentLength"), ContentLength);
  }
  if (header.startsWith(("HTTP/1.1 200 OK")))
  {
    HttpStatusOk = true;
    DOUTKV (("HttpStatusOk"), HttpStatusOk);
  }
  if (header == "\r") return false; // End of headers by empty line --> http
  return true;
}
bool JsonWebClient::processJson()
{
  if (!HttpStatusOk)
  {
    DOUT(("!HttpStatusOk"));
    if (callbackError != 0 && CallBackObject != 0)
      callbackError(this->CallBackObject, JwcProcessError::HttpErr, this->NetClient);
    stop();
    return false;
  }
  DOUT(("Parsing JSON"));
  if (ContentLength > JWC_BUFF_SIZE)
  {
    DOUT(("Message to big to parse"));
    if (callbackError != 0 && CallBackObject != 0)
      callbackError(this->CallBackObject, JwcProcessError::MsgTooBig, this->NetClient);
    stop();

    return false;
  }
  DynamicJsonDocument jsonBuffer (JWC_BUFF_SIZE);
  JsonObject payload = jsonBuffer.to<JsonObject>();
  deserializeJson(jsonBuffer, *NetClient);
  // JsonObject payload = jsonBuffer.parse(*NetClient); //ArduinoJson v5
  // if (!payload.success())//ArduinoJson v5
  if (jsonBuffer.isNull()) 
  {
    DOUT(("Skip message, JSON error"));
    if (callbackError != 0 && CallBackObject != 0)
      callbackError(this->CallBackObject, JwcProcessError::MsgJsonErr, this->NetClient);
    stop();
    return false;
  }

  DOUT(("Message successfully parsed."));
//  payload.printTo(Serial); //ArduinoJson v5
serializeJson(jsonBuffer, Serial);
//  Serial.println("" );

  if (callbackSuccess != 0 && CallBackObject != 0)
    callbackSuccess(this->CallBackObject, JwcProcessError::Ok, payload);
  State = JwcClientState::Unconnected;
  return true;
}
bool JsonWebClient::loop()
{
  bool res = false;
  	if (!markWdt && millis()-markWdt > CONNECT_WDT)
  	{
  		DOUT(("Client timeout, setting to JwcClientState::Unconnected"));
  		State = JwcClientState::Unconnected; // force new polling sequence
  		return res;
  	}  
  if (State == JwcClientState::Unconnected) return res;
  if (!NetClient->connected() && NetClient->available() == 0)
  {
    DOUT(("Client was not connected, setting to JwcClientState::Unconnected"));
    State = JwcClientState::Unconnected;
    return res;
  }
  if (State == JwcClientState::Connected) return res;
  while (NetClient->available() > 0 && State != JwcClientState::Unconnected)
  {
    res = true;
    DOUT (("Received data"));
    switch (State)
    {
      case JwcClientState::Waiting : {
          State = JwcClientState::Headers; DOUT ("Switch State to headers");
          break;
        }
      case JwcClientState::Headers : {
          if (!processHeader()) {
            State = JwcClientState::Json;  DOUT ("Switch State to json");
          }
          break;
        }
      case JwcClientState::Json  : {
          processJson();
          break;
        }
    }
  }
  if (!NetClient->connected() && NetClient->available() == 0)
  {
    DOUT(("Client is not connected, setting to JwcClientState::unconnected"));
    State = JwcClientState::Unconnected;
  }
  return res;
}

JwcClientState JsonWebClient::state()
{
  return State;
}

bool JsonWebClient::fire (String commands[], int count)
{
  markWdt = millis();
  DOUT (("Fire"));
  reConnect();
//  if (!NetClient->connected()) reConnect();
  if (State != JwcClientState::Connected) return false;
  if (!NetClient->connected()) return false;
  DOUTKV (("count"), count);
  for (int i = 0; i < count; i++)
  {
    DOUTKV (("command"), commands[i]);
    NetClient->println(commands[i]);
  }
  NetClient->flush();
  State = JwcClientState::Waiting;
  loop();
  return true; // not relevant, since the return value is not used?
}

