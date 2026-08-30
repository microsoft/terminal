//! Safe Rust implementation of Windows Terminal's VT state machine core.
//!
//! This module ports parser behavior only. Output and input engine semantics stay
//! behind [`StateMachineEngine`] and are migrated in later R01 increments.

const ESC: u16 = 0x1b;
const BEL: u16 = 0x07;
const CAN: u16 = 0x18;
const SUB: u16 = 0x1a;
const DEL: u16 = 0x7f;

pub const MAX_PARAMETER_VALUE: i32 = 65_535;
pub const MAX_PARAMETER_COUNT: usize = 32;
pub const MAX_SUBPARAMETER_COUNT: usize = 6;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct VtId(u64);

impl VtId {
    #[must_use]
    pub const fn value(self) -> u64 {
        self.0
    }

    #[must_use]
    pub fn from_ascii(text: &str) -> Self {
        let mut value = 0u64;
        for (index, byte) in text.bytes().take(7).enumerate() {
            value |= u64::from(byte) << (index * 8);
        }
        Self(value & 0x00ff_ffff_ffff_ffff)
    }
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Parameters {
    values: Vec<Option<i32>>,
    sub_values: Vec<Option<i32>>,
    sub_ranges: Vec<(u8, u8)>,
}

impl Parameters {
    #[must_use]
    pub fn from_values(values: Vec<Option<i32>>) -> Self {
        Self {
            values,
            sub_values: Vec::new(),
            sub_ranges: Vec::new(),
        }
    }

    #[must_use]
    pub fn size(&self) -> usize {
        self.values.len().max(1)
    }

    #[must_use]
    pub fn at(&self, index: usize) -> Option<i32> {
        self.values.get(index).copied().flatten()
    }

    #[must_use]
    pub fn subspan(&self, offset: usize) -> Self {
        let offset = offset.min(self.values.len());
        let range_offset = offset.min(self.sub_ranges.len());
        Self {
            values: self.values[offset..].to_vec(),
            sub_values: self.sub_values.clone(),
            sub_ranges: self.sub_ranges[range_offset..].to_vec(),
        }
    }

    #[must_use]
    pub fn sub_params_for(&self, index: usize) -> &[Option<i32>] {
        let Some(&(start, end)) = self.sub_ranges.get(index) else {
            return &[];
        };
        &self.sub_values[usize::from(start)..usize::from(end)]
    }

    #[must_use]
    pub fn values(&self) -> &[Option<i32>] {
        &self.values
    }
}

pub trait StateMachineEngine {
    fn unknown_sequence(&mut self) {}

    fn encountered_win32_input_mode_sequence(&self) -> bool {
        false
    }

    fn action_execute(&mut self, _code_unit: u16) -> bool {
        true
    }

    fn action_execute_from_escape(&mut self, _code_unit: u16) -> bool {
        true
    }

    fn action_print(&mut self, _code_unit: u16) -> bool {
        true
    }

    fn action_print_string(&mut self, _text: &[u16]) -> bool {
        true
    }

    fn action_pass_through_string(&mut self, _text: &[u16]) -> bool {
        true
    }

    fn action_esc_dispatch(&mut self, _id: VtId) -> bool {
        true
    }

    fn action_vt52_esc_dispatch(&mut self, _id: VtId, _parameters: &Parameters) -> bool {
        true
    }

    fn action_csi_dispatch(&mut self, _id: VtId, _parameters: &Parameters) -> bool {
        true
    }

    fn action_osc_dispatch(&mut self, _parameter: i32, _text: &[u16]) -> bool {
        true
    }

    fn action_ss3_dispatch(&mut self, _code_unit: u16, _parameters: &Parameters) -> bool {
        true
    }

    /// Starts a DCS data string. Returning `true` means subsequent data should
    /// be delivered through [`StateMachineEngine::action_dcs_put`].
    fn action_dcs_dispatch(&mut self, _id: VtId, _parameters: &Parameters) -> bool {
        false
    }

