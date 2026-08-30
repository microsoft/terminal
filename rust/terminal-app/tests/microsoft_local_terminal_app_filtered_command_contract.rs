use terminal_app::{FilteredCommand, TextRun, parse_pattern};

#[test]
fn microsoft_local_terminal_app_filtered_command_verify_highlighting_contract() {
    let mut command = FilteredCommand::new("AAAAAABBBBBBCCC");
    assert_eq!(command.name_highlights(), None);

    let empty = parse_pattern("");
    command.update_filter(Some(&empty));
    assert_eq!(command.name_highlights(), None);

    for (filter, expected) in [
        ("AAAAAABBBBBBCCC", vec![TextRun { start: 0, end: 14 }]),
        ("A", vec![TextRun { start: 0, end: 0 }]),
        ("a", vec![TextRun { start: 0, end: 0 }]),
        ("ab", vec![TextRun { start: 5, end: 6 }]),
        (
            "abcc",
            vec![TextRun { start: 5, end: 6 }, TextRun { start: 12, end: 13 }],
        ),
    ] {
        let pattern = parse_pattern(filter);
        command.update_filter(Some(&pattern));
        assert_eq!(command.name_highlights(), Some(expected.as_slice()));
    }

    let missing = parse_pattern("abcd");
    command.update_filter(Some(&missing));
    assert_eq!(command.name_highlights(), None);
}

#[test]
fn microsoft_local_terminal_app_filtered_command_verify_weight_contract() {
    let mut command = FilteredCommand::new("AAAAAABBBBBBCCC");
    command.update_filter(None);
    let null = command.weight();

    let empty = parse_pattern("");
    command.update_filter(Some(&empty));
    let empty_weight = command.weight();

    let full = parse_pattern("AAAAAABBBBBBCCC");
    command.update_filter(Some(&full));
    let full_weight = command.weight();

    let first = parse_pattern("A");
    command.update_filter(Some(&first));
    let first_weight = command.weight();

    let other_case = parse_pattern("a");
    command.update_filter(Some(&other_case));
    let other_case_weight = command.weight();

    let several = parse_pattern("ab");
    command.update_filter(Some(&several));
    let several_weight = command.weight();

    assert_eq!(null, 0);
    assert_eq!(empty_weight, 0);
    assert!(full_weight > 100);
    assert!(first_weight > 0 && first_weight < full_weight);
    assert!(other_case_weight > 0 && other_case_weight < full_weight);
    assert!(several_weight > other_case_weight && several_weight < full_weight);
}

#[test]
fn microsoft_local_terminal_app_filtered_command_verify_compare_contract() {
    let mut first = FilteredCommand::new("AAAAAABBBBBBCCC");
    let mut second = FilteredCommand::new("BBBBBCCC");
    assert_eq!(first.weight(), second.weight());
    assert!(FilteredCommand::compare(&first, &second));

    let empty = parse_pattern("");
    first.update_filter(Some(&empty));
    second.update_filter(Some(&empty));
    assert_eq!(first.weight(), second.weight());
    assert!(FilteredCommand::compare(&first, &second));

    let b = parse_pattern("B");
    first.update_filter(Some(&b));
    second.update_filter(Some(&b));
    assert!(first.weight() < second.weight());
    assert!(!FilteredCommand::compare(&first, &second));
}

#[test]
fn microsoft_local_terminal_app_filtered_command_verify_compare_ignore_case_contract() {
    let first = FilteredCommand::new("a");
    let second = FilteredCommand::new("B");
    assert_eq!(first.weight(), second.weight());
    assert!(FilteredCommand::compare(&first, &second));
}
