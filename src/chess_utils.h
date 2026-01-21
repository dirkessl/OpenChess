#ifndef CHESS_UTILS_H
#define CHESS_UTILS_H

#include <Arduino.h>

class ChessUtils {
 public:
  // Get color name from color character
  static const char* colorName(char color) {
    return (color == 'w') ? "White" : "Black";
  }

  // Convert castling rights bitmask (KQkq) to string used in FEN.
  // rights: bitmask where 0x01=K, 0x02=Q, 0x04=k, 0x08=q
  static String castlingRightsToString(uint8_t rights);

  // Convert board state to FEN notation
  // board: 8x8 array representing the chess board
  // currentTurn: 'w' for white's turn, 'b' for black's turn
  // castlingRights: castling availability string (e.g., "KQkq", "-" for none)
  // Returns: FEN string representation
  static String boardToFEN(const char board[8][8], char currentTurn, const char* castlingRights = "KQkq");

  // Parse FEN notation and update board state
  // fen: FEN string to parse
  // board: 8x8 array to update with parsed position
  // currentTurn: output parameter for whose turn it is - 'w' or 'b' (optional)
  // castlingRights: output parameter for castling availability (optional)
  static void fenToBoard(String fen, char board[8][8], char* currentTurn = nullptr, String* castlingRights = nullptr);

  // Print current board state to Serial for debugging
  // board: 8x8 array representing the chess board
  static void printBoard(const char board[8][8]);

  // Evaluate board position using simple material count
  // Returns evaluation in pawns (positive = white advantage, negative = black advantage)
  // Pawn=1, Knight=3, Bishop=3, Rook=5, Queen=9
  static float evaluatePosition(const char board[8][8]);

  // Initialize NVS for ESP32 (required before Preferences.begin)
  static bool ensureNvsInitialized();
};

#endif // CHESS_UTILS_H
