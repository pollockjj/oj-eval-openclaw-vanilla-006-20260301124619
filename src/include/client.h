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
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++) {
      client_map[i][j] = '?';
      mine_prob[i][j] = 0.5;
    }
  int first_row, first_column;
  std::cin >> first_row >> first_column;
  Execute(first_row, first_column, 0);
}

void ReadMap() {
  for (int i = 0; i < rows; i++) {
    std::string line;
    std::cin >> line;
    for (int j = 0; j < columns; j++)
      client_map[i][j] = line[j];
  }
}

struct NeighborInfo {
  int unknown_count, marked_count, number;
  std::vector<std::pair<int,int>> unknowns;
};

NeighborInfo GetNeighborInfo(int r, int c) {
  NeighborInfo info;
  info.unknown_count = info.marked_count = 0;
  info.number = client_map[r][c] - '0';
  for (int d = 0; d < 8; d++) {
    int nr = r + cdx[d], nc = c + cdy[d];
    if (CInBounds(nr, nc)) {
      if (client_map[nr][nc] == '?') {
        info.unknown_count++;
        info.unknowns.push_back({nr, nc});
      } else if (client_map[nr][nc] == '@')
        info.marked_count++;
    }
  }
  return info;
}

// Constraint: a set of variable indices must sum to mine_count
struct CConstraint {
  std::vector<int> vars; // sorted
  int mine_count;
};

// Backtracking solver for a connected component
// Returns true if any definite conclusions found
// Also fills in probabilities for probabilistic guessing
struct ComponentSolver {
  int n;
  std::vector<std::pair<int,int>> cells; // the cells in this component
  std::vector<CConstraint> constraints;
  // For each variable, which constraints involve it
  std::vector<std::vector<int>> var_constraints;
  
  // assignment: -1 = unset, 0 = safe, 1 = mine
  std::vector<int> assignment;
  // Count of valid assignments where cell k is a mine
  std::vector<long long> mine_count_sum;
  long long total_valid;
  
  // Definite results
  std::vector<int> definite_safe, definite_mine;
  
  bool timed_out;
  long long enum_count;
  static const long long MAX_ENUM = 5000000LL;
  
  void Init(int size) {
    n = size;
    var_constraints.resize(n);
    assignment.assign(n, -1);
    mine_count_sum.assign(n, 0);
    total_valid = 0;
    timed_out = false;
    enum_count = 0;
  }
  
  void AddConstraint(const CConstraint& c) {
    int idx = constraints.size();
    constraints.push_back(c);
    for (int v : c.vars)
      var_constraints[v].push_back(idx);
  }
  
  // Check if current partial assignment is consistent
  bool IsConsistent() {
    for (auto& c : constraints) {
      int assigned_mines = 0, unassigned = 0;
      for (int v : c.vars) {
        if (assignment[v] == -1) unassigned++;
        else if (assignment[v] == 1) assigned_mines++;
      }
      // Too many mines already
      if (assigned_mines > c.mine_count) return false;
      // Not enough unassigned to reach mine_count
      if (assigned_mines + unassigned < c.mine_count) return false;
    }
    return true;
  }
  
  // Check consistency only for constraints involving variable v
  bool IsConsistentFor(int v) {
    for (int ci : var_constraints[v]) {
      auto& c = constraints[ci];
      int assigned_mines = 0, unassigned = 0;
      for (int var : c.vars) {
        if (assignment[var] == -1) unassigned++;
        else if (assignment[var] == 1) assigned_mines++;
      }
      if (assigned_mines > c.mine_count) return false;
      if (assigned_mines + unassigned < c.mine_count) return false;
    }
    return true;
  }
  
  void Backtrack(int idx) {
    if (timed_out) return;
    if (++enum_count > MAX_ENUM) {
      timed_out = true;
      return;
    }
    
    if (idx == n) {
      // All assigned - check all constraints satisfied exactly
      for (auto& c : constraints) {
        int mines = 0;
        for (int v : c.vars)
          if (assignment[v] == 1) mines++;
        if (mines != c.mine_count) return;
      }
      total_valid++;
      for (int k = 0; k < n; k++)
        if (assignment[k] == 1) mine_count_sum[k]++;
      return;
    }
    
    // Try safe (0)
    assignment[idx] = 0;
    if (IsConsistentFor(idx))
      Backtrack(idx + 1);
    if (timed_out) return;
    
    // Try mine (1)
    assignment[idx] = 1;
    if (IsConsistentFor(idx))
      Backtrack(idx + 1);
    
    assignment[idx] = -1;
  }
  
  void Solve() {
    Backtrack(0);
    
    if (!timed_out && total_valid > 0) {
      for (int k = 0; k < n; k++) {
        if (mine_count_sum[k] == 0)
          definite_safe.push_back(k);
        else if (mine_count_sum[k] == total_valid)
          definite_mine.push_back(k);
      }
    }
  }
};

