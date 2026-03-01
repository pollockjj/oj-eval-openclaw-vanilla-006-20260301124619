#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <utility>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>

extern int rows;
extern int columns;
extern int total_mines;

char client_map[35][35];
// Mine probability for guessing
double mine_prob[35][35];

int remaining_mines_g;
int total_unknown_g;

int cdx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
int cdy[] = {-1, 0, 1, -1, 1, -1, 0, 1};

bool CInBounds(int r, int c) {
  return r >= 0 && r < rows && c >= 0 && c < columns;
}

void Execute(int r, int c, int type);

void InitGame() {
  remaining_mines_g = total_mines;
  total_unknown_g = rows * columns;
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      client_map[i][j] = '?';
      mine_prob[i][j] = 0.5;
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

// Definite deduction: find cells that MUST be mines or MUST be safe
bool DeduceDefinite(std::vector<std::pair<int,int>>& safe_cells, std::vector<std::pair<int,int>>& mine_cells) {
  bool found = false;
  
  // Basic: check each numbered cell
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] >= '1' && client_map[i][j] <= '8') {
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.unknown_count == 0) continue;
        int remaining = info.number - info.marked_count;
        if (remaining == info.unknown_count) {
          for (auto& p : info.unknowns) mine_cells.push_back(p);
          found = true;
        } else if (remaining == 0) {
          for (auto& p : info.unknowns) safe_cells.push_back(p);
          found = true;
        }
      }
    }
  }
  if (found) return true;
  
  // Global constraint
  if (remaining_mines_g == 0) {
    for (int i = 0; i < rows; i++)
      for (int j = 0; j < columns; j++)
        if (client_map[i][j] == '?') { safe_cells.push_back({i,j}); found = true; }
    if (found) return true;
  }
  if (remaining_mines_g == total_unknown_g) {
    for (int i = 0; i < rows; i++)
      for (int j = 0; j < columns; j++)
        if (client_map[i][j] == '?') { mine_cells.push_back({i,j}); found = true; }
    if (found) return true;
  }
  
  // Subset reasoning
  struct Constraint {
    std::vector<std::pair<int,int>> cells;
    int mine_count;
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
  
  // Deduplicate
  auto cmp = [](const Constraint& a, const Constraint& b) {
    if (a.cells != b.cells) return a.cells < b.cells;
    return a.mine_count < b.mine_count;
  };
  std::sort(constraints.begin(), constraints.end(), cmp);
  constraints.erase(std::unique(constraints.begin(), constraints.end(),
    [](const Constraint& a, const Constraint& b) {
      return a.cells == b.cells;
    }), constraints.end());
  
  for (int i = 0; i < (int)constraints.size(); i++) {
    for (int j = 0; j < (int)constraints.size(); j++) {
      if (i == j) continue;
      auto& smaller = constraints[i].cells;
      auto& larger = constraints[j].cells;
      if (smaller.size() >= larger.size()) continue;
      if (std::includes(larger.begin(), larger.end(), smaller.begin(), smaller.end())) {
        std::vector<std::pair<int,int>> diff;
        std::set_difference(larger.begin(), larger.end(), smaller.begin(), smaller.end(), std::back_inserter(diff));
        int diff_mines = constraints[j].mine_count - constraints[i].mine_count;
        if (diff_mines < 0) continue;
        if (diff_mines == 0) {
          for (auto& p : diff) safe_cells.push_back(p);
          found = true;
        } else if (diff_mines == (int)diff.size()) {
          for (auto& p : diff) mine_cells.push_back(p);
          found = true;
        }
      }
    }
  }
  if (found) return true;
  
  // Enumeration on connected components
  // Build frontier
  bool is_frontier[35][35];
  int frontier_id[35][35];
  memset(is_frontier, 0, sizeof(is_frontier));
  memset(frontier_id, -1, sizeof(frontier_id));
  std::vector<std::pair<int,int>> frontier;
  
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
  
  // Build adjacency for connected components
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
  
  std::vector<int> group(n_vars, -1);
  int n_groups = 0;
  std::vector<std::vector<int>> groups;
  
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
    groups.push_back(queue);
    n_groups++;
  }
  
  for (int g = 0; g < n_groups; g++) {
    int comp_size = groups[g].size();
    if (comp_size > 25) continue;
    
    auto& comp_vars = groups[g];
    
    // Get constraints
    struct LocalConstraint {
      int mask; // bitmask of variables in this constraint
      int mine_count;
    };
    std::vector<LocalConstraint> local_constraints;
    
    // Map from global frontier id to local index
    int local_id[900];
    memset(local_id, -1, sizeof(local_id));
    for (int k = 0; k < comp_size; k++)
      local_id[comp_vars[k]] = k;
    
    for (int i = 0; i < rows; i++) {
      for (int j = 0; j < columns; j++) {
        if (client_map[i][j] < '1' || client_map[i][j] > '8') continue;
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.unknown_count == 0) continue;
        
        int mask = 0;
        bool all_in = true;
        for (auto& p : info.unknowns) {
          int id = frontier_id[p.first][p.second];
          if (id < 0 || group[id] != g) { all_in = false; break; }
          mask |= (1 << local_id[id]);
        }
        if (!all_in) continue;
        if (mask == 0) continue;
        
        LocalConstraint lc;
        lc.mask = mask;
        lc.mine_count = info.number - info.marked_count;
        local_constraints.push_back(lc);
      }
    }
    
    std::vector<int> can_be_0(comp_size, 0), can_be_1(comp_size, 0);
    
    for (int mask = 0; mask < (1 << comp_size); mask++) {
      bool valid = true;
      for (auto& lc : local_constraints) {
        if (__builtin_popcount(mask & lc.mask) != lc.mine_count) { valid = false; break; }
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
  
  return found;
}

// Compute mine probabilities for all unknown cells using enumeration
void ComputeProbabilities() {
  bool is_frontier[35][35];
  int frontier_id_local[35][35];
  memset(is_frontier, 0, sizeof(is_frontier));
  memset(frontier_id_local, -1, sizeof(frontier_id_local));
  std::vector<std::pair<int,int>> frontier;
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      mine_prob[i][j] = -1; // unset
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
        frontier_id_local[i][j] = frontier.size();
        frontier.push_back({i, j});
      }
  
  int n_vars = frontier.size();
  
  // Build components
  std::vector<std::vector<int>> adj(n_vars);
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] < '1' || client_map[i][j] > '8') continue;
      NeighborInfo info = GetNeighborInfo(i, j);
      std::vector<int> ids;
      for (auto& p : info.unknowns) {
        int id = frontier_id_local[p.first][p.second];
        if (id >= 0) ids.push_back(id);
      }
      for (int a = 0; a < (int)ids.size(); a++)
        for (int b = a+1; b < (int)ids.size(); b++) {
          adj[ids[a]].push_back(ids[b]);
          adj[ids[b]].push_back(ids[a]);
        }
    }
  }
  
  std::vector<int> group(n_vars, -1);
  int n_groups = 0;
  std::vector<std::vector<int>> groups;
  
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
    groups.push_back(queue);
    n_groups++;
  }
  
  int non_frontier_unknown = 0;
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] == '?' && !is_frontier[i][j])
        non_frontier_unknown++;
  
  // For each component, compute probabilities via enumeration
  int frontier_mines_known = 0; // mines that must be in frontier (from components we can't enumerate)
  bool computed[900];
  memset(computed, 0, sizeof(computed));
  
  for (int g = 0; g < n_groups; g++) {
    int comp_size = groups[g].size();
    if (comp_size > 25) {
      // Can't enumerate - use heuristic
      // Average probability from adjacent constraints
      for (int k : groups[g]) {
        auto [r, c] = frontier[k];
        double sum_prob = 0;
        int count = 0;
        for (int d = 0; d < 8; d++) {
          int ni = r + cdx[d], nc = c + cdy[d];
          if (CInBounds(ni, nc) && client_map[ni][nc] >= '1' && client_map[ni][nc] <= '8') {
            NeighborInfo info = GetNeighborInfo(ni, nc);
            if (info.unknown_count > 0) {
              double p = (double)(info.number - info.marked_count) / info.unknown_count;
              sum_prob += p;
              count++;
            }
          }
        }
        if (count > 0)
          mine_prob[r][c] = sum_prob / count;
        else
          mine_prob[r][c] = (double)remaining_mines_g / total_unknown_g;
      }
      continue;
    }
    
    auto& comp_vars = groups[g];
    
    struct LocalConstraint {
      int mask;
      int mine_count;
    };
    std::vector<LocalConstraint> local_constraints;
    
    int local_id[900];
    memset(local_id, -1, sizeof(local_id));
    for (int k = 0; k < comp_size; k++)
      local_id[comp_vars[k]] = k;
    
    for (int i = 0; i < rows; i++) {
      for (int j = 0; j < columns; j++) {
        if (client_map[i][j] < '1' || client_map[i][j] > '8') continue;
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.unknown_count == 0) continue;
        
        int mask = 0;
        bool all_in = true;
        for (auto& p : info.unknowns) {
          int id = frontier_id_local[p.first][p.second];
          if (id < 0 || group[id] != g) { all_in = false; break; }
          mask |= (1 << local_id[id]);
        }
        if (!all_in) continue;
        if (mask == 0) continue;
        
        LocalConstraint lc;
        lc.mask = mask;
        lc.mine_count = info.number - info.marked_count;
        local_constraints.push_back(lc);
      }
    }
    
    long long total_valid = 0;
    std::vector<long long> mine_count_per_var(comp_size, 0);
    
    for (int mask = 0; mask < (1 << comp_size); mask++) {
      bool valid = true;
      for (auto& lc : local_constraints) {
        if (__builtin_popcount(mask & lc.mask) != lc.mine_count) { valid = false; break; }
      }
      if (valid) {
        // Also check: number of mines in this component <= remaining_mines_g
        int mines_here = __builtin_popcount(mask);
        // We don't strictly enforce global constraint per component but it's a filter
        total_valid++;
        for (int k = 0; k < comp_size; k++) {
          if (mask & (1 << k)) mine_count_per_var[k]++;
        }
      }
    }
    
    if (total_valid > 0) {
      for (int k = 0; k < comp_size; k++) {
        auto [r, c] = frontier[comp_vars[k]];
        mine_prob[r][c] = (double)mine_count_per_var[k] / total_valid;
        computed[comp_vars[k]] = true;
      }
    }
  }
  
  // Non-frontier unknowns: global probability
  // Estimate frontier mines from computed probabilities
  double expected_frontier_mines = 0;
  int frontier_computed = 0;
  for (int k = 0; k < n_vars; k++) {
    if (computed[k]) {
      auto [r, c] = frontier[k];
      expected_frontier_mines += mine_prob[r][c];
      frontier_computed++;
    }
  }
  
  double non_frontier_mine_estimate = remaining_mines_g - expected_frontier_mines;
  // Subtract uncomputed frontier cells' estimated contribution
  int uncomputed_frontier = n_vars - frontier_computed;
  
  // For uncomputed frontier cells, we already set their probability above
  // For non-frontier cells:
  if (non_frontier_unknown > 0) {
    double p = std::max(0.0, std::min(1.0, non_frontier_mine_estimate / (non_frontier_unknown + uncomputed_frontier)));
    for (int i = 0; i < rows; i++)
      for (int j = 0; j < columns; j++)
        if (client_map[i][j] == '?' && !is_frontier[i][j])
          mine_prob[i][j] = p;
  }
  
  // Set default for any unset
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] == '?' && mine_prob[i][j] < 0)
        mine_prob[i][j] = (double)remaining_mines_g / total_unknown_g;
}

