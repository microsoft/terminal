use terminal_adapter::{adapt_dispatch::PageGeometry, macro_execution::MacroExecutingProduct};

fn product() -> MacroExecutingProduct {
    MacroExecutingProduct::new(PageGeometry::new(20, 100, 29))
}

fn define_text(product: &mut MacroExecutingProduct, id: usize, text: &str) {
    product.process_str(&format!("\u{1b}P{id};0;0!z{text}\u{1b}\\"));
}

fn define_hex(product: &mut MacroExecutingProduct, id: usize, hex: &str) {
    product.process_str(&format!("\u{1b}P{id};0;1!z{hex}\u{1b}\\"));
}

fn clear_output(product: &mut MacroExecutingProduct) {
    product
        .machine_mut()
        .engine_mut()
        .dispatch_mut()
        .clear_printed_text();
}

fn output(product: &MacroExecutingProduct) -> String {
    product.machine().engine().dispatch().printed_text()
}

#[test]
fn microsoft_macro_invokes_match_default_maximum_and_out_of_range_ids() {
    let mut product = product();
    define_text(&mut product, 0, "Macro 0");
    define_text(&mut product, 1, "Macro 1");
    define_text(&mut product, 2, "Macro 2");
    define_text(&mut product, 63, "Macro 63");

    clear_output(&mut product);
    product.process_str("\u{1b}[*z");
    assert_eq!(output(&product), "Macro 0");

    clear_output(&mut product);
    product.process_str("\u{1b}[1*z");
    assert_eq!(output(&product), "Macro 1");

    clear_output(&mut product);
    product.process_str("\u{1b}[63*z");
    assert_eq!(output(&product), "Macro 63");

    clear_output(&mut product);
    product.process_str("\u{1b}[64*z");
    assert_eq!(output(&product), "");
}

#[test]
fn microsoft_undefined_macro_is_silent_and_following_invocation_still_runs() {
    let mut product = product();
    define_text(&mut product, 1, "Macro 1");

    clear_output(&mut product);
    product.process_str("[]\u{1b}[10*z\u{1b}[1*z");
    assert_eq!(output(&product), "[]Macro 1");
}

#[test]
fn microsoft_macro_invocation_recurses_immediately_and_stops_at_depth_sixteen() {
    let mut product = product();

    // Macro 0 = < ESC [ 1 * z >
    define_hex(&mut product, 0, "3C1B5B312A7A3E");
    // Macro 1 = [ ESC [ 0 * z ]
    define_hex(&mut product, 1, "5B1B5B302A7A5D");

    clear_output(&mut product);
    product.process_str("\u{1b}[0*z");

    assert_eq!(output(&product), "<[<[<[<[<[<[<[<[]>]>]>]>]>]>]>]>");
}
