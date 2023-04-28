#include "TelegramBotClient.h"

unsigned long markPollWDT = 0;

TelegramBotClient::TelegramBotClient (
  String token,
  Client& sslPollClient,
  Client& sslPostClient,
  TBC_CALLBACK_RECEIVE_SIGNATURE,
  TBC_CALLBACK_ERROR_SIGNATURE
)
{
  DOUT (F("New TelegramBotClient"));
  this->Parallel = (sslPostClient != sslPollClient);
  this->SslPollClient = new JsonWebClient(
    &sslPollClient, TELEGRAMHOST, TELEGRAMPORT, this,
    callbackPollSuccess, callbackPollError);
  this->SslPostClient = new JsonWebClient(
    &sslPostClient, TELEGRAMHOST, TELEGRAMPORT, this,
    callbackPostSuccess, callbackPostError);
  this->Token = String(token);
  DOUTKV (F("Token"), this->Token);
  this->setCallbacks(
    callbackReceive,
    callbackError);
}
TelegramBotClient::~TelegramBotClient()
{
  delete( SslPollClient );
  delete( SslPostClient );
}

void TelegramBotClient::setCallbacks (
  TBC_CALLBACK_RECEIVE_SIGNATURE,
  TBC_CALLBACK_ERROR_SIGNATURE)
{
  DOUT (F("setCallbacks"));
  this->callbackReceive = callbackReceive;
  this->callbackError = callbackError;
}

void TelegramBotClient::begin(
  TBC_CALLBACK_RECEIVE_SIGNATURE,
  TBC_CALLBACK_ERROR_SIGNATURE)
{
  markPollWDT = millis();
  setCallbacks(
    callbackReceive,
    callbackError);
}


bool TelegramBotClient::loop()
{
  if (millis()-markPollWDT > POLL_WDT)
  {
    Serial.println(F("Restarting..."));
    ESP.restart();
  }
  if (msgCount > 0 &&  (SslPostClient-> state() == JwcClientState::Unconnected
      || Parallel)
     )
  {
    DOUTKV(F("startPosting Outpointer"), msgOutPointer);
    DOUTKV(F("startPosting msgCount"), msgCount);
    startPosting(msgStore[msgOutPointer]);
    msgOutPointer++;
    msgOutPointer %= 5;
    msgCount--;
    return true;
  }
   
  SslPollClient->loop();
  SslPostClient->loop();

  if (
    SslPollClient->state() == JwcClientState::Unconnected
    &&
    ( SslPostClient-> state() == JwcClientState::Unconnected
      || Parallel)
     )
  {
    startPolling();
    return true;
  }
  return false;
}

void TelegramBotClient::startPolling()
{
  DOUT(F("startPolling"));
  String httpCommands[] =
  {
    "GET /bot"
    + String(Token)
    + "/getUpdates?limit=1&offset="
    + String(LastUpdateId)
    + "&timeout="
    + String(POLLINGTIMEOUT)
    + " HTTP/1.1",

    "User-Agent: " + String(USERAGENTSTRING),

    "Host: " + String(TELEGRAMHOST),

    "Accept: */*",

    "" // indicate end of headers with empty line (http)
  };
  SslPollClient->fire (httpCommands, 5);
}

String charToString(const char* tmp)
{
  if (tmp == 0) return String();
  return String(tmp);
}

