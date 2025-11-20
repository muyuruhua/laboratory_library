/*
   AFL++ Lattice-based Multi-Armed Bandit Mutation Strategy Implementation
   -----------------------------------------------------------------------
*/

/* Include afl-fuzz.h first to get all necessary definitions */
#include "afl-fuzz.h"
/* Then include our header */
#include "afl-lattice-mab.h"
#include <string.h>
#include <stdlib.h>

/* Define MUT_* constants locally to avoid including afl-mutations.h
   which contains global variable definitions that cause duplicate symbols */
#ifndef MUT_FLIPBIT
#define MUT_FLIPBIT 0
#define MUT_INTERESTING8 1
#define MUT_INTERESTING16 2
#define MUT_INTERESTING16BE 3
#define MUT_INTERESTING32 4
#define MUT_INTERESTING32BE 5
#define MUT_ARITH8_ 6
#define MUT_ARITH8 7
#define MUT_ARITH16_ 8
#define MUT_ARITH16BE_ 9
#define MUT_ARITH16 10
#define MUT_ARITH16BE 11
#define MUT_ARITH32_ 12
#define MUT_ARITH32BE_ 13
#define MUT_ARITH32 14
#define MUT_ARITH32BE 15
#define MUT_RAND8 16
#define MUT_CLONE_COPY 17
#define MUT_CLONE_FIXED 18
#define MUT_OVERWRITE_COPY 19
#define MUT_OVERWRITE_FIXED 20
#define MUT_BYTEADD 21
#define MUT_BYTESUB 22
#define MUT_FLIP8 23
#define MUT_SWITCH 24
#define MUT_DEL 25
#define MUT_SHUFFLE 26
#define MUT_DELONE 27
#define MUT_INSERTONE 28
#define MUT_ASCIINUM 29
#define MUT_INSERTASCIINUM 30
#define MUT_EXTRA_OVERWRITE 31
#define MUT_EXTRA_INSERT 32
#define MUT_AUTO_EXTRA_OVERWRITE 33
#define MUT_AUTO_EXTRA_INSERT 34
#define MUT_SPLICE_OVERWRITE 35
#define MUT_SPLICE_INSERT 36
#define MUT_MAX 37
#endif

/* Initialize mutation vector from mutation type */
mutation_vector_t create_mutation_vector(u32 mutation_type) {

  mutation_vector_t vec;
  memset(&vec, 0, sizeof(mutation_vector_t));
  
  vec.mutation_type = mutation_type;
  vec.magnitude = 0.0;
  vec.usage_count = 0;
  vec.total_reward = 0;
  vec.avg_reward = 0.0;
  vec.ucb_value = 0.0;
  
  /* Create vector representation: one-hot encoding */
  for (u32 i = 0; i < LATTICE_DIMENSION; ++i) {
    
    vec.dimension[i] = (i == mutation_type) ? 1 : 0;
    vec.position[i] = (i == mutation_type) ? 1 : 0;
    
  }
  
  /* Calculate magnitude */
  double sum_sq = 0.0;
  for (u32 i = 0; i < LATTICE_DIMENSION; ++i) {
    
    sum_sq += vec.dimension[i] * vec.dimension[i];
    
  }
  vec.magnitude = sqrt(sum_sq);
  
  return vec;
  
}

/* Initialize mutation lattice */
void init_mutation_lattice(mutation_lattice_t *lattice) {

  if (!lattice) { return; }
  
  memset(lattice, 0, sizeof(mutation_lattice_t));
  lattice->vector_count = LATTICE_DIMENSION;
  
  /* Create vectors for all mutation types */
  for (u32 i = 0; i < LATTICE_DIMENSION && i < MUT_MAX; ++i) {
    
    lattice->vectors[i] = create_mutation_vector(i);
    
  }
  
  /* Allocate neighbor matrix */
  size_t matrix_size = LATTICE_DIMENSION * LATTICE_DIMENSION * sizeof(u32);
  lattice->neighbor_matrix = (u32 *)malloc(matrix_size);
  if (lattice->neighbor_matrix) {
    
    memset(lattice->neighbor_matrix, 0, matrix_size);
    
    /* Initialize adjacency: vectors are neighbors if distance < threshold */
    for (u32 i = 0; i < LATTICE_DIMENSION; ++i) {
      
      for (u32 j = 0; j < LATTICE_DIMENSION; ++j) {
        
        if (i != j) {
          
          double dist = lattice_distance(&lattice->vectors[i],
                                        &lattice->vectors[j]);
          if (dist <= LATTICE_NEIGHBOR_RADIUS) {
            
            lattice->neighbor_matrix[i * LATTICE_DIMENSION + j] = 1;
            
          }
          
        } else {
          
          lattice->neighbor_matrix[i * LATTICE_DIMENSION + j] = 1;
          
        }
        
      }
      
    }
    
  }
  
  /* Calculate lattice density */
  u32 neighbor_count = 0;
  for (u32 i = 0; i < LATTICE_DIMENSION; ++i) {
    
    for (u32 j = 0; j < LATTICE_DIMENSION; ++j) {
      
      if (lattice->neighbor_matrix[i * LATTICE_DIMENSION + j]) {
        
        neighbor_count++;
        
      }
      
    }
    
  }
  lattice->lattice_density = (double)neighbor_count / 
                              (LATTICE_DIMENSION * LATTICE_DIMENSION);
  
}

