#include <math.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../include/dsc.h"
#include "../include/hashmap.h"
#include "../include/posix_compat.h"

#define CSV_SUFFIX ".csv"
#define TSV_SUFFIX ".tsv"
#define PROPERTIES_SUFFIX ".properties"
#define MAX_CUSTOM_SEP_LEN 3
#define MAX_CUSTOM_SEPS 16

// Structure for tracking separator statistics during inference
typedef struct {
    int max_consecutive;      // Maximum consecutive count seen
    int max_nf;               // Maximum field count seen for this separator
    bool has_zero_variance;   // Whether this separator has zero variance
    double confidence;        // Confidence score for this separator
    bool has_sectional_override;  // Whether this separator has a sectional override
    double chunk_weight;      // Weight of the most significant chunk
} separator_stats;

static field_sep CommonFS[] = {
    {'s', SPACE, false, 0, 0, 0, 0},
    {'t', TAB, false, 0, 0, 0, 0},
    {'p', PIPE, false, 0, 0, 0, 0},
    {'m', SEMICOLON, false, 0, 0, 0, 0},
    {'o', COLON, false, 0, 0, 0, 0},
    {'c', COMMA, false, 0, 0, 0, 0},
    {'e', "=", false, 0, 0, 0, 0},
    {'w', SPACEMULTI, true, 0, 0, 0, 0},
    {'z', SPACEMULTI_, true, 0, 0, 0, 0},
    };

// Array to track statistics for each separator during inference
static separator_stats CommonSeparatorStats[ARRAY_SIZE(CommonFS)];

// Array to track custom separators
static field_sep CustomFS[MAX_CUSTOM_SEPS];
static separator_stats CustomSeparatorStats[MAX_CUSTOM_SEPS];
static int CUSTOM_SEP_COUNT = 0;

int COMMON_FS_COUNT = ARRAY_SIZE(CommonFS);
static field_sep ds_sep = {'@', DS_SEP, false, 0, 0, 0, 0};
static hashmap *QuoteREs = NULL;

// Structure to track potential custom separators
typedef struct {
    char sep[MAX_CUSTOM_SEP_LEN + 1];  // +1 for null terminator
    int field_count;
    bool is_valid;
} potential_sep;

static potential_sep PotentialSeps[MAX_CUSTOM_SEPS];
static int POTENTIAL_SEP_COUNT = 0;

// Helper function to get unique field counts from CommonFSNFConsecCounts
static int *get_unique_field_counts(int sep_index, int *count, int (*consec_counts)[MAX_NF_COUNT]) {
    if (IS_DEBUG) {
        DEBUG_PRINT("get_unique_field_counts - Starting for separator index %d", sep_index);
    }
    
    int *unique_counts = malloc(MAX_NF_COUNT * sizeof(int));
    if (!unique_counts) {
        FAIL("Failed to allocate memory for unique field counts");
    }
    
    *count = 0;
    int max_nf = CommonSeparatorStats[sep_index].max_nf;
    
    if (IS_DEBUG) {
        DEBUG_PRINT("get_unique_field_counts - max_nf: %d", max_nf);
    }
    
    // Only scan up to max_nf to save time
    for (int i = 2; i <= max_nf; i++) {
        if (consec_counts[sep_index][i] >= 2) {
            unique_counts[(*count)++] = i;
            if (IS_DEBUG) {
                DEBUG_PRINT("get_unique_field_counts - Added field count %d (consecutive: %d)", 
                           i, consec_counts[sep_index][i]);
            }
        }
    }
    
    if (IS_DEBUG) {
        DEBUG_PRINT("get_unique_field_counts - Found %d unique field counts", *count);
    }
    
    return unique_counts;
}

// Initialize stats for all common separators
static void init_common_separator_stats(void) {
    for (int i = 0; i < COMMON_FS_COUNT; i++) {
        CommonSeparatorStats[i].max_consecutive = 0;
        CommonSeparatorStats[i].max_nf = 0;
        CommonSeparatorStats[i].has_zero_variance = false;
        CommonSeparatorStats[i].confidence = 0.0;
        CommonSeparatorStats[i].has_sectional_override = false;
        CommonSeparatorStats[i].chunk_weight = 0.0;
    }
}

