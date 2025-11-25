/*
   AFL++ Lattice-based Multi-Armed Bandit Mutation Strategy
   --------------------------------------------------------
   
   This module implements an advanced mutation operation selection strategy
   using Lattice Theory and Multi-Armed Bandit (MAB) algorithms.
   
   Key concepts:
   - Mutation Vectors: Each mutation operation is represented as a structured
     vector in high-dimensional discrete space
   - Lattice Theory: Strategy space is modeled as a discrete lattice, utilizing
     geometric properties (vector distribution, orthogonality, nearest neighbors)
   - MAB Optimization: Multi-armed bandit algorithms for intelligent strategy
     selection based on historical performance
*/

#ifndef AFL_LATTICE_MAB_H
#define AFL_LATTICE_MAB_H

#include "types.h"
#include <stdbool.h>
#include <math.h>
#include <stdint.h>

/* Forward declaration - will be properly defined in afl-fuzz.h */
struct afl_state;
typedef struct afl_state afl_state_t;

/* Configuration */
/* Note: MUT_MAX is defined in afl-mutations.h, but we avoid including it here
   to prevent circular dependencies. The value 37 matches MUT_MAX. */
#define LATTICE_DIMENSION 37  /* Number of mutation types (MUT_MAX) */
#define MAB_ALPHA 0.004       /* UCB exploration parameter (reduced to favor exploitation) */
#define MAB_EPSILON 0.02      /* Epsilon-greedy parameter (reduced to favor exploitation) */
#define LATTICE_NEIGHBOR_RADIUS 3  /* Radius for nearest neighbor search */
#define MAB_WINDOW_SIZE 1000  /* Window size for reward tracking */
#define EFFICIENCY_PENALTY_FACTOR 0.5  /* Increased penalty to abandon inefficient mutations earlier */
#define NEIGHBOR_EXPLORE_PROB 2  /* Reduced probability of exploring neighbors to save executions */
#define EFFICIENCY_THRESHOLD 30  /* Reduced threshold to filter inefficient mutations earlier */
#define MIN_EFFICIENCY_RATIO 0.0001  /* Increased minimum ratio to be more restrictive */
#define EFFICIENCY_REWARD_WEIGHT 30.0  /* Increased weight for efficiency reward */
#define COVERAGE_REWARD_WEIGHT 15.0  /* Increased weight for direct coverage gain */

/* Mutation Vector Structure */
typedef struct {

  u32 mutation_type;          /* MUT_* enum value */
  u32 dimension[LATTICE_DIMENSION];  /* Vector representation */
  u32 position[LATTICE_DIMENSION];    /* Position in lattice space */
  double magnitude;           /* Vector magnitude */
  u32 usage_count;            /* How many times this mutation was used */
  u64 total_reward;           /* Cumulative reward */
  double avg_reward;          /* Average reward */
  double ucb_value;           /* Upper Confidence Bound value */
  
} mutation_vector_t;

/* Lattice Structure */
typedef struct {

  mutation_vector_t vectors[LATTICE_DIMENSION];
  u32 vector_count;
  double lattice_density;    /* Density of vectors in space */
  u32 *neighbor_matrix;      /* Adjacency matrix for nearest neighbors */
  
} mutation_lattice_t;

/* MAB Arm (represents a mutation strategy) */
typedef struct {

  u32 mutation_type;
  u64 pull_count;            /* Number of times this arm was pulled */
  u64 total_reward;           /* Total reward accumulated */
  double avg_reward;          /* Average reward */
  double ucb_value;           /* Upper Confidence Bound */
  double epsilon_prob;        /* Probability for epsilon-greedy */
  u32 heap_index;             /* Index in heap array (for O(log n) updates) */
  
} mab_arm_t;

/* Heap node for priority queue optimization */
typedef struct {

  u32 arm_index;              /* Index to mab_arm_t in arms array */
  double ucb_value;            /* UCB value (heap key) */
  
} heap_node_t;

/* Priority queue (max heap) for efficient UCB selection */
typedef struct {

  heap_node_t nodes[LATTICE_DIMENSION];  /* Heap array */
  u32 size;                    /* Current heap size */
  u32 capacity;                /* Maximum capacity */
  
} ucb_heap_t;

