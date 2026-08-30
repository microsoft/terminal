use terminal_adapter::adapt_dispatch::{AdaptDispatchCore, PageGeometry};
use terminal_adapter::dcs_dispatch::AdapterDispatch;
use terminal_adapter::macro_buffer::InvocationContext;
use terminal_parser::output_engine::{
    DcsAction, LineFeedType, OutputAction, OutputStateMachineEngine, TermDispatch,
};
use terminal_parser::state_machine::{Parameters, StateMachine, VtId};

fn core() -> AdaptDispatchCore {
    AdaptDispatchCore::new(PageGeometry::new(20, 100, 29))
}

fn assert_deferred(action: OutputAction) {
    let mut dispatch = core();
    dispatch.dispatch(action.clone());
    assert_eq!(dispatch.take_deferred_actions(), vec![action]);
}

fn assert_deferred_sequence(actions: Vec<OutputAction>) {
    let expected = actions.clone();
    let mut dispatch = core();
    for action in actions {
        dispatch.dispatch(action);
    }
    assert_eq!(dispatch.take_deferred_actions(), expected);
}

fn parse_core(text: &str) -> AdaptDispatchCore {
    let engine = OutputStateMachineEngine::new(core());
    let mut machine = StateMachine::new(engine);
    machine.process_str(text);
    machine.engine().dispatch().clone()
}

fn assert_sgr_subparameters(sequence: &str, expected_main: i32, expected_sub: &[Option<i32>]) {
    let dispatch = parse_core(sequence);
    assert_eq!(dispatch.deferred_actions().len(), 1);
    let OutputAction::SetGraphicsRendition(parameters) = &dispatch.deferred_actions()[0] else {
        panic!("expected SGR action");
    };
    assert_eq!(parameters.at(0), Some(expected_main));
    assert_eq!(parameters.sub_params_for(0), expected_sub);
}

#[test]
fn microsoft_adapter_osc4_palette_report_preserves_query_indices() {
    assert_deferred_sequence(
        (0usize..=15)
            .map(OutputAction::RequestColorTableEntry)
            .collect(),
    );
}

#[test]
fn microsoft_adapter_xterm_color_resource_report_preserves_resource_ids() {
    assert_deferred_sequence(
        [10usize, 11, 12, 13]
            .into_iter()
            .map(OutputAction::RequestXtermColorResource)
            .collect(),
    );
}

#[test]
fn microsoft_adapter_tabulation_stop_report_preserves_decrqpsr_2_boundary() {
    assert_deferred(OutputAction::AdvancedCsi {
        id: VtId::from_ascii("$w"),
        parameters: Parameters::from_values(vec![Some(2)]),
    });
}

#[test]
fn microsoft_adapter_cursor_information_report_preserves_decrqpsr_1_boundary() {
    assert_deferred(OutputAction::AdvancedCsi {
        id: VtId::from_ascii("$w"),
        parameters: Parameters::from_values(vec![Some(1)]),
    });
}

#[test]
fn microsoft_adapter_cursor_keys_mode_preserves_decckm_set_and_reset() {
    assert_deferred_sequence(vec![
        OutputAction::SetMode {
            private: true,
            enabled: false,
            mode: 1,
        },
        OutputAction::SetMode {
            private: true,
            enabled: true,
            mode: 1,
        },
    ]);
}

#[test]
fn microsoft_adapter_keypad_mode_preserves_application_and_numeric_actions() {
    assert_deferred_sequence(vec![
        OutputAction::SetKeypadMode(false),
        OutputAction::SetKeypadMode(true),
    ]);
}

#[test]
fn microsoft_adapter_ansi_mode_preserves_decanm_set_and_reset_boundary() {
    assert_deferred_sequence(vec![
        OutputAction::SetMode {
            private: true,
            enabled: true,
            mode: 2,
        },
        OutputAction::SetMode {
            private: true,
            enabled: false,
            mode: 2,
        },
    ]);
}

#[test]
fn microsoft_adapter_allow_blinking_preserves_att610_mode_boundary() {
    assert_deferred_sequence(vec![
        OutputAction::SetMode {
            private: true,
            enabled: true,
            mode: 12,
        },
        OutputAction::SetMode {
            private: true,
            enabled: false,
            mode: 12,
        },
    ]);
}