/* Calculate distance between two vectors in lattice space */
double lattice_distance(const mutation_vector_t *v1, 
                        const mutation_vector_t *v2) {

  if (!v1 || !v2) { return 1e10; }
  
  double dist_sq = 0.0;
  for (u32 i = 0; i < LATTICE_DIMENSION; ++i) {
    
    double diff = (double)v1->dimension[i] - (double)v2->dimension[i];
    dist_sq += diff * diff;
    
  }
  
  return sqrt(dist_sq);
  
}

/* Find nearest neighbors in lattice */
void find_lattice_neighbors(const mutation_lattice_t *lattice,
                            const mutation_vector_t *vector,
                            u32 *neighbors, u32 max_neighbors, u32 *found) {

  if (!lattice || !vector || !neighbors || !found) {
    
    if (found) { *found = 0; }
    return;
    
  }
  
  *found = 0;
  
  /* Use neighbor matrix for fast lookup */
  u32 vec_idx = vector->mutation_type;
  if (vec_idx >= LATTICE_DIMENSION) { return; }
  
  for (u32 i = 0; i < LATTICE_DIMENSION && *found < max_neighbors; ++i) {
    
    if (lattice->neighbor_matrix[vec_idx * LATTICE_DIMENSION + i] &&
        i != vec_idx) {
      
      neighbors[*found] = i;
      (*found)++;
      
    }
    
  }
  
}

/* Check orthogonality between vectors */
bool are_vectors_orthogonal(const mutation_vector_t *v1,
                           const mutation_vector_t *v2) {

  if (!v1 || !v2) { return false; }
  
  /* Dot product should be zero for orthogonal vectors */
  double dot_product = 0.0;
  for (u32 i = 0; i < LATTICE_DIMENSION; ++i) {
    
    dot_product += (double)v1->dimension[i] * (double)v2->dimension[i];
    
  }
  
  /* Consider orthogonal if dot product is very small */
  return fabs(dot_product) < 1e-6;
  
}

/* Initialize MAB selector */
void mab_init(mab_selector_t *mab, u32 strategy_type) {

  if (!mab) { return; }
  
  memset(mab, 0, sizeof(mab_selector_t));
  mab->strategy_type = strategy_type;
  mab->arm_count = LATTICE_DIMENSION;
  mab->exploration_rate = MAB_ALPHA;
  
  /* Initialize all arms */
  for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
    
    mab->arms[i].mutation_type = i;
    mab->arms[i].pull_count = 0;
    mab->arms[i].total_reward = 0;
    mab->arms[i].avg_reward = 0.0;
    mab->arms[i].ucb_value = 1e10;  /* High initial value for exploration */
    mab->arms[i].epsilon_prob = 1.0 / mab->arm_count;  /* Uniform initial */
    
  }
  
  /* Initialize reward tracking */
  for (u32 i = 0; i < MAB_WINDOW_SIZE; ++i) {
    
    mab->recent_rewards[i] = 0.0;
    
  }
  mab->reward_index = 0;
  mab->avg_recent_reward = 0.0;
  
}

/* Update MAB arm reward */
void mab_update_reward(mab_selector_t *mab, u32 arm_index, double reward) {

  if (!mab || arm_index >= mab->arm_count) { return; }
  
  mab_arm_t *arm = &mab->arms[arm_index];
  
  /* Update statistics */
  arm->pull_count++;
  arm->total_reward += (u64)(reward * 1000);  /* Scale for integer storage */
  arm->avg_reward = (double)arm->total_reward / (arm->pull_count * 1000.0);
  
  /* Update UCB value with reduced exploration for efficiency */
  if (arm->pull_count > 0 && mab->total_pulls > 0) {
    
    /* Reduced exploration term to favor exploitation */
    double exploration = MAB_ALPHA * 
                        sqrt(log((double)mab->total_pulls) / 
                             (double)arm->pull_count);
    
    /* Additional efficiency factor: reduce UCB for arms with high pull count but low reward */
    double efficiency_factor = 1.0;
    if (arm->pull_count > 50 && arm->avg_reward < 0.01) {
      
      /* Penalize arms that have been tried many times with little reward */
      efficiency_factor = 0.8;  /* Reduce UCB by 20% */
      
    }
    
    arm->ucb_value = (arm->avg_reward + exploration) * efficiency_factor;
    
  }
  
  /* Update recent rewards window */
  mab->recent_rewards[mab->reward_index] = reward;
  mab->reward_index = (mab->reward_index + 1) % MAB_WINDOW_SIZE;
  
  /* Update average recent reward */
  double sum = 0.0;
  u32 count = 0;
  for (u32 i = 0; i < MAB_WINDOW_SIZE; ++i) {
    
    if (mab->recent_rewards[i] > 0.0) {
      
      sum += mab->recent_rewards[i];
      count++;
      
    }
    
  }
  if (count > 0) {
    
    mab->avg_recent_reward = sum / count;
    
  }
  
}

