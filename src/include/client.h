#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <utility>
#include <vector>
#include <cstring>
#include <algorithm>

extern int rows;
extern int columns;
extern int total_mines;

// Map state as seen by client
// '?' = unknown, '0'-'8' = visited number, '@' = marked mine
char client_map[35][35];

// For constraint-based solver
// 0 = unknown, 1 = known safe, 2 = known mine
int cell_status[35][35];

int remaining_mines;
int total_unknown;

int cdx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
int cdy[] = {-1, 0, 1, -1, 1, -1, 0, 1};

bool CInBounds(int r, int c) {
  return r >= 0 && r < rows && c >= 0 && c < columns;
}

void Execute(int r, int c, int type);

void InitGame() {
  // Initialize all global variables
  remaining_mines = total_mines;
  total_unknown = rows * columns;
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      client_map[i][j] = '?';
      cell_status[i][j] = 0;
    }
  }
  
  int first_row, first_column;
  std::cin >> first_row >> first_column;
  Execute(first_row, first_column, 0);
}

void ReadMap() {
  for (int i = 0; i < rows; i++) {
    std::string line;
    std::cin >> line;
    for (int j = 0; j < columns; j++) {
      client_map[i][j] = line[j];
    }
  }
}

// Get info about neighbors of a numbered cell
struct NeighborInfo {
  int unknown_count;
  int marked_count;
  int number;
  std::vector<std::pair<int,int>> unknowns;
};

NeighborInfo GetNeighborInfo(int r, int c) {
  NeighborInfo info;
  info.unknown_count = 0;
  info.marked_count = 0;
  info.number = client_map[r][c] - '0';
  for (int d = 0; d < 8; d++) {
    int nr = r + cdx[d], nc = c + cdy[d];
    if (CInBounds(nr, nc)) {
      if (client_map[nr][nc] == '?') {
        info.unknown_count++;
        info.unknowns.push_back({nr, nc});
      } else if (client_map[nr][nc] == '@') {
        info.marked_count++;
      }
    }
  }
  return info;
}

// Try to find definite mines and safe cells using basic constraint propagation
bool BasicSolve(std::vector<std::pair<int,int>>& safe_cells, std::vector<std::pair<int,int>>& mine_cells) {
  bool found = false;
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] >= '1' && client_map[i][j] <= '8') {
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.unknown_count == 0) continue;
        
        int remaining = info.number - info.marked_count;
        
        // All unknowns are mines
        if (remaining == info.unknown_count) {
          for (auto& p : info.unknowns) {
            mine_cells.push_back(p);
          }
          found = true;
        }
        // All unknowns are safe
        else if (remaining == 0) {
          for (auto& p : info.unknowns) {
            safe_cells.push_back(p);
          }
          found = true;
        }
      }
    }
  }
  return found;
}

// Constraint-based subset reasoning
// If constraint A is a subset of constraint B, we can derive new info
bool SubsetSolve(std::vector<std::pair<int,int>>& safe_cells, std::vector<std::pair<int,int>>& mine_cells) {
  struct Constraint {
    std::vector<std::pair<int,int>> cells;
    int mine_count; // remaining mines in these cells
  };
  
  std::vector<Constraint> constraints;
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] >= '1' && client_map[i][j] <= '8') {
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.unknown_count == 0) continue;
        int remaining = info.number - info.marked_count;
        if (remaining < 0 || remaining > info.unknown_count) continue;
        
        Constraint c;
        c.cells = info.unknowns;
        std::sort(c.cells.begin(), c.cells.end());
        c.mine_count = remaining;
        constraints.push_back(c);
      }
    }
  }
  
  // Remove duplicate constraints
  auto cmp = [](const Constraint& a, const Constraint& b) {
    if (a.cells != b.cells) return a.cells < b.cells;
    return a.mine_count < b.mine_count;
  };
  std::sort(constraints.begin(), constraints.end(), cmp);
  constraints.erase(std::unique(constraints.begin(), constraints.end(), 
    [](const Constraint& a, const Constraint& b) {
      return a.cells == b.cells && a.mine_count == b.mine_count;
    }), constraints.end());
  
  bool found = false;
  
  for (int i = 0; i < (int)constraints.size(); i++) {
    for (int j = 0; j < (int)constraints.size(); j++) {
      if (i == j) continue;
      // Check if constraints[i].cells is a subset of constraints[j].cells
      auto& smaller = constraints[i].cells;
      auto& larger = constraints[j].cells;
      if (smaller.size() >= larger.size()) continue;
      
      if (std::includes(larger.begin(), larger.end(), smaller.begin(), smaller.end())) {
        // The difference cells have (larger.mine_count - smaller.mine_count) mines
        std::vector<std::pair<int,int>> diff;
        std::set_difference(larger.begin(), larger.end(), smaller.begin(), smaller.end(), std::back_inserter(diff));
        
        int diff_mines = constraints[j].mine_count - constraints[i].mine_count;
        
        if (diff_mines < 0) continue;
        
        if (diff_mines == 0) {
          // All diff cells are safe
          for (auto& p : diff) {
            safe_cells.push_back(p);
          }
          found = true;
        } else if (diff_mines == (int)diff.size()) {
          // All diff cells are mines
          for (auto& p : diff) {
            mine_cells.push_back(p);
          }
          found = true;
        }
      }
    }
  }
  
  return found;
}

