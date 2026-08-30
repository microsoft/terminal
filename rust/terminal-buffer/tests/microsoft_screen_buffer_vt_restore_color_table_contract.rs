use terminal_buffer::color_table::ColorTableState;
use terminal_buffer::text_color::Rgb;

fn set_first_sixteen_white(state: &mut ColorTableState) {
    for index in 0..16 {
        assert!(state.apply_osc(4, &format!("{index};rgb:ff/ff/ff")));
        assert_eq!(state.color(index), Some(Rgb::new(255, 255, 255)));
    }
}

#[test]
fn microsoft_screen_buffer_vt_restore_color_table_report_contract() {
    let mut state = ColorTableState::default();
    let initial_campbell: [Rgb; 16] =
        std::array::from_fn(|index| state.color(index).expect("Campbell entry exists"));

    set_first_sixteen_white(&mut state);

    let hls_vectors = [
        (0, 0, 0, 0, Rgb::new(0, 0, 0)),
        (1, 0, 49, 59, Rgb::new(51, 51, 199)),
        (2, 120, 46, 71, Rgb::new(201, 34, 34)),
        (3, 240, 49, 59, Rgb::new(51, 199, 51)),
        (4, 60, 49, 59, Rgb::new(199, 51, 199)),
        (5, 300, 49, 59, Rgb::new(51, 199, 199)),
        (6, 180, 49, 59, Rgb::new(199, 199, 51)),
        (7, 0, 46, 0, Rgb::new(117, 117, 117)),
        (8, 0, 26, 0, Rgb::new(66, 66, 66)),
        (9, 0, 46, 28, Rgb::new(84, 84, 150)),
        (10, 120, 42, 38, Rgb::new(148, 66, 66)),
        (11, 240, 46, 28, Rgb::new(84, 150, 84)),
        (12, 60, 46, 28, Rgb::new(150, 84, 150)),
        (13, 300, 46, 28, Rgb::new(84, 150, 150)),
        (14, 180, 46, 28, Rgb::new(150, 150, 84)),
        (15, 0, 79, 0, Rgb::new(201, 201, 201)),
    ];

    for (index, hue, lightness, saturation, expected) in hls_vectors {
        assert!(
            state.apply_dec_color_definitions(&format!("{index};1;{hue};{lightness};{saturation}"))
        );
        assert_eq!(state.color(index), Some(expected), "HLS index={index}");
    }

    set_first_sixteen_white(&mut state);

    let rgb_vectors = [
        (0, 0, 0, 0, Rgb::new(0, 0, 0)),
        (1, 20, 20, 78, Rgb::new(51, 51, 199)),
        (2, 79, 13, 13, Rgb::new(201, 33, 33)),
        (3, 20, 78, 20, Rgb::new(51, 199, 51)),
        (4, 78, 20, 78, Rgb::new(199, 51, 199)),
        (5, 20, 78, 78, Rgb::new(51, 199, 199)),
        (6, 78, 78, 20, Rgb::new(199, 199, 51)),
        (7, 46, 46, 46, Rgb::new(117, 117, 117)),
        (8, 26, 26, 26, Rgb::new(66, 66, 66)),
        (9, 33, 33, 59, Rgb::new(84, 84, 150)),
        (10, 58, 26, 26, Rgb::new(148, 66, 66)),
        (11, 33, 59, 33, Rgb::new(84, 150, 84)),
        (12, 59, 33, 59, Rgb::new(150, 84, 150)),
        (13, 33, 59, 59, Rgb::new(84, 150, 150)),
        (14, 59, 59, 33, Rgb::new(150, 150, 84)),
        (15, 79, 79, 79, Rgb::new(201, 201, 201)),
    ];

    for (index, red, green, blue, expected) in rgb_vectors {
        assert!(state.apply_dec_color_definitions(&format!("{index};2;{red};{green};{blue}")));
        assert_eq!(state.color(index), Some(expected), "RGB index={index}");
    }

    set_first_sixteen_white(&mut state);

    assert!(state.apply_dec_color_definitions("0;1;120;50;100/2;1;240;50;100/4;1;360;50;100"));
    assert_eq!(state.color(0), Some(Rgb::new(255, 0, 0)));
    assert_eq!(state.color(2), Some(Rgb::new(0, 255, 0)));
    assert_eq!(state.color(4), Some(Rgb::new(0, 0, 255)));

    assert!(state.apply_dec_color_definitions("1;2;100;0;0/3;2;0;100;0/5;2;0;0;100"));
    assert_eq!(state.color(1), Some(Rgb::new(255, 0, 0)));
    assert_eq!(state.color(3), Some(Rgb::new(0, 255, 0)));
    assert_eq!(state.color(5), Some(Rgb::new(0, 0, 255)));

    assert!(state.apply_dec_color_definitions("6;1;;50;100"));
    assert_eq!(state.color(6), Some(Rgb::new(0, 0, 255)));
    assert!(state.apply_dec_color_definitions("7;1;120;;100"));
    assert_eq!(state.color(7), Some(Rgb::new(0, 0, 0)));
    assert!(state.apply_dec_color_definitions("8;1;120;50"));
    assert_eq!(state.color(8), Some(Rgb::new(128, 128, 128)));

    assert!(state.apply_dec_color_definitions("6;2;;50;100"));
    assert_eq!(state.color(6), Some(Rgb::new(0, 128, 255)));
    assert!(state.apply_dec_color_definitions("7;2;50;;100"));
    assert_eq!(state.color(7), Some(Rgb::new(128, 0, 255)));
    assert!(state.apply_dec_color_definitions("8;2;50;100"));
    assert_eq!(state.color(8), Some(Rgb::new(128, 255, 0)));

    assert!(state.apply_dec_color_definitions("9;1;480;50;100"));
    assert_eq!(state.color(9), Some(Rgb::new(255, 0, 0)));
    assert!(state.apply_dec_color_definitions("10;1;240;150;100"));
    assert_eq!(state.color(10), Some(Rgb::new(255, 255, 255)));
    assert!(state.apply_dec_color_definitions("11;1;0;50;120"));
    assert_eq!(state.color(11), Some(Rgb::new(0, 0, 255)));
    assert!(state.apply_dec_color_definitions("12;2;150;0;0"));
    assert_eq!(state.color(12), Some(Rgb::new(255, 0, 0)));
    assert!(state.apply_dec_color_definitions("13;2;0;150;0"));
    assert_eq!(state.color(13), Some(Rgb::new(0, 255, 0)));
    assert!(state.apply_dec_color_definitions("14;2;0;0;150"));
    assert_eq!(state.color(14), Some(Rgb::new(0, 0, 255)));

    state.reset_to_initial();
    for (index, expected) in initial_campbell.into_iter().enumerate() {
        assert_eq!(state.color(index), Some(expected), "RIS index={index}");
    }
}
