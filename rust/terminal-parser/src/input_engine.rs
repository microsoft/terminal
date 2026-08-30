//! Safe Rust implementation of Windows Terminal's VT input state-machine engine.
//!
//! The parser recognizes the VT grammar. This module converts input-side VT
//! sequences into a platform-neutral equivalent of the `IInteractDispatch`
//! contract so the parser can be validated without `INPUT_RECORD`, Win32, or C++.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Condvar, Mutex, PoisonError};
use std::time::{Duration, Instant};

use crate::state_machine::{Parameters, StateMachineEngine, VtId};

pub const RIGHT_ALT_PRESSED: u32 = 0x0001;
pub const LEFT_ALT_PRESSED: u32 = 0x0002;
pub const RIGHT_CTRL_PRESSED: u32 = 0x0004;
pub const LEFT_CTRL_PRESSED: u32 = 0x0008;
pub const SHIFT_PRESSED: u32 = 0x0010;
pub const ENHANCED_KEY: u32 = 0x0100;

pub const FROM_LEFT_1ST_BUTTON_PRESSED: u32 = 0x0001;
pub const RIGHTMOST_BUTTON_PRESSED: u32 = 0x0002;
pub const FROM_LEFT_2ND_BUTTON_PRESSED: u32 = 0x0004;

pub const MOUSE_MOVED: u32 = 0x0001;
pub const DOUBLE_CLICK: u32 = 0x0002;
pub const MOUSE_WHEELED: u32 = 0x0004;
pub const MOUSE_HWHEELED: u32 = 0x0008;

pub const SCROLL_DELTA_BACKWARD: u32 = 0xff80_0000;
pub const SCROLL_DELTA_FORWARD: u32 = 0x0080_0000;

const VT_SHIFT: i32 = 1;
const VT_ALT: i32 = 2;
const VT_CTRL: i32 = 4;

const SGR_SHIFT: i32 = 4;
const SGR_META: i32 = 8;
const SGR_CTRL: i32 = 16;
const SGR_DRAG: i32 = 32;

