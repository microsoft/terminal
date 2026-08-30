use terminal_adapter::adapt_dispatch::{PageGeometry, Point};
use terminal_adapter::dcs_dispatch::AdapterDispatch;
use terminal_parser::output_engine::{OutputAction, TermDispatch};

const START: Point = Point { x: 50, y: 34 };
const HOME: Point = Point { x: 0, y: 20 };

fn adapter() -> AdapterDispatch {
    let mut adapter = AdapterDispatch::new(PageGeometry::new(20, 100, 29));
    adapter.core_mut().set_cursor(START);
    adapter
}

fn reset_cursor(adapter: &mut AdapterDispatch) {
    adapter.core_mut().set_cursor(START);
}

fn assert_pages(adapter: &AdapterDispatch, active: i32, visible: i32) {
    assert_eq!(adapter.page_manager().active_page_number(), active);
    assert_eq!(adapter.page_manager().visible_page_number(), visible);
}

#[test]
fn microsoft_adapter_page_movement_matches_ppa_ppr_ppb_np_pp_and_decpccm() {
    let mut adapter = adapter();

    // PPA: absolute page moves preserve cursor coordinates and clamp to six pages.
    assert_pages(&adapter, 1, 1);
    adapter.dispatch(OutputAction::PagePositionAbsolute(3));
    assert_pages(&adapter, 3, 3);
    adapter.dispatch(OutputAction::PagePositionAbsolute(1));
    assert_pages(&adapter, 1, 1);
    adapter.dispatch(OutputAction::PagePositionAbsolute(9_999));
    assert_pages(&adapter, 6, 6);
    assert_eq!(adapter.core().cursor(), START);

    // PPR: omitted count is one and the upper bound remains page six.
    reset_cursor(&mut adapter);
    adapter.dispatch(OutputAction::PagePositionAbsolute(1));
    adapter.dispatch(OutputAction::PagePositionRelative(2));
    assert_pages(&adapter, 3, 3);
    adapter.dispatch(OutputAction::PagePositionRelative(1));
    assert_pages(&adapter, 4, 4);
    adapter.dispatch(OutputAction::PagePositionRelative(9_999));
    assert_pages(&adapter, 6, 6);
    assert_eq!(adapter.core().cursor(), START);

    // PPB: relative backwards movement clamps at page one without homing.
    reset_cursor(&mut adapter);
    adapter.dispatch(OutputAction::PagePositionBack(2));
    assert_pages(&adapter, 4, 4);
    adapter.dispatch(OutputAction::PagePositionBack(1));
    assert_pages(&adapter, 3, 3);
    adapter.dispatch(OutputAction::PagePositionBack(9_999));
    assert_pages(&adapter, 1, 1);
    assert_eq!(adapter.core().cursor(), START);

    // NP and PP differ from PPR/PPB by homing the cursor after the page move.
    reset_cursor(&mut adapter);
    adapter.dispatch(OutputAction::NextPage(2));
    assert_pages(&adapter, 3, 3);
    adapter.dispatch(OutputAction::NextPage(1));
    assert_pages(&adapter, 4, 4);
    adapter.dispatch(OutputAction::NextPage(9_999));
    assert_pages(&adapter, 6, 6);
    assert_eq!(adapter.core().cursor(), HOME);

    reset_cursor(&mut adapter);
    adapter.dispatch(OutputAction::PrecedingPage(2));
    assert_pages(&adapter, 4, 4);
    adapter.dispatch(OutputAction::PrecedingPage(1));
    assert_pages(&adapter, 3, 3);
    adapter.dispatch(OutputAction::PrecedingPage(9_999));
    assert_pages(&adapter, 1, 1);
    assert_eq!(adapter.core().cursor(), HOME);

    // DECPCCM couples visible page to active page; recoupling reveals active page.
    adapter.dispatch(OutputAction::SetMode {
        private: true,
        enabled: true,
        mode: 64,
    });
    adapter.dispatch(OutputAction::PagePositionAbsolute(2));
    assert_pages(&adapter, 2, 2);
    adapter.dispatch(OutputAction::SetMode {
        private: true,
        enabled: false,
        mode: 64,
    });
    adapter.dispatch(OutputAction::PagePositionAbsolute(4));
    assert_pages(&adapter, 4, 2);
    adapter.dispatch(OutputAction::SetMode {
        private: true,
        enabled: true,
        mode: 64,
    });
    assert_pages(&adapter, 4, 4);

    adapter.dispatch(OutputAction::PagePositionAbsolute(1));
    assert_pages(&adapter, 1, 1);
}
