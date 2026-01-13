#ifndef CHESS_UTILS_H
#define CHESS_UTILS_H

#include <Arduino.h>

class ChessUtils {
 public:
  // Convert board state to FEN notation
  // board: 8x8 array representing the chess board
  // isWhiteTurn: true if it's white's turn to move
  // castlingRights: castling availability string (e.g., "KQkq", "-" for none)
  // Returns: FEN string representation
  static String boardToFEN(const char board[8][8], bool isWhiteTurn, const char* castlingRights = "KQkq");

  // Parse FEN notation and update board state
  // fen: FEN string to parse
  // board: 8x8 array to update with parsed position
  // isWhiteTurn: output parameter for whose turn it is (optional)
  // castlingRights: output parameter for castling availability (optional)
  static void fenToBoard(String fen, char board[8][8], bool* isWhiteTurn = nullptr, String* castlingRights = nullptr);

  // Print current board state to Serial for debugging
  // board: 8x8 array representing the chess board
  static void printBoard(const char board[8][8]);
};

#endif // CHESS_UTILS_H
