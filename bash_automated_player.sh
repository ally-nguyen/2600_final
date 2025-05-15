#!/bin/bash

BROKER="34.19.95.21"
PORT="1883"
MOVE_REQUEST="move_request"
MOVE_RESPONSE="move_response"
BOARD_STATE="tic_tac_toe/board_state"
TURN_TOPIC="tic_tac_toe/turn"
GAME_STATUS="tic_tac_toe/game_status"

declare -A board
last_human_move=""

# initialize board to track moves
initialize_board() {
  for row in {0..2}; do
    for col in {0..2}; do
      board["$row,$col"]=" "
    done
  done

  last_human_move=""
}

initialize_board

update_board_from_state() {
  local board_state="$1"
  local row_index=0
  
  # board format:(rows separated by semicolons, cells by commas)
  IFS=';' read -ra rows <<< "$board_state"
  for row in "${rows[@]}"; do
    IFS=',' read -ra cells <<< "$row"
    col_index=0
    for cell in "${cells[@]}"; do
      board["$row_index,$col_index"]="$cell"
      ((col_index++))
    done
    ((row_index++))
  done
  
  echo "Current board state:"
  for i in {0..2}; do
    echo -n "| "
    for j in {0..2}; do
      echo -n "${board["$i,$j"]} | "
    done
    echo ""
  done
}

# function to track human moves
track_human_move() {
  local move_info="$1"

  if [[ "$move_info" == X:* ]]; then
    last_human_move="${move_info#X:}"
    echo "Human (X) played at position: $last_human_move"
  fi
}

# generate move for player O / bash player
generate_move() {

  empty_positions=()
  for row in {0..2}; do
    for col in {0..2}; do
      if [[ "${board["$row,$col"]}" == " " ]]; then
        empty_positions+=("$row,$col")
      fi
    done
  done

  # prevents repition in moves from human player or self
  if [[ ${#empty_positions[@]} -gt 0 ]]; then
    valid_positions=()
    for pos in "${empty_positions[@]}"; do
      if [[ "$pos" != "$last_human_move" ]]; then
        valid_positions+=("$pos")
      fi
    done
    # checks for valid position
    if [[ ${#valid_positions[@]} -gt 0 ]]; then
      random_index=$((RANDOM % ${#valid_positions[@]}))
      echo "${valid_positions[$random_index]}"
      return
    else
      # handles empty valid positions
      random_index=$((RANDOM % ${#empty_positions[@]}))
      echo "${empty_positions[$random_index]}"
      return
    fi
  fi
  
  # if board is full, return invalid move
  echo "-1,-1"
}

echo "Starting Tic-Tac-Toe Bot (Player O)..."
echo "Connecting to MQTT broker at $BROKER:$PORT"

# get current state of board from esp32
mosquitto_pub -h "$BROKER" -p "$PORT" -t "request_state" -m "1"

# subscribe to board state updates
mosquitto_sub -h "$BROKER" -p "$PORT" -t "$BOARD_STATE" | while read -r board_state; do
  echo "Received board state: $board_state"
  update_board_from_state "$board_state"
done &
BOARD_PID=$!

# subscribe to player X's moves to track them
mosquitto_sub -h "$BROKER" -p "$PORT" -t "$MOVE_REQUEST" | while read -r move; do
  track_human_move "$move"
done &
MOVE_TRACK_PID=$!

# subscribe to turn updates and make moves when its bash player  turn
mosquitto_sub -h "$BROKER" -p "$PORT" -t "$TURN_TOPIC" | while read -r turn; do
  echo "Current turn: $turn"
  if [[ "$turn" == "O" ]]; then
    
    sleep 1
    # to check one more time that moves are not being repeated
    move=$(generate_move)
    if [[ "$move" != "-1,-1" ]]; then
      echo "Bot (O) is playing move: $move"

      if [[ "$move" != "$last_human_move" ]]; then
        mosquitto_pub -h "$BROKER" -p "$PORT" -t "$MOVE_RESPONSE" -m "O:$move"
      else
        echo "Warning: Generated move matches human's last move! Regenerating..."GAME_STATUS="tic_tac_toe/game_status" 

        sleep 0.5
        move=$(generate_move)
        echo "New move: $move"
        mosquitto_pub -h "$BROKER" -p "$PORT" -t "$MOVE_RESPONSE" -m "O:$move"
      fi
    else
      echo "No valid moves available"
    fi
  fi
done &
TURN_PID=$!

# subscribe to game status 
mosquitto_sub -h "$BROKER" -p "$PORT" -t "$GAME_STATUS" | while read -r status; do
  echo "Game status: $status"
  if [[ "$status" == *"wins"* || "$status" == *"Tie"* ]]; then
    echo "Game over: $status"
    initialize_board
  fi
done &
STATUS_PID=$!

# Handle cleanup on exit
trap "kill $BOARD_PID $MOVE_TRACK_PID $TURN_PID $STATUS_PID 2>/dev/null; exit 0" EXIT INT TERM

# Keep the script running
while true; do
  sleep 1
done