bool DeduceDefinite(std::vector<std::pair<int,int>>& safe_cells, std::vector<std::pair<int,int>>& mine_cells) {
  bool found = false;
  
  // Basic constraint checking
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
  
  // Global constraints
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
  struct SimpleConstraint {
    std::vector<std::pair<int,int>> cells;
    int mine_count;
  };
  std::vector<SimpleConstraint> constraints;
  
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] >= '1' && client_map[i][j] <= '8') {
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.unknown_count == 0) continue;
        int remaining = info.number - info.marked_count;
        SimpleConstraint c;
        c.cells = info.unknowns;
        std::sort(c.cells.begin(), c.cells.end());
        c.mine_count = remaining;
        constraints.push_back(c);
      }
    }
  }
  
  // Deduplicate
  std::sort(constraints.begin(), constraints.end(), [](const SimpleConstraint& a, const SimpleConstraint& b) {
    return a.cells < b.cells || (a.cells == b.cells && a.mine_count < b.mine_count);
  });
  constraints.erase(std::unique(constraints.begin(), constraints.end(), [](const SimpleConstraint& a, const SimpleConstraint& b) {
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
  
  // Backtracking solver on connected components
  bool is_frontier[35][35];
  int frontier_id[35][35];
  memset(is_frontier, 0, sizeof(is_frontier));
  memset(frontier_id, -1, sizeof(frontier_id));
  std::vector<std::pair<int,int>> frontier;
  
  for (int i = 0; i < rows; i++)
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
  
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (is_frontier[i][j]) {
        frontier_id[i][j] = frontier.size();
        frontier.push_back({i, j});
      }
  
  int n_vars = frontier.size();
  if (n_vars == 0) return false;
  
  // Build adjacency
  std::vector<std::vector<int>> adj(n_vars);
  for (int i = 0; i < rows; i++)
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
  
  // Find connected components
  std::vector<int> group(n_vars, -1);
  std::vector<std::vector<int>> groups;
  int n_groups = 0;
  
  for (int i = 0; i < n_vars; i++) {
    if (group[i] != -1) continue;
    std::vector<int> queue = {i};
    group[i] = n_groups;
    for (int qi = 0; qi < (int)queue.size(); qi++)
      for (int nb : adj[queue[qi]])
        if (group[nb] == -1) {
          group[nb] = n_groups;
          queue.push_back(nb);
        }
    groups.push_back(queue);
    n_groups++;
  }
  
  for (int g = 0; g < n_groups; g++) {
    auto& comp = groups[g];
    int comp_size = comp.size();
    if (comp_size > 40) continue; // skip very large components
    
    ComponentSolver solver;
    solver.Init(comp_size);
    solver.cells.resize(comp_size);
    
    int local_id[900];
    memset(local_id, -1, sizeof(local_id));
    for (int k = 0; k < comp_size; k++) {
      local_id[comp[k]] = k;
      solver.cells[k] = frontier[comp[k]];
    }
    
    for (int i = 0; i < rows; i++)
      for (int j = 0; j < columns; j++) {
        if (client_map[i][j] < '1' || client_map[i][j] > '8') continue;
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.unknown_count == 0) continue;
        
        CConstraint cc;
        bool all_in = true;
        for (auto& p : info.unknowns) {
          int id = frontier_id[p.first][p.second];
          if (id < 0 || group[id] != g) { all_in = false; break; }
          cc.vars.push_back(local_id[id]);
        }
        if (!all_in || cc.vars.empty()) continue;
        std::sort(cc.vars.begin(), cc.vars.end());
        cc.mine_count = info.number - info.marked_count;
        solver.AddConstraint(cc);
      }
    
    solver.Solve();
    
    if (!solver.timed_out) {
      for (int k : solver.definite_safe) {
        safe_cells.push_back(solver.cells[k]);
        found = true;
      }
      for (int k : solver.definite_mine) {
        mine_cells.push_back(solver.cells[k]);
        found = true;
      }
    }
  }
  
  return found;
}

void ComputeProbabilities() {
  // Build frontier and components (same as above)
  bool is_frontier[35][35];
  int frontier_id[35][35];
  memset(is_frontier, 0, sizeof(is_frontier));
  memset(frontier_id, -1, sizeof(frontier_id));
  std::vector<std::pair<int,int>> frontier;
  
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++) {
      mine_prob[i][j] = -1;
      if (client_map[i][j] != '?') continue;
      for (int d = 0; d < 8; d++) {
        int ni = i + cdx[d], nj = j + cdy[d];
        if (CInBounds(ni, nj) && client_map[ni][nj] >= '0' && client_map[ni][nj] <= '8') {
          is_frontier[i][j] = true;
          break;
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
  int non_frontier_count = 0;
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] == '?' && !is_frontier[i][j])
        non_frontier_count++;
  
  if (n_vars == 0) {
    // All unknowns are non-frontier
    double p = (total_unknown_g > 0) ? (double)remaining_mines_g / total_unknown_g : 0;
    for (int i = 0; i < rows; i++)
      for (int j = 0; j < columns; j++)
        if (client_map[i][j] == '?')
          mine_prob[i][j] = p;
    return;
  }
  
  // Build adjacency and components
  std::vector<std::vector<int>> adj(n_vars);
  for (int i = 0; i < rows; i++)
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
  
  std::vector<int> group(n_vars, -1);
  std::vector<std::vector<int>> groups;
  int n_groups = 0;
  
  for (int i = 0; i < n_vars; i++) {
    if (group[i] != -1) continue;
    std::vector<int> queue = {i};
    group[i] = n_groups;
    for (int qi = 0; qi < (int)queue.size(); qi++)
      for (int nb : adj[queue[qi]])
        if (group[nb] == -1) {
          group[nb] = n_groups;
          queue.push_back(nb);
        }
    groups.push_back(queue);
    n_groups++;
  }
  
  double estimated_frontier_mines = 0;
  int computed_vars = 0;
  
  for (int g = 0; g < n_groups; g++) {
    auto& comp = groups[g];
    int comp_size = comp.size();
    
    if (comp_size <= 40) {
      ComponentSolver solver;
      solver.Init(comp_size);
      solver.cells.resize(comp_size);
      
      int local_id[900];
      memset(local_id, -1, sizeof(local_id));
      for (int k = 0; k < comp_size; k++) {
        local_id[comp[k]] = k;
        solver.cells[k] = frontier[comp[k]];
      }
      
      for (int i = 0; i < rows; i++)
        for (int j = 0; j < columns; j++) {
          if (client_map[i][j] < '1' || client_map[i][j] > '8') continue;
          NeighborInfo info = GetNeighborInfo(i, j);
          if (info.unknown_count == 0) continue;
          CConstraint cc;
          bool all_in = true;
          for (auto& p : info.unknowns) {
            int id = frontier_id[p.first][p.second];
            if (id < 0 || group[id] != g) { all_in = false; break; }
            cc.vars.push_back(local_id[id]);
          }
          if (!all_in || cc.vars.empty()) continue;
          std::sort(cc.vars.begin(), cc.vars.end());
          cc.mine_count = info.number - info.marked_count;
          solver.AddConstraint(cc);
        }
      
      solver.Solve();
      
      if (!solver.timed_out && solver.total_valid > 0) {
        for (int k = 0; k < comp_size; k++) {
          auto [r, c] = solver.cells[k];
          mine_prob[r][c] = (double)solver.mine_count_sum[k] / solver.total_valid;
          estimated_frontier_mines += mine_prob[r][c];
          computed_vars++;
        }
        continue;
      }
    }
    
    // Fallback: heuristic for large components
    for (int k : comp) {
      auto [r, c] = frontier[k];
      double sum_p = 0;
      int cnt = 0;
      for (int d = 0; d < 8; d++) {
        int ni = r + cdx[d], nc = c + cdy[d];
        if (CInBounds(ni, nc) && client_map[ni][nc] >= '1' && client_map[ni][nc] <= '8') {
          NeighborInfo info = GetNeighborInfo(ni, nc);
          if (info.unknown_count > 0) {
            sum_p += (double)(info.number - info.marked_count) / info.unknown_count;
            cnt++;
          }
        }
      }
      mine_prob[r][c] = (cnt > 0) ? sum_p / cnt : (double)remaining_mines_g / total_unknown_g;
      estimated_frontier_mines += mine_prob[r][c];
      computed_vars++;
    }
  }
  
  // Non-frontier probability
  double non_frontier_mines = std::max(0.0, remaining_mines_g - estimated_frontier_mines);
  double nf_prob = (non_frontier_count > 0) ? non_frontier_mines / non_frontier_count : 0;
  nf_prob = std::max(0.0, std::min(1.0, nf_prob));
  
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] == '?' && !is_frontier[i][j])
        mine_prob[i][j] = nf_prob;
  
  // Set any remaining unset
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] == '?' && mine_prob[i][j] < 0)
        mine_prob[i][j] = (double)remaining_mines_g / total_unknown_g;
}

