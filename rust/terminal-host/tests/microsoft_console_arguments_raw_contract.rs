use terminal_host::raw_console_arguments::parse_raw_console_arguments;

#[test]
fn microsoft_console_arguments_backslash_whitespace_vectors() {
    let parsed = parse_raw_console_arguments(
        "conhost.exe --headless\\ foo\\ --outpipe\\ bar\\ this\\ is\\ the\\ commandline",
    )
    .expect("Microsoft backslash-space vector parses");
    assert!(!parsed.headless());
    assert_eq!(
        parsed.client_commandline,
        "--headless\\ foo\\ --outpipe\\ bar\\ this\\ is\\ the\\ commandline"
    );

    let parsed = parse_raw_console_arguments(
        "conhost.exe --headless\\\tfoo\\\t--outpipe\\\tbar\\\tthis\\\tis\\\tthe\\\tcommandline",
    )
    .expect("Microsoft backslash-tab vector parses");
    assert!(!parsed.headless());
    assert_eq!(
        parsed.client_commandline,
        "--headless\\ foo\\ --outpipe\\ bar\\ this\\ is\\ the\\ commandline"
    );
}
