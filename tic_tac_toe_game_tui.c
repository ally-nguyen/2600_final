#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>    
#include <mosquitto.h> 
#include <ctype.h>     
#include <signal.h>    
#include <sys/wait.h>  
#include <time.h>      

// MQTT Broker details 
const char *mqtt_host = "34.19.95.21";
int mqtt_port = 1883;
char mqtt_client_id[50]; 
const char *subscribe_topic_board = "tic_tac_toe/board_state";
const char *subscribe_topic_turn = "tic_tac_toe/turn";
const char *subscribe_topic_status = "tic_tac_toe/game_status";
const char *publish_topic_move = "move_request";
const char *subscribe_topic_o_move = "move_response";
const char *game_mode_Topic = "game_mode";
const char *subscribe_topic_tournament_results = "tournament_results";
const char *publish_topic_player1_move = "player1_move";
const char *publish_topic_player2_move = "player2_move";

char board[3][3] = {{' ', ' ', ' '}, {' ', ' ', ' '}, {' ', ' ', ' '}};
char current_player = ' '; 
char my_player = ' ';      
struct mosquitto *mosq = NULL;
int initial_state_received = 0;
int game_over = 0;
char winner = ' ';   
int status_row = 10; 
int game_mode = 0;   
pid_t bash_player_pid = 0;
int waiting_for_esp32_response = 0; 

void reset_game()
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            board[i][j] = ' ';
    winner = ' ';
    game_over = 0;
    current_player = 'X'; 
    waiting_for_esp32_response = 0;
}

void clear_screen()
{
    printf("\033[2J\033[H");
}

void move_cursor(int row, int col)
{
    printf("\033[%d;%dH", row, col);
}

void display_board()
{
    move_cursor(3, 1);
    printf(" %c | %c | %c \n", board[0][0], board[0][1], board[0][2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c \n", board[1][0], board[1][1], board[1][2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c \n", board[2][0], board[2][1], board[2][2]);
}

int check_win(const char b[3][3])
{
    for (int i = 0; i < 3; i++)
        if (b[i][0] == b[i][1] && b[i][1] == b[i][2] && b[i][0] != ' ')
            return 1;
    for (int j = 0; j < 3; j++)
        if (b[0][j] == b[1][j] && b[1][j] == b[2][j] && b[0][j] != ' ')
            return 1;
    if (b[0][0] == b[1][1] && b[1][1] == b[2][2] && b[0][0] != ' ')
        return 1;
    if (b[0][2] == b[1][1] && b[1][1] == b[2][0] && b[0][2] != ' ')
        return 1;
    return 0;
}

int is_board_full(const char b[3][3])
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (b[i][j] == ' ')
                return 0;
    return 1;
}

int get_move(int player, int prompt_row, int error_row)
{
    int row = -1, col = -1;
    char player_marker = (player == 1) ? 'X' : 'O';

    move_cursor(prompt_row + 1, 1);
    printf("\033[K");
    printf("Player %c, enter move (row,col): ", player_marker);
    fflush(stdout);

    if (scanf("%d,%d", &row, &col) == 2)
    {
        while (getchar() != '\n')
            ;
        if (row >= 0 && row <= 2 && col >= 0 && col <= 2)
        {
            return (row * 3 + col);
        }
        else
        {
            move_cursor(error_row, 1);
            printf("\033[K");
            printf("Invalid coordinates. Row and column must be between 0 and 2.\n");
            return -1;
        }
    }
    else
    {
        while (getchar() != '\n')
            ;
        move_cursor(error_row, 1);
        printf("\033[K");
        printf("Invalid input format. Please enter row,col (e.g., 0,1).\n");
        return -1;
    }
}

