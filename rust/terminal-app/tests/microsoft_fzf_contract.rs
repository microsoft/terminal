use terminal_app::{MatchResult, TextRun, match_text, parse_pattern};

const SCORE_MATCH: i32 = 16;
const SCORE_GAP_START: i32 = -3;
const SCORE_GAP_EXTENSION: i32 = -1;
const BONUS_BOUNDARY: i32 = SCORE_MATCH / 2;
const BONUS_NON_WORD: i32 = SCORE_MATCH / 2;
const BONUS_CAMEL_123: i32 = BONUS_BOUNDARY + SCORE_GAP_EXTENSION;
const BONUS_CONSECUTIVE: i32 = -(SCORE_GAP_START + SCORE_GAP_EXTENSION);
const BONUS_FIRST_CHAR_MULTIPLIER: i32 = 2;

fn runs(items: &[(usize, usize)]) -> Vec<TextRun> {
    items
        .iter()
        .map(|&(start, end)| TextRun { start, end })
        .collect()
}

fn assert_match(pattern: &str, text: &str, expected_score: i32, expected_runs: &[(usize, usize)]) {
    let actual = match_text(text, &parse_pattern(pattern));
    if expected_score == 0 && expected_runs.is_empty() {
        assert_eq!(actual, None);
    } else {
        assert_eq!(
            actual,
            Some(MatchResult {
                score: expected_score,
                runs: runs(expected_runs),
            })
        );
    }
}

#[test]
fn microsoft_terminal_app_fzf_all_pattern_chars_do_not_match() {
    assert_match("fbb", "foo bar", 0, &[]);
}

