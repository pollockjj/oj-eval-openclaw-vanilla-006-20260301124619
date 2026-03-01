#ifndef SERVER_H
#define SERVER_H

#include <cstdlib>
#include <iostream>

int rows;
int columns;
int total_mines;
int game_state;

// Grid states
// 0 = unvisited, 1 = visited, 2 = marked
int grid_state[35][35];
// true if mine
bool is_mine[35][35];
// mine count for each cell
int mine_count[35][35];
// counters
int visit_count;
int marked_mine_count;
int total_non_mines;

int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};

bool InBounds(int r, int c) {
  return r >= 0 && r < rows && c >= 0 && c < columns;
}

int CountAdjacentMines(int r, int c) {
  int count = 0;
  for (int d = 0; d < 8; d++) {
    int nr = r + dx[d], nc = c + dy[d];
    if (InBounds(nr, nc) && is_mine[nr][nc]) count++;
  }
  return count;
}

void InitMap() {
  std::cin >> rows >> columns;
  total_mines = 0;
  visit_count = 0;
  marked_mine_count = 0;
  game_state = 0;
  
  for (int i = 0; i < rows; i++) {
    std::string line;
    std::cin >> line;
    for (int j = 0; j < columns; j++) {
      grid_state[i][j] = 0;
      is_mine[i][j] = (line[j] == 'X');
      if (is_mine[i][j]) total_mines++;
    }
  }
  
  total_non_mines = rows * columns - total_mines;
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      mine_count[i][j] = CountAdjacentMines(i, j);
    }
  }
}

void DoVisit(int r, int c);

void CheckWin() {
  if (visit_count == total_non_mines) {
    game_state = 1;
  }
}

void DoVisit(int r, int c) {
  if (!InBounds(r, c)) return;
  if (grid_state[r][c] != 0) return; // already visited or marked
  
  if (is_mine[r][c]) {
    grid_state[r][c] = 1; // visited
    game_state = -1;
    return;
  }
  
  grid_state[r][c] = 1;
  visit_count++;
  
  if (mine_count[r][c] == 0) {
    for (int d = 0; d < 8; d++) {
      int nr = r + dx[d], nc = c + dy[d];
      if (InBounds(nr, nc)) {
        DoVisit(nr, nc);
      }
    }
  }
}

void VisitBlock(int r, int c) {
  if (!InBounds(r, c)) return;
  if (grid_state[r][c] != 0) return; // already visited or marked - no effect
  
  DoVisit(r, c);
  if (game_state != -1) CheckWin();
}

void MarkMine(int r, int c) {
  if (!InBounds(r, c)) return;
  if (grid_state[r][c] != 0) return; // already visited or marked - no effect
  
  if (!is_mine[r][c]) {
    grid_state[r][c] = 2; // mark it (will show as X)
    game_state = -1;
    return;
  }
  
  grid_state[r][c] = 2;
  marked_mine_count++;
}

void AutoExplore(int r, int c) {
  if (!InBounds(r, c)) return;
  // Must be a visited non-mine grid
  if (grid_state[r][c] != 1 || is_mine[r][c]) return;
  
  // Count marked mines around
  int marked_count = 0;
  for (int d = 0; d < 8; d++) {
    int nr = r + dx[d], nc = c + dy[d];
    if (InBounds(nr, nc) && grid_state[nr][nc] == 2) {
      marked_count++;
    }
  }
  
  if (marked_count == mine_count[r][c]) {
    // Visit all unvisited unmarked neighbors
    for (int d = 0; d < 8; d++) {
      int nr = r + dx[d], nc = c + dy[d];
      if (InBounds(nr, nc) && grid_state[nr][nc] == 0) {
        DoVisit(nr, nc);
        if (game_state == -1) return;
      }
    }
    if (game_state != -1) CheckWin();
  }
}

void ExitGame() {
  if (game_state == 1) {
    std::cout << "YOU WIN!" << std::endl;
    std::cout << visit_count << " " << total_mines << std::endl;
  } else {
    std::cout << "GAME OVER!" << std::endl;
    std::cout << visit_count << " " << marked_mine_count << std::endl;
  }
  exit(0);
}

void PrintMap() {
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (grid_state[i][j] == 0) {
        // unvisited
        if (game_state == 1 && is_mine[i][j]) {
          std::cout << '@';
        } else {
          std::cout << '?';
        }
      } else if (grid_state[i][j] == 1) {
        // visited
        if (is_mine[i][j]) {
          std::cout << 'X';
        } else {
          std::cout << mine_count[i][j];
        }
      } else {
        // marked (grid_state == 2)
        if (is_mine[i][j]) {
          std::cout << '@';
        } else {
          std::cout << 'X';
        }
      }
    }
    std::cout << std::endl;
  }
}

#endif