/* Select mutation using MAB */
u32 mab_select_mutation(mab_selector_t *mab, afl_state_t *afl) {

  if (!mab || !afl) { return MUT_FLIPBIT; }
  
  mab->total_pulls++;
  
  u32 selected_arm = 0;
  
  switch (mab->strategy_type) {
    
    case 0: {  /* UCB (Upper Confidence Bound) */
      
      double max_ucb = -1e10;
      for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
        
        /* Update UCB before selection */
        if (mab->arms[i].pull_count > 0) {
          
          double exploration = MAB_ALPHA * 
                              sqrt(log((double)mab->total_pulls) / 
                                   (double)mab->arms[i].pull_count);
          mab->arms[i].ucb_value = mab->arms[i].avg_reward + exploration;
          
        } else {
          
          /* Unexplored arms get high UCB */
          mab->arms[i].ucb_value = 1e10;
          
        }
        
        if (mab->arms[i].ucb_value > max_ucb) {
          
          max_ucb = mab->arms[i].ucb_value;
          selected_arm = i;
          
        }
        
      }
      break;
      
    }
    
    case 1: {  /* Epsilon-Greedy */
      
      double rand_val = (double)rand_below(afl, 10000) / 10000.0;
      
      if (rand_val < MAB_EPSILON) {
        
        /* Explore: random selection */
        selected_arm = rand_below(afl, mab->arm_count);
        if (selected_arm >= MUT_MAX) { selected_arm = MUT_MAX - 1; }
        
      } else {
        
        /* Exploit: select best arm */
        double max_reward = -1e10;
        for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
          
          if (mab->arms[i].avg_reward > max_reward) {
            
            max_reward = mab->arms[i].avg_reward;
            selected_arm = i;
            
          }
          
        }
        
      }
      break;
      
    }
    
    default: {  /* Default to UCB */
      
      double max_ucb = -1e10;
      for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
        
        if (mab->arms[i].ucb_value > max_ucb) {
          
          max_ucb = mab->arms[i].ucb_value;
          selected_arm = i;
          
        }
        
      }
      break;
      
    }
    
  }
  
  /* Ensure valid mutation type */
  if (selected_arm >= MUT_MAX) { selected_arm = MUT_FLIPBIT; }
  
  return selected_arm;
  
}

/* Calculate reward for a mutation based on coverage gain and efficiency */
double calculate_mutation_reward(afl_state_t *afl, u32 mutation_type,
                                u32 new_edges, u32 new_paths) {

  (void)mutation_type;  /* Suppress unused parameter warning */
  if (!afl) { return 0.0; }
  
  /* Base reward from new coverage */
  double reward = (double)new_edges * 10.0 + (double)new_paths * 5.0;
  
  /* Bonus for finding crashes */
  if (afl->saved_crashes > 0) {
    
    reward += 100.0;
    
  }
  
  /* Efficiency-based reward: prefer mutations that achieve coverage with fewer executions */
  /* Track execution efficiency: reward = coverage / (executions + 1) */
  /* This encourages strategies that find coverage faster */
  if (afl->lattice_mab_ctx && mutation_type < LATTICE_DIMENSION) {
    
    mutation_vector_t *vec = &afl->lattice_mab_ctx->lattice.vectors[mutation_type];
    if (vec->usage_count > 0) {
      
      /* Efficiency metric: coverage per execution */
      double efficiency = (double)(new_edges + new_paths) / (double)(vec->usage_count + 1);
      
      /* Add efficiency bonus (scaled) */
      reward += efficiency * 5.0;
      
      /* Penalty for over-exploration: if usage count is very high but coverage is low */
      if (vec->usage_count > 100 && (new_edges + new_paths) == 0) {
        
        /* Small penalty for inefficient mutations */
        reward -= EFFICIENCY_PENALTY_FACTOR * (double)vec->usage_count / 1000.0;
        
      }
      
    }
    
  }
  
  /* Normalize reward */
  reward = reward / 1000.0;
  
  /* Ensure reward is non-negative (but can be small) */
  if (reward < 0.0) { reward = 0.0; }
  
  return reward;
  
}

