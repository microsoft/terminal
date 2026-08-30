use terminal_buffer::host_write::HostWriteState;
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_buffer::text_color::TextColor;
use terminal_buffer::virtual_bottom::VirtualBottomState;

const INACTIVE_CONTROLS: [u16; 24] = [
    0, 1, 2, 3, 4, 5, 6, 7, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 28, 29, 30, 31,
];

fn assert_ascii(buffer: &TextBuffer, row: u16, text: &[u8]) {
    for (column, byte) in text.iter().copied().enumerate() {
        assert_eq!(
            buffer.row(i32::from(row)).glyph_at(
                i32::try_from(column).expect("fixture column fits the text-buffer coordinate")
            ),
            &[u16::from(byte)]
        );
    }
}

#[test]
fn microsoft_screen_buffer_inactive_control_characters_contract() {
    let default = TextAttribute::default();

    for ordinal in INACTIVE_CONTROLS {
        let mut buffer = TextBuffer::new(16, 2, default).expect("fixture dimensions are valid");
        let mut writer = HostWriteState::new(TextBufferPoint { x: 0, y: 0 }, default);

        writer
            .write_vt(&mut buffer, &[ordinal])
            .expect("single inactive control is accepted");
        assert_eq!(writer.cursor(), TextBufferPoint { x: 0, y: 0 });
        assert_eq!(buffer.row(0).measure_right(), 0);

        writer
            .write_vt(&mut buffer, &[ordinal; 8])
            .expect("repeated inactive controls are accepted");
        assert_eq!(writer.cursor(), TextBufferPoint { x: 0, y: 0 });
        assert_eq!(buffer.row(0).measure_right(), 0);

        writer
            .write_vt(
                &mut buffer,
                &[ordinal, u16::from(b'f'), u16::from(b'o'), u16::from(b'o')],
            )
            .expect("inactive control before printable text is ignored");
        assert_eq!(writer.cursor(), TextBufferPoint { x: 3, y: 0 });
        assert_ascii(&buffer, 0, b"foo");

        // Microsoft's source uses LF as setup before this final case. LF is
        // independently owned by the line-feed seam, so start the host writer
        // at the equivalent next-row cursor and isolate the inactive controls.
        let mut next_row = HostWriteState::new(TextBufferPoint { x: 0, y: 1 }, default);
        next_row
            .write_vt(
                &mut buffer,
                &[
                    ordinal,
                    u16::from(b'f'),
                    u16::from(b'o'),
                    u16::from(b'o'),
                    ordinal,
                    u16::from(b'b'),
                    u16::from(b'a'),
                    u16::from(b'r'),
                    ordinal,
                ],
            )
            .expect("inactive controls between printable runs are ignored");
        assert_eq!(next_row.cursor(), TextBufferPoint { x: 6, y: 1 });
        assert_ascii(&buffer, 1, b"foobar");
    }
}

#[test]
fn microsoft_screen_buffer_dont_reset_colors_above_virtual_bottom_contract() {
    let default = TextAttribute::default();
    let mut colored = default;
    colored.set_foreground(TextColor::index16(TextColor::DARK_RED));
    colored.set_background(TextColor::index16(TextColor::DARK_BLUE));

    let mut buffer = TextBuffer::new(10, 8, default).expect("fixture dimensions are valid");
    let mut viewport = VirtualBottomState::new(10, 4);

    // Microsoft begins with the viewport one row below the physical origin and
    // the cursor on its bottom row. This updates virtual bottom to that row.
    viewport.set_viewport_origin(0, 1, true);
    let output_row = viewport.viewport().bottom();
    viewport.set_cursor_direct(0, output_row);
    assert_eq!(viewport.virtual_bottom(), output_row);

    let mut writer = HostWriteState::new(
        TextBufferPoint {
            x: 0,
            y: output_row,
        },
        colored,
    );
    writer
        .write_vt(&mut buffer, &[u16::from(b'X')])
        .expect("colored write succeeds");
    writer.set_current_attribute(default);
    writer
        .write_vt(&mut buffer, &[u16::from(b'X')])
        .expect("default-colored write succeeds");
    viewport.set_cursor_direct(writer.cursor().x, writer.cursor().y);

    let row = buffer.row(i32::from(output_row));
    assert_eq!(row.attribute_at(0), colored);
    assert_eq!(row.attribute_at(1), default);

    // Mouse-style scrollback moves only the visible viewport. Cursor and virtual
    // bottom remain at the output row, which is now below the visible viewport.
    viewport.set_viewport_origin(0, 0, false);
    assert!(viewport.cursor().y > viewport.viewport().bottom());
    assert_eq!(viewport.virtual_bottom(), output_row);

    writer
        .write_vt(&mut buffer, &[u16::from(b'X')])
        .expect("offscreen write succeeds");
    viewport.set_cursor_direct(writer.cursor().x, writer.cursor().y);

    let row = buffer.row(i32::from(output_row));
    assert_eq!(row.glyph_at(0), &[u16::from(b'X')]);
    assert_eq!(row.glyph_at(1), &[u16::from(b'X')]);
    assert_eq!(row.glyph_at(2), &[u16::from(b'X')]);
    assert_eq!(row.attribute_at(0), colored);
    assert_eq!(row.attribute_at(1), default);
    assert_eq!(row.attribute_at(2), default);
    assert_eq!(
        writer.cursor(),
        TextBufferPoint {
            x: 3,
            y: output_row
        }
    );
    assert_eq!(viewport.virtual_bottom(), output_row);
}