void Decide() {
  remaining_mines_g = total_mines;
  total_unknown_g = 0;
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] == '@') remaining_mines_g--;
      if (client_map[i][j] == '?') total_unknown_g++;
    }
  
  std::vector<std::pair<int,int>> safe_cells, mine_cells;
  DeduceDefinite(safe_cells, mine_cells);
  
  auto dedup = [](std::vector<std::pair<int,int>>& v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
  };
  dedup(safe_cells);
  dedup(mine_cells);
  
  if (!mine_cells.empty()) {
    Execute(mine_cells[0].first, mine_cells[0].second, 1);
    return;
  }
  
  // Auto-explore
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] >= '1' && client_map[i][j] <= '8') {
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.marked_count == info.number && info.unknown_count > 0) {
          Execute(i, j, 2);
          return;
        }
      }
  
  if (!safe_cells.empty()) {
    Execute(safe_cells[0].first, safe_cells[0].second, 0);
    return;
  }
  
  // Guess
  ComputeProbabilities();
  
  double best_prob = 2.0;
  int best_r = -1, best_c = -1;
  
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] == '?' && mine_prob[i][j] < best_prob) {
        best_prob = mine_prob[i][j];
        best_r = i;
        best_c = j;
      }
  
  if (best_r >= 0) {
    Execute(best_r, best_c, 0);
    return;
  }
  
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] == '?') {
        Execute(i, j, 0);
        return;
      }
}

#endif