// Gauss elimination based solver for more complex reasoning
// Each numbered cell with unknown neighbors gives a linear constraint
// We can use Gaussian elimination over GF(2)-like logic, but here we do it 
// with integer constraints: sum of variables = mine_count

// Variables: unknown cells adjacent to numbered cells (the "frontier")
// For each constraint: sum of subset of variables = remaining_mine_count

struct GaussConstraint {
  std::vector<int> var_indices; // indices into frontier_cells
  int mine_count;
};

bool GaussSolve(std::vector<std::pair<int,int>>& safe_cells, std::vector<std::pair<int,int>>& mine_cells) {
  // Collect frontier cells (unknown cells adjacent to at least one number)
  std::vector<std::pair<int,int>> frontier;
  bool is_frontier[35][35];
  int frontier_id[35][35];
  memset(is_frontier, 0, sizeof(is_frontier));
  memset(frontier_id, -1, sizeof(frontier_id));
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] != '?') continue;
      for (int d = 0; d < 8; d++) {
        int ni = i + cdx[d], nj = j + cdy[d];
        if (CInBounds(ni, nj) && client_map[ni][nj] >= '0' && client_map[ni][nj] <= '8') {
          is_frontier[i][j] = true;
          break;
        }
      }
    }
  }
  
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (is_frontier[i][j]) {
        frontier_id[i][j] = frontier.size();
        frontier.push_back({i, j});
      }
  
  int n_vars = frontier.size();
  if (n_vars == 0) return false;
  if (n_vars > 200) return false; // too many for enumeration
  
  // Build constraints
  std::vector<std::vector<double>> matrix;
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] < '1' || client_map[i][j] > '8') continue;
      NeighborInfo info = GetNeighborInfo(i, j);
      if (info.unknown_count == 0) continue;
      
      std::vector<double> row(n_vars + 1, 0);
      for (auto& p : info.unknowns) {
        int id = frontier_id[p.first][p.second];
        if (id >= 0) row[id] = 1;
      }
      row[n_vars] = info.number - info.marked_count;
      matrix.push_back(row);
    }
  }
  
  int n_eq = matrix.size();
  if (n_eq == 0) return false;
  
  // Gaussian elimination
  int pivot_row = 0;
  std::vector<int> pivot_col(n_eq, -1);
  
  for (int col = 0; col < n_vars && pivot_row < n_eq; col++) {
    // Find pivot
    int best = -1;
    for (int r = pivot_row; r < n_eq; r++) {
      if (std::abs(matrix[r][col]) > 0.5) {
        best = r;
        break;
      }
    }
    if (best == -1) continue;
    
    std::swap(matrix[pivot_row], matrix[best]);
    pivot_col[pivot_row] = col;
    
    for (int r = 0; r < n_eq; r++) {
      if (r == pivot_row) continue;
      if (std::abs(matrix[r][col]) > 0.5) {
        double factor = matrix[r][col] / matrix[pivot_row][col];
        for (int c = 0; c <= n_vars; c++) {
          matrix[r][c] -= factor * matrix[pivot_row][c];
        }
      }
    }
    pivot_row++;
  }
  
  // After elimination, check each row for definite values
  bool found = false;
  for (int r = 0; r < pivot_row; r++) {
    // Count non-zero entries
    std::vector<int> nonzero_vars;
    double rhs = matrix[r][n_vars];
    
    for (int c = 0; c < n_vars; c++) {
      double v = matrix[r][c];
      if (std::abs(v) > 0.5) {
        nonzero_vars.push_back(c);
      }
    }
    
    if (nonzero_vars.empty()) continue;
    
    // Check if all coefficients are the same sign (+1 or -1)
    // If all +1: sum of vars = rhs
    // If rhs == 0, all are safe. If rhs == count, all are mines.
    bool all_positive = true, all_negative = true;
    for (int c : nonzero_vars) {
      if (matrix[r][c] < 0) all_positive = false;
      if (matrix[r][c] > 0) all_negative = false;
    }
    
    if (all_positive) {
      int count = nonzero_vars.size();
      int irhs = (int)(rhs + 0.5);
      if (irhs == 0) {
        for (int c : nonzero_vars) {
          safe_cells.push_back(frontier[c]);
        }
        found = true;
      } else if (irhs == count) {
        for (int c : nonzero_vars) {
          mine_cells.push_back(frontier[c]);
        }
        found = true;
      }
    } else if (all_negative) {
      int count = nonzero_vars.size();
      int irhs = (int)(-rhs + 0.5);
      if (irhs == 0) {
        for (int c : nonzero_vars) {
          safe_cells.push_back(frontier[c]);
        }
        found = true;
      } else if (irhs == count) {
        for (int c : nonzero_vars) {
          mine_cells.push_back(frontier[c]);
        }
        found = true;
      }
    }
    // Handle mixed signs: a - b = 0 means a == b, etc.
    // For now handle the simple case: two variables with opposite signs
    else if (nonzero_vars.size() == 2) {
      int c1 = nonzero_vars[0], c2 = nonzero_vars[1];
      double v1 = matrix[r][c1], v2 = matrix[r][c2];
      // v1*x1 + v2*x2 = rhs, where x1,x2 in {0,1}
      // Enumerate: check which (x1,x2) pairs are feasible
      int irhs = (int)(rhs + (rhs > 0 ? 0.5 : -0.5));
      bool f00 = (std::abs(0 - irhs) < 0.01);
      bool f01 = (std::abs(v2 - irhs) < 0.01);
      bool f10 = (std::abs(v1 - irhs) < 0.01);
      bool f11 = (std::abs(v1 + v2 - irhs) < 0.01);
      
      // Also need x1,x2 in {0,1}
      int feasible = f00 + f01 + f10 + f11;
      if (feasible > 0) {
        // Check if x1 is determined
        bool x1_can_be_0 = f00 || f01;
        bool x1_can_be_1 = f10 || f11;
        bool x2_can_be_0 = f00 || f10;
        bool x2_can_be_1 = f01 || f11;
        
        if (x1_can_be_0 && !x1_can_be_1) { safe_cells.push_back(frontier[c1]); found = true; }
        if (x1_can_be_1 && !x1_can_be_0) { mine_cells.push_back(frontier[c1]); found = true; }
        if (x2_can_be_0 && !x2_can_be_1) { safe_cells.push_back(frontier[c2]); found = true; }
        if (x2_can_be_1 && !x2_can_be_0) { mine_cells.push_back(frontier[c2]); found = true; }
      }
    }
  }
  
  // Also try: enumerate over connected components of frontier if small enough
  // Find connected groups (cells sharing a constraint)
  if (!found && n_vars <= 30) {
    // Try brute force on small groups
    // Group variables by connected components via constraints
    std::vector<int> group(n_vars, -1);
    int n_groups = 0;
    
    // Build adjacency
    std::vector<std::vector<int>> adj(n_vars);
    for (int i = 0; i < rows; i++) {
      for (int j = 0; j < columns; j++) {
        if (client_map[i][j] < '1' || client_map[i][j] > '8') continue;
        NeighborInfo info = GetNeighborInfo(i, j);
        std::vector<int> ids;
        for (auto& p : info.unknowns) {
          int id = frontier_id[p.first][p.second];
          if (id >= 0) ids.push_back(id);
        }
        for (int a = 0; a < (int)ids.size(); a++)
          for (int b = a+1; b < (int)ids.size(); b++) {
            adj[ids[a]].push_back(ids[b]);
            adj[ids[b]].push_back(ids[a]);
          }
      }
    }
    
    // BFS to find components
    for (int i = 0; i < n_vars; i++) {
      if (group[i] != -1) continue;
      std::vector<int> queue = {i};
      group[i] = n_groups;
      for (int qi = 0; qi < (int)queue.size(); qi++) {
        for (int nb : adj[queue[qi]]) {
          if (group[nb] == -1) {
            group[nb] = n_groups;
            queue.push_back(nb);
          }
        }
      }
      n_groups++;
    }
    
    // For each small component, enumerate
    for (int g = 0; g < n_groups; g++) {
      std::vector<int> comp_vars;
      for (int i = 0; i < n_vars; i++)
        if (group[i] == g) comp_vars.push_back(i);
      
      if ((int)comp_vars.size() > 20) continue;
      
      int comp_size = comp_vars.size();
      
      // Get constraints involving only these variables
      struct LocalConstraint {
        std::vector<int> local_ids; // indices into comp_vars
        int mine_count;
      };
      std::vector<LocalConstraint> local_constraints;
      
      for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
          if (client_map[i][j] < '1' || client_map[i][j] > '8') continue;
          NeighborInfo info = GetNeighborInfo(i, j);
          if (info.unknown_count == 0) continue;
          
          std::vector<int> lids;
          bool all_in_comp = true;
          for (auto& p : info.unknowns) {
            int id = frontier_id[p.first][p.second];
            if (id < 0 || group[id] != g) { all_in_comp = false; break; }
            // Find local index
            for (int k = 0; k < comp_size; k++)
              if (comp_vars[k] == id) { lids.push_back(k); break; }
          }
          if (!all_in_comp) continue;
          if (lids.empty()) continue;
          
          LocalConstraint lc;
          lc.local_ids = lids;
          lc.mine_count = info.number - info.marked_count;
          local_constraints.push_back(lc);
        }
      }
      
      // Enumerate all 2^comp_size assignments
      std::vector<int> can_be_0(comp_size, 0), can_be_1(comp_size, 0);
      
      for (int mask = 0; mask < (1 << comp_size); mask++) {
        bool valid = true;
        for (auto& lc : local_constraints) {
          int cnt = 0;
          for (int lid : lc.local_ids) {
            if (mask & (1 << lid)) cnt++;
          }
          if (cnt != lc.mine_count) { valid = false; break; }
        }
        if (valid) {
          for (int k = 0; k < comp_size; k++) {
            if (mask & (1 << k)) can_be_1[k]++;
            else can_be_0[k]++;
          }
        }
      }
      
      for (int k = 0; k < comp_size; k++) {
        if (can_be_0[k] > 0 && can_be_1[k] == 0) {
          safe_cells.push_back(frontier[comp_vars[k]]);
          found = true;
        } else if (can_be_1[k] > 0 && can_be_0[k] == 0) {
          mine_cells.push_back(frontier[comp_vars[k]]);
          found = true;
        }
      }
    }
  }
  
  return found;
}