// Calculate confidence score for a separator
static double calculate_separator_confidence(int sep_index, int total_lines) {
    separator_stats *stat = &CommonSeparatorStats[sep_index];
    field_sep *fs = &CommonFS[sep_index];
    
    // Base confidence on variance and consecutive counts
    double variance_factor = 1.0 - (fs->var / (double)total_lines);
    double consecutive_factor = (double)stat->max_consecutive / total_lines;
    
    return (variance_factor + consecutive_factor) / 2.0;
}

static field_sep *get_common_fs(char key) {
    for (int i = 0; i < COMMON_FS_COUNT; i++) {
        if (CommonFS[i].key == key) {
            return &CommonFS[i];
        }
    }
    UNREACHABLE("No common field sep found for this key.");
}

static void set_static_maps() {
    if (!QuoteREs) {
        QuoteREs = hashmap_create(16);
    }
}

static char *get_quoted_fields_re(field_sep fs, char *q) {
    // TODO: CRLF in fields
    char qsep_key[17];
    stpcpy(stpcpy(qsep_key, q), &fs.key);

    if (IS_DEBUG) {
        printf("Looking up pattern for key: %s\n", qsep_key);
    }

    void* stored_pattern;
    if (hashmap_get_string(QuoteREs, qsep_key, &stored_pattern)) {
        if (IS_DEBUG) {
            printf("Found stored pattern: %s\n", (char*)stored_pattern);
        }
        return stored_pattern;
    }

    char sep_search[16];
    if (strcmp(fs.sep, PIPE) == 0) {
        stpcpy(stpcpy(sep_search, BACKSLASH), fs.sep);
    } else {
        stpcpy(sep_search, fs.sep);
    }

    char qs[20]; // quote + sep
    stpcpy(stpcpy(qs, q), sep_search);
    char spq[20]; // sep + quote
    stpcpy(stpcpy(spq, sep_search), q);
    char *exc_arr[7] = {"[^", q, "]*[^", sep_search, "]*[^", q, "]+"};
    char exc[50];
    nstpcpy(exc, exc_arr, 7);
    char *re_arr[22] = {"(^", q,  qs,  "|", spq, qs, "|", spq, q,   "$|", q,
                        exc,  qs, "|", spq, exc, qs, "|", spq, exc, q,    "$)"};
    char pattern[230];
    nstpcpy(pattern, re_arr, 22);

    DEBUG_PRINT("Generated pattern: '%s'", pattern);

    // Allocate and store the pattern
    char* stored_re = strdup(pattern);
    if (!stored_re) {
        FAIL("Failed to allocate memory for regex pattern");
    }

    if (!hashmap_put_string(QuoteREs, qsep_key, stored_re)) {
        free(stored_re);
        FAIL("Failed to store regex pattern in hashmap");
    }

    if (IS_DEBUG) {
        printf("Created and stored new pattern: %s\n", stored_re);
    }

    return stored_re;
}

static int get_fields_quote(char *line, field_sep fs) {
    char dqre[230];
    if (IS_DEBUG)
        printf("Getting quote RE 2\n");
    char *pattern = get_quoted_fields_re(fs, (char *)DOUBLEQUOT);
    DEBUG_PRINT("Testing double quote pattern: '%s'", pattern);
    if (rematch(pattern, line, true)) {
        DEBUG_PRINT("Double quote pattern matched");
        return 2;
    }
    char sqre[230];
    if (IS_DEBUG)
        printf("Getting quote RE 1\n");
    pattern = get_quoted_fields_re(fs, (char *)SINGLEQUOT);
    DEBUG_PRINT("Testing single quote pattern: '%s'", pattern);
    if (rematch(pattern, line, true)) {
        DEBUG_PRINT("Single quote pattern matched");
        return 1;
    }
    DEBUG_PRINT("No quote pattern matched");
    return 0;
}

static int count_matches_for_quoted_field_line(char *re, char *line) {
    DEBUG_PRINT("Testing line: '%s'", line);
    DEBUG_PRINT("Against pattern: '%s'", re);
    return 0;
}

static void cleanup_quotes(void* key, void* value) {
    free(value);
}

