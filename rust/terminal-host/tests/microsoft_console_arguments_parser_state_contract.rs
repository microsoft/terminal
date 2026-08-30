use terminal_host::console_argument_parser::{
    ConsoleArgumentError, ConsoleArgumentState, parse_console_arguments,
};

fn tokens(values: &[&str]) -> Vec<String> {
    values.iter().map(|value| (*value).to_owned()).collect()
}

fn parse(values: &[&str]) -> Result<ConsoleArgumentState, ConsoleArgumentError> {
    parse_console_arguments(&tokens(values))
}

#[test]
fn microsoft_console_arguments_initial_size_contract() {
    let parsed = parse(&["--width", "120", "--height", "30"]).expect("valid dimensions");
    assert_eq!((parsed.width, parsed.height), (120, 30));

    let parsed = parse(&["--width", "120"]).expect("width only");
    assert_eq!((parsed.width, parsed.height), (120, 0));

    let parsed = parse(&["--height", "30"]).expect("height only");
    assert_eq!((parsed.width, parsed.height), (0, 30));

    assert_eq!(parse(&["--width", "0"]).expect("zero width").width, 0);
    assert_eq!(
        parse(&["--width", "-1"]).expect("minus one width").width,
        -1
    );
    assert_eq!(
        parse(&["--width", "foo"]),
        Err(ConsoleArgumentError::InvalidValue("dimension"))
    );
    assert_eq!(
        parse(&["--width", "2foo"]),
        Err(ConsoleArgumentError::InvalidValue("dimension"))
    );
    assert_eq!(
        parse(&["--width", "65535"]),
        Err(ConsoleArgumentError::InvalidValue("dimension"))
    );
}

#[test]
fn microsoft_console_arguments_headless_arg_contract() {
    let parsed = parse(&["--headless"]).expect("headless");
    assert!(parsed.headless());
    assert!(parsed.create_server_handle());

    let parsed = parse(&["--headless", "0x4"]).expect("headless with server handle");
    assert!(parsed.headless());
    assert_eq!(parsed.server_handle, Some(4));
    assert!(!parsed.create_server_handle());

    let parsed = parse(&["--headless", "--headless"]).expect("duplicate headless");
    assert!(parsed.headless());

    let parsed = parse(&["--", "foo.exe", "--headless"]).expect("client headless argument");
    assert!(!parsed.headless());
    assert_eq!(parsed.client_commandline, "foo.exe --headless");
}

#[test]
fn microsoft_console_arguments_signal_handle_contract() {
    let parsed = parse(&["--server", "0x4", "--signal", "0x8"]).expect("server and signal");
    assert_eq!(parsed.server_handle, Some(4));
    assert_eq!(parsed.signal_handle, Some(8));
    assert!(!parsed.create_server_handle());

    assert_eq!(
        parse(&["--server", "0x4", "--signal", "ASDF"]),
        Err(ConsoleArgumentError::InvalidValue("handle"))
    );
    assert_eq!(
        parse(&["--signal", "--server", "0x4"]),
        Err(ConsoleArgumentError::InvalidValue("handle"))
    );
}

#[test]
fn microsoft_console_arguments_feature_arg_contract() {
    parse(&["--feature", "pty"]).expect("supported feature");
    assert_eq!(
        parse(&["--feature", "tty"]),
        Err(ConsoleArgumentError::InvalidValue("feature"))
    );
    parse(&["--feature", "pty", "--feature", "pty"]).expect("repeated supported feature");
    assert_eq!(
        parse(&["--feature", "pty", "--feature", "tty"]),
        Err(ConsoleArgumentError::InvalidValue("feature"))
    );
    assert_eq!(
        parse(&["--feature", "pty", "--feature"]),
        Err(ConsoleArgumentError::MissingValue("--feature"))
    );
    assert_eq!(
        parse(&["--feature", "pty", "--feature", "--signal", "foo"]),
        Err(ConsoleArgumentError::InvalidValue("feature"))
    );
}