void on_connect(struct mosquitto *mosq, void *userdata, int res)
{
    if (res == MOSQ_ERR_SUCCESS)
    {
        move_cursor(status_row, 1);
        printf("\033[K");
        printf("Connected to MQTT broker\n");
        mosquitto_subscribe(mosq, NULL, subscribe_topic_board, 1);
        mosquitto_subscribe(mosq, NULL, subscribe_topic_turn, 1);
        mosquitto_subscribe(mosq, NULL, subscribe_topic_status, 1);
        mosquitto_subscribe(mosq, NULL, subscribe_topic_tournament_results, 0);

        if (game_mode == 1)
        {
            
            mosquitto_subscribe(mosq, NULL, subscribe_topic_o_move, 1);
        }

        
        mosquitto_publish(mosq, NULL, "request_state", 0, NULL, 0, false);
    }
    else
    {
        move_cursor(status_row, 1);
        printf("\033[K");
        printf("Connection error: %s\n", mosquitto_strerror(res));
    }
}

void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg)
{
    if (strcmp(msg->topic, subscribe_topic_board) == 0)
    {
        char payload[msg->payloadlen + 1];
        memcpy(payload, msg->payload, msg->payloadlen);
        payload[msg->payloadlen] = '\0';
        int index = 0;
        char *row_str = strtok(payload, ";");
        while (row_str != NULL && index < 3)
        {
            char *cell = strtok(row_str, ",");
            int col_index = 0;
            while (cell != NULL && col_index < 3)
            {
                if (index >= 0 && index < 3 && col_index >= 0 && col_index < 3 && cell != NULL && strlen(cell) > 0)
                {
                    board[index][col_index] = cell[0];
                }
                cell = strtok(NULL, ",");
                col_index++;
            }
            row_str = strtok(NULL, ";");
            index++;
        }
        display_board();
        initial_state_received = 1;
        waiting_for_esp32_response = 0; 
    }
    else if (strcmp(msg->topic, subscribe_topic_o_move) == 0 && game_mode == 1)
    {
        char payload[msg->payloadlen + 1];
        memcpy(payload, msg->payload, msg->payloadlen);
        payload[msg->payloadlen] = '\0';
        int row, col;
        if (sscanf(payload, "O:%d,%d", &row, &col) == 2)
        {
            if (row >= 0 && row <= 2 && col >= 0 && col <= 2 && board[row][col] == ' ')
            {
                board[row][col] = 'O';
                display_board();
                current_player = 'X'; 
                waiting_for_esp32_response = 0;
            }
            else
            {
                move_cursor(status_row + 2, 1);
                printf("\033[K");
                printf("Invalid AI move received.\n");
                current_player = 'O'
                char request_payload[1] = {'1'}; /
                mosquitto_publish(mosq, NULL, "request_ai_move", strlen(request_payload), request_payload, 0, false);
            }
        }
        else
        {
            move_cursor(status_row + 2, 1);
            printf("\033[K");
            printf("Error parsing AI move.\n");
        }
    }
    else if (strcmp(msg->topic, subscribe_topic_turn) == 0)
    {
        current_player = ((char *)msg->payload)[0];
        
        move_cursor(status_row + 2, 1);
        printf("\033[K");
        printf("Current turn: Player %c\n", current_player);
        
        if (game_mode == 2)
        { 
            if (current_player == my_player)
            {
                move_cursor(status_row + 3, 1);
                printf("\033[K");
                printf("It's your turn! Please make a move.\n");
            }
            else
            {
                move_cursor(status_row + 3, 1);
                printf("\033[K");
                printf("Waiting for other player's move...\n");
            }
        }

        waiting_for_esp32_response = 0;
    }
    else if (strcmp(msg->topic, subscribe_topic_status) == 0)
    {
        char payload[msg->payloadlen + 1];
        memcpy(payload, msg->payload, msg->payloadlen);
        payload[msg->payloadlen] = '\0';

        move_cursor(status_row + 1, 1);
        printf("\033[K");
        printf("Status: %-30s", payload);
        fflush(stdout);

        if (strcmp(payload, "X wins!") == 0)
        {
            game_over = 1;
            winner = 'X';
        }
        else if (strcmp(payload, "O wins!") == 0)
        {
            game_over = 1;
            winner = 'O';
        }
        else if (strcmp(payload, "Draw!") == 0)
        {
            game_over = 1;
            winner = 'D';
        }

        if (game_over)
        {
            move_cursor(status_row + 3, 1);
            printf("\033[K");
            printf("Game over: %s\n", payload);
            fflush(stdout);
        }
    }
    else if (strcmp(msg->topic, subscribe_topic_tournament_results) == 0)
    {
        move_cursor(status_row + 4, 1);
        printf("\033[K");
        printf("Tournament Results: %s\n", (char *)msg->payload);
        move_cursor(status_row + 5, 1);
        printf("Press any key to return to menu...");
        getchar();
        game_mode = 0; 
        clear_screen();
    }
}