void Decide() {
  // Update counts
  remaining_mines_g = total_mines;
  total_unknown_g = 0;
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] == '@') remaining_mines_g--;
      if (client_map[i][j] == '?') total_unknown_g++;
    }
  }
  
  // Try definite deductions
  std::vector<std::pair<int,int>> safe_cells, mine_cells;
  DeduceDefinite(safe_cells, mine_cells);
  
  // Deduplicate
  auto dedup = [](std::vector<std::pair<int,int>>& v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
  };
  dedup(safe_cells);
  dedup(mine_cells);
  
  // Priority: mark mines first (enables auto-explore), then safe cells
  if (!mine_cells.empty()) {
    Execute(mine_cells[0].first, mine_cells[0].second, 1);
    return;
  }
  
  // Check auto-explore opportunities
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
  
  if (!safe_cells.empty()) {
    Execute(safe_cells[0].first, safe_cells[0].second, 0);
    return;
  }
  
  // No definite move - compute probabilities and guess
  ComputeProbabilities();
  
  double best_prob = 2.0;
  int best_r = -1, best_c = -1;
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] != '?') continue;
      if (mine_prob[i][j] < best_prob) {
        best_prob = mine_prob[i][j];
        best_r = i;
        best_c = j;
      }
    }
  }
  
  if (best_r >= 0) {
    Execute(best_r, best_c, 0);
    return;
  }
  
  // Fallback
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] == '?') {
        Execute(i, j, 0);
        return;
      }
}

#endif
