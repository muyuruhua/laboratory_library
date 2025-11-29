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
#define MAB_ALPHA 0.004        /* UCB exploration parameter (increased for better exploration) */
#define MAB_EPSILON 0.02      /* Epsilon-greedy parameter (increased for better exploration) */
#define LATTICE_NEIGHBOR_RADIUS 3  /* Radius for nearest neighbor search */
#define MAB_WINDOW_SIZE 1000  /* Window size for reward tracking */
#define EFFICIENCY_PENALTY_FACTOR 0.5  /* Reduced penalty to avoid premature abandonment */
#define NEIGHBOR_EXPLORE_PROB 2  /* Increased probability of exploring neighbors */
#define EFFICIENCY_THRESHOLD 30  /* Increased threshold to allow more exploration */
#define MIN_EFFICIENCY_RATIO 0.0001  /* Reduced minimum ratio to be less restrictive */
#define EFFICIENCY_REWARD_WEIGHT 30.0  /* Balanced weight for efficiency reward */
#define COVERAGE_REWARD_WEIGHT 15.0  /* Increased weight for direct coverage gain */

/* Semantic-aware and Grammar-aware configuration */
#define SEMANTIC_AWARE_ENABLED 1  /* Enable semantic-aware mutation selection */
#define GRAMMAR_AWARE_ENABLED 1   /* Enable grammar-aware mutation selection */
#define SEMANTIC_PRECISION_BOOST 1.5  /* Boost factor for precision-generating mutations */
#define GRAMMAR_MATCH_BOOST 1.3  /* Boost factor for grammar-matching mutations */
#define ARITHMETIC_MUTATION_WEIGHT 1.8  /* Weight for arithmetic mutations (for precise values) */
#define ASCII_MUTATION_WEIGHT 1.5  /* Weight for ASCII-related mutations (for text inputs) */

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

/* Semantic-aware context: tracks conditions and precision requirements */
typedef struct {
  
  u32 condition_detected;     /* Flag: condition branch detected (e.g., X==1) */
  u32 precision_required;      /* Flag: precise value generation needed */
  u32 arithmetic_preference;  /* Preference for arithmetic mutations */
  u64 condition_count;        /* Number of condition branches encountered */
  
} semantic_context_t;

/* Grammar-aware context: tracks input format and structure */
typedef struct {
  
  u32 input_mode;             /* 0=default, 1=text, 2=binary */
  u32 text_preference;        /* Preference for text-related mutations */
  u32 binary_preference;      /* Preference for binary-related mutations */
  u32 ascii_detected;         /* Flag: ASCII input detected */
  u32 structure_detected;     /* Flag: structured input detected */
  
} grammar_context_t;

/* Lattice-MAB Context */
struct lattice_mab_context {

  mutation_lattice_t lattice;
  mab_selector_t mab;
  bool enabled;               /* Whether to use lattice-MAB strategy */
  bool use_original_fallback; /* Fallback to original strategy if needed */
  
  /* Semantic and Grammar awareness */
  semantic_context_t semantic_ctx;
  grammar_context_t grammar_ctx;
  
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

/* Semantic-aware functions */
void semantic_context_init(semantic_context_t *ctx);
void semantic_context_update(semantic_context_t *ctx, afl_state_t *afl);
double get_semantic_boost(u32 mutation_type, const semantic_context_t *ctx);

/* Grammar-aware functions */
void grammar_context_init(grammar_context_t *ctx, u32 input_mode);
void grammar_context_update(grammar_context_t *ctx, afl_state_t *afl, u32 input_mode);
double get_grammar_boost(u32 mutation_type, const grammar_context_t *ctx, u32 input_mode);

/* Check if mutation is suitable for semantic precision */
bool is_precision_mutation(u32 mutation_type);

/* Check if mutation is suitable for grammar/format */
bool is_grammar_mutation(u32 mutation_type, u32 input_mode);

#endif /* AFL_LATTICE_MAB_H */

