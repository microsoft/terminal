use terminal_host::alias::AliasStore;

const SOURCE: &str = "foo one two three four five six    seven eight nine ten eleven twelve";

#[test]
fn microsoft_alias_test_match_and_copy_contract() {
    let cases = [
        ("bar", "bar\r\n"),
        ("bar $1", "bar one\r\n"),
        ("bar $2", "bar two\r\n"),
        ("bar $3", "bar three\r\n"),
        ("bar $4", "bar four\r\n"),
        ("bar $5", "bar five\r\n"),
        ("bar $6", "bar six\r\n"),
        ("bar $7", "bar seven\r\n"),
        ("bar $8", "bar eight\r\n"),
        ("bar $9", "bar nine\r\n"),
        (
            "bar $3 $1 $4 $1 $5 $9",
            "bar three one four one five nine\r\n",
        ),
        (
            "bar $*",
            "bar one two three four five six    seven eight nine ten eleven twelve\r\n",
        ),
        ("longer", "longer\r\n"),
        ("redirect $1$goutput $2", "redirect one>output two\r\n"),
        ("REDIRECT $1$GOUTPUT $2", "REDIRECT one>OUTPUT two\r\n"),
        ("append $1$g$goutput $2", "append one>>output two\r\n"),
        ("APPEND $1$G$GOUTPUT $2", "APPEND one>>OUTPUT two\r\n"),
        (
            "redirect $1$linputfile.$2",
            "redirect one<inputfile.two\r\n",
        ),
        (
            "REDIRECT $1$LINPUTFILE.$2",
            "REDIRECT one<INPUTFILE.two\r\n",
        ),
        ("pipe $1$boutput $2", "pipe one|output two\r\n"),
        ("PIPE $1$BOUTPUT $2", "PIPE one|OUTPUT two\r\n"),
        ("run$tmultiple$tcommands", "run\r\nmultiple\r\ncommands\r\n"),
        ("MyMoney$$$$$$App", "MyMoney$$$$$$App\r\n"),
        ("Invalid$Apple", "Invalid$Apple\r\n"),
        ("IEndInA$", "IEndInA$\r\n"),
        (
            "megamix $7$Gfun $1 $b test $9 $L $2.txt$tall$$the$$things $*$tat$g$gonce.log",
            "megamix seven>fun one | test nine < two.txt\r\nall$$the$$things one two three four five six    seven eight nine ten eleven twelve\r\nat>>once.log\r\n",
        ),
    ];

    for (target, expected) in cases {
        let mut store = AliasStore::new();
        store.add("test.exe", "foo", target);
        let actual = store
            .match_and_copy(SOURCE, "TEST.EXE")
            .expect("Microsoft alias must match");
        assert_eq!(actual.text, expected, "target={target}");
        assert_eq!(
            actual.line_count,
            expected.matches("\r\n").count(),
            "target={target}"
        );
    }
}

#[test]
fn microsoft_alias_test_match_and_copy_invalid_exe_name_contract() {
    let store = AliasStore::new();
    assert!(store.match_and_copy("Source", "").is_none());
}

#[test]
fn microsoft_alias_test_match_and_copy_exe_not_found_contract() {
    let store = AliasStore::new();
    assert!(store.match_and_copy("Source", "exe.exe").is_none());
}

#[test]
fn microsoft_alias_test_match_and_copy_alias_not_found_contract() {
    let mut store = AliasStore::new();
    store.add("exe.exe", "wrongSource", "someTarget");
    assert!(store.match_and_copy("Source", "exe.exe").is_none());
}

#[test]
fn microsoft_alias_test_match_and_copy_leading_spaces_contract() {
    let mut store = AliasStore::new();
    store.add("exe.exe", "Source", "someTarget");
    assert!(store.match_and_copy(" Source", "exe.exe").is_none());
}