// Select the best separator from multiple zero-variance options
static int select_best_zero_variance_separator(field_sep **NoVar, int count) {
    int best_index = -1;
    size_t best_length = 0;
    
    // First pass: find the longest separator
    for (int i = 0; i < count; i++) {
        if (!NoVar[i]) continue;
        size_t len = strlen(NoVar[i]->sep);
        if (len > best_length) {
            best_length = len;
            best_index = i;
        }
    }
    
    if (best_index == -1) {
        FAIL("No valid zero-variance separator found despite having multiple candidates");
    }
    
    // Second pass: check if any other separator is contained within the longest one
    for (int i = 0; i < count; i++) {
        if (!NoVar[i] || i == best_index) continue;
        
        // If the current separator is contained within the best one, and is shorter,
        // keep the longer one (best_index)
        if (strstr(NoVar[best_index]->sep, NoVar[i]->sep) != NULL) {
            continue;
        }
        
        // If the best separator is contained within the current one, and current is longer,
        // update best_index
        if (strstr(NoVar[i]->sep, NoVar[best_index]->sep) != NULL) {
            best_index = i;
            best_length = strlen(NoVar[i]->sep);
        }
    }
    
    return best_index;
}

// Check if a character is a potential separator
static bool is_potential_sep_char(char c) {
    return !isalnum(c) && c != '_' && c != ' ' && c != '\t';
}

// First phase: collect potential custom separators
static void collect_potential_separators(const char *line) {
    const char *p = line;
    char potential_sep[MAX_CUSTOM_SEP_LEN + 1] = {0};
    int sep_len = 0;
    
    while (*p) {
        if (is_potential_sep_char(*p)) {
            // Start or continue building a potential separator
            if (sep_len < MAX_CUSTOM_SEP_LEN) {
                potential_sep[sep_len++] = *p;
                potential_sep[sep_len] = '\0';
                
                // Count fields for this potential separator
                int field_count = count_matches_for_line_str(potential_sep, (char*)line, strlen(line));
                if (field_count > 1) {
                    // Add to potential separators
                    strncpy(PotentialSeps[POTENTIAL_SEP_COUNT].sep, potential_sep, MAX_CUSTOM_SEP_LEN);
                    PotentialSeps[POTENTIAL_SEP_COUNT].sep[MAX_CUSTOM_SEP_LEN] = '\0';
                    PotentialSeps[POTENTIAL_SEP_COUNT].field_count = field_count;
                    PotentialSeps[POTENTIAL_SEP_COUNT].is_valid = false;
                    POTENTIAL_SEP_COUNT++;
                }
            }
        } else {
            // Reset separator building
            sep_len = 0;
        }
        p++;
    }
}

// Second phase: filter potential separators
static void filter_custom_separators(const char *line) {
    for (int i = 0; i < POTENTIAL_SEP_COUNT; i++) {
        potential_sep *ps = &PotentialSeps[i];
        int nf = count_matches_for_line_str(ps->sep, (char*)line, strlen(line));
        
        // If field count matches, this is a valid custom separator
        if (nf == ps->field_count) {
            ps->is_valid = true;
            
            // Add to actual custom separators
            CustomFS[CUSTOM_SEP_COUNT].key = 'x';  // Use 'x' for custom separators
            strncpy(CustomFS[CUSTOM_SEP_COUNT].sep, ps->sep, MAX_CUSTOM_SEP_LEN);
            CustomFS[CUSTOM_SEP_COUNT].sep[MAX_CUSTOM_SEP_LEN] = '\0';
            CustomFS[CUSTOM_SEP_COUNT].is_regex = false;
            CustomFS[CUSTOM_SEP_COUNT].total = nf * 2;
            CustomFS[CUSTOM_SEP_COUNT].prev_nf = nf;
            CustomFS[CUSTOM_SEP_COUNT].var = 0;
            CustomFS[CUSTOM_SEP_COUNT].sum_var = 0;
            
            // Initialize stats for the new custom separator
            CustomSeparatorStats[CUSTOM_SEP_COUNT].max_consecutive = 2;  // We know it's at least 2
            CustomSeparatorStats[CUSTOM_SEP_COUNT].max_nf = nf;
            CustomSeparatorStats[CUSTOM_SEP_COUNT].has_zero_variance = false;
            CustomSeparatorStats[CUSTOM_SEP_COUNT].confidence = 0.0;
            CustomSeparatorStats[CUSTOM_SEP_COUNT].has_sectional_override = false;
            CustomSeparatorStats[CUSTOM_SEP_COUNT].chunk_weight = 0.0;
            
            CUSTOM_SEP_COUNT++;
        }
    }
}