/* Select mutation using lattice properties + MAB */
u32 lattice_mab_select_mutation(lattice_mab_context_t *ctx, afl_state_t *afl,
                               u32 input_mode, u32 fuzz_mode) {

  (void)input_mode;  /* Suppress unused parameter warning */
  (void)fuzz_mode;   /* Suppress unused parameter warning */
  if (!ctx || !afl || !ctx->enabled) {
    
    /* Fallback to original strategy */
    if (ctx) { ctx->original_selections++; }
    return 0;  /* Will trigger original selection */
    
  }
  
  ctx->lattice_selections++;
  ctx->total_mutations++;
  
  /* Use MAB to select base mutation */
  u32 base_mutation = mab_select_mutation(&ctx->mab, afl);
  
  /* Apply lattice-based refinement */
  mutation_vector_t *selected_vec = &ctx->lattice.vectors[base_mutation];
  
  /* Find nearest neighbors */
  u32 neighbors[LATTICE_NEIGHBOR_RADIUS];
  u32 neighbor_count = 0;
  find_lattice_neighbors(&ctx->lattice, selected_vec, neighbors,
                        LATTICE_NEIGHBOR_RADIUS, &neighbor_count);
  
  /* Consider neighbors if they have better rewards */
  u32 best_mutation = base_mutation;
  double best_reward = ctx->mab.arms[base_mutation].avg_reward;
  
  for (u32 i = 0; i < neighbor_count; ++i) {
    
    u32 neighbor_idx = neighbors[i];
    if (neighbor_idx < ctx->mab.arm_count) {
      
      double neighbor_reward = ctx->mab.arms[neighbor_idx].avg_reward;
      
      /* Prefer neighbors with significantly better rewards (more conservative) */
      if (neighbor_reward > best_reward * 1.1) {  /* Only if 10% better */
        
        /* Reduced exploration probability for efficiency */
        if (rand_below(afl, 100) < NEIGHBOR_EXPLORE_PROB) {  /* Reduced from 30% to 10% */
          
          best_mutation = neighbor_idx;
          best_reward = neighbor_reward;
          
        }
        
      }
      
    }
    
  }
  
  /* Ensure valid mutation type */
  if (best_mutation >= MUT_MAX) { best_mutation = MUT_FLIPBIT; }
  
  return best_mutation;
  
}

/* Update lattice-MAB with feedback */
void lattice_mab_update(lattice_mab_context_t *ctx, u32 mutation_type,
                       double reward) {

  if (!ctx || mutation_type >= LATTICE_DIMENSION) { return; }
  
  /* Update MAB arm */
  mab_update_reward(&ctx->mab, mutation_type, reward);
  
  /* Update lattice vector statistics */
  if (mutation_type < ctx->lattice.vector_count) {
    
    mutation_vector_t *vec = &ctx->lattice.vectors[mutation_type];
    vec->usage_count++;
    vec->total_reward += (u64)(reward * 1000);
    vec->avg_reward = (double)vec->total_reward / (vec->usage_count * 1000.0);
    
  }
  
}

/* Initialize lattice-MAB system */
void lattice_mab_init(lattice_mab_context_t *ctx, afl_state_t *afl) {

  if (!ctx || !afl) { return; }
  
  memset(ctx, 0, sizeof(lattice_mab_context_t));
  
  ctx->enabled = true;
  ctx->use_original_fallback = true;
  
  /* Initialize lattice */
  init_mutation_lattice(&ctx->lattice);
  
  /* Initialize MAB (use UCB strategy) */
  mab_init(&ctx->mab, 0);
  
}

/* Cleanup lattice-MAB system */
void lattice_mab_cleanup(lattice_mab_context_t *ctx) {

  if (!ctx) { return; }
  
  if (ctx->lattice.neighbor_matrix) {
    
    free(ctx->lattice.neighbor_matrix);
    ctx->lattice.neighbor_matrix = NULL;
    
  }
  
  memset(ctx, 0, sizeof(lattice_mab_context_t));
  
}

/* Get statistics */
void lattice_mab_get_stats(const lattice_mab_context_t *ctx,
                          u64 *lattice_sel, u64 *original_sel,
                          double *avg_reward) {

  if (!ctx) { return; }
  
  if (lattice_sel) { *lattice_sel = ctx->lattice_selections; }
  if (original_sel) { *original_sel = ctx->original_selections; }
  if (avg_reward) { *avg_reward = ctx->mab.avg_recent_reward; }
  
}