void cleanup_bash_player()
{
    if (bash_player_pid > 0)
    {
        kill(bash_player_pid, SIGTERM);
        waitpid(bash_player_pid, NULL, 0);
        bash_player_pid = 0;
        move_cursor(status_row + 3, 1);
        printf("\033[K");
        printf("Bash AI player terminated.\n");
    }
}

int main()
{
    clear_screen();

    
    if (game_mode == 0)
    {
        move_cursor(1, 1);
        printf("Tic-Tac-Toe Game:\n");
        printf("1. Single Player OR vs. AI)\n");
        printf("2. Two Player \n");
        printf("Enter your choice (1 or 2): ");

        if (scanf("%d", &game_mode) != 1)
        {
            while (getchar() != '\n')
                ;
            game_mode = 0;
        }
        while (getchar() != '\n')
            ;

        
        if (game_mode == 2)
        {
            char player_choice;
            move_cursor(5, 1);
            printf("Which player do you want to be? (X/O): ");
            player_choice = getchar();
            while (getchar() != '\n')
                ;

            my_player = (player_choice == 'X' || player_choice == 'x') ? 'X' : 'O';

            
            snprintf(mqtt_client_id, sizeof(mqtt_client_id), "tictactoe_player_%c_%d",
                     my_player, (int)time(NULL));
        }
        else
        {
            
            my_player = 'X'; 
            snprintf(mqtt_client_id, sizeof(mqtt_client_id), "tictactoe_tui_x");
        }
    }

    move_cursor(1, 1);
    printf("\033[J");

    
    mosquitto_lib_init();
    mosq = mosquitto_new(mqtt_client_id, true, NULL);
    if (!mosq)
    {
        fprintf(stderr, "Error: Out of memory.\n");
        return 1;
    }
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    int rc = mosquitto_connect(mosq, mqtt_host, mqtt_port, 60);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        move_cursor(status_row, 1);
        printf("\033[K");
        printf("Error connecting to broker: %s\n", mosquitto_strerror(rc));
        mosquitto_lib_cleanup();
        return 1;
    }
    mosquitto_loop_start(mosq);

    int prompt_row = 12;
    int error_row = 14;

    move_cursor(1, 1);
    printf("\033[J");

    
    if (game_mode == 2 && my_player == 'X')
    {
        
        char game_mode_payload[2];
        snprintf(game_mode_payload, sizeof(game_mode_payload), "%d", game_mode);
        mosquitto_publish(mosq, NULL, "game_mode", strlen(game_mode_payload), game_mode_payload, 0, false);
        move_cursor(status_row, 1);
        printf("\033[K");
        printf("Game started in Two Player mode. You are Player %c.\n", my_player);
    }
    else if (game_mode == 1)
    {
        
        char game_mode_payload[2];
        snprintf(game_mode_payload, sizeof(game_mode_payload), "%d", game_mode);
        mosquitto_publish(mosq, NULL, "game_mode", strlen(game_mode_payload), game_mode_payload, 0, false);
        move_cursor(status_row, 1);
        printf("\033[K");
        printf("Game started in Single Player mode.\n");
    }
    else if (game_mode == 2 && my_player == 'O')
    {
        
        move_cursor(status_row, 1);
        printf("\033[K");
        printf("Joining Two Player game. You are Player %c.\n", my_player);
    }

    reset_game();
    display_board();
    move_cursor(status_row, 1);
    printf("\033[K");

    if (game_mode == 1)
    {
        move_cursor(prompt_row - 1, 1);
        printf("\033[K");
        printf("You are Player X playing against AI");
        current_player = 'X';
    }
    else if (game_mode == 2)
    {
        move_cursor(prompt_row - 1, 1);
        printf("\033[K");
        printf("Two Player Mode. You are Player %c.\n", my_player);
        current_player = 'X'; 
    }

    while (game_mode == 1 || game_mode == 2)
    {
        if (game_over)
        {
            move_cursor(status_row + 2, 1);
            printf("\033[K");
            if (winner == 'D')
            {
                printf("Game over! It's a Draw!\n");
            }
            else
            {
                printf("Game over! Player %c wins!\n", winner);
            }

            move_cursor(status_row + 3, 1);
            printf("\033[K");
            printf("Play again? (Y/N): ");
            fflush(stdout);
            char response = getchar();
            while (getchar() != '\n')
                ;
            if (response == 'Y' || response == 'y')
            {
                reset_game();
                clear_screen();
                display_board();
                move_cursor(status_row, 1);
                printf("\033[K");

                if (game_mode == 2)
                {
                    printf("New game started in Two-Player mode. You are Player %c.\n", my_player);
                }
                else
                {
                    printf("New game started in Single Player mode.\n");
                }

                
                if (game_mode != 2 || my_player == 'X')
                {
                    mosquitto_publish(mosq, NULL, "reset_game", 1, "1", 0, false);
                }

                current_player = 'X';
                game_over = 0;
            }
            else
            {
                break;
            }
        }
        else if (!waiting_for_esp32_response)
        {
            
            if (game_mode == 1 && current_player == 'X')
            {
                int move_index = -1;
                do
                {
                    move_index = get_move(1, prompt_row, error_row);
                    if (move_index != -1 && board[move_index / 3][move_index % 3] != ' ')
                    {
                        move_cursor(error_row, 1);
                        printf("\033[K");
                        printf("That spot is already taken. Please try again.\n");
                        move_index = -1;
                    }
                } while (move_index == -1);

                int row = move_index / 3;
                int col = move_index % 3;

                
                char move_payload[10];
                snprintf(move_payload, sizeof(move_payload), "X:%d,%d", row, col);
                mosquitto_publish(mosq, NULL, publish_topic_move, strlen(move_payload), move_payload, 0, false);

               
                board[row][col] = 'X';
                display_board();

                
                waiting_for_esp32_response = 1;
                current_player = 'O'; 

                move_cursor(status_row + 2, 1);
                printf("\033[K");
                printf("Waiting for AI response...\n");
            }
            
            else if (game_mode == 2 && current_player == my_player)
            {
                int player_num = (my_player == 'X') ? 1 : 2;
                int move_index = -1;

                move_cursor(prompt_row, 1);
                printf("\033[K");
                printf("It's your turn (Player %c)\n", my_player);

                do
                {
                    move_index = get_move(player_num, prompt_row, error_row);
                    if (move_index != -1 && board[move_index / 3][move_index % 3] != ' ')
                    {
                        move_cursor(error_row, 1);
                        printf("\033[K");
                        printf("That spot is already taken. Please try again.\n");
                        move_index = -1;
                    }
                } while (move_index == -1);

                int row = move_index / 3;
                int col = move_index % 3;

                
                char move_payload[10];
                snprintf(move_payload, sizeof(move_payload), "%c:%d,%d", my_player, row, col);

                const char *topic = (my_player == 'X') ? publish_topic_player1_move : publish_topic_player2_move;

                mosquitto_publish(mosq, NULL, topic, strlen(move_payload), move_payload, 0, false);

                
                board[row][col] = my_player;
                display_board();

                
                waiting_for_esp32_response = 1;

                move_cursor(status_row + 2, 1);
                printf("\033[K");
                printf("Move sent, waiting for other player...\n");
            }
            else if (game_mode == 2 && current_player != my_player)
            {
                
                move_cursor(status_row + 3, 1);
                printf("\033[K");
                printf("Waiting for Player %c's move...\n", current_player);
                usleep(500000); 
            }
        }

        
        usleep(100000); 
    }

    
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    move_cursor(status_row + 4, 1);
    printf("Thanks for playing! Goodbye.\n");

    return 0;
}