// Infer a field separator from data.
int infer_field_separator(int argc, char **argv, data_file_t *file) {
    if (!file->is_piped) {
        if (endswith(file->filename, CSV_SUFFIX)) {
            file->fs = get_common_fs('c');
            return 0;
        } else if (endswith(file->filename, TSV_SUFFIX)) {
            file->fs = get_common_fs('t');
            return 0;
        } else if (endswith(file->filename, PROPERTIES_SUFFIX)) {
            file->fs = get_common_fs('e');
            return 0;
        }
    }

    set_static_maps();
    init_common_separator_stats();  // Initialize stats tracking

    FILE *fp = get_readable_fp(file);
    int n_valid_rows = 0;
    int max_rows = 500;
    char line[BUF_SIZE];
    int line_counter = 0;
    hashmap *Quotes = hashmap_create(16);
    int CommonFSCount[COMMON_FS_COUNT][max_rows];
    int CommonFSNFConsecCounts[COMMON_FS_COUNT][MAX_NF_COUNT];
    int CustomFSCount[MAX_CUSTOM_SEPS][max_rows];
    int CustomFSNFConsecCounts[MAX_CUSTOM_SEPS][MAX_NF_COUNT];

    if (!Quotes) {
        FAIL("Failed to create Quotes hashmap");
    }

    if (IS_DEBUG)
        printf("Quotes: %p QuoteREs: %p\n", (void*)Quotes, (void*)QuoteREs);

    // Init base values for multiarrays
    for (int i = 0; i < COMMON_FS_COUNT; i++) {
        for (int j = 0; j < max_rows; j++) {
            CommonFSCount[i][j] = 0;

            if (j < MAX_NF_COUNT) {
                CommonFSNFConsecCounts[i][j] = 0;
            }
        }
    }
    
    // Initialize custom separator arrays
    for (int i = 0; i < MAX_CUSTOM_SEPS; i++) {
        for (int j = 0; j < max_rows; j++) {
            CustomFSCount[i][j] = 0;
            
            if (j < MAX_NF_COUNT) {
                CustomFSNFConsecCounts[i][j] = 0;
            }
        }
    }

    while (fgets(line, BUF_SIZE, fp) != NULL) {
        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;
        ++line_counter;

        if (IS_DEBUG) {
            DEBUG_PRINT("Inferfs: %d %s", line_counter, line);
        }

        if (rematch(EMPTY_LINE_RE, line, true)) {
            DEBUG_PRINT("Empty line skipped");
            continue;
        }

        ++n_valid_rows;

        if (n_valid_rows >= max_rows) {
            break;
        }

        if (n_valid_rows < 10 && strstr(line, DS_SEP)) {
            hashmap_free(Quotes, NULL);
            file->fs = &ds_sep;
            return 0;
        }

        const size_t len = strlen(line);

        // First line: collect potential separators
        if (n_valid_rows == 1) {
            collect_potential_separators(line);
        }
        // Second line: filter potential separators
        else if (n_valid_rows == 2) {
            filter_custom_separators(line);
        }
        // Process custom separators for every line after
        else {
            for (int i = 0; i < CUSTOM_SEP_COUNT; i++) {
                field_sep *fs = &CustomFS[i];
                int nf = count_matches_for_line_str(fs->sep, (char*)line, strlen(line));
                
                if (nf >= 2) {
                    CustomFSCount[i][line_counter] = nf;
                    fs->total += nf;
                    
                    int prev_nf = fs->prev_nf;
                    int consec_count = 0;
                    
                    if (prev_nf && nf != prev_nf) {
                        consec_count = CustomFSNFConsecCounts[i][prev_nf];
                        
                        if (consec_count && consec_count < 3) {
                            // reset because the previous run was not sufficiently consecutive
                            CustomFSNFConsecCounts[i][prev_nf] = 0;
                        }
                    }
                    
                    fs->prev_nf = nf;
                    CustomFSNFConsecCounts[i][nf]++;
                    consec_count = CustomFSNFConsecCounts[i][nf];
                    
                    separator_stats *stats_for_separator = &CustomSeparatorStats[i];
                    if (consec_count > stats_for_separator->max_consecutive) {
                        stats_for_separator->max_consecutive = consec_count;
                    }
                    
                    // Update max_nf if needed
                    if (nf > stats_for_separator->max_nf) {
                        stats_for_separator->max_nf = nf;
                    }
                }
            }
        }

        for (int s = 0; s < COMMON_FS_COUNT; s++) {
            field_sep *fs = &CommonFS[s];

            // Handle quoting
            char *quote;
            void* n_quotes_ptr;
            int n_quotes = 0;

            DEBUG_PRINT("Getting quote \"%s\"", fs->sep);

            if (!hashmap_get_string(Quotes, &fs->key, &n_quotes_ptr)) {
                n_quotes = get_fields_quote(line, *fs);
                if (n_quotes) {
                    int* stored_quotes = malloc(sizeof(int));
                    if (!stored_quotes) {
                        FAIL("Failed to allocate memory for quote count");
                    }
                    *stored_quotes = n_quotes;
                    if (!hashmap_put_string(Quotes, &fs->key, stored_quotes)) {
                        free(stored_quotes);
                        FAIL("Failed to store quote count in hashmap");
                    }
                }
            } else {
                n_quotes = *(int*)n_quotes_ptr;
            }

            int nf = 0;

            if (n_quotes) {
                quote = (n_quotes == 1) ? SINGLEQUOT : DOUBLEQUOT;
                char *re = get_quoted_fields_re(*fs, quote);
                DEBUG_PRINT("Using quoted field counting for line: '%s'", line);
                nf = count_matches_for_quoted_field_line(re, line);
            } else if (fs->is_regex) {
                DEBUG_PRINT("Testing regex separator '%s' on line: '%s'", fs->sep, line);
                nf = count_matches_for_line_regex(fs->sep, line, len);
            } else {
                DEBUG_PRINT("Using char counting for line: '%s'", line);
                nf = count_matches_for_line_char(fs->sep[0], line, len);
            }

            if (IS_DEBUG) {
                printf("inferfs - Value of nf for sep \"%s\" : %d\n", fs->sep, nf);
            }

            if (nf < 2)
                continue;

            CommonFSCount[s][line_counter] = nf;
            fs->total += nf;

            int prev_nf = fs->prev_nf;
            int consec_count = -1;

            if (prev_nf && nf != prev_nf) {
                consec_count = CommonFSNFConsecCounts[s][prev_nf];

                if (consec_count && consec_count < 3) {
                    // reset because the previous run was not sufficiently consecutive
                    CommonFSNFConsecCounts[s][prev_nf] = 0;
                }
            }

            fs->prev_nf = nf;

            CommonFSNFConsecCounts[s][nf]++;
            consec_count = CommonFSNFConsecCounts[s][nf];
            separator_stats *stats_for_separator = &CommonSeparatorStats[s];
            if (consec_count > stats_for_separator->max_consecutive) {
                stats_for_separator->max_consecutive = consec_count;
            }

            // Update max_nf if needed
            if (nf > stats_for_separator->max_nf) {
                stats_for_separator->max_nf = nf;
            }
        }
    }

    if (max_rows > n_valid_rows)
        max_rows = n_valid_rows;

    // Free the stored quote counts
    hashmap_free(Quotes, cleanup_quotes);

    if (IS_DEBUG) {
        DEBUG_PRINT("inferfs - max_rows: %d n_valid_rows: %d", max_rows, n_valid_rows);
        DEBUG_PRINT("inferfs - Starting common separator variance calculations");
    }

    field_sep *NoVar[COMMON_FS_COUNT + CUSTOM_SEP_COUNT];
    int winning_s = -1;
    int custom_winning_s = -1;
    double max_confidence = 0.0;
    int no_var_count = 0;

    for (int s = 0; s < COMMON_FS_COUNT; s++) {
        field_sep *fs = &CommonFS[s];
        float average_nf = (float)fs->total / max_rows;

        if (IS_DEBUG) {
            DEBUG_PRINT("inferfs - Processing separator %d (%c), total: %d, average_nf: %f", 
                       s, fs->key, fs->total, average_nf);
        }

        // Get unique field counts for this separator
        int unique_count;
        if (IS_DEBUG) {
            DEBUG_PRINT("inferfs - Getting unique field counts for separator %c", fs->key);
        }
        int *unique_counts = get_unique_field_counts(s, &unique_count, CommonFSNFConsecCounts);
        
        if (IS_DEBUG) {
            DEBUG_PRINT("inferfs - Got %d unique field counts for separator %c", unique_count, fs->key);
        }
        
        // Check for sectional override
        double max_chunk_weight = 0.0;
        bool has_sectional_override = false;
        
        for (int i = 0; i < unique_count; i++) {
            int nf = unique_counts[i];
            double chunk_weight = (double)CommonFSNFConsecCounts[s][nf] / max_rows;
            
            if (IS_DEBUG) {
                DEBUG_PRINT("inferfs - Field count %d: chunk_weight %f", nf, chunk_weight);
            }
            
            if (chunk_weight < 0.6) {
                if (IS_DEBUG) {
                    DEBUG_PRINT("inferfs - Chunk weight for %d separator %s is too low: %f", nf, fs->key, chunk_weight);
                }
                continue;
            }
            
            has_sectional_override = true;
            double chunk_weight_composite = chunk_weight * nf;
            
            if (chunk_weight_composite > max_chunk_weight) {
                if (IS_DEBUG) {
                    DEBUG_PRINT("inferfs - New max chunk weight for %d separator %c: %f", nf, fs->key, chunk_weight_composite);
                }
                max_chunk_weight = chunk_weight_composite;
            }
        }
        
        free(unique_counts);
        
        if (average_nf < 2 && !has_sectional_override) {
            if (IS_DEBUG) {
                DEBUG_PRINT("inferfs - Average field count for %d separator %c is too low: %f", s, fs->key, average_nf);
            }
            continue;
        }

        if (IS_DEBUG) {
            DEBUG_PRINT("inferfs - Starting variance calculation for separator %d", s);
        }

        // Calculate variance
        for (int j = 0; j < max_rows; j++) {
            int row_nf_count = CommonFSCount[s][j];
            if (!row_nf_count) {
                continue;
            }
            double point_var = pow(row_nf_count - average_nf, 2);
            if (IS_DEBUG) {
                DEBUG_PRINT("inferfs - %d %c %d %f", j, fs->key, (int)row_nf_count,
                       point_var);
            }
            fs->sum_var += point_var;
        }

        fs->var = fs->sum_var / max_rows;

        if (IS_DEBUG) {
            DEBUG_PRINT("inferfs - %c %f %f", fs->key, average_nf, fs->var);
        }

        // Calculate confidence score
        double confidence = calculate_separator_confidence(s, max_rows);
        CommonSeparatorStats[s].confidence = confidence;
        CommonSeparatorStats[s].has_sectional_override = has_sectional_override;
        CommonSeparatorStats[s].chunk_weight = max_chunk_weight;

        if (fs->var == 0) {
            if (IS_DEBUG) {
                DEBUG_PRINT("inferfs - Separator %c has zero variance", fs->key);
            }
            NoVar[s] = fs;
            winning_s = s;
            no_var_count++;
        } else if (winning_s < 0 || fs->var < CommonFS[winning_s].var) {
            if (IS_DEBUG) {
                DEBUG_PRINT("inferfs - Separator %c has lower variance than current winner %c", fs->key, CommonFS[winning_s].key);
            }
            winning_s = s;
        }
    }

    if (IS_DEBUG) {
        DEBUG_PRINT("inferfs - CUSTOM_SEP_COUNT: %d", CUSTOM_SEP_COUNT);
        DEBUG_PRINT("inferfs - Starting custom separator variance calculations");
    }

    // Process custom separators
    for (int s = 0; s < CUSTOM_SEP_COUNT; s++) {
        field_sep *fs = &CustomFS[s];
        float average_nf = (float)fs->total / max_rows;

        if (IS_DEBUG) {
            DEBUG_PRINT("inferfs - average_nf %f", average_nf);
        }

        if (average_nf < 2) {
            continue;
        }

        // Calculate variance starting from row 3 (index 2)
        for (int j = 2; j < max_rows; j++) {
            int row_nf_count = CustomFSCount[s][j];
            if (!row_nf_count) {
                continue;
            }
            double point_var = pow(row_nf_count - average_nf, 2);
            if (IS_DEBUG) {
                DEBUG_PRINT("inferfs - %d %s %d %f", j, fs->sep, (int)row_nf_count,
                       point_var);
            }
            fs->sum_var += point_var;
        }

        fs->var = fs->sum_var / max_rows;

        if (IS_DEBUG) {
            DEBUG_PRINT("inferfs - %s %f %f", fs->sep, average_nf, fs->var);
        }

        if (fs->var == 0) {
            NoVar[COMMON_FS_COUNT + s] = fs;
            custom_winning_s = s;
            if (IS_DEBUG) {
                DEBUG_PRINT("inferfs - Zero variance custom separator found");
            }
        } else if (custom_winning_s < 0 || fs->var < CustomFS[custom_winning_s].var) {
            custom_winning_s = s;
            if (IS_DEBUG) {
                DEBUG_PRINT("inferfs - New winning custom separator");
            }
        }
    }

    // Handle multiple zero-variance separators
    if (no_var_count > 1) {
        int best_no_var = select_best_zero_variance_separator(NoVar, COMMON_FS_COUNT + CUSTOM_SEP_COUNT);
        if (best_no_var != -1) {
            if (best_no_var < COMMON_FS_COUNT) {
                winning_s = best_no_var;
                custom_winning_s = -1;
            } else {
                winning_s = -1;
                custom_winning_s = best_no_var - COMMON_FS_COUNT;
            }
        }
    } else if (no_var_count == 1) {
        // Find the single zero-variance separator
        for (int s = 0; s < COMMON_FS_COUNT + CUSTOM_SEP_COUNT; s++) {
            if (NoVar[s]) {
                if (s < COMMON_FS_COUNT) {
                    winning_s = s;
                    custom_winning_s = -1;
                } else {
                    winning_s = -1;
                    custom_winning_s = s - COMMON_FS_COUNT;
                }
                break;
            }
        }
    }

    // Determine final winner between common and custom separators
    if (winning_s >= 0 && custom_winning_s >= 0) {
        // Compare variances between best common and best custom separator
        if (CommonFS[winning_s].var <= CustomFS[custom_winning_s].var) {
            custom_winning_s = -1;  // Common separator wins
        } else {
            winning_s = -1;  // Custom separator wins
        }
    }

    if (IS_DEBUG) {
        if (winning_s > -1) {
            DEBUG_PRINT("inferfs - Winner: %d \"%s\" (confidence: %f)", winning_s,
                   CommonFS[winning_s].sep, max_confidence);
        }
        else if (custom_winning_s > -1) {
            DEBUG_PRINT("inferfs - Winner: custom %d \"%s\" (confidence: %f)", custom_winning_s,
                   CustomFS[custom_winning_s].sep, max_confidence);
        }
        else {
            DEBUG_PRINT("inferfs - No winner identified, using \"%s\"", SPACEMULTI);
        }
    }

    // rewind(fp);
    // fclose(fp);

    if (winning_s < 0 && custom_winning_s < 0) {
        file->fs = get_common_fs('w');
        return 1;
    } else if (winning_s >= 0) {
        file->fs = &CommonFS[winning_s];
        return 0;
    } else {
        file->fs = &CustomFS[custom_winning_s];
        return 0;
    }

    return 0;
}

// Cleanup function for regex patterns stored in the hashmap
static void cleanup_regex_pattern(void* key, void* value) {
    free(value); // Free the allocated pattern
}

// Add cleanup function to be called at program exit
void cleanup_inferfs(void) {
    if (QuoteREs) {
        hashmap_free(QuoteREs, cleanup_regex_pattern);
        QuoteREs = NULL;
    }
}

