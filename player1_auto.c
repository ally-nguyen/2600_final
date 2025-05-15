#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mosquitto.h>
#include <time.h>
#include <signal.h>

const char *mqtt_host = "34.19.95.21";
int mqtt_port = 1883;
const char *mqtt_client_id = "random_player1_bot";

const char *publish_topic_player1_move = "player1_move";
const char *subscribe_topic_player2_move = "player2_move";
const char *subscribe_topic_board = "tic_tac_toe/board_state";
const char *subscribe_topic_turn = "tic_tac_toe/turn";
const char *subscribe_topic_status = "tic_tac_toe/game_status";

char board[3][3] = {{' ', ' ', ' '}, {' ', ' ', ' '}, {' ', ' ', ' '}};
char current_player = ' ';
int game_over = 0;
int my_turn = 0;
struct mosquitto *mosq = NULL;
volatile int running = 1;

// shuts down program when hit ctrl c
void signal_handler(int signum)
{
    running = 0;
}

// updates board based on move
void update_board(int row, int col, char player)
{
    if (row >= 0 && row < 3 && col >= 0 && col < 3 && board[row][col] == ' ')
    {
        board[row][col] = player;
        printf("Updated board - position [%d,%d] is now %c\n", row, col, player);

        for (int i = 0; i < 3; i++)
        {
            printf(" %c | %c | %c \n", board[i][0], board[i][1], board[i][2]);
            if (i < 2)
                printf("---+---+---\n");
        }
    }
    else if (row >= 0 && row < 3 && col >= 0 && col < 3)
    {
        printf("Warning: Tried to update position [%d,%d] to %c but it's already occupied with %c\n",
               row, col, player, board[row][col]);
    }
}
// resets the game
void reset_game()
{
    printf("Resetting game board\n");
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            board[i][j] = ' ';
        }
    }
    game_over = 0;

    // displays empty board for visuals
    printf("New empty board:\n");
    for (int i = 0; i < 3; i++)
    {
        printf(" %c | %c | %c \n", board[i][0], board[i][1], board[i][2]);
        if (i < 2)
            printf("---+---+---\n");
    }
}

// check if the board is full
int is_board_full()
{
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (board[i][j] == ' ')
            {
                return 0;
            }
        }
    }
    return 1;
}

// makes a random move for player 1
void make_random_move()
{
    if (game_over || !my_turn)
    {
        return;
    }

    // searches for all empty cells
    int empty_positions[9][2];
    int empty_count = 0;

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (board[i][j] == ' ')
            {
                empty_positions[empty_count][0] = i;
                empty_positions[empty_count][1] = j;
                empty_count++;
            }
        }
    }

    // found no empty cells
    if (empty_count == 0)
    {
        printf("No valid moves available\n");
        return;
    }

    // generate a random index in coordinate format
    int random_index = rand() % empty_count;
    int row = empty_positions[random_index][0];
    int col = empty_positions[random_index][1];

    char move_payload[20];
    snprintf(move_payload, sizeof(move_payload), "X:%d,%d", row, col);

    // publish move to player1 topic
    mosquitto_publish(mosq, NULL, publish_topic_player1_move,
                      strlen(move_payload), move_payload, 0, false);

    printf("Player 1 (X) making random move: %s\n", move_payload);

    update_board(row, col, 'X');
    my_turn = 0; // Wait for next turn
}

// callback when connected to mqtt
void on_connect(struct mosquitto *mosq, void *userdata, int result)
{
    if (result == 0)
    {
        printf("Connected to MQTT broker\n");

        // Subscribe to necessary topics
        mosquitto_subscribe(mosq, NULL, subscribe_topic_player2_move, 0);
        mosquitto_subscribe(mosq, NULL, subscribe_topic_board, 0);
        mosquitto_subscribe(mosq, NULL, subscribe_topic_turn, 0);
        mosquitto_subscribe(mosq, NULL, subscribe_topic_status, 0);

        // call for initial board state
        mosquitto_publish(mosq, NULL, "request_state", 0, NULL, 0, false);
    }
    else
    {
        fprintf(stderr, "Failed to connect to MQTT broker: %s\n",
                mosquitto_strerror(result));
    }
}

// callback when message is received
void on_message(struct mosquitto *mosq, void *userdata,
                const struct mosquitto_message *message)
{
    char *payload = NULL;
    if (message->payload != NULL)
    {
        payload = malloc(message->payloadlen + 1);
        memcpy(payload, message->payload, message->payloadlen);
        payload[message->payloadlen] = '\0';
    }

    // handle board updates
    if (strcmp(message->topic, subscribe_topic_board) == 0)
    {
        if (payload)
        {
            // parses board state from payload
            int index = 0;
            char *row_str = strtok(payload, ";");
            while (row_str != NULL && index < 3)
            {
                char *cell = strtok(row_str, ",");
                int col_index = 0;
                while (cell != NULL && col_index < 3)
                {
                    if (strlen(cell) > 0)
                    {
                        board[index][col_index] = cell[0];
                    }
                    cell = strtok(NULL, ",");
                    col_index++;
                }
                row_str = strtok(NULL, ";");
                index++;
            }
            printf("Current board state:\n");
            for (int i = 0; i < 3; i++)
            {
                printf(" %c | %c | %c \n", board[i][0], board[i][1], board[i][2]);
                if (i < 2)
                    printf("---+---+---\n");
            }
        }
    }
    // handle turn updates
    else if (strcmp(message->topic, subscribe_topic_turn) == 0)
    {
        if (payload)
        {
            current_player = payload[0];
            printf("Current turn: Player %c\n", current_player);

            if (current_player == 'X')
            {
                my_turn = 1;
                sleep(1);
                make_random_move();
            }
            else
            {
                my_turn = 0;
            }
        }
    }
    // handle player 2 moves
    else if (strcmp(message->topic, subscribe_topic_player2_move) == 0)
    {
        if (payload)
        {
            int row, col;
            if (sscanf(payload, "O:%d,%d", &row, &col) == 2)
            {
                printf("Player 2 (O) move received: O:%d,%d\n", row, col);
                update_board(row, col, 'O');
            }
        }
    }
    // handle game status updates
    else if (strcmp(message->topic, subscribe_topic_status) == 0)
    {
        if (payload)
        {
            printf("Game status: %s\n", payload);

            if (strstr(payload, "wins") || strstr(payload, "Draw"))
            {
                game_over = 1;
                printf("Game over. Ready for next game.\n");
                reset_game();
            }
        }
    }
    if (payload)
    {
        free(payload);
    }
}

int main()
{
    // set up signal handling to use ctrl c to terminate program
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // seed the random number generator
    srand(time(NULL));

    // initialize libmosquitto
    mosquitto_lib_init();

    // create a new client instance
    mosq = mosquitto_new(mqtt_client_id, true, NULL);
    if (!mosq)
    {
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }

    // set callbacks
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    // connect to the broker
    int rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        fprintf(stderr, "Error connecting to broker: %s\n", mosquitto_strerror(rc));
        mosquitto_lib_cleanup();
        return 1;
    }
    mosquitto_loop_start(mosq);

    printf("Random Player 1 (X) bot started. Press Ctrl+C to exit.\n");

    while (running)
    {
        if (my_turn && !game_over)
        {
            make_random_move();
        }

        usleep(100000);
    }

    printf("\nShutting down...\n");
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return 0;
}