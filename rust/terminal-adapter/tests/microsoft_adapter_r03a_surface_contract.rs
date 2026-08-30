use terminal_adapter::adapt_dispatch::{AdaptDispatchCore, PageGeometry};
use terminal_parser::output_engine::{
    DcsAction, DeviceAttributesKind, OutputAction, OutputStateMachineEngine, TermDispatch,
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

fn parse(text: &str) -> AdaptDispatchCore {
    let engine = OutputStateMachineEngine::new(core());
    let mut machine = StateMachine::new(engine);
    machine.process_str(text);
    machine.engine().dispatch().clone()
}

#[test]
fn microsoft_adapter_cursor_hide_show_preserves_dectcem_boundary_action() {
    for enabled in [false, true] {
        assert_deferred(OutputAction::SetMode {
            private: true,
            enabled,
            mode: 25,
        });
    }
}

#[test]
fn microsoft_adapter_graphics_base_preserves_sgr_reset_boundary_action() {
    assert_deferred(OutputAction::SetGraphicsRendition(Parameters::default()));
}

#[test]
fn microsoft_adapter_graphics_single_preserves_single_sgr_parameter_boundary_action() {
    // Exact data source from AdapterTest::GraphicsSingleTests. This witness deliberately
    // proves the complete parser/adapter boundary matrix, not TextAttribute application.
    for parameter in [
        0, 1, 2, 4, 7, 8, 9, 21, 22, 24, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 39, 40, 41,
        42, 43, 44, 45, 46, 47, 49, 53, 55, 90, 91, 92, 93, 94, 95, 96, 97, 100, 101, 102, 103,
        104, 105, 106, 107,
    ] {
        assert_deferred(OutputAction::SetGraphicsRendition(Parameters::from_values(
            vec![Some(parameter)],
        )));
    }
}

#[test]
fn microsoft_adapter_graphics_single_with_subparams_preserves_parser_shape() {
    // Exact four data-driven vectors from AdapterTest::GraphicsSingleWithSubParamTests:
    // curly underline and indexed foreground/background/underline colors.
    let cases = [
        ("\u{1b}[4:3m", 4, vec![Some(3)]),
        ("\u{1b}[38:5:1m", 38, vec![Some(5), Some(1)]),
        ("\u{1b}[48:5:15m", 48, vec![Some(5), Some(15)]),
        ("\u{1b}[58:5:1m", 58, vec![Some(5), Some(1)]),
    ];

    for (sequence, expected_parameter, expected_subparams) in cases {
        let dispatch = parse(sequence);
        let actions = dispatch.deferred_actions();
        assert_eq!(actions.len(), 1, "sequence={sequence:?}");
        let OutputAction::SetGraphicsRendition(parameters) = &actions[0] else {
            panic!("expected SGR action for {sequence:?}");
        };
        assert_eq!(parameters.at(0), Some(expected_parameter));
        assert_eq!(parameters.sub_params_for(0), expected_subparams.as_slice());
    }
}

#[test]
fn microsoft_adapter_graphics_push_pop_preserves_stack_boundary_actions_in_order() {
    // AdapterTest::GraphicsPushPopTests exercises empty stacks, nesting, a partial
    // attribute save (intense/background/double underline), and underline-only saves.
    // Rust does not yet apply the TextAttribute stack, but every stack action shape is
    // retained in the same source order at the adapter boundary.
    let empty = Parameters::default();
    let partial = Parameters::from_values(vec![Some(1), Some(10), Some(21)]);
    let underline_only = Parameters::from_values(vec![Some(4)]);

    let expected = vec![
        OutputAction::PushGraphicsRendition(empty.clone()),
        OutputAction::PopGraphicsRendition,
        OutputAction::PushGraphicsRendition(empty.clone()),
        OutputAction::PopGraphicsRendition,
        OutputAction::PushGraphicsRendition(empty.clone()),
        OutputAction::PushGraphicsRendition(empty),
        OutputAction::PopGraphicsRendition,
        OutputAction::PopGraphicsRendition,
        OutputAction::PushGraphicsRendition(partial),
        OutputAction::PopGraphicsRendition,
        OutputAction::PushGraphicsRendition(underline_only.clone()),
        OutputAction::PopGraphicsRendition,
        OutputAction::PushGraphicsRendition(underline_only.clone()),
        OutputAction::PopGraphicsRendition,
        OutputAction::PushGraphicsRendition(underline_only),
        OutputAction::PopGraphicsRendition,
    ];

    let mut dispatch = core();
    for action in expected.clone() {
        dispatch.dispatch(action);
    }
    assert_eq!(dispatch.take_deferred_actions(), expected);
}

#[test]
fn microsoft_adapter_graphics_persist_brightness_preserves_sgr_ordering_boundary() {
    // Full SGR command traces from AdapterTest::GraphicsPersistBrightnessTests.
    // The downstream intensity mutation remains outside AdaptDispatchCore, so this is
    // exhaustive boundary evidence while the Microsoft source contract stays Partial.
    let parameters = [
        0, 34, 1, 32, // reset, dark blue, intense, dark green
        0, 94, 34, // reset, bright blue, dark blue
        0, 34, 1, 94, 34, 32, // reset, dark blue, intense, bright blue, dark blue, dark green
    ];
    let expected = parameters
        .into_iter()
        .map(|parameter| {
            OutputAction::SetGraphicsRendition(Parameters::from_values(vec![Some(parameter)]))
        })
        .collect::<Vec<_>>();

    let mut dispatch = core();
    for action in expected.clone() {
        dispatch.dispatch(action);
    }
    assert_eq!(dispatch.take_deferred_actions(), expected);
}

#[test]
fn microsoft_adapter_device_status_operating_status_preserves_dsr_boundary() {
    assert_deferred(OutputAction::DeviceStatusReport {
        private: false,
        status: 5,
        id: None,
    });
}

#[test]
fn microsoft_adapter_device_status_cursor_position_preserves_cpr_boundary() {
    assert_deferred(OutputAction::DeviceStatusReport {
        private: false,
        status: 6,
        id: None,
    });
}

#[test]
fn microsoft_adapter_device_status_extended_cursor_position_preserves_decxcpr_boundary() {
    assert_deferred(OutputAction::DeviceStatusReport {
        private: true,
        status: 6,
        id: None,
    });
}

#[test]
fn microsoft_adapter_device_status_macro_space_preserves_private_62_boundary() {
    assert_deferred(OutputAction::DeviceStatusReport {
        private: true,
        status: 62,
        id: None,
    });
}

#[test]
fn microsoft_adapter_device_status_memory_checksum_preserves_private_63_and_id_boundary() {
    assert_deferred(OutputAction::DeviceStatusReport {
        private: true,
        status: 63,
        id: Some(56),
    });
}

#[test]
fn microsoft_adapter_device_status_private_status_preserves_all_microsoft_status_codes() {
    let mut dispatch = core();
    let statuses = [15, 25, 26, 55, 56, 75, 85];
    let expected = statuses
        .into_iter()
        .map(|status| OutputAction::DeviceStatusReport {
            private: true,
            status,
            id: None,
        })
        .collect::<Vec<_>>();
    for action in expected.clone() {
        dispatch.dispatch(action);
    }
    assert_eq!(dispatch.take_deferred_actions(), expected);
}

#[test]
fn microsoft_adapter_primary_device_attributes_preserves_primary_da_boundary() {
    assert_deferred(OutputAction::DeviceAttributes(
        DeviceAttributesKind::Primary,
    ));
}

#[test]
fn microsoft_adapter_secondary_device_attributes_preserves_secondary_da_boundary() {
    assert_deferred(OutputAction::DeviceAttributes(
        DeviceAttributesKind::Secondary,
    ));
}

#[test]
fn microsoft_adapter_tertiary_device_attributes_preserves_tertiary_da_boundary() {
    assert_deferred(OutputAction::DeviceAttributes(
        DeviceAttributesKind::Tertiary,
    ));
}

#[test]
fn microsoft_adapter_request_displayed_extent_preserves_decrqde_boundary() {
    assert_deferred(OutputAction::RequestDisplayedExtent);
}

#[test]
fn microsoft_adapter_request_terminal_parameters_preserves_permission_parameter() {
    for permission in [0, 1] {
        assert_deferred(OutputAction::RequestTerminalParameters(permission));
    }
}

#[test]
fn microsoft_adapter_request_settings_preserves_decrqss_dcs_boundary() {
    assert_deferred(OutputAction::DcsBegin(DcsAction::RequestSetting));
}

#[test]
fn microsoft_adapter_request_standard_mode_preserves_decrqm_boundary() {
    for mode in [4, 20] {
        assert_deferred(OutputAction::RequestMode {
            private: false,
            mode,
        });
    }
}

#[test]
fn microsoft_adapter_request_private_mode_preserves_dec_private_decrqm_boundary() {
    for mode in [
        1, 3, 5, 6, 7, 8, 12, 25, 40, 66, 67, 69, 117, 1000, 1002, 1003, 1004, 1005, 1006, 1007,
        1049, 2004, 9001,
    ] {
        assert_deferred(OutputAction::RequestMode {
            private: true,
            mode,
        });
    }
}

#[test]
fn microsoft_adapter_request_permanent_mode_preserves_2027_boundary() {
    assert_deferred(OutputAction::RequestMode {
        private: true,
        mode: 2027,
    });
}

#[test]
fn microsoft_adapter_request_checksum_report_preserves_decrqcra_advanced_csi_boundary() {
    assert_deferred(OutputAction::AdvancedCsi {
        id: VtId::from_ascii("*y"),
        parameters: Parameters::from_values(vec![
            Some(7),
            Some(1),
            Some(1),
            Some(1),
            Some(1),
            Some(1),
        ]),
    });
}

#[test]
fn microsoft_adapter_color_table_report_preserves_terminal_state_report_boundary() {
    for color_model in [1, 2] {
        assert_deferred(OutputAction::AdvancedCsi {
            id: VtId::from_ascii("$u"),
            parameters: Parameters::from_values(vec![Some(2), Some(color_model)]),
        });
    }
}