// Count unknowns and remaining mines for probability guess
std::pair<int,int> BestGuess() {
  // Count frontier unknowns and their mine probability
  bool is_frontier[35][35];
  memset(is_frontier, 0, sizeof(is_frontier));
  
  int total_unknown_count = 0;
  int frontier_count = 0;
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] == '?') {
        total_unknown_count++;
        for (int d = 0; d < 8; d++) {
          int ni = i + cdx[d], nj = j + cdy[d];
          if (CInBounds(ni, nj) && client_map[ni][nj] >= '0' && client_map[ni][nj] <= '8') {
            is_frontier[i][j] = true;
            frontier_count++;
            break;
          }
        }
      }
    }
  }
  
  // Count remaining mines
  int mines_left = remaining_mines;
  
  // If there are non-frontier unknowns, their mine probability is roughly
  // mines_left / total_unknown_count (uniform prior)
  // Frontier cells might have higher or lower probability based on constraints
  
  // Simple heuristic: pick the unknown cell with the lowest estimated mine probability
  // For frontier cells, use constraint info; for non-frontier, use global probability
  
  // Simpler approach: prefer corners, edges (fewer neighbors = less risk often)
  // Or: prefer cells adjacent to low numbers
  
  double best_score = 1e9;
  int best_r = -1, best_c = -1;
  
  // For non-frontier cells (far from numbers), mine probability = mines_left / total_unknown_count
  // This is often low for large boards
  
  // For frontier cells, try to estimate probability
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] != '?') continue;
      
      double score;
      if (!is_frontier[i][j]) {
        // Non-frontier: global probability, slightly prefer these
        if (total_unknown_count > 0)
          score = (double)mines_left / total_unknown_count;
        else
          score = 0.5;
      } else {
        // Frontier: estimate from adjacent number constraints
        // Average of (remaining_mines_for_neighbor / unknown_count_for_neighbor)
        double sum_prob = 0;
        int count = 0;
        for (int d = 0; d < 8; d++) {
          int ni = i + cdx[d], nj = j + cdy[d];
          if (CInBounds(ni, nj) && client_map[ni][nj] >= '1' && client_map[ni][nj] <= '8') {
            NeighborInfo info = GetNeighborInfo(ni, nj);
            if (info.unknown_count > 0) {
              double p = (double)(info.number - info.marked_count) / info.unknown_count;
              sum_prob += p;
              count++;
            }
          }
        }
        if (count > 0)
          score = sum_prob / count;
        else
          score = 0.5;
      }
      
      if (score < best_score) {
        best_score = score;
        best_r = i;
        best_c = j;
      }
    }
  }
  
  return {best_r, best_c};
}

