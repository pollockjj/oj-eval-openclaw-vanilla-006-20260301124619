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

struct CConstraint {
  std::vector<int> vars;
  int mine_count;
};

// Backtracking solver with constraint propagation
struct ComponentSolver {
  int n;
  std::vector<std::pair<int,int>> cells;
  std::vector<CConstraint> constraints;
  std::vector<std::vector<int>> var_constraints;
  
  std::vector<int> assignment; // -1=unset, 0=safe, 1=mine
  std::vector<long long> mine_count_sum;
  long long total_valid;
  
  std::vector<int> definite_safe, definite_mine;
  
  bool timed_out;
  long long enum_count;
  static const long long MAX_ENUM = 8000000LL;
  
  // Global mine constraint: total mines in this component
  int min_mines_in_comp, max_mines_in_comp;
  
  void Init(int size) {
    n = size;
    var_constraints.resize(n);
    assignment.assign(n, -1);
    mine_count_sum.assign(n, 0);
    total_valid = 0;
    timed_out = false;
    enum_count = 0;
    min_mines_in_comp = 0;
    max_mines_in_comp = size;
  }
  
  void AddConstraint(const CConstraint& c) {
    int idx = constraints.size();
    constraints.push_back(c);
    for (int v : c.vars)
      var_constraints[v].push_back(idx);
  }
  
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
    if (++enum_count > MAX_ENUM) { timed_out = true; return; }
    
    if (idx == n) {
      // Verify all constraints
      int total_m = 0;
      for (auto& c : constraints) {
        int mines = 0;
        for (int v : c.vars)
          if (assignment[v] == 1) mines++;
        if (mines != c.mine_count) return;
      }
      for (int k = 0; k < n; k++)
        if (assignment[k] == 1) total_m++;
      
      // Check global constraint
      if (total_m < min_mines_in_comp || total_m > max_mines_in_comp) return;
      
      total_valid++;
      for (int k = 0; k < n; k++)
        if (assignment[k] == 1) mine_count_sum[k]++;
      return;
    }
    
    // Count current mines
    int curr_mines = 0, remaining_vars = n - idx;
    for (int k = 0; k < idx; k++)
      if (assignment[k] == 1) curr_mines++;
    
    // Try safe (0) - prune if can't reach min_mines
    if (curr_mines + (remaining_vars - 1) >= min_mines_in_comp) {
      assignment[idx] = 0;
      if (IsConsistentFor(idx))
        Backtrack(idx + 1);
      if (timed_out) return;
    }
    
    // Try mine (1) - prune if already at max
    if (curr_mines + 1 <= max_mines_in_comp) {
      assignment[idx] = 1;
      if (IsConsistentFor(idx))
        Backtrack(idx + 1);
    }
    
    assignment[idx] = -1;
  }
  
  void Solve() {
    Backtrack(0);
    if (!timed_out && total_valid > 0) {
      for (int k = 0; k < n; k++) {
        if (mine_count_sum[k] == 0) definite_safe.push_back(k);
        else if (mine_count_sum[k] == total_valid) definite_mine.push_back(k);
      }
    }
  }
};

// Build frontier info used by both deduction and probability computation
struct FrontierInfo {
  bool is_frontier[35][35];
  int frontier_id[35][35];
  std::vector<std::pair<int,int>> frontier;
  std::vector<std::vector<int>> groups;
  std::vector<int> group;
  int n_groups;
  int non_frontier_count;
  
  void Build() {
    memset(is_frontier, 0, sizeof(is_frontier));
    memset(frontier_id, -1, sizeof(frontier_id));
    frontier.clear();
    groups.clear();
    non_frontier_count = 0;
    
    for (int i = 0; i < rows; i++)
      for (int j = 0; j < columns; j++) {
        if (client_map[i][j] != '?') continue;
        bool on_frontier = false;
        for (int d = 0; d < 8; d++) {
          int ni = i + cdx[d], nj = j + cdy[d];
          if (CInBounds(ni, nj) && client_map[ni][nj] >= '0' && client_map[ni][nj] <= '8') {
            on_frontier = true;
            break;
          }
        }
        if (on_frontier) is_frontier[i][j] = true;
        else non_frontier_count++;
      }
    
    for (int i = 0; i < rows; i++)
      for (int j = 0; j < columns; j++)
        if (is_frontier[i][j]) {
          frontier_id[i][j] = frontier.size();
          frontier.push_back({i, j});
        }
    
    int n_vars = frontier.size();
    if (n_vars == 0) { n_groups = 0; return; }
    
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
    
    group.assign(n_vars, -1);
    n_groups = 0;
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
  }
  
  ComponentSolver BuildSolver(int g) {
    auto& comp = groups[g];
    int comp_size = comp.size();
    
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
    
    // Set global mine constraint for this component
    // We know total remaining_mines_g, non_frontier_count cells are not in frontier
    // At most remaining_mines_g mines in this component (trivially)
    // At least max(0, remaining_mines_g - non_frontier_count - other_frontier_cells)
    int other_frontier = (int)frontier.size() - comp_size;
    solver.min_mines_in_comp = std::max(0, remaining_mines_g - non_frontier_count - other_frontier);
    solver.max_mines_in_comp = std::min(comp_size, remaining_mines_g);
    
    return solver;
  }
};