    fn action_dcs_put(&mut self, _code_unit: u16) -> bool {
        true
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ParserMode {
    AcceptC1,
    Ansi,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum State {
    Ground,
    Escape,
    EscapeIntermediate,
    CsiEntry,
    CsiIntermediate,
    CsiIgnore,
    CsiParam,
    CsiSubParam,
    OscParam,
    OscString,
    OscTermination,
    Ss3Entry,
    Ss3Param,
    Vt52Param,
    DcsEntry,
    DcsIgnore,
    DcsIntermediate,
    DcsParam,
    DcsPassThrough,
    SosPmApcString,
}

#[derive(Debug, Clone, Copy)]
struct ParserConfig {
    input_engine: bool,
    accept_c1: bool,
    ansi: bool,
}

#[derive(Debug, Clone, Copy, Default)]
struct OverflowState {
    parameter_limit: bool,
    sub_parameter_limit: bool,
}

#[derive(Debug, Clone, Copy, Default)]
struct RuntimeState {
    dcs_handler_active: bool,
    processing_last_character: bool,
}

#[derive(Debug, Clone, Copy, Default)]
struct VtIdBuilder {
    accumulator: u64,
    shift: usize,
}

impl VtIdBuilder {
    fn clear(&mut self) {
        self.accumulator = 0;
        self.shift = 0;
    }

    fn add_intermediate(&mut self, code_unit: u16) {
        if self.shift + 16 >= u64::BITS as usize {
            self.accumulator = 0;
        } else {
            self.accumulator += u64::from(code_unit) << self.shift;
            self.shift += 8;
        }
    }

    fn finalize(self, final_code_unit: u16) -> VtId {
        VtId(
            (self.accumulator + (u64::from(final_code_unit) << self.shift)) & 0x00ff_ffff_ffff_ffff,
        )
    }
}

pub struct StateMachine<E: StateMachineEngine> {
    engine: E,
    config: ParserConfig,
    state: State,
    identifier: VtIdBuilder,
    parameters: Vec<Option<i32>>,
    overflow: OverflowState,
    sub_parameters: Vec<Option<i32>>,
    sub_parameter_ranges: Vec<(u8, u8)>,
    sub_parameter_counter: usize,
    osc_string: Vec<u16>,
    osc_parameter: i32,
    runtime: RuntimeState,
    sequence_buffer: Vec<u16>,
    on_csi_complete: Option<Box<dyn FnMut()>>,
}

impl<E: StateMachineEngine> StateMachine<E> {
    #[must_use]
    pub fn new(engine: E) -> Self {
        Self::with_input_engine(engine, false)
    }

    #[must_use]
    pub fn new_input(engine: E) -> Self {
        Self::with_input_engine(engine, true)
    }

    fn with_input_engine(engine: E, is_engine_for_input: bool) -> Self {
        let mut machine = Self {
            engine,
            config: ParserConfig {
                input_engine: is_engine_for_input,
                accept_c1: is_engine_for_input,
                ansi: true,
            },
            state: State::Ground,
            identifier: VtIdBuilder::default(),
            parameters: Vec::new(),
            overflow: OverflowState::default(),
            sub_parameters: Vec::new(),
            sub_parameter_ranges: Vec::new(),
            sub_parameter_counter: 0,
            osc_string: Vec::new(),
            osc_parameter: 0,
            runtime: RuntimeState::default(),
            sequence_buffer: Vec::new(),
            on_csi_complete: None,
        };
        machine.action_clear();
        machine
    }

    #[must_use]
    pub const fn state(&self) -> State {
        self.state
    }

    #[must_use]
    pub const fn is_processing_last_character(&self) -> bool {
        self.runtime.processing_last_character
    }

    #[must_use]
    pub const fn engine(&self) -> &E {
        &self.engine
    }

    pub fn engine_mut(&mut self) -> &mut E {
        &mut self.engine
    }

    pub fn set_parser_mode(&mut self, mode: ParserMode, enabled: bool) {
        match mode {
            ParserMode::AcceptC1 => self.config.accept_c1 = enabled,
            ParserMode::Ansi => self.config.ansi = enabled,
        }
    }

    #[must_use]
    pub const fn get_parser_mode(&self, mode: ParserMode) -> bool {
        match mode {
            ParserMode::AcceptC1 => self.config.accept_c1,
            ParserMode::Ansi => self.config.ansi,
        }
    }

    pub fn on_csi_complete(&mut self, callback: impl FnMut() + 'static) {
        self.on_csi_complete = Some(Box::new(callback));
    }

    pub fn reset_state(&mut self) {
        self.enter_ground();
    }

    pub fn process_str(&mut self, text: &str) {
        let units = text.encode_utf16().collect::<Vec<_>>();
        self.process_utf16(&units);
    }

    pub fn process_utf16(&mut self, text: &[u16]) {
        let mut index = 0usize;

        while index < text.len() {
            if self.state == State::Ground {
                let run_start = index;
                while index < text.len() && !is_actionable_control(text[index]) {
                    index += 1;
                }
                if index > run_start {
                    let _ = self.engine.action_print_string(&text[run_start..index]);
                }
                if index >= text.len() {
                    break;
                }
            }

            self.runtime.processing_last_character = index + 1 >= text.len();
            self.process_code_unit(text[index]);
            index += 1;
        }
    }

    pub fn process_code_unit(&mut self, code_unit: u16) {
        let from_anywhere = code_unit == CAN || code_unit == SUB;

        if from_anywhere && !(self.state == State::Escape && self.config.input_engine) {
            self.action_interrupt();
            let _ = self.engine.action_execute(code_unit);
            self.enter_ground();
            return;
        }

        if is_c1_control(code_unit) {
            if self.config.accept_c1 {
                self.process_code_unit(ESC);
                self.process_code_unit(code_unit - 0x40);
            }
            return;
        }

        if code_unit == ESC && !matches!(self.state, State::OscString | State::OscParam) {
            let preserve_sequence =
                self.state == State::DcsIgnore && !self.sequence_buffer.is_empty();
            self.action_interrupt();
            if preserve_sequence {
                self.enter_escape_preserving_sequence();
            } else {
                self.enter_escape();
            }
            return;
        }

        if self.state != State::Ground {
            self.sequence_buffer.push(code_unit);
        }

        match self.state {
            State::Ground => self.event_ground(code_unit),
            State::Escape => self.event_escape(code_unit),
            State::EscapeIntermediate => self.event_escape_intermediate(code_unit),
            State::CsiEntry => self.event_csi_entry(code_unit),
            State::CsiIntermediate => self.event_csi_intermediate(code_unit),
            State::CsiIgnore => self.event_csi_ignore(code_unit),
            State::CsiParam => self.event_csi_param(code_unit),
            State::CsiSubParam => self.event_csi_sub_param(code_unit),
            State::OscParam => self.event_osc_param(code_unit),
            State::OscString => self.event_osc_string(code_unit),
            State::OscTermination => self.event_osc_termination(code_unit),
            State::Ss3Entry => self.event_ss3_entry(code_unit),
            State::Ss3Param => self.event_ss3_param(code_unit),
            State::Vt52Param => self.event_vt52_param(code_unit),
            State::DcsIgnore | State::SosPmApcString => {}
            State::DcsEntry => self.event_dcs_entry(code_unit),
            State::DcsIntermediate => self.event_dcs_intermediate(code_unit),
            State::DcsParam => self.event_dcs_param(code_unit),
            State::DcsPassThrough => self.event_dcs_pass_through(code_unit),
        }
    }

    pub fn flush_to_terminal(&mut self) -> bool {
        if self.sequence_buffer.is_empty() {
            return true;
        }
        let sequence = self.sequence_buffer.clone();
        self.engine.action_pass_through_string(&sequence)
    }

    fn snapshot_parameters(&self) -> Parameters {
        Parameters {
            values: self.parameters.clone(),
            sub_values: self.sub_parameters.clone(),
            sub_ranges: self.sub_parameter_ranges.clone(),
        }
    }

    fn action_clear(&mut self) {
        self.identifier.clear();
        self.parameters.clear();
        self.overflow.parameter_limit = false;
        self.sub_parameters.clear();
        self.sub_parameter_ranges.clear();
        self.sub_parameter_counter = 0;
        self.overflow.sub_parameter_limit = false;
        self.osc_string.clear();
        self.osc_parameter = 0;
        self.runtime.dcs_handler_active = false;
    }

    fn action_collect(&mut self, code_unit: u16) {
        self.identifier.add_intermediate(code_unit);
    }

    fn action_param(&mut self, code_unit: u16) {
        if self.overflow.parameter_limit {
            return;
        }

        if self.parameters.is_empty() {
            self.parameters.push(None);
            let start = u8::try_from(self.sub_parameters.len()).unwrap_or(u8::MAX);
            self.sub_parameter_ranges.push((start, start));
        }

        if code_unit == u16::from(b';') {
            if self.parameters.len() >= MAX_PARAMETER_COUNT {
                self.overflow.parameter_limit = true;
            } else {
                self.parameters.push(None);
                self.sub_parameter_counter = 0;
                self.overflow.sub_parameter_limit = false;
                let start = u8::try_from(self.sub_parameters.len()).unwrap_or(u8::MAX);
                self.sub_parameter_ranges.push((start, start));
            }
            return;
        }

        let current = self.parameters.last().copied().flatten().unwrap_or(0);
        *self.parameters.last_mut().expect("parameter exists") =
            Some(accumulate(current, code_unit));
    }

    fn action_sub_param(&mut self, code_unit: u16) {
        if self.overflow.sub_parameter_limit {
            return;
        }

        if self.parameters.is_empty() {
            self.parameters.push(None);
            let start = u8::try_from(self.sub_parameters.len()).unwrap_or(u8::MAX);
            self.sub_parameter_ranges.push((start, start));
        }

        if code_unit == u16::from(b':') {
            if self.sub_parameter_counter >= MAX_SUBPARAMETER_COUNT {
                self.overflow.sub_parameter_limit = true;
            } else {
                self.sub_parameters.push(None);
                if let Some((_, end)) = self.sub_parameter_ranges.last_mut() {
                    *end = end.saturating_add(1);
                }
                self.sub_parameter_counter += 1;
            }
            return;
        }

        if self.sub_parameters.is_empty() {
            self.sub_parameters.push(None);
            if let Some((_, end)) = self.sub_parameter_ranges.last_mut() {
                *end = end.saturating_add(1);
            }
        }
        let current = self.sub_parameters.last().copied().flatten().unwrap_or(0);
        *self
            .sub_parameters
            .last_mut()
            .expect("sub parameter exists") = Some(accumulate(current, code_unit));
    }

    fn action_csi_dispatch(&mut self, code_unit: u16) {
        let id = self.identifier.finalize(code_unit);
        let parameters = self.snapshot_parameters();
        if !self.engine.action_csi_dispatch(id, &parameters) {
            let _ = self.flush_to_terminal();
        }
    }

    fn action_esc_dispatch(&mut self, code_unit: u16) {
        let id = self.identifier.finalize(code_unit);
        if !self.engine.action_esc_dispatch(id) {
            let _ = self.flush_to_terminal();
        }
    }

    fn action_vt52_dispatch(&mut self, code_unit: u16) {
        let id = self.identifier.finalize(code_unit);
        let parameters = self.snapshot_parameters();
        if !self.engine.action_vt52_esc_dispatch(id, &parameters) {
            let _ = self.flush_to_terminal();
        }
    }

    fn action_osc_dispatch(&mut self) {
        let text = self.osc_string.clone();
        if !self.engine.action_osc_dispatch(self.osc_parameter, &text) {
            let _ = self.flush_to_terminal();
        }
    }

    fn action_ss3_dispatch(&mut self, code_unit: u16) {
        let parameters = self.snapshot_parameters();
        if !self.engine.action_ss3_dispatch(code_unit, &parameters) {
            let _ = self.flush_to_terminal();
        }
    }

    fn action_dcs_dispatch(&mut self, code_unit: u16) {
        let id = self.identifier.finalize(code_unit);
        let parameters = self.snapshot_parameters();
        self.runtime.dcs_handler_active = self.engine.action_dcs_dispatch(id, &parameters);
        if self.runtime.dcs_handler_active {
            self.state = State::DcsPassThrough;
            self.sequence_buffer.clear();
        } else {
            self.state = State::DcsIgnore;
        }
    }

    fn action_interrupt(&mut self) {
        if self.state == State::DcsPassThrough && self.runtime.dcs_handler_active {
            let _ = self.engine.action_dcs_put(ESC);
            self.runtime.dcs_handler_active = false;
        }
    }

    fn enter_ground(&mut self) {
        self.state = State::Ground;
        self.sequence_buffer.clear();
    }

    fn enter_escape(&mut self) {
        self.state = State::Escape;
        self.action_clear();
        self.sequence_buffer.clear();
        self.sequence_buffer.push(ESC);
    }

    fn enter_escape_preserving_sequence(&mut self) {
        self.state = State::Escape;
        self.action_clear();
        self.sequence_buffer.push(ESC);
    }

    fn enter_csi_entry(&mut self) {
        self.state = State::CsiEntry;
        self.action_clear();
    }

    fn execute_csi_complete_callback(&mut self) {
        if let Some(mut callback) = self.on_csi_complete.take() {
            callback();
        }
    }

    fn event_ground(&mut self, code_unit: u16) {
        if is_c0(code_unit) || code_unit == DEL {
            let _ = self.engine.action_execute(code_unit);
        } else {
            let _ = self.engine.action_print(code_unit);
        }
    }

    fn event_escape(&mut self, code_unit: u16) {
        if is_c0(code_unit) || (self.config.input_engine && matches!(code_unit, CAN | SUB)) {
            if self.config.input_engine {
                if !self.engine.action_execute_from_escape(code_unit) {
                    let _ = self.flush_to_terminal();
                }
                self.enter_ground();
            } else {
                let _ = self.engine.action_execute(code_unit);
            }
        } else if code_unit == DEL {
            if self.config.input_engine {
                self.action_esc_dispatch(code_unit);
                self.enter_ground();
            }
        } else if is_intermediate(code_unit) {
            if self.config.input_engine {
                self.action_esc_dispatch(code_unit);
                self.enter_ground();
            } else {
                self.action_collect(code_unit);
                self.state = State::EscapeIntermediate;
            }
        } else if self.config.ansi {
            match code_unit {
                value if value == u16::from(b'[') => self.enter_csi_entry(),
                value if value == u16::from(b']') => self.state = State::OscParam,
                value if value == u16::from(b'O') && self.config.input_engine => {
                    self.state = State::Ss3Entry;
                    self.action_clear();
                }
                value if value == u16::from(b'P') => {
                    self.state = State::DcsEntry;
                    self.action_clear();
                }
                value
                    if value == u16::from(b'X')
                        || value == u16::from(b'^')
                        || value == u16::from(b'_') =>
                {
                    self.state = State::SosPmApcString;
                    self.sequence_buffer.clear();
                    self.engine.unknown_sequence();
                }
                _ => {
                    self.action_esc_dispatch(code_unit);
                    self.enter_ground();
                }
            }
        } else if code_unit == u16::from(b'Y') {
            self.state = State::Vt52Param;
        } else {
            self.action_vt52_dispatch(code_unit);
            self.enter_ground();
        }
    }

    fn event_escape_intermediate(&mut self, code_unit: u16) {
        if is_c0(code_unit) {
            let _ = self.engine.action_execute(code_unit);
        } else if is_intermediate(code_unit) {
            self.action_collect(code_unit);
        } else if code_unit == DEL {
        } else if self.config.ansi {
            self.action_esc_dispatch(code_unit);
            self.enter_ground();
        } else if code_unit == u16::from(b'Y') {
            self.state = State::Vt52Param;
        } else {
            self.action_vt52_dispatch(code_unit);
            self.enter_ground();
        }
    }

    fn event_csi_entry(&mut self, code_unit: u16) {
        if is_c0(code_unit) {
            let _ = self.engine.action_execute(code_unit);
        } else if code_unit == DEL {
        } else if is_intermediate(code_unit) {
            self.action_collect(code_unit);
            self.state = State::CsiIntermediate;
        } else if is_numeric(code_unit) || code_unit == u16::from(b';') {
            self.action_param(code_unit);
            self.state = State::CsiParam;
        } else if code_unit == u16::from(b':') {
            self.action_sub_param(code_unit);
            self.state = State::CsiSubParam;
        } else if is_private_marker(code_unit) {
            self.action_collect(code_unit);
            self.state = State::CsiParam;
        } else {
            self.action_csi_dispatch(code_unit);
            self.enter_ground();
            self.execute_csi_complete_callback();
        }
    }

    fn event_csi_intermediate(&mut self, code_unit: u16) {
        if is_c0(code_unit) {
            let _ = self.engine.action_execute(code_unit);
        } else if is_intermediate(code_unit) {
            self.action_collect(code_unit);
        } else if code_unit == DEL {
        } else if is_intermediate_invalid(code_unit) {
            self.state = State::CsiIgnore;
        } else {
            self.action_csi_dispatch(code_unit);
            self.enter_ground();
            self.execute_csi_complete_callback();
        }
    }

    fn event_csi_ignore(&mut self, code_unit: u16) {
        if is_c0(code_unit) {
            let _ = self.engine.action_execute(code_unit);
        } else if code_unit == DEL
            || is_intermediate(code_unit)
            || is_intermediate_invalid(code_unit)
        {
        } else {
            self.enter_ground();
        }
    }

    fn event_csi_param(&mut self, code_unit: u16) {
        if is_c0(code_unit) {
            let _ = self.engine.action_execute(code_unit);
        } else if code_unit == DEL {
        } else if is_numeric(code_unit) || code_unit == u16::from(b';') {
            self.action_param(code_unit);
        } else if code_unit == u16::from(b':') {
            self.action_sub_param(code_unit);
            self.state = State::CsiSubParam;
        } else if is_intermediate(code_unit) {
            self.action_collect(code_unit);
            self.state = State::CsiIntermediate;
        } else if is_private_marker(code_unit) {
            self.state = State::CsiIgnore;
        } else {
            self.action_csi_dispatch(code_unit);
            self.enter_ground();
            self.execute_csi_complete_callback();
        }
    }

    fn event_csi_sub_param(&mut self, code_unit: u16) {
        if is_c0(code_unit) {
            let _ = self.engine.action_execute(code_unit);
        } else if code_unit == DEL {
        } else if is_numeric(code_unit) || code_unit == u16::from(b':') {
            self.action_sub_param(code_unit);
        } else if code_unit == u16::from(b';') {
            self.action_param(code_unit);
            self.state = State::CsiParam;
        } else if is_intermediate(code_unit) {
            self.action_collect(code_unit);
            self.state = State::CsiIntermediate;
        } else if is_private_marker(code_unit) {
            self.state = State::CsiIgnore;
        } else {
            self.action_csi_dispatch(code_unit);
            self.enter_ground();
            self.execute_csi_complete_callback();
        }
    }

    fn event_osc_param(&mut self, code_unit: u16) {
        if code_unit == BEL {
            self.action_osc_dispatch();
            self.enter_ground();
        } else if code_unit == ESC {
            self.state = State::OscTermination;
        } else if is_numeric(code_unit) {
            self.osc_parameter = accumulate(self.osc_parameter, code_unit);
        } else if code_unit == u16::from(b';') {
            self.state = State::OscString;
        }
    }

    fn event_osc_string(&mut self, code_unit: u16) {
        if code_unit == BEL {
            self.action_osc_dispatch();
            self.enter_ground();
        } else if code_unit == ESC {
            self.state = State::OscTermination;
        } else if !is_osc_invalid(code_unit) {
            self.osc_string.push(code_unit);
        }
    }

    fn event_osc_termination(&mut self, code_unit: u16) {
        if code_unit == u16::from(b'\\') {
            self.action_osc_dispatch();
            self.enter_ground();
        } else {
            self.enter_escape();
            self.sequence_buffer.push(code_unit);
            self.event_escape(code_unit);
        }
    }

    fn event_ss3_entry(&mut self, code_unit: u16) {
        if is_c0(code_unit) {
            let _ = self.engine.action_execute(code_unit);
        } else if code_unit == DEL {
        } else if code_unit == u16::from(b':') {
            self.state = State::CsiIgnore;
        } else if is_numeric(code_unit) || code_unit == u16::from(b';') {
            self.action_param(code_unit);
            self.state = State::Ss3Param;
        } else {
            self.action_ss3_dispatch(code_unit);
            self.enter_ground();
        }
    }

    fn event_ss3_param(&mut self, code_unit: u16) {
        if is_c0(code_unit) {
            let _ = self.engine.action_execute(code_unit);
        } else if code_unit == DEL {
        } else if is_numeric(code_unit) || code_unit == u16::from(b';') {
            self.action_param(code_unit);
        } else if is_private_marker(code_unit) || code_unit == u16::from(b':') {
            self.state = State::CsiIgnore;
        } else {
            self.action_ss3_dispatch(code_unit);
            self.enter_ground();
        }
    }

    fn event_vt52_param(&mut self, code_unit: u16) {
        if is_c0(code_unit) {
            let _ = self.engine.action_execute(code_unit);
        } else if code_unit == DEL {
        } else {
            self.parameters.push(Some(i32::from(code_unit)));
            if self.parameters.len() == 2 {
                self.action_vt52_dispatch(u16::from(b'Y'));
                self.enter_ground();
            }
        }
    }

    fn event_dcs_entry(&mut self, code_unit: u16) {
        if is_c0(code_unit) || code_unit == DEL {
        } else if code_unit == u16::from(b':') {
            self.state = State::DcsIgnore;
            self.sequence_buffer.clear();
        } else if is_numeric(code_unit) || code_unit == u16::from(b';') {
            self.action_param(code_unit);
            self.state = State::DcsParam;
        } else if is_intermediate(code_unit) {
            self.action_collect(code_unit);
            self.state = State::DcsIntermediate;
        } else {
            self.action_dcs_dispatch(code_unit);
        }
    }

    fn event_dcs_intermediate(&mut self, code_unit: u16) {
        if is_c0(code_unit) || code_unit == DEL {
        } else if is_intermediate(code_unit) {
            self.action_collect(code_unit);
        } else if is_intermediate_invalid(code_unit) {
            self.state = State::DcsIgnore;
            self.sequence_buffer.clear();
        } else {
            self.action_dcs_dispatch(code_unit);
        }
    }

    fn event_dcs_param(&mut self, code_unit: u16) {
        if is_numeric(code_unit) || code_unit == u16::from(b';') {
            self.action_param(code_unit);
        } else if is_intermediate(code_unit) {
            self.action_collect(code_unit);
            self.state = State::DcsIntermediate;
        } else if is_private_marker(code_unit) || code_unit == u16::from(b':') {
            self.state = State::DcsIgnore;
            self.sequence_buffer.clear();
        } else if !is_c0(code_unit) && code_unit != DEL {
            self.action_dcs_dispatch(code_unit);
        }
    }

    fn event_dcs_pass_through(&mut self, code_unit: u16) {
        if (is_c0(code_unit) || (0x20..DEL).contains(&code_unit))
            && !self.engine.action_dcs_put(code_unit)
        {
            self.state = State::DcsIgnore;
            self.runtime.dcs_handler_active = false;
        }
    }
}

const fn is_numeric(code_unit: u16) -> bool {
    code_unit >= b'0' as u16 && code_unit <= b'9' as u16
}

const fn is_c0(code_unit: u16) -> bool {
    code_unit <= 0x17 || code_unit == 0x19 || (code_unit >= 0x1c && code_unit <= 0x1f)
}

const fn is_c1_control(code_unit: u16) -> bool {
    code_unit >= 0x80 && code_unit <= 0x9f
}

const fn is_intermediate(code_unit: u16) -> bool {
    code_unit >= 0x20 && code_unit <= 0x2f
}

const fn is_private_marker(code_unit: u16) -> bool {
    code_unit >= b'<' as u16 && code_unit <= b'?' as u16
}

const fn is_intermediate_invalid(code_unit: u16) -> bool {
    is_numeric(code_unit)
        || code_unit == b':' as u16
        || code_unit == b';' as u16
        || is_private_marker(code_unit)
}

const fn is_osc_invalid(code_unit: u16) -> bool {
    code_unit <= 0x17 || code_unit == 0x19 || (code_unit >= 0x1c && code_unit <= 0x1f)
}

const fn is_actionable_control(code_unit: u16) -> bool {
    code_unit <= 0x1f || code_unit == DEL || is_c1_control(code_unit)
}

fn accumulate(current: i32, code_unit: u16) -> i32 {
    let digit = i32::from(code_unit - u16::from(b'0'));
    (current * 10 + digit).min(MAX_PARAMETER_VALUE)
}

#[cfg(test)]
mod tests {
    use super::{Parameters, StateMachine, StateMachineEngine, VtId};

    #[derive(Default)]
    struct TestEngine {
        passthrough_dispatches: bool,
        printed: Vec<u16>,
        passed_through: Vec<u16>,
        executed: Vec<u16>,
        csi_id: VtId,
        csi_params: Vec<i32>,
        dcs_id: VtId,
        dcs_params: Vec<i32>,
        dcs_data: Vec<u16>,
    }

    impl TestEngine {
        fn reset(&mut self) {
            self.printed.clear();
            self.passed_through.clear();
            self.executed.clear();
            self.csi_id = VtId::default();
            self.csi_params.clear();
            self.dcs_id = VtId::default();
            self.dcs_params.clear();
            self.dcs_data.clear();
        }
    }

    impl StateMachineEngine for TestEngine {
        fn action_execute(&mut self, code_unit: u16) -> bool {
            self.executed.push(code_unit);
            true
        }

        fn action_print_string(&mut self, text: &[u16]) -> bool {
            self.printed.extend_from_slice(text);
            true
        }

        fn action_pass_through_string(&mut self, text: &[u16]) -> bool {
            self.passed_through.extend_from_slice(text);
            true
        }

        fn action_csi_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
            if self.passthrough_dispatches {
                return false;
            }
            self.csi_id = id;
            self.csi_params = parameters
                .values()
                .iter()
                .map(|value| value.unwrap_or(0))
                .collect();
            true
        }

        fn action_osc_dispatch(&mut self, _parameter: i32, _text: &[u16]) -> bool {
            !self.passthrough_dispatches
        }

        fn action_dcs_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
            self.dcs_id = id;
            self.dcs_params = parameters
                .values()
                .iter()
                .map(|value| value.unwrap_or(0))
                .collect();
            self.dcs_data.clear();
            true
        }

        fn action_dcs_put(&mut self, code_unit: u16) -> bool {
            self.dcs_data.push(code_unit);
            true
        }
    }

    #[test]
    fn two_state_machines_do_not_interfere() {
        let mut first = StateMachine::new(TestEngine::default());
        let mut second = StateMachine::new(TestEngine::default());

        first.process_str("\u{1b}[12");
        second.process_str("\u{1b}[3C");
        first.process_str(";34m");

        assert_eq!(first.engine().csi_params, [12, 34]);
        assert_eq!(second.engine().csi_params, [3]);
    }

    #[test]
    fn bulk_text_is_printed_as_one_run() {
        let mut machine = StateMachine::new(TestEngine::default());
        machine.process_str("12345 Hello World");
        assert_eq!(to_string(&machine.engine().printed), "12345 Hello World");
    }

    #[test]
    fn unhandled_csi_is_passed_through_without_losing_prior_text() {
        let engine = TestEngine {
            passthrough_dispatches: true,
            ..TestEngine::default()
        };
        let mut machine = StateMachine::new(engine);

        machine.process_str("12345 Hello World\u{1b}[?999h");

        assert_eq!(to_string(&machine.engine().printed), "12345 Hello World");
        assert_eq!(to_string(&machine.engine().passed_through), "\u{1b}[?999h");
    }

    #[test]
    fn unhandled_sequences_survive_split_writes() {
        let engine = TestEngine {
            passthrough_dispatches: true,
            ..TestEngine::default()
        };
        let mut machine = StateMachine::new(engine);

        machine.process_str("\u{1b}[?12");
        assert!(machine.engine().passed_through.is_empty());
        machine.process_str("34h");
        assert_eq!(to_string(&machine.engine().passed_through), "\u{1b}[?1234h");

        machine.engine_mut().reset();
        machine.process_str("\u{1b}[?2");
        machine.process_str("34");
        assert!(machine.engine().passed_through.is_empty());
        machine.process_str("5h");
        assert_eq!(to_string(&machine.engine().passed_through), "\u{1b}[?2345h");

        machine.engine_mut().reset();
        machine.process_str("\u{1b}]99;foo\u{1b}");
        assert!(machine.engine().passed_through.is_empty());
        machine.process_str("\\");
        assert_eq!(
            to_string(&machine.engine().passed_through),
            "\u{1b}]99;foo\u{1b}\\"
        );
    }

    #[test]
    fn dcs_data_is_delivered_and_st_can_terminate_it() {
        let mut machine = StateMachine::new(TestEngine::default());
        machine.process_str("\u{1b}P1;2;3|data string");
        machine.process_str("\u{1b}\\");
        machine.process_str("printed text");

        assert_eq!(machine.engine().dcs_id, VtId::from_ascii("|"));
        assert_eq!(machine.engine().dcs_params, [1, 2, 3]);
        assert_eq!(to_string(&machine.engine().dcs_data), "data string\u{1b}");
        assert_eq!(to_string(&machine.engine().printed), "printed text");
    }

    #[test]
    fn dcs_can_be_terminated_by_csi_can_or_sub() {
        for (terminator, expected_csi, expected_executed) in [
            ("\u{1b}[m", VtId::from_ascii("m"), Vec::new()),
            ("\u{18}", VtId::default(), vec![0x18]),
            ("\u{1a}", VtId::default(), vec![0x1a]),
        ] {
            let mut machine = StateMachine::new(TestEngine::default());
            machine.process_str("\u{1b}P1;2;3|data string");
            machine.process_str(terminator);
            machine.process_str("printed text");

            assert_eq!(machine.engine().dcs_id, VtId::from_ascii("|"));
            assert_eq!(machine.engine().dcs_params, [1, 2, 3]);
            assert_eq!(to_string(&machine.engine().dcs_data), "data string\u{1b}");
            assert_eq!(machine.engine().csi_id, expected_csi);
            assert_eq!(machine.engine().executed, expected_executed);
            assert_eq!(to_string(&machine.engine().printed), "printed text");
        }
    }

    #[test]
    fn parameter_subspan_matches_terminal_semantics() {
        let parameters = Parameters::from_values(vec![Some(12), Some(34), Some(56), Some(78)]);

        let all = parameters.subspan(0);
        assert_eq!(all.size(), 4);
        assert_eq!(all.at(0), Some(12));
        assert_eq!(all.at(3), Some(78));

        let tail = parameters.subspan(2);
        assert_eq!(tail.size(), 2);
        assert_eq!(tail.at(0), Some(56));
        assert_eq!(tail.at(1), Some(78));

        for offset in [4, 6] {
            let empty = parameters.subspan(offset);
            assert_eq!(empty.size(), 1);
            assert_eq!(empty.at(0), None);
        }
    }

    #[test]
    fn parameter_values_saturate_at_terminal_limit() {
        let mut machine = StateMachine::new(TestEngine::default());
        machine.process_str("\u{1b}[999999999m");
        assert_eq!(machine.engine().csi_params, [super::MAX_PARAMETER_VALUE]);
    }

    fn to_string(units: &[u16]) -> String {
        String::from_utf16(units).expect("test data is valid UTF-16")
    }
}