#[test]
fn microsoft_terminal_app_fzf_consecutive_chars() {
    assert_match(
        "oba",
        "foobar",
        SCORE_MATCH * 3 + BONUS_CONSECUTIVE * 2,
        &[(2, 4)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_consecutive_chars_first_char_bonus() {
    assert_match(
        "foo",
        "foobar",
        SCORE_MATCH * 3
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 2,
        &[(0, 2)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_non_word_boundary_consecutive_chars() {
    assert_match(
        "zshc",
        "/man1/zshcompctl.1",
        SCORE_MATCH * 4
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_FIRST_CHAR_MULTIPLIER * BONUS_CONSECUTIVE * 3,
        &[(6, 9)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_non_word_chars_case_insensitive() {
    assert_match(
        "foo-b",
        "xFoo-Bar Baz",
        (SCORE_MATCH + BONUS_CAMEL_123 * BONUS_FIRST_CHAR_MULTIPLIER)
            + (SCORE_MATCH + BONUS_CAMEL_123)
            + (SCORE_MATCH + BONUS_CAMEL_123)
            + (SCORE_MATCH + BONUS_BOUNDARY)
            + (SCORE_MATCH + BONUS_NON_WORD),
        &[(1, 5)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_non_word_chars_with_gap() {
    assert_match(
        "12356",
        "abc123 456",
        (SCORE_MATCH + BONUS_CAMEL_123 * BONUS_FIRST_CHAR_MULTIPLIER)
            + (SCORE_MATCH + BONUS_CAMEL_123)
            + (SCORE_MATCH + BONUS_CAMEL_123)
            + SCORE_GAP_START
            + SCORE_GAP_EXTENSION
            + SCORE_MATCH
            + SCORE_MATCH
            + BONUS_CONSECUTIVE,
        &[(3, 5), (8, 9)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_camel_case_bonus() {
    assert_match(
        "def56",
        "abcDEF 456",
        (SCORE_MATCH + BONUS_CAMEL_123 * BONUS_FIRST_CHAR_MULTIPLIER)
            + (SCORE_MATCH + BONUS_CAMEL_123)
            + (SCORE_MATCH + BONUS_CAMEL_123)
            + SCORE_GAP_START
            + SCORE_GAP_EXTENSION
            + SCORE_MATCH
            + (SCORE_MATCH + BONUS_CONSECUTIVE),
        &[(3, 5), (8, 9)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_boundary_and_first_char_multiplier() {
    assert_match(
        "fbb",
        "foo bar baz",
        SCORE_MATCH * 3
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_BOUNDARY * 2
            + SCORE_GAP_START * 2
            + SCORE_GAP_EXTENSION * 4,
        &[(0, 0), (4, 4), (8, 8)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_matches_case_insensitive() {
    assert_match(
        "FBB",
        "foo bar baz",
        SCORE_MATCH * 3
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_BOUNDARY * 2
            + SCORE_GAP_START * 2
            + SCORE_GAP_EXTENSION * 4,
        &[(0, 0), (4, 4), (8, 8)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_multiple_terms() {
    let term1_score = SCORE_MATCH * 2
        + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
        + BONUS_FIRST_CHAR_MULTIPLIER * BONUS_CONSECUTIVE;
    let term2_score = SCORE_MATCH * 4
        + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
        + BONUS_FIRST_CHAR_MULTIPLIER * BONUS_CONSECUTIVE * 3;

    assert_match(
        "sp anta",
        "Split Pane, split: horizontal, profile: SSH: Antares",
        term1_score + term2_score,
        &[(0, 1), (45, 48)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_multiple_terms_all_chars_match() {
    let term_score = SCORE_MATCH * 3
        + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
        + BONUS_FIRST_CHAR_MULTIPLIER * BONUS_CONSECUTIVE * 2;

    assert_match("foo bar", "foo bar", term_score * 2, &[(0, 2), (4, 6)]);
}

#[test]
fn microsoft_terminal_app_fzf_multiple_terms_not_all_match() {
    assert_match(
        "sp anta zz",
        "Split Pane, split: horizontal, profile: SSH: Antares",
        0,
        &[],
    );
}

#[test]
fn microsoft_terminal_app_fzf_case_insensitive_bonus_boundary() {
    assert_match(
        "fbb",
        "Foo Bar Baz",
        SCORE_MATCH * 3
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_BOUNDARY * 2
            + SCORE_GAP_START * 2
            + SCORE_GAP_EXTENSION * 4,
        &[(0, 0), (4, 4), (8, 8)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_traceback_first_equal_score() {
    assert_match(
        "bar",
        "Foo Bar Bar",
        (SCORE_MATCH + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER)
            + (SCORE_MATCH + BONUS_BOUNDARY)
            + (SCORE_MATCH + BONUS_BOUNDARY),
        &[(4, 6)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_traceback_highest_score() {
    assert_match(
        "bar",
        "Foo aBar Bar",
        SCORE_MATCH * 3 + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER * 2,
        &[(9, 11)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_traceback_highest_score_gaps() {
    assert_match(
        "bar",
        "Boo Author Raz Bar",
        SCORE_MATCH * 3
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 2,
        &[(15, 17)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_traceback_earlier_chars_no_bonus() {
    assert_match(
        "clts",
        "close all tabs after this",
        SCORE_MATCH * 4
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_FIRST_CHAR_MULTIPLIER * BONUS_CONSECUTIVE
            + SCORE_GAP_START
            + SCORE_GAP_EXTENSION * 7
            + BONUS_BOUNDARY
            + SCORE_GAP_START
            + SCORE_GAP_EXTENSION,
        &[(0, 1), (10, 10), (13, 13)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_gap_boundary_can_beat_consecutive() {
    let consecutive_score = SCORE_MATCH * 3 + BONUS_CONSECUTIVE * 2;
    let gap_score = SCORE_MATCH * 3
        + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
        + BONUS_BOUNDARY * 2
        + SCORE_GAP_START
        + SCORE_GAP_EXTENSION * 2
        + SCORE_GAP_START
        + SCORE_GAP_EXTENSION;

    assert_match("oob", "foobar", consecutive_score, &[(1, 3)]);
    assert_match("oob", "out-of-bound", gap_score, &[(0, 0), (4, 4), (7, 7)]);
    assert!(gap_score > consecutive_score);
}

#[test]
fn microsoft_terminal_app_fzf_consecutive_beats_gap_with_first_char_bonus() {
    let consecutive_score = SCORE_MATCH * 3
        + BONUS_FIRST_CHAR_MULTIPLIER * BONUS_BOUNDARY
        + BONUS_FIRST_CHAR_MULTIPLIER * BONUS_CONSECUTIVE * 2;
    let gap_score =
        SCORE_MATCH * 3 + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER + SCORE_GAP_START * 2;

    assert_match("oob", "oobar", consecutive_score, &[(0, 2)]);
    assert_match("oob", "oaoabound", gap_score, &[(0, 0), (2, 2), (4, 4)]);
    assert!(consecutive_score > gap_score);
}

#[test]
fn microsoft_terminal_app_fzf_consecutive_beats_gap_without_bonus() {
    let consecutive_score = SCORE_MATCH * 3 + BONUS_CONSECUTIVE * 2;
    let gap_score = SCORE_MATCH * 3 + SCORE_GAP_START * 2;

    assert_match("oob", "aoobar", consecutive_score, &[(1, 3)]);
    assert_match("oob", "aoaoabound", gap_score, &[(1, 1), (3, 3), (5, 5)]);
    assert!(consecutive_score > gap_score);
}

#[test]
fn microsoft_terminal_app_fzf_gap_first_char_bonus_can_beat_consecutive() {
    let consecutive_score = SCORE_MATCH * 2 + BONUS_CONSECUTIVE;
    let gap_score =
        SCORE_MATCH * 2 + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER + SCORE_GAP_START;

    assert_match("ob", "aobar", consecutive_score, &[(1, 2)]);
    assert_match("ob", "oabar", gap_score, &[(0, 0), (2, 2)]);
    assert!(gap_score > consecutive_score);
}

#[test]
fn microsoft_terminal_app_fzf_gap_3_four_char_no_consecutive_no_longer_beats_consecutive() {
    let consecutive_score = SCORE_MATCH * 4 + BONUS_CONSECUTIVE * 3;
    let gap_score =
        SCORE_MATCH * 4 + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER + SCORE_GAP_START * 3;

    assert_match("obar", "aobar", consecutive_score, &[(1, 4)]);
    assert_match(
        "obar",
        "oabzazr",
        gap_score,
        &[(0, 0), (2, 2), (4, 4), (6, 6)],
    );
    assert!(consecutive_score > gap_score);
}

#[test]
fn microsoft_terminal_app_fzf_gap_11_two_char_no_longer_beats_consecutive() {
    let consecutive_score = SCORE_MATCH * 2 + BONUS_CONSECUTIVE;
    let gap_score = SCORE_MATCH * 2
        + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
        + SCORE_GAP_START
        + SCORE_GAP_EXTENSION * 10;

    assert_match("ob", "aobar", consecutive_score, &[(1, 2)]);
    assert_match("ob", "oaaaaaaaaaaabar", gap_score, &[(0, 0), (12, 12)]);
    assert!(consecutive_score > gap_score);
}

#[test]
fn microsoft_terminal_app_fzf_gap_11_three_char_one_consecutive_no_longer_beats_consecutive() {
    let consecutive_score = SCORE_MATCH * 3 + BONUS_CONSECUTIVE * 2;
    let gap_score = SCORE_MATCH * 3
        + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
        + BONUS_CONSECUTIVE
        + SCORE_GAP_START
        + SCORE_GAP_EXTENSION * 10;

    assert_match("oba", "aobar", consecutive_score, &[(1, 3)]);
    assert_match("oba", "oaaaaaaaaaaabar", gap_score, &[(0, 0), (12, 13)]);
    assert!(consecutive_score > gap_score);
}

#[test]
fn microsoft_terminal_app_fzf_gap_5_three_char_no_consecutive_no_longer_beats_consecutive() {
    let consecutive_score = SCORE_MATCH * 3 + BONUS_CONSECUTIVE * 2;
    let gap_score = SCORE_MATCH * 3
        + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
        + SCORE_GAP_START
        + SCORE_GAP_EXTENSION * 2
        + SCORE_GAP_START
        + SCORE_GAP_EXTENSION;

    assert_match("oba", "aobar", consecutive_score, &[(1, 3)]);
    assert_match("oba", "oaaabzzar", gap_score, &[(0, 0), (4, 4), (7, 7)]);
    assert!(consecutive_score > gap_score);
}

#[test]
fn microsoft_terminal_app_fzf_russian_case_mismatch() {
    assert_match(
        "новая",
        "Новая вкладка",
        SCORE_MATCH * 5
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 4,
        &[(0, 4)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_russian_case_match() {
    assert_match(
        "Новая",
        "Новая вкладка",
        SCORE_MATCH * 5
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 4,
        &[(0, 4)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_english_case_match() {
    assert_match(
        "Newer",
        "Newer tab",
        SCORE_MATCH * 5
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 4,
        &[(0, 4)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_english_case_mismatch() {
    assert_match(
        "newer",
        "Newer tab",
        SCORE_MATCH * 5
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 4,
        &[(0, 4)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_surrogate_pair() {
    assert_match(
        "N😀ewer",
        "N😀ewer tab",
        SCORE_MATCH * 6
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 5,
        &[(0, 6)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_french_case_match() {
    assert_match(
        "Éco",
        "École",
        SCORE_MATCH * 3
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 2,
        &[(0, 2)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_french_case_mismatch() {
    assert_match(
        "Éco",
        "école",
        SCORE_MATCH * 3
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 2,
        &[(0, 2)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_german_case_match() {
    assert_match(
        "fuß",
        "Fußball",
        SCORE_MATCH * 3
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 2,
        &[(0, 2)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_greek_case_mismatch() {
    assert_match(
        "λόγοσ",
        "λόγος",
        SCORE_MATCH * 5
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 4,
        &[(0, 4)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_greek_case_match() {
    assert_match(
        "λόγος",
        "λόγος",
        SCORE_MATCH * 5
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 4,
        &[(0, 4)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_surrogate_pair_utf16_consecutive() {
    assert_match(
        "N𠀋N😀𝄞e𐐷",
        "N𠀋N😀𝄞e𐐷 tab",
        SCORE_MATCH * 7
            + BONUS_BOUNDARY * BONUS_FIRST_CHAR_MULTIPLIER
            + BONUS_CONSECUTIVE * BONUS_FIRST_CHAR_MULTIPLIER * 6,
        &[(0, 10)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_surrogate_pair_utf16_prefer_consecutive() {
    assert_match(
        "𠀋😀",
        "N𠀋😀wer 😀b𐐷 ",
        SCORE_MATCH * 2 + BONUS_CONSECUTIVE * 2,
        &[(1, 4)],
    );
}

#[test]
fn microsoft_terminal_app_fzf_surrogate_pair_utf16_gap_boundary() {
    assert_match(
        "𠀋😀",
        "N𠀋wer 😀b𐐷 ",
        SCORE_MATCH * 2 + SCORE_GAP_START + SCORE_GAP_EXTENSION * 3 + BONUS_BOUNDARY,
        &[(1, 2), (7, 8)],
    );
}