void TelegramBotClient::pollSuccess(JwcProcessError err, JsonObject& payload)
{
  markPollWDT = millis();
  DOUT(F("pollSuccess"));
  if (!payload["ok"])
  {
    DOUT(F("Skip message, Server error"));
    if (callbackError != 0)
      callbackError(TelegramProcessError::RetPollErr, err);
    return;
  }
  Message* msg = new Message();
  msg->UpdateId = payload["result"][0]["update_id"];
  DOUTKV(F("UpdateId"), msg->UpdateId);
  LastUpdateId = msg->UpdateId + 1;
  msg->MessageId = payload["result"][0]["message"]["message_id"];
  DOUTKV(F("MessageId"), msg->MessageId);
  msg->FromId = payload["result"][0]["message"]["from"]["id"];
  DOUTKV(F("FromId"), msg->FromId);
  msg->FromIsBot = payload["result"][0]["message"]["from"]["is_bot"];
  DOUTKV(F("FromIsBot"), msg->FromIsBot);
  msg->FromFirstName = charToString(payload["result"][0]["message"]["from"]["first_name"]);
  DOUTKV(F("FromFirstName"), msg->FromFirstName);
  msg->FromLastName = charToString(payload["result"][0]["message"]["from"]["last_name"]);
  DOUTKV(F("FromLastName"), msg->FromLastName);
  msg->FromLanguageCode = charToString(payload["result"][0]["message"]["from"]["language_code"]);
  DOUTKV(F("FromLanguageCode"), msg->FromLanguageCode);
  msg->ChatId = payload["result"][0]["message"]["chat"]["id"];
  DOUTKV(F("ChatId"), msg->ChatId);
  msg->ChatFirstName = charToString(payload["result"][0]["message"]["chat"]["first_name"]);
  DOUTKV(F("ChatFirstName"), msg->ChatFirstName);
  msg->ChatLastName = charToString(payload["result"][0]["message"]["chat"]["last_name"]);
  DOUTKV(F("ChatLastName"), msg->ChatLastName);
  msg->ChatType = charToString(payload["result"][0]["message"]["chat"]["type"]);
  DOUTKV(F("ChatType"), msg->ChatType);
  msg->Text = charToString(payload["result"][0]["message"]["text"]);
  DOUTKV(F("Text"), msg->Text);
  msg->Date = payload["result"][0]["message"]["date"];
  DOUTKV(F("Date"), msg->Date);
  msg->disable_notification = payload["result"][0]["message"]["disable_notification"];
  DOUTKV(F("disable_notification"), msg->disable_notification);
  
  if (msg->FromId == 0 || msg->ChatId == 0 || msg->Text.length() == 0)
  {
    // no message, just the timeout from server
    DOUT(F("Timout by server"));
  }
  else
  {
    if (callbackReceive != 0)
    {
      callbackReceive(TelegramProcessError::Ok, err, msg);
    }
  }
  delete (msg);
}
void TelegramBotClient::pollError(JwcProcessError err, Client* client)
{
  DOUT(F("pollError"));
  switch (err)
  {
    case JwcProcessError::HttpErr: {
        if (callbackError != 0) callbackError(TelegramProcessError::JcwPollErr, err); break;
      }
    case JwcProcessError::MsgTooBig: {
        if (callbackError != 0) callbackError(TelegramProcessError::JcwPollErr, err);

        // This should find the update_id in JSON:
        // {"ok":true,"result":[{"update_id":512650849, ...
        String token = "";
        do
        {
          token = client->readStringUntil(',');
          DOUTKV ("Token", token);
        } while (token.indexOf("update_id") <= 0);
        token = token.substring(token.lastIndexOf(":") + 1);
        DOUTKV ("Token", token);
        LastUpdateId = token.toInt() + 1;
        DOUTKV ("LastUpdateId", LastUpdateId);
        break;
      }
    case JwcProcessError::MsgJsonErr: {
        if (callbackError != 0) callbackError(TelegramProcessError::JcwPollErr, err);
        //TODO: Try to get LastUpdateId somehow
        LastUpdateId++;
        break;
      }
  }
}

void TelegramBotClient::startPosting(String msg) {
  if (!Parallel) SslPollClient->stop();
  DOUT(F("Stopping PollClient"));
  String httpCommands[] =
  {
    "POST /bot"
    + String(Token)
    + "/sendMessage"
    + " HTTP/1.1",

    "Host: " + String(TELEGRAMHOST),

    "User-Agent: " + String(USERAGENTSTRING),

    "Content-Type: application/json",

    "Connection: close",

    "Content-Length: " + String (msg.length()),

    "", // indicate end of headers by empty line --> http

    msg
  };
  DOUTKV(F("posting]"), msg);
  SslPostClient->fire(httpCommands, 8);
}