#[test]
fn microsoft_adapter_line_feed_preserves_all_three_dispatch_types() {
    assert_deferred_sequence(vec![
        OutputAction::LineFeed(LineFeedType::WithoutReturn),
        OutputAction::LineFeed(LineFeedType::WithReturn),
        OutputAction::LineFeed(LineFeedType::DependsOnMode),
    ]);
}

#[test]
fn microsoft_adapter_console_title_preserves_nonempty_and_empty_titles() {
    assert_deferred_sequence(vec![
        OutputAction::SetWindowTitle("Foo bar".to_owned()),
        OutputAction::SetWindowTitle(String::new()),
    ]);
}

#[test]
fn microsoft_adapter_mouse_modes_preserve_all_six_input_mode_boundaries() {
    let modes = [1000, 1005, 1006, 1002, 1003, 1007];
    let mut actions = Vec::new();
    for mode in modes {
        actions.push(OutputAction::SetMode {
            private: true,
            enabled: true,
            mode,
        });
        actions.push(OutputAction::SetMode {
            private: true,
            enabled: false,
            mode,
        });
    }
    assert_deferred_sequence(actions);
}

#[test]
fn microsoft_adapter_xterm_256_color_preserves_indexed_sgr_vectors() {
    for values in [
        vec![Some(38), Some(5), Some(2)],
        vec![Some(48), Some(5), Some(9)],
        vec![Some(38), Some(5), Some(42)],
        vec![Some(48), Some(5), Some(142)],
        vec![Some(38), Some(5), Some(9)],
    ] {
        assert_deferred(OutputAction::SetGraphicsRendition(Parameters::from_values(
            values,
        )));
    }
}

#[test]
fn microsoft_adapter_extended_color_default_parameters_preserve_omissions() {
    for values in [
        vec![Some(38), Some(5)],
        vec![Some(48), Some(5), None],
        vec![Some(38), Some(2)],
        vec![Some(48), Some(2), Some(123)],
        vec![Some(38), Some(2), None, None, Some(123)],
        vec![Some(38), Some(2), Some(283), Some(182), Some(123)],
        vec![Some(38), Some(5), Some(283)],
    ] {
        assert_deferred(OutputAction::SetGraphicsRendition(Parameters::from_values(
            values,
        )));
    }
}

#[test]
fn microsoft_adapter_extended_subparameter_color_preserves_subparameter_shape() {
    for (sequence, main, subparameters) in [
        ("\u{1b}[38:5m", 38, vec![Some(5)]),
        ("\u{1b}[48:5:m", 48, vec![Some(5), None]),
        ("\u{1b}[38:2m", 38, vec![Some(2)]),
        ("\u{1b}[48:2::123m", 48, vec![Some(2), None, Some(123)]),
        (
            "\u{1b}[38:2::::123m",
            38,
            vec![Some(2), None, None, None, Some(123)],
        ),
        (
            "\u{1b}[38:2:7:182:182:123m",
            38,
            vec![Some(2), Some(7), Some(182), Some(182), Some(123)],
        ),
        (
            "\u{1b}[48:2::128:283:155m",
            48,
            vec![Some(2), None, Some(128), Some(283), Some(155)],
        ),
        ("\u{1b}[38:5:283m", 38, vec![Some(5), Some(283)]),
    ] {
        assert_sgr_subparameters(sequence, main, &subparameters);
    }
}

#[test]
fn microsoft_adapter_set_color_table_value_preserves_full_index_domain_edges() {
    for index in 0usize..256 {
        assert_deferred(OutputAction::SetColorTableEntry {
            index,
            color: 0x0003_0201,
        });
    }
}