/* MAB Strategy Selector */
typedef struct {

  mab_arm_t arms[LATTICE_DIMENSION];
  u32 arm_count;
  u64 total_pulls;            /* Total number of arm pulls */
  u32 strategy_type;          /* 0=UCB, 1=Epsilon-Greedy, 2=Thompson Sampling */
  double exploration_rate;    /* Current exploration rate */
  
  /* Priority queue for O(log n) selection */
  ucb_heap_t ucb_heap;
  bool use_heap;              /* Whether to use heap optimization */
  
  /* Reward tracking */
  double recent_rewards[MAB_WINDOW_SIZE];
  u32 reward_index;
  double avg_recent_reward;
  
} mab_selector_t;

/* Lattice-MAB Context */
struct lattice_mab_context {

  mutation_lattice_t lattice;
  mab_selector_t mab;
  bool enabled;               /* Whether to use lattice-MAB strategy */
  bool use_original_fallback; /* Fallback to original strategy if needed */
  
  /* Statistics */
  u64 lattice_selections;
  u64 original_selections;
  u64 total_mutations;
  
};

/* Only define typedef if not already defined in afl-fuzz.h */
#ifndef LATTICE_MAB_CONTEXT_TYPEDEF_DEFINED
#define LATTICE_MAB_CONTEXT_TYPEDEF_DEFINED
typedef struct lattice_mab_context lattice_mab_context_t;
#endif

/* Function declarations */

/* Initialize lattice-MAB system */
void lattice_mab_init(lattice_mab_context_t *ctx, afl_state_t *afl);

/* Cleanup lattice-MAB system */
void lattice_mab_cleanup(lattice_mab_context_t *ctx);

/* Create mutation vector from mutation type */
mutation_vector_t create_mutation_vector(u32 mutation_type);

/* Initialize mutation lattice */
void init_mutation_lattice(mutation_lattice_t *lattice);

/* Calculate distance between two vectors in lattice space */
double lattice_distance(const mutation_vector_t *v1, 
                        const mutation_vector_t *v2);

/* Find nearest neighbors in lattice */
void find_lattice_neighbors(const mutation_lattice_t *lattice,
                            const mutation_vector_t *vector,
                            u32 *neighbors, u32 max_neighbors, u32 *found);

/* Check orthogonality between vectors */
bool are_vectors_orthogonal(const mutation_vector_t *v1,
                           const mutation_vector_t *v2);

/* Initialize MAB selector */
void mab_init(mab_selector_t *mab, u32 strategy_type);

/* Initialize UCB heap */
void ucb_heap_init(ucb_heap_t *heap);

/* Heap operations for priority queue */
void ucb_heap_insert(ucb_heap_t *heap, u32 arm_index, double ucb_value);
void ucb_heap_update(ucb_heap_t *heap, u32 heap_index, double new_ucb_value, mab_arm_t *arms);
u32 ucb_heap_peek_max(ucb_heap_t *heap);
void ucb_heap_rebuild(mab_selector_t *mab);

/* Update MAB arm reward */
void mab_update_reward(mab_selector_t *mab, u32 arm_index, double reward);

/* Select mutation using MAB */
u32 mab_select_mutation(mab_selector_t *mab, afl_state_t *afl);

/* Select mutation using lattice properties + MAB */
u32 lattice_mab_select_mutation(lattice_mab_context_t *ctx, afl_state_t *afl,
                               u32 input_mode, u32 fuzz_mode);

/* Calculate reward for a mutation based on coverage gain */
double calculate_mutation_reward(afl_state_t *afl, u32 mutation_type,
                                u32 new_edges, u32 new_paths);

/* Update lattice-MAB with feedback */
void lattice_mab_update(lattice_mab_context_t *ctx, u32 mutation_type,
                       double reward);

/* Get statistics */
void lattice_mab_get_stats(const lattice_mab_context_t *ctx,
                          u64 *lattice_sel, u64 *original_sel,
                          double *avg_reward);

#endif /* AFL_LATTICE_MAB_H */