const VK_BACK: u16 = 0x08;
const VK_TAB: u16 = 0x09;
const VK_RETURN: u16 = 0x0d;
const VK_SHIFT: u16 = 0x10;
const VK_CONTROL: u16 = 0x11;
const VK_MENU: u16 = 0x12;
const VK_ESCAPE: u16 = 0x1b;
const VK_SPACE: u16 = 0x20;
const VK_PRIOR: u16 = 0x21;
const VK_NEXT: u16 = 0x22;
const VK_END: u16 = 0x23;
const VK_HOME: u16 = 0x24;
const VK_LEFT: u16 = 0x25;
const VK_UP: u16 = 0x26;
const VK_RIGHT: u16 = 0x27;
const VK_DOWN: u16 = 0x28;
const VK_INSERT: u16 = 0x2d;
const VK_DELETE: u16 = 0x2e;
const VK_F1: u16 = 0x70;
const VK_F2: u16 = 0x71;
const VK_F3: u16 = 0x72;
const VK_F4: u16 = 0x73;
const VK_F5: u16 = 0x74;
const VK_F6: u16 = 0x75;
const VK_F7: u16 = 0x76;
const VK_F8: u16 = 0x77;
const VK_F9: u16 = 0x78;
const VK_F10: u16 = 0x79;
const VK_F11: u16 = 0x7a;
const VK_F12: u16 = 0x7b;
const VK_OEM_1: u16 = 0xba;
const VK_OEM_PLUS: u16 = 0xbb;
const VK_OEM_COMMA: u16 = 0xbc;
const VK_OEM_MINUS: u16 = 0xbd;
const VK_OEM_PERIOD: u16 = 0xbe;
const VK_OEM_2: u16 = 0xbf;
const VK_OEM_3: u16 = 0xc0;
const VK_OEM_4: u16 = 0xdb;
const VK_OEM_5: u16 = 0xdc;
const VK_OEM_6: u16 = 0xdd;
const VK_OEM_7: u16 = 0xde;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Point {
    pub x: i32,
    pub y: i32,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct KeyEvent {
    pub key_down: bool,
    pub repeat_count: u16,
    pub virtual_key: u16,
    pub scan_code: u16,
    pub unicode_char: u16,
    pub control_key_state: u32,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct MouseEvent {
    pub position: Point,
    pub button_state: u32,
    pub control_key_state: u32,
    pub event_flags: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InputRecord {
    Key(KeyEvent),
    Mouse(MouseEvent),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum InputAction {
    WriteInput(Vec<InputRecord>),
    WriteCtrlKey(KeyEvent),
    WriteString(Vec<u16>),
    WriteStringRaw(Vec<u16>),
    MoveCursor { row: i32, column: i32 },
    FocusChanged(bool),
}

/// Platform-neutral destination for input-engine operations.
pub trait InputDispatch {
    fn dispatch(&mut self, action: InputAction);

    fn is_vt_input_enabled(&self) -> bool {
        false
    }
}

#[derive(Debug, Clone, Default)]
pub struct DeviceAttributeLatch {
    inner: Arc<(Mutex<u64>, Condvar)>,
}

impl DeviceAttributeLatch {
    #[must_use]
    pub fn value(&self) -> u64 {
        let (lock, _) = &*self.inner;
        *lock.lock().unwrap_or_else(PoisonError::into_inner)
    }

    #[must_use]
    pub fn wait(&self, timeout: Duration) -> u64 {
        let (lock, condition) = &*self.inner;
        let guard = lock.lock().unwrap_or_else(PoisonError::into_inner);
        let result = condition.wait_timeout_while(guard, timeout, |bits| *bits == 0);
        let (guard, _) = result.unwrap_or_else(PoisonError::into_inner);
        *guard
    }

    fn publish(&self, bits: u64) {
        let (lock, condition) = &*self.inner;
        let mut value = lock.lock().unwrap_or_else(PoisonError::into_inner);
        *value |= bits;
        condition.notify_all();
    }
}

#[derive(Debug, Clone, Copy)]
struct MouseClick {
    position: Point,
    button: i32,
    at: Instant,
}

pub struct InputStateMachineEngine<D: InputDispatch> {
    dispatch: D,
    device_attributes: DeviceAttributeLatch,
    capture_next_cursor_position_report: AtomicBool,
    encountered_win32_input_mode_sequence: bool,
    expecting_string_terminator: bool,
    mouse_button_state: u32,
    double_click_time: Duration,
    last_mouse_click: Option<MouseClick>,
}

impl<D: InputDispatch> InputStateMachineEngine<D> {
    #[must_use]
    pub fn new(dispatch: D) -> Self {
        Self {
            dispatch,
            device_attributes: DeviceAttributeLatch::default(),
            capture_next_cursor_position_report: AtomicBool::new(false),
            encountered_win32_input_mode_sequence: false,
            expecting_string_terminator: false,
            mouse_button_state: 0,
            double_click_time: Duration::from_millis(500),
            last_mouse_click: None,
        }
    }

    #[must_use]
    pub const fn dispatch(&self) -> &D {
        &self.dispatch
    }

    pub const fn dispatch_mut(&mut self) -> &mut D {
        &mut self.dispatch
    }

    #[must_use]
    pub fn into_dispatch(self) -> D {
        self.dispatch
    }

    pub fn capture_next_cursor_position_report(&self) {
        self.capture_next_cursor_position_report
            .store(true, Ordering::Relaxed);
    }

    #[must_use]
    pub fn device_attribute_latch(&self) -> DeviceAttributeLatch {
        self.device_attributes.clone()
    }

    #[must_use]
    pub fn wait_until_da1(&self, timeout: Duration) -> u64 {
        let value = self.device_attributes.wait(timeout);
        self.capture_next_cursor_position_report
            .store(false, Ordering::Relaxed);
        value
    }

    pub fn set_double_click_time(&mut self, duration: Duration) {
        self.double_click_time = duration;
    }

    #[must_use]
    pub const fn encountered_win32_input_mode_sequence(&self) -> bool {
        self.encountered_win32_input_mode_sequence
    }

    #[must_use]
    pub fn generate_win32_key(parameters: &Parameters) -> KeyEvent {
        KeyEvent {
            key_down: raw_parameter(parameters, 3, 0) != 0,
            repeat_count: saturated_u16(raw_parameter(parameters, 5, 1)),
            virtual_key: saturated_u16(raw_parameter(parameters, 0, 0)),
            scan_code: saturated_u16(raw_parameter(parameters, 1, 0)),
            unicode_char: saturated_u16(raw_parameter(parameters, 2, 0)),
            control_key_state: saturated_u32(raw_parameter(parameters, 4, 0)),
        }
    }

    fn emit(&mut self, action: InputAction) {
        self.dispatch.dispatch(action);
    }

    fn action_control(&mut self, code_unit: u16, write_alt: bool) -> bool {
        if code_unit == 0x03 && !write_alt {
            let mut key = KeyEvent {
                key_down: true,
                repeat_count: 1,
                virtual_key: u16::from(b'C'),
                scan_code: 0,
                unicode_char: 0x03,
                control_key_state: LEFT_CTRL_PRESSED,
            };
            self.emit(InputAction::WriteCtrlKey(key));
            key.key_down = false;
            self.emit(InputAction::WriteCtrlKey(key));
            return true;
        }

        if code_unit < 0x20 {
            let (unicode_char, virtual_key, write_ctrl) = match code_unit {
                0x08 => (0x7f, VK_BACK, true),
                0x09 => (0x09, VK_TAB, false),
                0x0d => (0x0d, VK_RETURN, false),
                0x1b => (0x1b, VK_ESCAPE, false),
                _ => (code_unit, control_virtual_key(code_unit), true),
            };
            let mut modifiers = if write_ctrl { LEFT_CTRL_PRESSED } else { 0 };
            if write_alt {
                modifiers |= LEFT_ALT_PRESSED;
            }
            self.write_single_key(unicode_char, virtual_key, modifiers);
            return true;
        }

        if code_unit == 0x7f {
            let modifiers = if write_alt { LEFT_ALT_PRESSED } else { 0 };
            self.write_single_key(0x08, VK_BACK, modifiers);
            return true;
        }

        self.action_print(code_unit)
    }

    fn action_escape(&mut self, id: VtId) -> bool {
        if self.expecting_string_terminator && id_is(id, "\\") {
            self.expecting_string_terminator = false;
            return false;
        }
        if self.dispatch.is_vt_input_enabled() {
            return false;
        }

        let code_unit = id.value().to_le_bytes()[0].into();
        if code_unit == 0x7f {
            return self.action_control(code_unit, true);
        }
        if let Some((virtual_key, modifiers)) = key_from_ascii(code_unit) {
            self.write_single_key(code_unit, virtual_key, modifiers | LEFT_ALT_PRESSED);
        }
        true
    }

    fn action_csi(&mut self, id: VtId, parameters: &Parameters) -> bool {
        let vt_input_enabled = self.dispatch.is_vt_input_enabled();

        if id_is(id, "<M") || id_is(id, "<m") {
            if vt_input_enabled {
                return false;
            }
            let encoding = raw_parameter(parameters, 0, 0);
            let position = Point {
                x: numeric_parameter(parameters, 1) - 1,
                y: numeric_parameter(parameters, 2) - 1,
            };
            if let Some((button_state, event_flags)) =
                self.update_sgr_mouse_state(id, encoding, position)
            {
                let event = MouseEvent {
                    position,
                    button_state,
                    control_key_state: sgr_mouse_modifiers(encoding),
                    event_flags,
                };
                self.emit(InputAction::WriteInput(vec![InputRecord::Mouse(event)]));
            }
            return true;
        }

        if id_is(id, "R") {
            if self
                .capture_next_cursor_position_report
                .swap(false, Ordering::Relaxed)
            {
                self.emit(InputAction::MoveCursor {
                    row: numeric_parameter(parameters, 0),
                    column: numeric_parameter(parameters, 1),
                });
                return true;
            }
            if self.encountered_win32_input_mode_sequence || vt_input_enabled {
                return false;
            }
            self.write_cursor_key(id, parameters);
            return true;
        }

        if is_cursor_key(id) {
            if vt_input_enabled {
                return false;
            }
            self.write_cursor_key(id, parameters);
            return true;
        }

        if id_is(id, "~") {
            if vt_input_enabled {
                return false;
            }
            if let Some(virtual_key) = generic_virtual_key(raw_parameter(parameters, 0, 0)) {
                let mut modifiers = vt_modifiers(parameters.at(1));
                if (1..=6).contains(&raw_parameter(parameters, 0, 0)) {
                    modifiers |= ENHANCED_KEY;
                }
                self.write_virtual_key(virtual_key, modifiers);
            }
            return true;
        }

        if id_is(id, "Z") {
            if vt_input_enabled {
                return false;
            }
            self.write_virtual_key(VK_TAB, SHIFT_PRESSED);
            return true;
        }

        if id_is(id, "I") {
            self.emit(InputAction::FocusChanged(true));
            return true;
        }
        if id_is(id, "O") {
            self.emit(InputAction::FocusChanged(false));
            return true;
        }

        if id_is(id, "?c") {
            if self.device_attributes.value() != 0 {
                return false;
            }
            let mut attributes = 1u64;
            if parameters.values().len() >= 2 && raw_parameter(parameters, 0, 0) >= 61 {
                for value in parameters.values().iter().skip(1).flatten().copied() {
                    if (1..64).contains(&value) {
                        let shift = u32::try_from(value).unwrap_or_default();
                        attributes |= 1u64 << shift;
                    }
                }
            }
            self.device_attributes.publish(attributes);
            return true;
        }

        if id_is(id, "_") {
            let key = Self::generate_win32_key(parameters);
            self.emit(InputAction::WriteCtrlKey(key));
            self.encountered_win32_input_mode_sequence = true;
            return true;
        }

        false
    }

    fn write_cursor_key(&mut self, id: VtId, parameters: &Parameters) {
        let Some(virtual_key) = cursor_virtual_key(id) else {
            return;
        };
        let mut modifiers = vt_modifiers(parameters.at(1));
        if !matches!(virtual_key, VK_F1 | VK_F2 | VK_F3 | VK_F4) {
            modifiers |= ENHANCED_KEY;
        }
        self.write_virtual_key(virtual_key, modifiers);
    }

    fn action_ss3(&mut self, code_unit: u16) -> bool {
        if self.dispatch.is_vt_input_enabled() {
            return false;
        }
        if let Some(virtual_key) = ss3_virtual_key(code_unit) {
            self.write_virtual_key(virtual_key, 0);
        }
        true
    }

    fn write_virtual_key(&mut self, virtual_key: u16, modifiers: u32) {
        self.write_single_key(char_for_virtual_key(virtual_key), virtual_key, modifiers);
    }

    fn write_single_key(&mut self, unicode_char: u16, virtual_key: u16, modifiers: u32) {
        let mut records = Vec::with_capacity(8);
        let mut current_modifiers = 0u32;

        if modifiers & SHIFT_PRESSED != 0 {
            current_modifiers |= SHIFT_PRESSED;
            records.push(InputRecord::Key(modifier_event(
                true,
                VK_SHIFT,
                current_modifiers,
            )));
        }
        if modifiers & LEFT_ALT_PRESSED != 0 {
            current_modifiers |= LEFT_ALT_PRESSED;
            records.push(InputRecord::Key(modifier_event(
                true,
                VK_MENU,
                current_modifiers,
            )));
        }
        if modifiers & LEFT_CTRL_PRESSED != 0 {
            current_modifiers |= LEFT_CTRL_PRESSED;
            records.push(InputRecord::Key(modifier_event(
                true,
                VK_CONTROL,
                current_modifiers,
            )));
        }

        let mut key = KeyEvent {
            key_down: true,
            repeat_count: 1,
            virtual_key,
            scan_code: 0,
            unicode_char,
            control_key_state: modifiers,
        };
        records.push(InputRecord::Key(key));
        key.key_down = false;
        records.push(InputRecord::Key(key));

        if modifiers & LEFT_CTRL_PRESSED != 0 {
            current_modifiers &= !LEFT_CTRL_PRESSED;
            records.push(InputRecord::Key(modifier_event(
                false,
                VK_CONTROL,
                current_modifiers,
            )));
        }
        if modifiers & LEFT_ALT_PRESSED != 0 {
            current_modifiers &= !LEFT_ALT_PRESSED;
            records.push(InputRecord::Key(modifier_event(
                false,
                VK_MENU,
                current_modifiers,
            )));
        }
        if modifiers & SHIFT_PRESSED != 0 {
            current_modifiers &= !SHIFT_PRESSED;
            records.push(InputRecord::Key(modifier_event(
                false,
                VK_SHIFT,
                current_modifiers,
            )));
        }

        self.emit(InputAction::WriteInput(records));
    }

    fn update_sgr_mouse_state(
        &mut self,
        id: VtId,
        encoding: i32,
        position: Point,
    ) -> Option<(u32, u32)> {
        let button_id = (encoding & 0x3) | ((encoding & 0xc0) >> 4);
        let mut button_state = self.mouse_button_state;
        let mut event_flags = 0u32;
        let mut button_flag = 0u32;

        match button_id {
            0 => button_flag = FROM_LEFT_1ST_BUTTON_PRESSED,
            1 => button_flag = FROM_LEFT_2ND_BUTTON_PRESSED,
            2 => button_flag = RIGHTMOST_BUTTON_PRESSED,
            3 => {}
            4 => {
                button_state |= SCROLL_DELTA_FORWARD;
                event_flags |= MOUSE_WHEELED;
            }
            5 => {
                button_state |= SCROLL_DELTA_BACKWARD;
                event_flags |= MOUSE_WHEELED;
            }
            6 => {
                button_state |= SCROLL_DELTA_BACKWARD;
                event_flags |= MOUSE_HWHEELED;
            }
            7 => {
                button_state |= SCROLL_DELTA_FORWARD;
                event_flags |= MOUSE_HWHEELED;
            }
            _ => return None,
        }

        if id_is(id, "<M") {
            button_state |= button_flag;
            if matches!(button_id, 0..=2) {
                let now = Instant::now();
                if let Some(previous) = self.last_mouse_click
                    && previous.position == position
                    && previous.button == button_id
                    && now.duration_since(previous.at) < self.double_click_time
                {
                    event_flags |= DOUBLE_CLICK;
                    self.last_mouse_click = None;
                } else {
                    self.last_mouse_click = Some(MouseClick {
                        position,
                        button: button_id,
                        at: now,
                    });
                }
            }
        } else if id_is(id, "<m") {
            button_state &= !button_flag;
        } else {
            return None;
        }

        if encoding & SGR_DRAG != 0 {
            event_flags |= MOUSE_MOVED;
        }

        self.mouse_button_state = button_state & 0xffff;
        Some((button_state, event_flags))
    }
}

impl<D: InputDispatch> StateMachineEngine for InputStateMachineEngine<D> {
    fn encountered_win32_input_mode_sequence(&self) -> bool {
        self.encountered_win32_input_mode_sequence
    }

    fn action_execute(&mut self, code_unit: u16) -> bool {
        self.action_control(code_unit, false)
    }

    fn action_execute_from_escape(&mut self, code_unit: u16) -> bool {
        if self.dispatch.is_vt_input_enabled() {
            return false;
        }
        self.action_control(code_unit, true)
    }

    fn action_print(&mut self, code_unit: u16) -> bool {
        if let Some((virtual_key, modifiers)) = key_from_ascii(code_unit) {
            self.write_single_key(code_unit, virtual_key, modifiers);
        }
        true
    }

    fn action_print_string(&mut self, text: &[u16]) -> bool {
        if !text.is_empty() {
            self.emit(InputAction::WriteString(text.to_vec()));
        }
        true
    }

    fn action_pass_through_string(&mut self, text: &[u16]) -> bool {
        if !text.is_empty() {
            self.emit(InputAction::WriteStringRaw(text.to_vec()));
        }
        true
    }

    fn action_esc_dispatch(&mut self, id: VtId) -> bool {
        self.action_escape(id)
    }

    fn action_vt52_esc_dispatch(&mut self, _id: VtId, _parameters: &Parameters) -> bool {
        false
    }

    fn action_csi_dispatch(&mut self, id: VtId, parameters: &Parameters) -> bool {
        self.action_csi(id, parameters)
    }

    fn action_dcs_dispatch(&mut self, _id: VtId, _parameters: &Parameters) -> bool {
        self.expecting_string_terminator = true;
        false
    }

    fn action_osc_dispatch(&mut self, _parameter: i32, _text: &[u16]) -> bool {
        false
    }

    fn action_ss3_dispatch(&mut self, code_unit: u16, _parameters: &Parameters) -> bool {
        self.action_ss3(code_unit)
    }
}

fn modifier_event(key_down: bool, virtual_key: u16, control_key_state: u32) -> KeyEvent {
    KeyEvent {
        key_down,
        repeat_count: 1,
        virtual_key,
        scan_code: 0,
        unicode_char: 0,
        control_key_state,
    }
}

fn control_virtual_key(code_unit: u16) -> u16 {
    if code_unit == 0 {
        u16::from(b'@')
    } else {
        code_unit.saturating_add(0x40)
    }
}

fn key_from_ascii(code_unit: u16) -> Option<(u16, u32)> {
    let byte = u8::try_from(code_unit).ok()?;
    let pair = match byte {
        b'a'..=b'z' => (u16::from(byte.to_ascii_uppercase()), 0),
        b'A'..=b'Z' => (u16::from(byte), SHIFT_PRESSED),
        b'0'..=b'9' => (u16::from(byte), 0),
        b' ' => (VK_SPACE, 0),
        b'-' => (VK_OEM_MINUS, 0),
        b'_' => (VK_OEM_MINUS, SHIFT_PRESSED),
        b'=' => (VK_OEM_PLUS, 0),
        b'+' => (VK_OEM_PLUS, SHIFT_PRESSED),
        b'[' => (VK_OEM_4, 0),
        b'{' => (VK_OEM_4, SHIFT_PRESSED),
        b']' => (VK_OEM_6, 0),
        b'}' => (VK_OEM_6, SHIFT_PRESSED),
        b'\\' => (VK_OEM_5, 0),
        b'|' => (VK_OEM_5, SHIFT_PRESSED),
        b';' => (VK_OEM_1, 0),
        b':' => (VK_OEM_1, SHIFT_PRESSED),
        b'\'' => (VK_OEM_7, 0),
        b'"' => (VK_OEM_7, SHIFT_PRESSED),
        b',' => (VK_OEM_COMMA, 0),
        b'<' => (VK_OEM_COMMA, SHIFT_PRESSED),
        b'.' => (VK_OEM_PERIOD, 0),
        b'>' => (VK_OEM_PERIOD, SHIFT_PRESSED),
        b'/' => (VK_OEM_2, 0),
        b'?' => (VK_OEM_2, SHIFT_PRESSED),
        b'`' => (VK_OEM_3, 0),
        b'~' => (VK_OEM_3, SHIFT_PRESSED),
        b'!' => (u16::from(b'1'), SHIFT_PRESSED),
        b'@' => (u16::from(b'2'), SHIFT_PRESSED),
        b'#' => (u16::from(b'3'), SHIFT_PRESSED),
        b'$' => (u16::from(b'4'), SHIFT_PRESSED),
        b'%' => (u16::from(b'5'), SHIFT_PRESSED),
        b'^' => (u16::from(b'6'), SHIFT_PRESSED),
        b'&' => (u16::from(b'7'), SHIFT_PRESSED),
        b'*' => (u16::from(b'8'), SHIFT_PRESSED),
        b'(' => (u16::from(b'9'), SHIFT_PRESSED),
        b')' => (u16::from(b'0'), SHIFT_PRESSED),
        _ => return None,
    };
    Some(pair)
}

fn char_for_virtual_key(virtual_key: u16) -> u16 {
    match virtual_key {
        VK_BACK => 0x08,
        VK_TAB => 0x09,
        VK_RETURN => 0x0d,
        VK_ESCAPE => 0x1b,
        value if (u16::from(b'0')..=u16::from(b'9')).contains(&value) => value,
        value if (u16::from(b'A')..=u16::from(b'Z')).contains(&value) => value,
        _ => 0,
    }
}

fn cursor_virtual_key(id: VtId) -> Option<u16> {
    if id_is(id, "A") {
        Some(VK_UP)
    } else if id_is(id, "B") {
        Some(VK_DOWN)
    } else if id_is(id, "C") {
        Some(VK_RIGHT)
    } else if id_is(id, "D") {
        Some(VK_LEFT)
    } else if id_is(id, "H") {
        Some(VK_HOME)
    } else if id_is(id, "F") {
        Some(VK_END)
    } else if id_is(id, "P") {
        Some(VK_F1)
    } else if id_is(id, "Q") {
        Some(VK_F2)
    } else if id_is(id, "R") {
        Some(VK_F3)
    } else if id_is(id, "S") {
        Some(VK_F4)
    } else {
        None
    }
}

fn is_cursor_key(id: VtId) -> bool {
    cursor_virtual_key(id).is_some() && !id_is(id, "R")
}

fn generic_virtual_key(identifier: i32) -> Option<u16> {
    match identifier {
        1 => Some(VK_HOME),
        2 => Some(VK_INSERT),
        3 => Some(VK_DELETE),
        4 => Some(VK_END),
        5 => Some(VK_PRIOR),
        6 => Some(VK_NEXT),
        15 => Some(VK_F5),
        17 => Some(VK_F6),
        18 => Some(VK_F7),
        19 => Some(VK_F8),
        20 => Some(VK_F9),
        21 => Some(VK_F10),
        23 => Some(VK_F11),
        24 => Some(VK_F12),
        _ => None,
    }
}

fn ss3_virtual_key(code_unit: u16) -> Option<u16> {
    match u8::try_from(code_unit).ok()? {
        b'A' => Some(VK_UP),
        b'B' => Some(VK_DOWN),
        b'C' => Some(VK_RIGHT),
        b'D' => Some(VK_LEFT),
        b'H' => Some(VK_HOME),
        b'F' => Some(VK_END),
        b'P' => Some(VK_F1),
        b'Q' => Some(VK_F2),
        b'R' => Some(VK_F3),
        b'S' => Some(VK_F4),
        _ => None,
    }
}

fn vt_modifiers(parameter: Option<i32>) -> u32 {
    let encoded = parameter.unwrap_or(1).max(1) - 1;
    let mut modifiers = 0u32;
    if encoded & VT_SHIFT != 0 {
        modifiers |= SHIFT_PRESSED;
    }
    if encoded & VT_ALT != 0 {
        modifiers |= LEFT_ALT_PRESSED;
    }
    if encoded & VT_CTRL != 0 {
        modifiers |= LEFT_CTRL_PRESSED;
    }
    modifiers
}

fn sgr_mouse_modifiers(encoding: i32) -> u32 {
    let mut modifiers = 0u32;
    if encoding & SGR_SHIFT != 0 {
        modifiers |= SHIFT_PRESSED;
    }
    if encoding & SGR_META != 0 {
        modifiers |= LEFT_ALT_PRESSED;
    }
    if encoding & SGR_CTRL != 0 {
        modifiers |= LEFT_CTRL_PRESSED;
    }
    modifiers
}

fn numeric_parameter(parameters: &Parameters, index: usize) -> i32 {
    match parameters.at(index) {
        Some(value) if value > 0 => value,
        _ => 1,
    }
}

fn raw_parameter(parameters: &Parameters, index: usize, default: i32) -> i32 {
    parameters.at(index).unwrap_or(default)
}

fn saturated_u16(value: i32) -> u16 {
    u16::try_from(value.clamp(0, i32::from(u16::MAX))).unwrap_or_default()
}

fn saturated_u32(value: i32) -> u32 {
    u32::try_from(value.max(0)).unwrap_or_default()
}

fn id_is(id: VtId, text: &str) -> bool {
    id.value() == VtId::from_ascii(text).value()
}

#[cfg(test)]
mod tests {
    use super::{
        DOUBLE_CLICK, ENHANCED_KEY, FROM_LEFT_1ST_BUTTON_PRESSED, InputAction, InputDispatch,
        InputRecord, InputStateMachineEngine, LEFT_ALT_PRESSED, LEFT_CTRL_PRESSED, MOUSE_HWHEELED,
        MOUSE_MOVED, Point, RIGHTMOST_BUTTON_PRESSED, SCROLL_DELTA_BACKWARD, SCROLL_DELTA_FORWARD,
        SHIFT_PRESSED, VK_F3, VK_LEFT, VK_TAB, VK_UP,
    };
    use crate::state_machine::{Parameters, ParserMode, StateMachine};
    use std::time::Duration;

    #[derive(Debug, Default)]
    struct RecordingDispatch {
        actions: Vec<InputAction>,
        vt_input_enabled: bool,
    }

    impl InputDispatch for RecordingDispatch {
        fn dispatch(&mut self, action: InputAction) {
            self.actions.push(action);
        }

        fn is_vt_input_enabled(&self) -> bool {
            self.vt_input_enabled
        }
    }

    fn machine() -> StateMachine<InputStateMachineEngine<RecordingDispatch>> {
        StateMachine::new_input(InputStateMachineEngine::new(RecordingDispatch::default()))
    }

    fn key_actions(
        machine: &StateMachine<InputStateMachineEngine<RecordingDispatch>>,
    ) -> Vec<super::KeyEvent> {
        machine
            .engine()
            .dispatch()
            .actions
            .iter()
            .filter_map(|action| match action {
                InputAction::WriteInput(records) => {
                    records.iter().find_map(|record| match record {
                        InputRecord::Key(key)
                            if key.key_down && !matches!(key.virtual_key, 0x10..=0x12) =>
                        {
                            Some(*key)
                        }
                        _ => None,
                    })
                }
                _ => None,
            })
            .collect()
    }

    #[test]
    fn c0_and_alt_controls_match_input_contract() {
        let mut machine = machine();
        machine.process_utf16(&[0x03]);
        machine.process_utf16(&[0x1b, 0x04]);
        machine.process_utf16(&[0x1b, 0x7f]);

        assert!(matches!(
            machine.engine().dispatch().actions[0],
            InputAction::WriteCtrlKey(key) if key.key_down && key.virtual_key == u16::from(b'C') && key.control_key_state == LEFT_CTRL_PRESSED
        ));
        let keys = key_actions(&machine);
        assert_eq!(keys.len(), 2);
        assert_eq!(keys[0].unicode_char, 0x04);
        assert_eq!(
            keys[0].control_key_state,
            LEFT_CTRL_PRESSED | LEFT_ALT_PRESSED
        );
        assert_eq!(keys[1].unicode_char, 0x08);
        assert_eq!(keys[1].control_key_state, LEFT_ALT_PRESSED);
    }

    #[test]
    fn printable_and_non_ascii_runs_use_the_string_dispatch_boundary() {
        let mut machine = machine();
        machine.process_str("hello旅");
        assert_eq!(
            machine.engine().dispatch().actions,
            [InputAction::WriteString("hello旅".encode_utf16().collect())]
        );
    }

    #[test]
    fn csi_cursor_keys_preserve_modifiers_and_enhanced_key_state() {
        let mut machine = machine();
        machine.process_str("\u{1b}[A\u{1b}[1;3D\u{1b}[1;4R");
        let keys = key_actions(&machine);
        assert_eq!(keys.len(), 3);
        assert_eq!(keys[0].virtual_key, VK_UP);
        assert_eq!(keys[0].control_key_state, ENHANCED_KEY);
        assert_eq!(keys[1].virtual_key, VK_LEFT);
        assert_eq!(keys[1].control_key_state, ENHANCED_KEY | LEFT_ALT_PRESSED);
        assert_eq!(keys[2].virtual_key, VK_F3);
        assert_eq!(keys[2].control_key_state, SHIFT_PRESSED | LEFT_ALT_PRESSED);
    }

    #[test]
    fn cursor_position_capture_consumes_exactly_one_f3_shaped_report() {
        let engine = InputStateMachineEngine::new(RecordingDispatch::default());
        engine.capture_next_cursor_position_report();
        let mut machine = StateMachine::new_input(engine);
        machine.process_str("\u{1b}[1;4R\u{1b}[1;4R");
        assert!(matches!(
            machine.engine().dispatch().actions[0],
            InputAction::MoveCursor { row: 1, column: 4 }
        ));
        let keys = key_actions(&machine);
        assert_eq!(keys.len(), 1);
        assert_eq!(keys[0].virtual_key, VK_F3);
    }

    #[test]
    fn generic_keys_backtab_and_ss3_are_decoded_without_win32() {
        let mut machine = machine();
        machine.process_str("\u{1b}[5~\u{1b}[Z\u{1b}OA");
        let keys = key_actions(&machine);
        assert_eq!(keys.len(), 3);
        assert_eq!(keys[0].control_key_state, ENHANCED_KEY);
        assert_eq!(keys[1].virtual_key, VK_TAB);
        assert_eq!(keys[1].control_key_state, SHIFT_PRESSED);
        assert_eq!(keys[2].virtual_key, VK_UP);
        assert_eq!(keys[2].control_key_state, 0);
    }

    #[test]
    fn vt_input_mode_passes_keyboard_sequences_through_but_keeps_focus_events() {
        let dispatch = RecordingDispatch {
            vt_input_enabled: true,
            ..RecordingDispatch::default()
        };
        let mut machine = StateMachine::new_input(InputStateMachineEngine::new(dispatch));
        machine.process_str("\u{1b}[A\u{1b}OA\u{1b}[I\u{1b}[O");
        assert_eq!(
            machine.engine().dispatch().actions,
            [
                InputAction::WriteStringRaw("\u{1b}[A".encode_utf16().collect()),
                InputAction::WriteStringRaw("\u{1b}OA".encode_utf16().collect()),
                InputAction::FocusChanged(true),
                InputAction::FocusChanged(false),
            ]
        );
    }

    #[test]
    fn da1_attributes_are_latched_once() {
        let mut machine = machine();
        let latch = machine.engine().device_attribute_latch();
        machine.process_str("\u{1b}[?65;1;4;22c");
        let expected = 1u64 | (1u64 << 1) | (1u64 << 4) | (1u64 << 22);
        assert_eq!(latch.wait(Duration::from_millis(1)), expected);

        machine.process_str("\u{1b}[?65;6c");
        assert_eq!(latch.value(), expected);
        assert!(matches!(
            machine.engine().dispatch().actions.last(),
            Some(InputAction::WriteStringRaw(_))
        ));
    }

    #[test]
    fn win32_input_sequence_preserves_serialized_key_fields_and_changes_f3_heuristic() {
        let mut machine = machine();
        machine.process_str("\u{1b}[1;2;65;1;5;6_\u{1b}[3;4R");
        assert!(machine.engine().encountered_win32_input_mode_sequence());
        assert!(matches!(
            machine.engine().dispatch().actions[0],
            InputAction::WriteCtrlKey(key)
                if key.virtual_key == 1
                    && key.scan_code == 2
                    && key.unicode_char == 65
                    && key.key_down
                    && key.control_key_state == 5
                    && key.repeat_count == 6
        ));
        assert!(matches!(
            machine.engine().dispatch().actions[1],
            InputAction::WriteStringRaw(_)
        ));
    }

    #[test]
    fn win32_key_defaults_match_terminal_contract() {
        let key = InputStateMachineEngine::<RecordingDispatch>::generate_win32_key(
            &Parameters::from_values(vec![Some(1), Some(2), Some(3), Some(4), Some(5)]),
        );
        assert_eq!(key.virtual_key, 1);
        assert_eq!(key.scan_code, 2);
        assert_eq!(key.unicode_char, 3);
        assert!(key.key_down);
        assert_eq!(key.control_key_state, 5);
        assert_eq!(key.repeat_count, 1);

        let defaults = InputStateMachineEngine::<RecordingDispatch>::generate_win32_key(
            &Parameters::default(),
        );
        assert_eq!(defaults.repeat_count, 1);
        assert_eq!(defaults.virtual_key, 0);
    }

    #[test]
    fn sgr_mouse_tracks_buttons_modifiers_drag_and_wheel_state() {
        let mut machine = machine();
        machine.process_str(
            "\u{1b}[<0;1;1M\u{1b}[<32;2;2M\u{1b}[<0;2;2m\u{1b}[<66;3;4M\u{1b}[<67;4;5M",
        );
        let mice = machine
            .engine()
            .dispatch()
            .actions
            .iter()
            .filter_map(|action| match action {
                InputAction::WriteInput(records) => {
                    records.iter().find_map(|record| match record {
                        InputRecord::Mouse(mouse) => Some(*mouse),
                        InputRecord::Key(_) => None,
                    })
                }
                _ => None,
            })
            .collect::<Vec<_>>();
        assert_eq!(mice.len(), 5);
        assert_eq!(mice[0].button_state, FROM_LEFT_1ST_BUTTON_PRESSED);
        assert_eq!(mice[0].position, Point { x: 0, y: 0 });
        assert_eq!(mice[1].event_flags, MOUSE_MOVED);
        assert_eq!(mice[1].position, Point { x: 1, y: 1 });
        assert_eq!(mice[2].button_state, 0);
        assert_eq!(mice[3].button_state, SCROLL_DELTA_BACKWARD);
        assert_eq!(mice[3].event_flags, MOUSE_HWHEELED);
        assert_eq!(mice[3].control_key_state, 0);
        assert_eq!(mice[4].button_state, SCROLL_DELTA_FORWARD);
        assert_eq!(mice[4].event_flags, MOUSE_HWHEELED);
    }

    #[test]
    fn sgr_mouse_supports_horizontal_wheel_and_double_click() {
        let mut engine = InputStateMachineEngine::new(RecordingDispatch::default());
        engine.set_double_click_time(Duration::from_secs(1));
        let mut machine = StateMachine::new_input(engine);
        machine.process_str(
            "\u{1b}[<2;1;1M\u{1b}[<2;1;1m\u{1b}[<2;1;1M\u{1b}[<66;2;2M\u{1b}[<67;3;3M",
        );
        let mice = machine
            .engine()
            .dispatch()
            .actions
            .iter()
            .filter_map(|action| match action {
                InputAction::WriteInput(records) => {
                    records.iter().find_map(|record| match record {
                        InputRecord::Mouse(mouse) => Some(*mouse),
                        InputRecord::Key(_) => None,
                    })
                }
                _ => None,
            })
            .collect::<Vec<_>>();
        assert_eq!(mice[2].button_state, RIGHTMOST_BUTTON_PRESSED);
        assert_eq!(mice[2].event_flags, DOUBLE_CLICK);
        assert_eq!(mice[3].button_state & 0xffff_0000, SCROLL_DELTA_BACKWARD);
        assert_eq!(mice[3].event_flags, MOUSE_HWHEELED);
        assert_eq!(mice[4].button_state & 0xffff_0000, SCROLL_DELTA_FORWARD);
        assert_eq!(mice[4].event_flags, MOUSE_HWHEELED);
    }

    #[test]
    fn mouse_modifier_bits_match_terminal_encoding() {
        let mut machine = machine();
        machine.process_str("\u{1b}[<28;1;1M");
        let InputAction::WriteInput(records) = &machine.engine().dispatch().actions[0] else {
            panic!("expected mouse input");
        };
        let InputRecord::Mouse(mouse) = records[0] else {
            panic!("expected mouse record");
        };
        assert_eq!(
            mouse.control_key_state,
            SHIFT_PRESSED | LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED
        );
    }

    #[test]
    fn osc_and_vt52_responses_are_raw_passthrough() {
        let mut machine = machine();
        machine.process_str("\u{1b}]10;rgb:ff/00/00\u{7}");
        machine.set_parser_mode(ParserMode::Ansi, false);
        machine.process_str("\u{1b}A");
        assert_eq!(machine.engine().dispatch().actions.len(), 2);
        assert!(matches!(
            machine.engine().dispatch().actions[0],
            InputAction::WriteStringRaw(_)
        ));
        assert!(matches!(
            machine.engine().dispatch().actions[1],
            InputAction::WriteStringRaw(_)
        ));
    }

    #[test]
    fn unhandled_dcs_is_buffered_until_string_terminator_and_flushed_raw() {
        let mut machine = machine();
        machine.process_str("\u{1b}P1;2qabc");
        assert!(machine.engine().dispatch().actions.is_empty());
        machine.process_str("def\u{1b}\\");
        assert_eq!(
            machine.engine().dispatch().actions,
            [InputAction::WriteStringRaw(
                "\u{1b}P1;2qabcdef\u{1b}\\".encode_utf16().collect()
            )]
        );
    }
}
