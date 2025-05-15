#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>

const char *ssid = "Allison IPhone";
const char *password = "12345678";
const char *pubTopic = "move_request";
const char *subTopic = "move_response";
const char *gameTopic = "played_games";
const char *boardStateTopic = "tic_tac_toe/board_state";
const char *turnTopic = "tic_tac_toe/turn";
const char *gameStatusTopic = "tic_tac_toe/game_status";

const char *mqtt_server = "34.19.95.21";

WiFiClient espClient;
PubSubClient client(espClient);

char board[3][3];
int xWins = 0, oWins = 0, ties = 0, gamesPlayed = 0;
volatile int receivedRow = -1, receivedCol = -1;
volatile bool moveReceived = false;
volatile char receivedPlayer = ' ';
char currentPlayer = 'X';

volatile int receivedORow = -1, receivedOCol = -1;
volatile bool oMoveReceived = false;
volatile char receivedOPlayer = ' ';
int gameMode = 1;
const char *gameModeTopic = "game_mode";

void setup()
{
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  resetBoard();
}

void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20)
  {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\nFailed to connect to WiFi!");
  }
}

void resetBoard()
{
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      board[i][j] = ' ';
}

void publishBoardState()
{
  String boardStr = "";
  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      boardStr += board[i][j];
      if (j < 2)
        boardStr += ",";
    }
    if (i < 2)
      boardStr += ";";
  }
  client.publish(boardStateTopic, boardStr.c_str());
  Serial.printf("Published board state: %s\n", boardStr.c_str());
}

void publishGameStatus(String status)
{
  client.publish(gameStatusTopic, status.c_str());
  Serial.printf("Published game status: %s\n", status.c_str());
}

void publishCurrentPlayer()
{
  client.publish(turnTopic, String(currentPlayer).c_str());
  Serial.printf("Published current player: %c\n", currentPlayer);
}

void callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  String msg = String((char *)payload);
  Serial.printf("Received message on topic %s: %s\n", topic, msg.c_str());

  if (String(topic) == "move_request")
  {
    if (msg.length() >= 3 && msg.charAt(1) == ':')
    {
      receivedPlayer = msg.charAt(0);
      int colonIndex = msg.indexOf(':');
      if (colonIndex > 0)
      {
        String movePart = msg.substring(colonIndex + 1);
        int commaIndex = movePart.indexOf(',');
        if (commaIndex > 0)
        {
          receivedRow = movePart.substring(0, commaIndex).toInt();
          receivedCol = movePart.substring(commaIndex + 1).toInt();
          moveReceived = true;
          Serial.printf("Received move from %c at (%d, %d)\n", receivedPlayer, receivedRow, receivedCol);
        }
        else
        {
          Serial.println("Invalid move format received (missing comma).");
        }
      }
      else
      {
        Serial.println("Invalid move format received (missing colon).");
      }
    }
    else
    {
      Serial.println("Invalid move message format.");
    }
  }
  else if (String(topic) == "request_state")
  {
    Serial.println("Received request_state, publishing initial board and player.");
    publishBoardState();
    publishCurrentPlayer();
  }
  else if (String(topic) == subTopic)
  {
    if (msg.length() >= 3 && msg.charAt(1) == ':')
    {
      receivedOPlayer = msg.charAt(0);
      int colonIndex = msg.indexOf(':');
      if (colonIndex > 0)
      {
        String movePart = msg.substring(colonIndex + 1);
        int commaIndex = movePart.indexOf(',');
        if (commaIndex > 0)
        {
          receivedORow = movePart.substring(0, commaIndex).toInt();
          receivedOCol = movePart.substring(commaIndex + 1).toInt();
          oMoveReceived = true;
          Serial.printf("Received move from %c at (%d, %d) (Player O)\n", receivedOPlayer, receivedORow, receivedOCol);
          publishBoardState();
        }
        else
        {
          Serial.println("Invalid move format received (missing comma) from O." + msg);
        }
      }
      else
      {
        Serial.println("Invalid move format received (missing colon) from O." + msg);
      }
    }
    else
    {
      Serial.println("Invalid move message format from O." + msg);
    }
  }
  else if (String(topic) == "game_mode")
  {
    int newGameMode = msg.toInt();
    Serial.printf("Received game mode: %d\n", newGameMode);

    gameMode = newGameMode;
    resetBoard();
    publishBoardState();
    publishCurrentPlayer();
    publishGameStatus("Game in progress");
  }
  else if (String(topic) == "player1_move" && currentPlayer == 'X')
  {
    int r, c;
    if (sscanf(msg.c_str(), "X:%d,%d", &r, &c) == 2)
    {
      if (r >= 0 && r <= 2 && c >= 0 && c <= 2)
      {
        receivedRow = r;
        receivedCol = c;
        receivedPlayer = 'X';
        moveReceived = true;
        Serial.printf("Received move from Player X (Human) at (%d, %d)\n", receivedRow, receivedCol);
      }
      else
      {
        Serial.println("Invalid move coordinates from Player X (Human): " + msg);
      }
    }
    else
    {
      Serial.println("Invalid move format from Player X (Human): " + msg);
    }
  }
  else if (String(topic) == "player2_move" && currentPlayer == 'O')
  {
    int r, c;
    if (sscanf(msg.c_str(), "O:%d,%d", &r, &c) == 2)
    {
      if (r >= 0 && r <= 2 && c >= 0 && c <= 2)
      {
        receivedORow = r;
        receivedOCol = c;
        receivedOPlayer = 'O';
        oMoveReceived = true;
        Serial.printf("Received move from Player O (Human) at (%d, %d)\n", receivedORow, receivedOCol);
      }
      else
      {
        Serial.println("Invalid move coordinates from Player O (Human): " + msg);
      }
    }
    else
    {
      Serial.println("Invalid move format from Player O (Human): " + msg);
    }
  }
}