FrontierInfo finfo;

bool DeduceDefinite(std::vector<std::pair<int,int>>& safe_cells, std::vector<std::pair<int,int>>& mine_cells) {
  bool found = false;
  
  // Basic
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] >= '1' && client_map[i][j] <= '8') {
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.unknown_count == 0) continue;
        int rem = info.number - info.marked_count;
        if (rem == info.unknown_count) {
          for (auto& p : info.unknowns) mine_cells.push_back(p);
          found = true;
        } else if (rem == 0) {
          for (auto& p : info.unknowns) safe_cells.push_back(p);
          found = true;
        }
      }
    }
  if (found) return true;
  
  // Global
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
  struct SC { std::vector<std::pair<int,int>> cells; int mc; };
  std::vector<SC> constraints;
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++) {
      if (client_map[i][j] >= '1' && client_map[i][j] <= '8') {
        NeighborInfo info = GetNeighborInfo(i, j);
        if (info.unknown_count == 0) continue;
        SC c; c.cells = info.unknowns; c.mc = info.number - info.marked_count;
        std::sort(c.cells.begin(), c.cells.end());
        constraints.push_back(c);
      }
    }
  std::sort(constraints.begin(), constraints.end(), [](const SC& a, const SC& b) { return a.cells < b.cells; });
  constraints.erase(std::unique(constraints.begin(), constraints.end(), [](const SC& a, const SC& b) { return a.cells == b.cells; }), constraints.end());
  
  for (int i = 0; i < (int)constraints.size(); i++)
    for (int j = 0; j < (int)constraints.size(); j++) {
      if (i == j || constraints[i].cells.size() >= constraints[j].cells.size()) continue;
      if (std::includes(constraints[j].cells.begin(), constraints[j].cells.end(),
                        constraints[i].cells.begin(), constraints[i].cells.end())) {
        std::vector<std::pair<int,int>> diff;
        std::set_difference(constraints[j].cells.begin(), constraints[j].cells.end(),
                           constraints[i].cells.begin(), constraints[i].cells.end(), std::back_inserter(diff));
        int dm = constraints[j].mc - constraints[i].mc;
        if (dm < 0) continue;
        if (dm == 0) { for (auto& p : diff) safe_cells.push_back(p); found = true; }
        else if (dm == (int)diff.size()) { for (auto& p : diff) mine_cells.push_back(p); found = true; }
      }
    }
  if (found) return true;
  
  // Backtracking on components
  finfo.Build();
  
  for (int g = 0; g < finfo.n_groups; g++) {
    if ((int)finfo.groups[g].size() > 50) continue;
    
    ComponentSolver solver = finfo.BuildSolver(g);
    solver.Solve();
    
    if (!solver.timed_out && solver.total_valid > 0) {
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
  finfo.Build();
  
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      mine_prob[i][j] = -1;
  
  double estimated_frontier_mines = 0;
  bool comp_computed[200];
  memset(comp_computed, 0, sizeof(comp_computed));
  
  for (int g = 0; g < finfo.n_groups; g++) {
    if ((int)finfo.groups[g].size() <= 50) {
      ComponentSolver solver = finfo.BuildSolver(g);
      solver.Solve();
      
      if (!solver.timed_out && solver.total_valid > 0) {
        int comp_size = finfo.groups[g].size();
        for (int k = 0; k < comp_size; k++) {
          auto [r, c] = solver.cells[k];
          mine_prob[r][c] = (double)solver.mine_count_sum[k] / solver.total_valid;
          estimated_frontier_mines += mine_prob[r][c];
        }
        comp_computed[g] = true;
        continue;
      }
    }
    
    // Heuristic fallback
    for (int k : finfo.groups[g]) {
      auto [r, c] = finfo.frontier[k];
      double sum_p = 0; int cnt = 0;
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
      double p = (cnt > 0) ? sum_p / cnt : (double)remaining_mines_g / total_unknown_g;
      mine_prob[r][c] = p;
      estimated_frontier_mines += p;
    }
  }
  
  // Non-frontier probability
  double nf_mines = std::max(0.0, (double)remaining_mines_g - estimated_frontier_mines);
  double nf_prob = (finfo.non_frontier_count > 0) ? nf_mines / finfo.non_frontier_count : 0;
  nf_prob = std::max(0.0, std::min(1.0, nf_prob));
  
  for (int i = 0; i < rows; i++)
    for (int j = 0; j < columns; j++)
      if (client_map[i][j] == '?' && !finfo.is_frontier[i][j])
        mine_prob[i][j] = nf_prob;
  
  // Fallback
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
  
  // Priority: mark mines (enables auto-explore)
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
  
  // Guess with probability
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
