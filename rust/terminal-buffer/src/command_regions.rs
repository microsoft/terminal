//! Portable shell-integration command and prompt region tracking.
//!
//! Windows Terminal's OSC 133 integration marks prompt, command, and output
//! boundaries independently from the VT parser. This module owns the buffer
//! state produced by those dispatch events: command capture, wrapped command
//! rows, scrollbar marks, and prompt/command/output extents.

use crate::geometry::Point;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum RegionPhase {
    Idle,
    Prompt,
    Command,
    Output,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CommandMark {
    pub start: Point,
    pub end: Point,
    pub command_end: Option<Point>,
    pub output_end: Option<Point>,
}

impl CommandMark {
    fn new(start: Point) -> Self {
        Self {
            start,
            end: start,
            command_end: None,
            output_end: None,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CommandRegionState {
    width: i32,
    cursor: Point,
    phase: RegionPhase,
    marks: Vec<CommandMark>,
    marked_rows: Vec<i32>,
    active_mark: Option<usize>,
    current_command: String,
    completed_commands: Vec<String>,
    last_written_end: Option<Point>,
}

impl CommandRegionState {
    #[must_use]
    pub fn new(width: i32) -> Self {
        assert!(width > 0, "command-region width must be positive");
        Self {
            width,
            cursor: Point::default(),
            phase: RegionPhase::Idle,
            marks: Vec::new(),
            marked_rows: Vec::new(),
            active_mark: None,
            current_command: String::new(),
            completed_commands: Vec::new(),
            last_written_end: None,
        }
    }

    #[must_use]
    pub const fn cursor(&self) -> Point {
        self.cursor
    }

    #[must_use]
    pub fn current_command(&self) -> &str {
        &self.current_command
    }

    #[must_use]
    pub fn commands(&self) -> Vec<String> {
        let mut commands = self.completed_commands.clone();
        if self.phase == RegionPhase::Command && !self.current_command.is_empty() {
            commands.push(self.current_command.clone());
        }
        commands
    }

    #[must_use]
    pub fn marks(&self) -> &[CommandMark] {
        &self.marks
    }

    #[must_use]
    pub fn row_has_scrollbar_data(&self, row: i32) -> bool {
        self.marked_rows.contains(&row)
    }

    /// OSC 133;A — begin a prompt and attach scrollbar metadata to this row.
    pub fn prompt_start(&mut self) {
        let index = self.marks.len();
        self.marks.push(CommandMark::new(self.cursor));
        if !self.marked_rows.contains(&self.cursor.y) {
            self.marked_rows.push(self.cursor.y);
        }
        self.active_mark = Some(index);
        self.phase = RegionPhase::Prompt;
        self.last_written_end = None;
    }

    /// OSC 133;B — prompt complete, command input begins.
    pub fn command_start(&mut self) {
        if let Some(mark) = self.active_mark.and_then(|index| self.marks.get_mut(index)) {
            mark.end = self.cursor;
        }
        self.current_command.clear();
        self.phase = RegionPhase::Command;
        self.last_written_end = None;
    }

    /// OSC 133;C — command complete, output begins.
    pub fn command_end(&mut self) {
        if let Some(mark) = self.active_mark.and_then(|index| self.marks.get_mut(index)) {
            mark.command_end = Some(self.cursor);
        }
        if !self.current_command.is_empty() {
            self.completed_commands.push(self.current_command.clone());
        }
        self.phase = RegionPhase::Output;
        self.last_written_end = None;
    }

    /// OSC 133;D — output complete. The output extent ends at the last written
    /// cell, not at the cursor position after a trailing CR/LF.
    pub fn command_finished(&mut self) {
        if self.phase == RegionPhase::Output
            && let (Some(index), Some(end)) = (self.active_mark, self.last_written_end)
            && let Some(mark) = self.marks.get_mut(index)
        {
            mark.output_end = Some(end);
        }
        self.phase = RegionPhase::Idle;
        self.last_written_end = None;
    }

    /// Writes printable text. Presentation-only controls such as SGR are
    /// intentionally handled outside this owner and therefore never enter the
    /// command string.
    pub fn write_text(&mut self, text: &str) {
        for ch in text.chars() {
            if self.phase == RegionPhase::Command {
                self.current_command.push(ch);
            }
            self.advance_printable();
            self.last_written_end = Some(self.cursor);
        }
    }

    pub fn line_feed(&mut self) {
        self.cursor.y = self.cursor.y.saturating_add(1);
    }

    pub fn carriage_return(&mut self) {
        self.cursor.x = 0;
    }

    pub fn crlf(&mut self) {
        self.carriage_return();
        self.line_feed();
    }

    fn advance_printable(&mut self) {
        self.cursor.x = self.cursor.x.saturating_add(1);
        if self.cursor.x >= self.width {
            self.cursor.x = 0;
            self.cursor.y = self.cursor.y.saturating_add(1);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn write_prompt(state: &mut CommandRegionState, path: &str) {
        state.command_finished(); // OSC 133;D
        state.prompt_start(); // OSC 133;A
        // OSC 9;9 working-directory metadata is non-printing.
        state.write_text("PWSH ");
        state.write_text(path);
        state.write_text("> ");
        state.command_start(); // OSC 133;B
    }

    #[test]
    fn microsoft_simple_mark_command_contract() {
        let mut state = CommandRegionState::new(80);
        state.write_text("Zero");
        state.line_feed();

        let first_prompt_row = state.cursor().y;
        state.prompt_start();
        state.write_text("A Prompt");
        state.command_start();
        state.write_text("my_command");
        state.command_end();
        state.line_feed();

        assert!(state.row_has_scrollbar_data(first_prompt_row));

        state.write_text("Two");
        state.line_feed();
        assert_eq!(state.current_command(), "my_command");

        state.command_finished();
        state.prompt_start();
        state.write_text("B Prompt");
        state.command_start();
        assert_eq!(state.current_command(), "");

        state.write_text("some of a command");
        // Microsoft's SGR 31 between these writes is presentation-only.
        state.write_text(" & more of a command");

        assert_eq!(
            state.current_command(),
            "some of a command & more of a command"
        );
        assert_eq!(
            state.commands(),
            vec![
                "my_command".to_string(),
                "some of a command & more of a command".to_string()
            ]
        );
    }

    #[test]
    fn microsoft_simple_wrapped_command_contract() {
        let mut state = CommandRegionState::new(80);
        state.write_text("Zero");
        state.line_feed();

        let original_row = state.cursor().y;
        state.prompt_start();
        state.write_text("A Prompt");
        state.command_start();

        let wrapped_command = "0".repeat(100);
        state.write_text(&wrapped_command);
        let continuation_row = state.cursor().y;
        assert_ne!(original_row, continuation_row);
        assert!(state.row_has_scrollbar_data(original_row));
        assert!(!state.row_has_scrollbar_data(continuation_row));

        state.command_end();
        state.line_feed();
        state.write_text("Two");
        state.line_feed();
        assert_eq!(state.current_command(), wrapped_command);

        state.command_finished();
        state.prompt_start();
        state.write_text("B Prompt");
        state.command_start();
        state.write_text("some of a command");
        state.write_text(" & more of a command");

        assert_eq!(
            state.commands(),
            vec![
                wrapped_command,
                "some of a command & more of a command".to_string()
            ]
        );
    }

    #[test]
    fn microsoft_simple_prompt_regions_contract() {
        let mut state = CommandRegionState::new(80);

        write_prompt(&mut state, r"C:\Windows");
        state.write_text("Foo-bar");
        state.command_end();
        state.crlf();
        state.write_text("This is some text     ");
        state.crlf();
        state.write_text("with varying amounts  ");
        state.crlf();
        state.write_text("of whitespace         ");
        state.crlf();

        write_prompt(&mut state, r"C:\Windows");

        assert_eq!(state.cursor(), Point::new(17, 4));
        assert!(state.row_has_scrollbar_data(0));
        assert!(state.row_has_scrollbar_data(4));
        assert_eq!(state.marks().len(), 2);

        let first = &state.marks()[0];
        assert_eq!(first.start, Point::new(0, 0));
        assert_eq!(first.end, Point::new(17, 0));
        assert_eq!(first.command_end, Some(Point::new(24, 0)));
        assert_eq!(first.output_end, Some(Point::new(22, 3)));

        let second = &state.marks()[1];
        assert_eq!(second.start, Point::new(0, 4));
        assert_eq!(second.end, Point::new(17, 4));
        assert_eq!(second.command_end, None);
        assert_eq!(second.output_end, None);
    }
}