void Decide() {
  // Update remaining mines count
  remaining_mines = total_mines;
  total_unknown = 0;
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] == '@') remaining_mines--;
      if (client_map[i][j] == '?') total_unknown++;
    }
  }
  
  // Step 1: Try basic constraint propagation
  std::vector<std::pair<int,int>> safe_cells, mine_cells;
  
  BasicSolve(safe_cells, mine_cells);
  
  if (!mine_cells.empty() && safe_cells.empty()) {
    // Also try subset + gauss to find safe cells
    SubsetSolve(safe_cells, mine_cells);
    if (safe_cells.empty()) GaussSolve(safe_cells, mine_cells);
  }
  
  if (safe_cells.empty() && mine_cells.empty()) {
    SubsetSolve(safe_cells, mine_cells);
  }
  
  if (safe_cells.empty() && mine_cells.empty()) {
    GaussSolve(safe_cells, mine_cells);
  }
  
  // Remove duplicates
  auto dedup = [](std::vector<std::pair<int,int>>& v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
  };
  dedup(safe_cells);
  dedup(mine_cells);
  
  // Prefer marking mines first if we know them, then visiting safe cells
  if (!mine_cells.empty()) {
    Execute(mine_cells[0].first, mine_cells[0].second, 1);
    return;
  }
  
  if (!safe_cells.empty()) {
    Execute(safe_cells[0].first, safe_cells[0].second, 0);
    return;
  }
  
  // Check if any numbered cell can be auto-explored (all mines around it marked)
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] >= '1' && client_map[i][j] <= '8') {
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.marked_count == info.number && info.unknown_count > 0) {
          Execute(i, j, 2);
          return;
        }
      }
    }
  }
  
  // No definite move - have to guess
  // Check if remaining_mines == 0, all unknowns are safe
  if (remaining_mines == 0) {
    for (int i = 0; i < rows; i++)
      for (int j = 0; j < columns; j++)
        if (client_map[i][j] == '?') {
          Execute(i, j, 0);
          return;
        }
  }
  
  // Check if remaining_mines == total_unknown, all unknowns are mines
  if (remaining_mines == total_unknown) {
    for (int i = 0; i < rows; i++)
      for (int j = 0; j < columns; j++)
        if (client_map[i][j] == '?') {
          Execute(i, j, 1);
          return;
        }
  }
  
  // Make best guess
  auto [gr, gc] = BestGuess();
  if (gr >= 0) {
    Execute(gr, gc, 0);
    return;
  }
  
  // Fallback: visit any unknown
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] == '?') {
        Execute(i, j, 0);
        return;
      }
}

#endif