void TelegramBotClient::postMessage(long chatId, String text, TBCKeyBoard &keyBoard, bool disable_notification)
{
  if (chatId == 0) {
    DOUT(F("Chat not defined."));
    return;
  }

  DOUT(F("postMessage"));
  DOUTKV(F("chatId"), chatId);
  DOUTKV(F("text"), text);

  DynamicJsonBuffer jsonBuffer (JWC_BUFF_SIZE);
  JsonObject& obj = jsonBuffer.createObject();
  obj["chat_id"] = chatId;
  obj["text"] = text;
  obj["disable_notification"] = disable_notification;
  if (keyBoard.length() > 0 )
  {
    JsonObject& jsonReplyMarkup = obj.createNestedObject("reply_markup");
    JsonArray& jsonKeyBoard = jsonReplyMarkup.createNestedArray("keyboard");
    DOUTKV(F("keyBoard.length()"), keyBoard.length());
    for (int i = 0; i < keyBoard.length(); i++)
    {
      DOUTKV(F("board.length(i): "), keyBoard.length(i));
      JsonArray& jsonRow = jsonKeyBoard.createNestedArray();
      for (int ii = 0; ii < keyBoard.length(i); ii++)
      {
        jsonRow.add(keyBoard.get(i, ii));
      }
    }
    obj.set<bool>("one_time_keyboard", keyBoard.getOneTime());
    obj.set<bool>("resize_keyboard", keyBoard.getResize());
    obj.set<bool>("selective", false);

  }

  String msgString;
  obj.printTo(msgString);
  DOUTKV(F("json"), msgString);
//  startPosting(msgString);
  if (msgCount > MAX_MSG) return ;
  if (msgString.length() > MSG_LEN) return;
  msgCount++;
  DOUTKV(F("msgCount"), msgCount);

  DOUTKV(F("msgInPointer"), msgInPointer);
  msgString.toCharArray(msgStore[msgInPointer],msgString.length());
  DOUTKV(F("msgStore[msgInPointer]"), msgStore[msgInPointer]);
  msgInPointer++;
  msgInPointer %= 5; // points to next available store
}

void TelegramBotClient::postSuccess(JwcProcessError err, JsonObject& json)
{
  markPollWDT = millis();
  DOUT(F("postSuccess"));
//json.printTo(Serial);
}
void TelegramBotClient::postError(JwcProcessError err, Client* client)
{
  DOUT(F("postError"));
  while (client->available() > 0)
  {
    String line = client->readStringUntil('\n');
    DOUTKV(F("line"), line);
  }
}

TBCKeyBoard::TBCKeyBoard(uint count, bool oneTime, bool resize)
{
  this->Count = count;
  this->Counter = 0;
  this->Rows = new TBCKeyBoardRow[count];
  this->OneTime = oneTime;
  this->Resize = resize;
  for (int i = 0; i < count; i++)
  {
    this->Rows[i].Count = 0;
    this->Rows[i].Buttons = 0;
  }
}
TBCKeyBoard::~TBCKeyBoard ()
{
  for (int i = 0; i < Count; i++)
  {
    delete (Rows[i].Buttons);
  }
  delete(this->Rows);
}


TBCKeyBoard& TBCKeyBoard::push(uint count, const String buttons[])
{

  if (Counter >= Count) return *this;
  Rows[Counter].Count = count;
  Rows[Counter].Buttons = new String[count];
  for (int i = 0; i < count; i++)
  {
    Rows[Counter].Buttons[i] = buttons[i];
  }
  Counter++;

  return *this;

}

const String TBCKeyBoard::get(uint row, uint col)
{
  if (row >= Count) return "";
  if (col >= Rows[row].Count) return "";
  return Rows[row].Buttons[col];
}
const int TBCKeyBoard::length (uint row)
{
  if (row >= Count) return 0;
  return Rows[row].Count;
}
const int TBCKeyBoard::length ()
{
  return Count;
}