#[test]
fn microsoft_adapter_soft_font_size_detection_preserves_decdld_parameters_boundary() {
    // Cover the source contract's matrix-size, explicit-size, font-set and usage families.
    // Computed FontBuffer cell sizes and bitmap inference remain downstream migration debt.
    for values in [
        vec![Some(5), Some(0), Some(0), Some(0)],
        vec![Some(6), Some(0), Some(1), Some(0)],
        vec![Some(7), Some(20), Some(0), Some(0)],
        vec![Some(13), Some(17), Some(0), Some(1)],
        vec![Some(9), Some(25), Some(1), Some(1)],
        vec![Some(18), Some(38), Some(0), Some(1)],
        vec![Some(0), Some(0), Some(2), Some(0)],
        vec![Some(0), Some(0), Some(3), Some(1)],
    ] {
        assert_deferred(OutputAction::DcsBegin(DcsAction::DownloadDrcs(
            Parameters::from_values(values),
        )));
    }
}

#[test]
fn microsoft_adapter_c1_parser_mode_preserves_accept_and_coding_system_boundaries() {
    assert_deferred_sequence(vec![
        OutputAction::AcceptC1Controls(true),
        OutputAction::AcceptC1Controls(false),
        OutputAction::DesignateCodingSystem(VtId::from_ascii("@").value()),
        OutputAction::DesignateCodingSystem(VtId::from_ascii("G").value()),
    ]);
}

#[test]
fn microsoft_adapter_assign_user_preference_charset_preserves_decaupss_boundary() {
    for charset_size in [0, 1] {
        assert_deferred(OutputAction::DcsBegin(
            DcsAction::AssignUserPreferenceCharset(Parameters::from_values(vec![Some(
                charset_size,
            )])),
        ));
    }
}

#[test]
fn microsoft_adapter_request_user_preference_charset_preserves_decrqupss_boundary() {
    assert_deferred(OutputAction::AdvancedCsi {
        id: VtId::from_ascii("&u"),
        parameters: Parameters::default(),
    });
}

#[test]
fn microsoft_adapter_macro_invokes_preserve_ids_bounds_and_depth_core() {
    let engine =
        OutputStateMachineEngine::new(AdapterDispatch::new(PageGeometry::new(20, 100, 29)));
    let mut machine = StateMachine::new(engine);
    machine.process_str("\u{1b}P0;0;0!zMacro 0\u{1b}\\");
    machine.process_str("\u{1b}P2;0;0!zMacro 2\u{1b}\\");
    machine.process_str("\u{1b}P63;0;0!zMacro 63\u{1b}\\");

    let buffer = machine.engine().dispatch().macro_buffer();
    for (id, expected) in [(0usize, "Macro 0"), (2, "Macro 2"), (63, "Macro 63")] {
        let prepared = buffer
            .prepare_invoke(id, InvocationContext::default())
            .expect("Microsoft macro id must be invokable");
        assert_eq!(
            prepared.sequence(),
            expected.encode_utf16().collect::<Vec<_>>()
        );
    }
    assert!(
        buffer
            .prepare_invoke(64, InvocationContext::default())
            .is_none()
    );

    let mut context = InvocationContext::default();
    for _ in 0..16 {
        let prepared = buffer
            .prepare_invoke(0, context)
            .expect("depth through sixteen must remain valid");
        context = prepared.context();
    }
    assert!(buffer.prepare_invoke(0, context).is_none());
}

#[test]
fn microsoft_adapter_window_manipulation_reports_preserve_function_codes() {
    assert_deferred_sequence(
        [18, 14, 16]
            .into_iter()
            .map(|function| OutputAction::WindowManipulation {
                function,
                parameter1: 1,
                parameter2: 1,
            })
            .collect(),
    );
}

#[test]
fn microsoft_adapter_menu_completions_preserve_vscode_action_payloads() {
    assert_deferred_sequence(vec![
        OutputAction::VsCodeAction("Completions;10;20;3".to_owned()),
        OutputAction::VsCodeAction("Completions;1;2;3;{ \"foo\": 1, \"bar\": 2 }".to_owned()),
        OutputAction::VsCodeAction(
            "Completions;10;20;30;{ \"foo\": \"what;ever\", \"bar\": 2 }".to_owned(),
        ),
    ]);
}

#[test]
fn microsoft_adapter_send_c1_control_preserves_7bit_and_8bit_boundaries() {
    assert_deferred_sequence(vec![
        OutputAction::SendC1Controls(false),
        OutputAction::SendC1Controls(true),
    ]);
}
