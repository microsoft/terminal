//! Portable input-buffer queue/coalescing semantics from conhost.
//!
//! This owner carries deterministic `InputBuffer` behavior without Win32
//! handles. Codepage conversion remains a platform adapter; the buffer owns the
//! resulting DBCS padding/order once encoded bytes are supplied.

use std::collections::VecDeque;

pub const MOUSE_MOVED: u32 = 0x0001;
pub const VK_SHIFT: u16 = 0x10;
pub const VK_CONTROL: u16 = 0x11;
pub const VK_MENU: u16 = 0x12;
pub const VK_PAUSE: u16 = 0x13;
pub const VK_CAPITAL: u16 = 0x14;
pub const VK_NUMLOCK: u16 = 0x90;
pub const VK_SCROLL: u16 = 0x91;
pub const DEFAULT_INPUT_MODE: u32 = 0x01f7;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct KeyEvent {
    pub key_down: bool,
    pub repeat_count: u16,
    pub virtual_key: u16,
    pub scan_code: u16,
    pub unicode_char: u16,
    pub control_key_state: u32,
}

impl KeyEvent {
    #[must_use]
    pub const fn new(
        key_down: bool,
        repeat_count: u16,
        virtual_key: u16,
        scan_code: u16,
        unicode_char: u16,
        control_key_state: u32,
    ) -> Self {
        Self {
            key_down,
            repeat_count,
            virtual_key,
            scan_code,
            unicode_char,
            control_key_state,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MouseEvent {
    pub x: i16,
    pub y: i16,
    pub event_flags: u32,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum InputEvent {
    Key(KeyEvent),
    Mouse(MouseEvent),
    Menu,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ReadOptions {
    pub peek: bool,
    pub stream: bool,
}

impl ReadOptions {
    pub const NORMAL: Self = Self {
        peek: false,
        stream: false,
    };
    pub const PEEK: Self = Self {
        peek: true,
        stream: false,
    };
    pub const STREAM: Self = Self {
        peek: false,
        stream: true,
    };
    pub const STREAM_PEEK: Self = Self {
        peek: true,
        stream: true,
    };
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InputBuffer {
    storage: VecDeque<InputEvent>,
    input_mode: u32,
    wait_signaled: bool,
    output_suspended: bool,
}

impl Default for InputBuffer {
    fn default() -> Self {
        Self {
            storage: VecDeque::new(),
            input_mode: DEFAULT_INPUT_MODE,
            wait_signaled: false,
            output_suspended: false,
        }
    }
}

impl InputBuffer {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    #[must_use]
    pub fn ready_event_count(&self) -> usize {
        self.storage.len()
    }

    #[must_use]
    pub fn events(&self) -> &VecDeque<InputEvent> {
        &self.storage
    }

    #[must_use]
    pub const fn input_mode(&self) -> u32 {
        self.input_mode
    }

    pub const fn set_input_mode(&mut self, mode: u32) {
        self.input_mode = mode;
    }

    #[must_use]
    pub const fn wait_signaled(&self) -> bool {
        self.wait_signaled
    }

    pub const fn set_wait_signaled(&mut self, signaled: bool) {
        self.wait_signaled = signaled;
    }

    #[must_use]
    pub const fn output_suspended(&self) -> bool {
        self.output_suspended
    }

    pub fn write(&mut self, event: InputEvent) -> usize {
        if self.handle_console_suspension(&event) {
            return 0;
        }

        if self.try_coalesce(&event) {
            self.wait_signaled = true;
            return 1;
        }

        self.storage.push_back(event);
        self.wait_signaled = true;
        1
    }

    pub fn write_bulk<I>(&mut self, events: I) -> usize
    where
        I: IntoIterator<Item = InputEvent>,
    {
        let events: Vec<_> = events.into_iter().collect();
        let count = events.len();
        self.storage.extend(events);
        if count != 0 {
            self.wait_signaled = true;
        }
        count
    }

    pub fn prepend<I>(&mut self, events: I) -> usize
    where
        I: IntoIterator<Item = InputEvent>,
    {
        let events: Vec<_> = events.into_iter().collect();
        let count = events.len();
        for event in events.into_iter().rev() {
            self.storage.push_front(event);
        }
        if count != 0 {
            self.wait_signaled = true;
        }
        count
    }

    pub fn flush(&mut self) {
        self.storage.clear();
        self.wait_signaled = false;
    }

    pub fn flush_all_but_keys(&mut self) {
        self.storage
            .retain(|event| matches!(event, InputEvent::Key(_)));
        self.wait_signaled = !self.storage.is_empty();
    }

    pub fn reinitialize(&mut self) {
        self.storage.clear();
        self.input_mode = DEFAULT_INPUT_MODE;
        self.wait_signaled = false;
        self.output_suspended = false;
    }

    #[must_use]
    pub fn read(&mut self, count: usize, options: ReadOptions) -> Vec<InputEvent> {
        if options.stream {
            return self.read_stream(count, options.peek);
        }

        let amount = count.min(self.storage.len());
        let output = if options.peek {
            self.storage.iter().take(amount).cloned().collect()
        } else {
            self.storage.drain(..amount).collect()
        };
        if !options.peek {
            self.wait_signaled = !self.storage.is_empty();
        }
        output
    }

    /// Applies the portable DBCS-padding part of the non-Unicode read contract.
    /// Codepage conversion itself is supplied by the platform adapter.
    #[must_use]
    pub fn read_with_codepage<F>(&mut self, output_slots: usize, mut encode: F) -> Vec<InputEvent>
    where
        F: FnMut(u16) -> Option<Vec<u8>>,
    {
        let mut output = Vec::new();
        while output.len() < output_slots {
            let Some(event) = self.storage.pop_front() else {
                break;
            };
            match event {
                InputEvent::Key(key) if key.unicode_char > 0x7f => {
                    if let Some(bytes) = encode(key.unicode_char) {
                        for byte in bytes {
                            if output.len() == output_slots {
                                break;
                            }
                            let mut converted = key.clone();
                            converted.unicode_char = u16::from(byte);
                            output.push(InputEvent::Key(converted));
                        }
                    } else {
                        output.push(InputEvent::Key(key));
                    }
                }
                other => output.push(other),
            }
        }
        self.wait_signaled = !self.storage.is_empty();
        output
    }

    fn read_stream(&mut self, count: usize, peek: bool) -> Vec<InputEvent> {
        let mut output = Vec::new();
        for _ in 0..count {
            let Some(front) = self.storage.front().cloned() else {
                break;
            };
            match front {
                InputEvent::Key(mut key) if key.repeat_count > 1 => {
                    key.repeat_count = 1;
                    output.push(InputEvent::Key(key));
                    if !peek {
                        let InputEvent::Key(stored) = self
                            .storage
                            .front_mut()
                            .expect("front exists while stream-reading")
                        else {
                            unreachable!("matched key front must remain a key")
                        };
                        stored.repeat_count -= 1;
                    }
                }
                other => {
                    output.push(other);
                    if !peek {
                        self.storage.pop_front();
                    }
                }
            }
        }
        if !peek {
            self.wait_signaled = !self.storage.is_empty();
        }
        output
    }

    fn try_coalesce(&mut self, event: &InputEvent) -> bool {
        let Some(last) = self.storage.back_mut() else {
            return false;
        };
        match (last, event) {
            (InputEvent::Mouse(previous), InputEvent::Mouse(current))
                if previous.event_flags == MOUSE_MOVED && current.event_flags == MOUSE_MOVED =>
            {
                previous.x = current.x;
                previous.y = current.y;
                true
            }
            (InputEvent::Key(previous), InputEvent::Key(current))
                if keys_match_for_coalescing(previous, current) =>
            {
                previous.repeat_count = previous.repeat_count.saturating_add(current.repeat_count);
                true
            }
            _ => false,
        }
    }

    fn handle_console_suspension(&mut self, event: &InputEvent) -> bool {
        let InputEvent::Key(key) = event else {
            return false;
        };
        if !key.key_down {
            return false;
        }

        if !self.output_suspended && key.virtual_key == VK_PAUSE {
            self.output_suspended = true;
            return true;
        }
        if self.output_suspended && !is_system_key(key.virtual_key) {
            self.output_suspended = false;
            return true;
        }
        false
    }
}

fn keys_match_for_coalescing(left: &KeyEvent, right: &KeyEvent) -> bool {
    !is_surrogate(left.unicode_char)
        && !is_surrogate(right.unicode_char)
        && left.key_down == right.key_down
        && left.virtual_key == right.virtual_key
        && left.scan_code == right.scan_code
        && left.unicode_char == right.unicode_char
        && left.control_key_state == right.control_key_state
}

const fn is_surrogate(value: u16) -> bool {
    value >= 0xd800 && value <= 0xdfff
}

const fn is_system_key(virtual_key: u16) -> bool {
    matches!(
        virtual_key,
        VK_SHIFT | VK_CONTROL | VK_MENU | VK_CAPITAL | VK_NUMLOCK | VK_SCROLL
    )
}
