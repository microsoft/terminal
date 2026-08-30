const SCORE_MATCH: i32 = 16;
const SCORE_GAP_START: i32 = -3;
const SCORE_GAP_EXTENSION: i32 = -1;
const BOUNDARY_BONUS: i32 = SCORE_MATCH / 2;
const NON_WORD_BONUS: i32 = SCORE_MATCH / 2;
const CAMEL_CASE_BONUS: i32 = BOUNDARY_BONUS + SCORE_GAP_EXTENSION;
const BONUS_CONSECUTIVE: i32 = -(SCORE_GAP_START + SCORE_GAP_EXTENSION);
const BONUS_FIRST_CHAR_MULTIPLIER: i32 = 2;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum CharClass {
    NonWord,
    Lower,
    Upper,
    Digit,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct Pattern {
    terms: Vec<Vec<char>>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct TextRun {
    pub start: usize,
    pub end: usize,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct MatchResult {
    pub score: i32,
    pub runs: Vec<TextRun>,
}

#[must_use]
pub fn parse_pattern(pattern: &str) -> Pattern {
    let terms = pattern
        .split(' ')
        .filter(|term| !term.is_empty())
        .map(|term| term.chars().map(simple_fold).collect())
        .collect();

    Pattern { terms }
}

#[must_use]
pub fn match_text(text: &str, pattern: &Pattern) -> Option<MatchResult> {
    if pattern.terms.is_empty() {
        return Some(MatchResult::default());
    }

    let text_code_points = text.chars().collect::<Vec<_>>();
    let mut total_score = 0;
    let mut all_positions = Vec::new();

    for term in &pattern.terms {
        let mut term_positions = Vec::new();
        let score = fuzzy_match_v2(&text_code_points, term, &mut term_positions);
        if score <= 0 {
            return None;
        }

        total_score += score;
        all_positions.extend(term_positions);
    }

    all_positions.sort_unstable();
    all_positions.dedup();

    let mut runs = Vec::new();
    let mut next_position = 0;
    let mut utf16_offset = 0;
    let mut run_start = None;

    for (code_point_index, code_point) in text_code_points.iter().copied().enumerate() {
        let code_point_width = code_point.len_utf16();
        let is_match = all_positions.get(next_position).copied() == Some(code_point_index);

        if is_match {
            run_start.get_or_insert(utf16_offset);
            next_position += 1;
        } else if let Some(start) = run_start.take() {
            runs.push(TextRun {
                start,
                end: utf16_offset - 1,
            });
        }

        utf16_offset += code_point_width;
    }

    if let Some(start) = run_start {
        runs.push(TextRun {
            start,
            end: utf16_offset - 1,
        });
    }

    Some(MatchResult {
        score: total_score,
        runs,
    })
}

fn simple_fold(code_point: char) -> char {
    match code_point {
        '\u{03c2}' => '\u{03c3}',
        _ => code_point.to_lowercase().next().unwrap_or(code_point),
    }
}

fn class_of(code_point: char) -> CharClass {
    if code_point.is_uppercase() {
        CharClass::Upper
    } else if code_point.is_lowercase() || code_point.is_alphabetic() {
        CharClass::Lower
    } else if code_point.is_numeric() {
        CharClass::Digit
    } else {
        CharClass::NonWord
    }
}

fn calculate_bonus(previous: CharClass, current: CharClass) -> i32 {
    if previous == CharClass::NonWord && current != CharClass::NonWord {
        BOUNDARY_BONUS
    } else if (previous == CharClass::Lower && current == CharClass::Upper)
        || (previous != CharClass::Digit && current == CharClass::Digit)
    {
        CAMEL_CASE_BONUS
    } else if current == CharClass::NonWord {
        NON_WORD_BONUS
    } else {
        0
    }
}

fn try_skip(input: &[char], search: char, start: usize) -> Option<usize> {
    input[start..]
        .iter()
        .position(|candidate| *candidate == search)
        .map(|offset| start + offset)
}

fn fuzzy_index(input: &[char], pattern: &[char]) -> Option<usize> {
    let mut index = 0;
    let mut first_index = 0;

    for (pattern_index, search) in pattern.iter().copied().enumerate() {
        index = try_skip(input, search, index)?;
        if pattern_index == 0 && index > 0 {
            first_index = index - 1;
        }
        index += 1;
    }

    Some(first_index)
}

#[allow(clippy::too_many_lines)]
fn fuzzy_match_v2(text: &[char], pattern: &[char], positions: &mut Vec<usize>) -> i32 {
    if pattern.is_empty() {
        return 0;
    }

    let folded_text = text.iter().copied().map(simple_fold).collect::<Vec<_>>();
    let Some(first_index_of) = fuzzy_index(&folded_text, pattern) else {
        return 0;
    };

    let mut initial_scores = vec![0_i32; text.len()];
    let mut consecutive_scores = vec![0_usize; text.len()];
    let mut first_occurrence = vec![0_usize; pattern.len()];
    let mut bonuses = vec![0_i32; text.len()];

    let mut max_score = 0;
    let mut max_score_position = 0;
    let mut pattern_index = 0;
    let mut last_index = 0;
    let first_pattern_char = pattern[0];
    let mut current_pattern_char = pattern[0];
    let mut previous_initial_score = 0;
    let mut previous_class = CharClass::NonWord;
    let mut in_gap = false;

    for absolute_index in first_index_of..folded_text.len() {
        let current_char = folded_text[absolute_index];
        let current_class = class_of(text[absolute_index]);
        let bonus = calculate_bonus(previous_class, current_class);
        bonuses[absolute_index] = bonus;
        previous_class = current_class;

        if current_char == current_pattern_char {
            if pattern_index < pattern.len() {
                first_occurrence[pattern_index] = absolute_index;
                pattern_index += 1;
                if pattern_index < pattern.len() {
                    current_pattern_char = pattern[pattern_index];
                }
            }
            last_index = absolute_index;
        }

        if current_char == first_pattern_char {
            let score = SCORE_MATCH + bonus * BONUS_FIRST_CHAR_MULTIPLIER;
            initial_scores[absolute_index] = score;
            consecutive_scores[absolute_index] = 1;

            if pattern.len() == 1 && score > max_score {
                max_score = score;
                max_score_position = absolute_index;
                if bonus == BOUNDARY_BONUS {
                    break;
                }
            }
            in_gap = false;
        } else {
            initial_scores[absolute_index] = (previous_initial_score
                + if in_gap {
                    SCORE_GAP_EXTENSION
                } else {
                    SCORE_GAP_START
                })
            .max(0);
            consecutive_scores[absolute_index] = 0;
            in_gap = true;
        }

        previous_initial_score = initial_scores[absolute_index];
    }

    if pattern_index != pattern.len() {
        return 0;
    }

    if pattern.len() == 1 {
        positions.push(max_score_position);
        return max_score;
    }

    let first_occurrence_of_first_char = first_occurrence[0];
    let width = last_index - first_occurrence_of_first_char + 1;
    let rows = pattern.len();
    let matrix_size = width * rows;

    let mut score_matrix = vec![0_i32; matrix_size];
    score_matrix[..width].copy_from_slice(
        &initial_scores[first_occurrence_of_first_char..first_occurrence_of_first_char + width],
    );

    let mut consecutive_matrix = vec![0_usize; matrix_size];
    consecutive_matrix[..width].copy_from_slice(
        &consecutive_scores[first_occurrence_of_first_char..first_occurrence_of_first_char + width],
    );

    for offset in 0..pattern.len() - 1 {
        let pattern_char_offset = first_occurrence[offset + 1];
        let slice_len = last_index - pattern_char_offset + 1;
        current_pattern_char = pattern[offset + 1];
        pattern_index = offset + 1;
        let row = pattern_index * width;
        in_gap = false;
        let start_column = pattern_char_offset - first_occurrence_of_first_char;

        if start_column > 0 {
            score_matrix[row + start_column - 1] = 0;
        }

        for relative_index in 0..slice_len {
            let column = pattern_char_offset + relative_index;
            let column_offset = column - first_occurrence_of_first_char;
            let left_score = if relative_index == 0 {
                0
            } else {
                score_matrix[row + column_offset - 1]
            };
            let score = left_score
                + if in_gap {
                    SCORE_GAP_EXTENSION
                } else {
                    SCORE_GAP_START
                };

            let mut diagonal_score = 0;
            let mut consecutive = 0;

            if folded_text[column] == current_pattern_char {
                diagonal_score = score_matrix[row - width + column_offset - 1] + SCORE_MATCH;
                let mut bonus = bonuses[column];
                consecutive = consecutive_matrix[row - width + column_offset - 1] + 1;

                if bonus == BOUNDARY_BONUS {
                    consecutive = 1;
                } else if consecutive > 1 {
                    let chain_start = column + 1 - consecutive;
                    bonus = bonus.max(BONUS_CONSECUTIVE).max(bonuses[chain_start]);
                }

                if diagonal_score + bonus < score {
                    diagonal_score += bonuses[column];
                    consecutive = 0;
                } else {
                    diagonal_score += bonus;
                }
            }

            consecutive_matrix[row + column_offset] = consecutive;
            in_gap = diagonal_score < score;
            let cell_score = 0.max(diagonal_score.max(score));

            if offset + 2 == pattern.len() && cell_score > max_score {
                max_score = cell_score;
                max_score_position = column;
            }

            score_matrix[row + column_offset] = cell_score;
        }
    }

    let mut current_column = max_score_position;
    pattern_index = pattern.len() - 1;
    let mut prefer_current_match = true;

    loop {
        let row_start = pattern_index * width;
        let column_offset = current_column - first_occurrence_of_first_char;
        let cell_score = score_matrix[row_start + column_offset];
        let diagonal_score =
            if pattern_index > 0 && current_column >= first_occurrence[pattern_index] {
                score_matrix[row_start - width + column_offset - 1]
            } else {
                0
            };
        let left_score = if current_column > first_occurrence[pattern_index] {
            score_matrix[row_start + column_offset - 1]
        } else {
            0
        };

        if cell_score > diagonal_score
            && (cell_score > left_score || (cell_score == left_score && prefer_current_match))
        {
            positions.push(current_column);
            if pattern_index == 0 {
                break;
            }
            pattern_index -= 1;
        }

        if current_column == 0 {
            break;
        }
        current_column -= 1;

        if row_start + column_offset >= matrix_size {
            break;
        }

        prefer_current_match = consecutive_matrix[row_start + column_offset] > 1
            || (row_start + width + column_offset + 1 < matrix_size
                && consecutive_matrix[row_start + width + column_offset + 1] > 0);
    }

    max_score
}