bool checkWin(char player)
{
  for (int i = 0; i < 3; i++)
    if ((board[i][0] == player && board[i][1] == player && board[i][2] == player) ||
        (board[0][i] == player && board[1][i] == player && board[2][i] == player))
      return true;

  if ((board[0][0] == player && board[1][1] == player && board[2][2] == player) ||
      (board[0][2] == player && board[1][1] == player && board[2][0] == player))
    return true;

  return false;
}

bool isBoardFull()
{
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      if (board[i][j] == ' ')
        return false;
  return true;
}

bool isValidMove(int row, int col)
{
  return board[row][col] == ' ';
}

void publishGamesPlayed()
{
  String gameStatus = String(gamesPlayed);
  client.publish(gameTopic, gameStatus.c_str());
}

void publishGameMode()
{
  client.publish(gameModeTopic, String(gameMode).c_str(), true);
  Serial.printf("Published game mode: %d\n", gameMode);
}

void finalScore()
{
  String result = "X wins: " + String(xWins) + ", O wins: " + String(oWins) + ", Ties: " + String(ties) + ", Total games: " + String(gamesPlayed);
  client.publish("final_score", result.c_str());
}

void reconnect()
{
  Serial.print("Connecting to MQTT...");
  String clientId = "ESP32Client-";
  clientId += WiFi.macAddress();
  clientId.replace(":", "");

  if (client.connect(clientId.c_str()))
  {
    Serial.println("Connected!");
    client.subscribe("move_request");
    client.subscribe("move_response");
    client.subscribe("request_state");
    client.subscribe("player1_move");
    client.subscribe("player2_move");
    client.subscribe(("gameModeTopic"));

    resetBoard();
    publishBoardState();
    publishCurrentPlayer();
  }
  else
  {
    Serial.print("Failed, rc=");
    Serial.print(client.state());
    Serial.println(" Retrying in 5s...");
    delay(5000);
  }
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  if (moveReceived)
  {
    Serial.println("DEBUG: moveReceived is true");
    if (receivedPlayer == currentPlayer)
    {
      Serial.printf("DEBUG: receivedPlayer (%c) == currentPlayer (%c)\n", receivedPlayer, currentPlayer);
      if (isValidMove(receivedRow, receivedCol))
      {
        board[receivedRow][receivedCol] = receivedPlayer;
        publishBoardState();
        if (checkWin(receivedPlayer))
        {
          publishGameStatus(String(receivedPlayer) + " wins!");
          if (receivedPlayer == 'X')
            xWins++;
          else
            oWins++;
          gamesPlayed++;
          resetBoard();
          publishBoardState();
          currentPlayer = 'X';
          publishCurrentPlayer();
        }
        else if (isBoardFull())
        {
          publishGameStatus("Tie game!");
          ties++;
          gamesPlayed++;
          resetBoard();
          publishBoardState();
          currentPlayer = 'X';
          publishCurrentPlayer();
        }
        else
        {
          currentPlayer = (currentPlayer == 'X') ? 'O' : 'X';
          Serial.printf("DEBUG: Switching currentPlayer to %c\n", currentPlayer);
          publishCurrentPlayer();
          Serial.println("DEBUG: publishCurrentPlayer() called");
        }
      }
      else
      {
        Serial.println("Invalid move attempted by " + String(receivedPlayer));
      }
    }
    else
    {
      Serial.println("It's not " + String(receivedPlayer) + "'s turn.");
    }
    moveReceived = false;
    receivedPlayer = ' ';
    receivedRow = -1;
    receivedCol = -1;
  }
  if (oMoveReceived && currentPlayer == 'O')
  {
    if (isValidMove(receivedORow, receivedOCol))
    {
      board[receivedORow][receivedOCol] = receivedOPlayer;
      publishBoardState();
      if (checkWin(receivedOPlayer))
      {
        publishGameStatus(String(receivedOPlayer) + " wins!");
        if (receivedOPlayer == 'X')
          xWins++;
        else
          oWins++;
        gamesPlayed++;
        resetBoard();
        publishBoardState();
        currentPlayer = 'X';
        publishCurrentPlayer();
      }
      else if (isBoardFull())
      {
        publishGameStatus("Tie game!");
        ties++;
        gamesPlayed++;
        resetBoard();
        publishBoardState();
        currentPlayer = 'X';
        publishCurrentPlayer();
      }
      else
      {
        currentPlayer = 'X';
        publishCurrentPlayer();
      }
    }
    else
    {
      Serial.println("Invalid move attempted by " + String(receivedOPlayer));
    }
    oMoveReceived = false;
    receivedOPlayer = ' ';
    receivedORow = -1;
    receivedOCol = -1;
  }

  delay(100);
}
