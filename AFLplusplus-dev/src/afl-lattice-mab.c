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
#include <math.h>

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
  if (lattice->neighbor_matrix) {
    
    for (u32 i = 0; i < LATTICE_DIMENSION; ++i) {
      
      for (u32 j = 0; j < LATTICE_DIMENSION; ++j) {
        
        if (lattice->neighbor_matrix[i * LATTICE_DIMENSION + j]) {
          
          neighbor_count++;
          
        }
        
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
  if (!lattice->neighbor_matrix) { return; }
  
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

/* Initialize UCB heap */
void ucb_heap_init(ucb_heap_t *heap) {

  if (!heap) { return; }
  
  memset(heap, 0, sizeof(ucb_heap_t));
  heap->capacity = LATTICE_DIMENSION;
  heap->size = 0;
  
}

/* Heap helper functions */
static void heap_swap(heap_node_t *a, heap_node_t *b) {

  heap_node_t temp = *a;
  *a = *b;
  *b = temp;
  
}

/* Heapify up (for insert and increase key) */
static void heapify_up(ucb_heap_t *heap, u32 index, mab_arm_t *arms) {

  if (index == 0) { return; }
  
  u32 parent = (index - 1) / 2;
  
  if (heap->nodes[parent].ucb_value < heap->nodes[index].ucb_value) {
    
    heap_swap(&heap->nodes[parent], &heap->nodes[index]);
    
    /* Update heap_index in arm */
    u32 parent_arm = heap->nodes[parent].arm_index;
    u32 index_arm = heap->nodes[index].arm_index;
    arms[parent_arm].heap_index = parent;
    arms[index_arm].heap_index = index;
    
    heapify_up(heap, parent, arms);
    
  }
  
}

/* Heapify down (for delete and decrease key) */
static void heapify_down(ucb_heap_t *heap, u32 index, mab_arm_t *arms) {

  u32 left = 2 * index + 1;
  u32 right = 2 * index + 2;
  u32 largest = index;
  
  if (left < heap->size && 
      heap->nodes[left].ucb_value > heap->nodes[largest].ucb_value) {
    
    largest = left;
    
  }
  
  if (right < heap->size && 
      heap->nodes[right].ucb_value > heap->nodes[largest].ucb_value) {
    
    largest = right;
    
  }
  
  if (largest != index) {
    
    heap_swap(&heap->nodes[index], &heap->nodes[largest]);
    
    /* Update heap_index in arm */
    u32 index_arm = heap->nodes[index].arm_index;
    u32 largest_arm = heap->nodes[largest].arm_index;
    arms[index_arm].heap_index = index;
    arms[largest_arm].heap_index = largest;
    
    heapify_down(heap, largest, arms);
    
  }
  
}

/* Insert node into heap */
void ucb_heap_insert(ucb_heap_t *heap, u32 arm_index, double ucb_value) {

  if (!heap || heap->size >= heap->capacity) { return; }
  
  u32 index = heap->size++;
  heap->nodes[index].arm_index = arm_index;
  heap->nodes[index].ucb_value = ucb_value;
  
  /* Note: heap_index will be set by heapify_up or caller */
  
}

/* Update UCB value in heap */
void ucb_heap_update(ucb_heap_t *heap, u32 heap_index, double new_ucb_value, 
                     mab_arm_t *arms) {

  if (!heap || heap_index >= heap->size) { return; }
  
  double old_value = heap->nodes[heap_index].ucb_value;
  heap->nodes[heap_index].ucb_value = new_ucb_value;
  
  if (new_ucb_value > old_value) {
    
    /* Value increased, move up */
    heapify_up(heap, heap_index, arms);
    
  } else if (new_ucb_value < old_value) {
    
    /* Value decreased, move down */
    heapify_down(heap, heap_index, arms);
    
  }
  
}

/* Get maximum UCB value (peek at root) */
u32 ucb_heap_peek_max(ucb_heap_t *heap) {

  if (!heap || heap->size == 0) { return 0; }
  
  return heap->nodes[0].arm_index;
  
}

/* Rebuild heap from scratch (used when UCB values change significantly) */
void ucb_heap_rebuild(mab_selector_t *mab) {

  if (!mab) { return; }
  
  ucb_heap_t *heap = &mab->ucb_heap;
  heap->size = 0;
  
  /* Insert all arms into heap */
  for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
    
    u32 index = heap->size++;
    heap->nodes[index].arm_index = i;
    heap->nodes[index].ucb_value = mab->arms[i].ucb_value;
    mab->arms[i].heap_index = index;
    
  }
  
  /* Build heap by heapifying from bottom up */
  for (u32 i = (heap->size - 1) / 2; i > 0; --i) {
    
    heapify_down(heap, i - 1, mab->arms);
    
  }
  
  /* Final heapify at root */
  if (heap->size > 0) {
    
    heapify_down(heap, 0, mab->arms);
    
  }
  
}

/* Initialize MAB selector */
void mab_init(mab_selector_t *mab, u32 strategy_type) {

  if (!mab) { return; }
  
  memset(mab, 0, sizeof(mab_selector_t));
  mab->strategy_type = strategy_type;
  mab->arm_count = LATTICE_DIMENSION;
  mab->exploration_rate = MAB_ALPHA;
  mab->use_heap = true;  /* Enable heap optimization by default */
  
  /* Initialize heap */
  ucb_heap_init(&mab->ucb_heap);
  
  /* Initialize all arms */
  for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
    
    mab->arms[i].mutation_type = i;
    mab->arms[i].pull_count = 0;
    mab->arms[i].total_reward = 0;
    mab->arms[i].avg_reward = 0.0;
    mab->arms[i].ucb_value = 1e10;  /* High initial value for exploration */
    mab->arms[i].epsilon_prob = 1.0 / mab->arm_count;  /* Uniform initial */
    mab->arms[i].heap_index = LATTICE_DIMENSION;  /* Invalid index initially */
    
  }
  
  /* Initialize reward tracking */
  for (u32 i = 0; i < MAB_WINDOW_SIZE; ++i) {
    
    mab->recent_rewards[i] = 0.0;
    
  }
  mab->reward_index = 0;
  mab->avg_recent_reward = 0.0;
  
  /* Build initial heap */
  ucb_heap_rebuild(mab);
  
}

/* Update MAB arm reward */
void mab_update_reward(mab_selector_t *mab, u32 arm_index, double reward) {

  if (!mab || arm_index >= mab->arm_count) { return; }
  
  mab_arm_t *arm = &mab->arms[arm_index];
  
  /* Update statistics */
  arm->pull_count++;
  arm->total_reward += (u64)(reward * 1000);  /* Scale for integer storage */
  arm->avg_reward = (double)arm->total_reward / (arm->pull_count * 1000.0);
  
  /* Calculate new UCB value */
  double old_ucb = arm->ucb_value;
  double new_ucb = old_ucb;
  
  /* Update UCB value with aggressive efficiency-based weighting */
  if (arm->pull_count > 0 && mab->total_pulls > 0) {
    
    /* Reduced exploration term to favor exploitation */
    double exploration = MAB_ALPHA * 
                        sqrt(log((double)mab->total_pulls) / 
                             (double)arm->pull_count);
    
    /* Efficiency factor: reduce UCB for arms with high pull count but low reward */
    double efficiency_factor = 1.0;
    if (arm->pull_count > EFFICIENCY_THRESHOLD) {
      
      /* Calculate efficiency: reward per pull */
      double efficiency = arm->avg_reward / (double)arm->pull_count;
      
      /* More aggressive progressive penalty based on efficiency */
      if (efficiency < MIN_EFFICIENCY_RATIO) {
        
        /* Very strong penalty for very inefficient arms */
        efficiency_factor = 0.1;  /* Reduce UCB by 90% */
        
      } else if (efficiency < MIN_EFFICIENCY_RATIO * 2) {
        
        /* Strong penalty for moderately inefficient arms */
        efficiency_factor = 0.3;  /* Reduce UCB by 70% */
        
      } else if (efficiency < MIN_EFFICIENCY_RATIO * 3) {
        
        /* Moderate penalty for slightly inefficient arms */
        efficiency_factor = 0.5;  /* Reduce UCB by 50% */
        
      } else if (efficiency < MIN_EFFICIENCY_RATIO * 5) {
        
        /* Small penalty for marginally inefficient arms */
        efficiency_factor = 0.7;  /* Reduce UCB by 30% */
        
      }
      
    }
    
    new_ucb = (arm->avg_reward + exploration) * efficiency_factor;
    arm->ucb_value = new_ucb;
    
  }
  
  /* Update heap if UCB value changed and heap is enabled */
  if (mab->use_heap && arm->heap_index < mab->ucb_heap.size && 
      fabs(new_ucb - old_ucb) > 1e-9) {
    
    ucb_heap_update(&mab->ucb_heap, arm->heap_index, new_ucb, mab->arms);
    
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
    
    case 0: {  /* UCB (Upper Confidence Bound) with efficiency weighting and direct filtering */
      
      if (mab->use_heap) {
        
        /* Optimized path: use heap for O(log n) selection */
        /* First, update all UCB values (total_pulls changed) */
        for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
          
          /* Direct efficiency filtering: skip arms with very low efficiency */
          if (mab->arms[i].pull_count > EFFICIENCY_THRESHOLD) {
            
            double efficiency = mab->arms[i].avg_reward / (double)mab->arms[i].pull_count;
            
            /* Skip arms with efficiency below minimum threshold */
            if (efficiency < MIN_EFFICIENCY_RATIO) {
              
              /* Set UCB to very low value so it won't be selected */
              if (mab->arms[i].heap_index < mab->ucb_heap.size) {
                
                ucb_heap_update(&mab->ucb_heap, mab->arms[i].heap_index, -1e10, mab->arms);
                
              }
              continue;
              
            }
            
          }
          
          /* Calculate new UCB value */
          double new_ucb = 0.0;
          if (mab->arms[i].pull_count > 0) {
            
            /* Reduced exploration term to favor exploitation */
            double exploration = MAB_ALPHA * 
                                sqrt(log((double)mab->total_pulls) / 
                                     (double)mab->arms[i].pull_count);
            
            /* Efficiency factor: penalize arms with high pull count but low reward */
            double efficiency_factor = 1.0;
            if (mab->arms[i].pull_count > EFFICIENCY_THRESHOLD) {
              
              /* Calculate efficiency: reward per pull */
              double efficiency = mab->arms[i].avg_reward / (double)mab->arms[i].pull_count;
              
              /* More aggressive progressive penalty based on efficiency */
              if (efficiency < MIN_EFFICIENCY_RATIO) {
                
                efficiency_factor = 0.1;  /* Reduce UCB by 90% for very inefficient arms */
                
              } else if (efficiency < MIN_EFFICIENCY_RATIO * 2) {
                
                efficiency_factor = 0.3;  /* Reduce UCB by 70% for moderately inefficient arms */
                
              } else if (efficiency < MIN_EFFICIENCY_RATIO * 3) {
                
                efficiency_factor = 0.5;  /* Reduce UCB by 50% for slightly inefficient arms */
                
              } else if (efficiency < MIN_EFFICIENCY_RATIO * 5) {
                
                efficiency_factor = 0.7;  /* Reduce UCB by 30% for marginally inefficient arms */
                
              }
              
            }
            
            new_ucb = (mab->arms[i].avg_reward + exploration) * efficiency_factor;
            
          } else {
            
            /* Unexplored arms get moderate UCB to encourage exploration */
            new_ucb = 0.5;  /* Reduced to favor exploitation over exploration */
            
          }
          
          /* Update UCB value and heap */
          mab->arms[i].ucb_value = new_ucb;
          if (mab->arms[i].heap_index < mab->ucb_heap.size) {
            
            ucb_heap_update(&mab->ucb_heap, mab->arms[i].heap_index, new_ucb, mab->arms);
            
          }
          
        }
        
        /* Get maximum UCB from heap - O(1) */
        selected_arm = ucb_heap_peek_max(&mab->ucb_heap);
        
      } else {
        
        /* Fallback: original O(n) linear scan */
        double max_ucb = -1e10;
        for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
          
          /* Direct efficiency filtering: skip arms with very low efficiency */
          if (mab->arms[i].pull_count > EFFICIENCY_THRESHOLD) {
            
            double efficiency = mab->arms[i].avg_reward / (double)mab->arms[i].pull_count;
            
            /* Skip arms with efficiency below minimum threshold */
            if (efficiency < MIN_EFFICIENCY_RATIO) {
              
              continue;  /* Skip this arm completely */
              
            }
            
          }
          
          /* Update UCB before selection */
          if (mab->arms[i].pull_count > 0) {
            
            /* Reduced exploration term to favor exploitation */
            double exploration = MAB_ALPHA * 
                                sqrt(log((double)mab->total_pulls) / 
                                     (double)mab->arms[i].pull_count);
            
            /* Efficiency factor: penalize arms with high pull count but low reward */
            double efficiency_factor = 1.0;
            if (mab->arms[i].pull_count > EFFICIENCY_THRESHOLD) {
              
              /* Calculate efficiency: reward per pull */
              double efficiency = mab->arms[i].avg_reward / (double)mab->arms[i].pull_count;
              
            /* More aggressive progressive penalty based on efficiency */
            if (efficiency < MIN_EFFICIENCY_RATIO) {
              
              efficiency_factor = 0.1;  /* Reduce UCB by 90% for very inefficient arms */
              
            } else if (efficiency < MIN_EFFICIENCY_RATIO * 2) {
              
              efficiency_factor = 0.3;  /* Reduce UCB by 70% for moderately inefficient arms */
              
            } else if (efficiency < MIN_EFFICIENCY_RATIO * 3) {
              
              efficiency_factor = 0.5;  /* Reduce UCB by 50% for slightly inefficient arms */
              
            } else if (efficiency < MIN_EFFICIENCY_RATIO * 5) {
              
              efficiency_factor = 0.7;  /* Reduce UCB by 30% for marginally inefficient arms */
              
            }
              
            }
            
            mab->arms[i].ucb_value = (mab->arms[i].avg_reward + exploration) * efficiency_factor;
            
          } else {
            
            /* Unexplored arms get moderate UCB to encourage exploration */
            mab->arms[i].ucb_value = 0.5;  /* Reduced to favor exploitation over exploration */
            
          }
          
          if (mab->arms[i].ucb_value > max_ucb) {
            
            max_ucb = mab->arms[i].ucb_value;
            selected_arm = i;
            
          }
          
        }
        
      }
      break;
      
    }
    
    case 1: {  /* Epsilon-Greedy with efficiency filtering */
      
      double rand_val = (double)rand_below(afl, 10000) / 10000.0;
      
      if (rand_val < MAB_EPSILON) {
        
        /* Explore: random selection from efficient arms only */
        u32 efficient_arms[MUT_MAX];
        u32 efficient_count = 0;
        
        for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
          
          /* Only consider efficient arms for exploration */
          if (mab->arms[i].pull_count == 0 || 
              mab->arms[i].pull_count <= EFFICIENCY_THRESHOLD ||
              (mab->arms[i].avg_reward / (double)mab->arms[i].pull_count) >= MIN_EFFICIENCY_RATIO) {
            
            efficient_arms[efficient_count++] = i;
            
          }
          
        }
        
        if (efficient_count > 0) {
          
          selected_arm = efficient_arms[rand_below(afl, efficient_count)];
          
        } else {
          
          /* Fallback: select best arm if no efficient arms */
          selected_arm = 0;
          
        }
        
        if (selected_arm >= MUT_MAX) { selected_arm = MUT_MAX - 1; }
        
      } else {
        
        /* Exploit: select best arm based on efficiency */
        double max_efficiency = -1e10;
        for (u32 i = 0; i < mab->arm_count && i < MUT_MAX; ++i) {
          
          /* Skip inefficient arms */
          if (mab->arms[i].pull_count > EFFICIENCY_THRESHOLD) {
            
            double efficiency = mab->arms[i].avg_reward / (double)mab->arms[i].pull_count;
            if (efficiency < MIN_EFFICIENCY_RATIO) {
              
              continue;  /* Skip inefficient arms */
              
            }
            
          }
          
          /* Use efficiency as selection criterion */
          double efficiency = 0.0;
          if (mab->arms[i].pull_count > 0) {
            
            efficiency = mab->arms[i].avg_reward / (double)mab->arms[i].pull_count;
            
          } else {
            
            efficiency = 0.1;  /* Small bonus for unexplored arms */
            
          }
          
          if (efficiency > max_efficiency) {
            
            max_efficiency = efficiency;
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
  
  /* Base reward from new coverage (increased weight to emphasize coverage discovery) */
  double reward = (double)new_edges * COVERAGE_REWARD_WEIGHT + (double)new_paths * (COVERAGE_REWARD_WEIGHT * 0.5);
  
  /* Bonus for finding crashes */
  if (afl->saved_crashes > 0) {
    
    reward += 100.0;
    
  }
  
  /* Efficiency-based reward: balance between coverage discovery and efficiency */
  if (afl->lattice_mab_ctx && mutation_type < LATTICE_DIMENSION) {
    
    mutation_vector_t *vec = &afl->lattice_mab_ctx->lattice.vectors[mutation_type];
    
    /* Calculate historical efficiency: total coverage per total usage */
    double historical_efficiency = 0.0;
    if (vec->usage_count > 0) {
      
      /* Use historical average reward as proxy for efficiency */
      historical_efficiency = vec->avg_reward;
      
      /* Efficiency bonus: reward mutations that have good historical performance */
      if (historical_efficiency > 0.0) {
        
        reward += historical_efficiency * EFFICIENCY_REWARD_WEIGHT;
        
      }
      
    } else {
      
      /* Moderate bonus for unexplored mutations to encourage exploration */
      reward += 0.5;  /* Reduced to favor exploitation over exploration */
      
    }
    
    /* Adaptive penalty: only penalize if mutation is clearly inefficient */
    if (vec->usage_count > EFFICIENCY_THRESHOLD) {
      
      /* Calculate current efficiency ratio */
      double current_efficiency = 0.0;
      if (vec->usage_count > 0) {
        
        current_efficiency = historical_efficiency;
        
      }
      
      /* Only apply penalty if efficiency is significantly below threshold */
      if (current_efficiency < MIN_EFFICIENCY_RATIO && (new_edges + new_paths) == 0) {
        
        /* Increased penalty to abandon inefficient mutations earlier */
        double penalty = EFFICIENCY_PENALTY_FACTOR * 
                        (double)vec->usage_count / 50.0;  /* Increased penalty */
        reward -= penalty;
        
      }
      
    }
    
    /* Increased penalty for zero coverage: if usage exceeds threshold */
    if (vec->usage_count > EFFICIENCY_THRESHOLD && (new_edges + new_paths) == 0) {
      
      /* More aggressive progressive penalty */
      double progressive_penalty = EFFICIENCY_PENALTY_FACTOR * 
                                  (double)vec->usage_count / 40.0;  /* Increased penalty */
      reward -= progressive_penalty;
      
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
    return MUT_MAX;  /* Return MUT_MAX to trigger original selection (not 0, since MUT_FLIPBIT=0) */
    
  }
  
  ctx->lattice_selections++;
  ctx->total_mutations++;
  
  /* Use MAB to select base mutation */
  u32 base_mutation = mab_select_mutation(&ctx->mab, afl);
  
  /* Apply lattice-based refinement (only if we have enough data) */
  mutation_vector_t *selected_vec = &ctx->lattice.vectors[base_mutation];
  
  /* Skip neighbor exploration if base mutation is efficient (to save computation) */
  u32 best_mutation = base_mutation;
  double best_reward = ctx->mab.arms[base_mutation].avg_reward;
  
  /* Explore neighbors less frequently to save executions */
  bool should_explore_neighbors = false;
  if (selected_vec->usage_count > 0) {
    
    double base_efficiency = best_reward / (double)(selected_vec->usage_count + 1);
    /* Only explore if base is very inefficient AND we haven't explored much */
    if (base_efficiency < MIN_EFFICIENCY_RATIO * 2 && selected_vec->usage_count < 10) {
      
      should_explore_neighbors = true;  /* Explore only when necessary */
      
    }
    
  } else {
    
    /* Only explore if we have very few total mutations to avoid wasting executions */
    if (ctx->total_mutations < 50) {
      should_explore_neighbors = true;  /* Limited exploration for unexplored base */
    }
    
  }
  
  if (should_explore_neighbors) {
    
    /* Find nearest neighbors */
    u32 neighbors[LATTICE_NEIGHBOR_RADIUS];
    u32 neighbor_count = 0;
    find_lattice_neighbors(&ctx->lattice, selected_vec, neighbors,
                          LATTICE_NEIGHBOR_RADIUS, &neighbor_count);
    
    /* Consider neighbors if they have better rewards AND efficiency */
    for (u32 i = 0; i < neighbor_count; ++i) {
      
      u32 neighbor_idx = neighbors[i];
      if (neighbor_idx < ctx->mab.arm_count) {
        
        double neighbor_reward = ctx->mab.arms[neighbor_idx].avg_reward;
        
        /* Calculate neighbor efficiency for comparison */
        mutation_vector_t *neighbor_vec = &ctx->lattice.vectors[neighbor_idx];
        double neighbor_efficiency = 0.0;
        if (neighbor_vec->usage_count > 0) {
          
          neighbor_efficiency = neighbor_reward / (double)(neighbor_vec->usage_count + 1);
          
        }
        
        double base_efficiency = 0.0;
        if (selected_vec->usage_count > 0) {
          
          base_efficiency = best_reward / (double)(selected_vec->usage_count + 1);
          
        }
        
        /* Prefer neighbors with significantly better rewards or efficiency */
        if (neighbor_reward > best_reward * 1.2 || 
            (neighbor_efficiency > base_efficiency * 1.1 && neighbor_reward >= best_reward * 1.1)) {
          
          /* Reduced exploration probability to save executions */
          if (rand_below(afl, 100) < NEIGHBOR_EXPLORE_PROB) {
            
            best_mutation = neighbor_idx;
            best_reward = neighbor_reward;
            
          }
          
        } else if (neighbor_reward > best_reward * 1.5 || 
                   (neighbor_efficiency > base_efficiency * 1.2 && neighbor_reward >= best_reward * 1.2)) {
          
          /* For very significant improvements, switch */
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

