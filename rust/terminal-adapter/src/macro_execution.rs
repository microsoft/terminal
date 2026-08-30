//! Safe live-parser execution seam for DEC macro invocation.
//!
//! `TermDispatch` cannot hold a mutable reference back to the `StateMachine`
//! that owns it. Windows Terminal's DECMAC invocation, however, requires an
//! invoked macro to be fed immediately back through that same parser. This
//! decorator keeps the ownership acyclic: it intercepts `CSI Ps * z`, prepares
//! the macro from the product's canonical [`MacroBuffer`], and exposes an owned
//! pending sequence to [`MacroExecutingProduct`], which feeds it back into the
//! same live parser before processing the next outer code unit.

use terminal_parser::{
    output_engine::{DcsAction, OutputAction, OutputStateMachineEngine, TermDispatch},
    state_machine::{Parameters, StateMachine, VtId},
};

use crate::{
    adapt_dispatch::PageGeometry, macro_buffer::InvocationContext,
    product_dispatch::AdaptDispatchProductState,
};

const INVOKE_MACRO_ID: &str = "*z";

#[derive(Debug, Clone, PartialEq, Eq)]
struct PendingMacroInvocation {
    sequence: Vec<u16>,
    context: InvocationContext,
}

pub struct MacroExecutionDispatch {
    product: AdaptDispatchProductState,
    invocation_context: InvocationContext,
    pending: Option<PendingMacroInvocation>,
    printed: Vec<u16>,
}

impl MacroExecutionDispatch {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        Self {
            product: AdaptDispatchProductState::new(geometry),
            invocation_context: InvocationContext::default(),
            pending: None,
            printed: Vec::new(),
        }
    }

    #[must_use]
    pub const fn product(&self) -> &AdaptDispatchProductState {
        &self.product
    }

    pub const fn product_mut(&mut self) -> &mut AdaptDispatchProductState {
        &mut self.product
    }

    #[must_use]
    pub fn printed_text(&self) -> String {
        String::from_utf16_lossy(&self.printed)
    }

    pub fn clear_printed_text(&mut self) {
        self.printed.clear();
    }

    const fn set_invocation_context(&mut self, context: InvocationContext) {
        self.invocation_context = context;
    }

    #[must_use]
    const fn invocation_context(&self) -> InvocationContext {
        self.invocation_context
    }

    fn take_pending(&mut self) -> Option<PendingMacroInvocation> {
        self.pending.take()
    }

    fn invoke_macro(&mut self, parameters: &Parameters) {
        let macro_id = parameters.at(0).unwrap_or(0);
        let Ok(macro_id) = usize::try_from(macro_id) else {
            return;
        };
        let Some(prepared) = self
            .product
            .macro_reports()
            .buffer()
            .prepare_invoke(macro_id, self.invocation_context)
        else {
            return;
        };

        self.pending = Some(PendingMacroInvocation {
            sequence: prepared.sequence().to_vec(),
            context: prepared.context(),
        });
    }
}

impl TermDispatch for MacroExecutionDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        match action {
            OutputAction::AdvancedCsi { id, parameters }
                if id == VtId::from_ascii(INVOKE_MACRO_ID) =>
            {
                self.invoke_macro(&parameters);
            }
            OutputAction::Print(unit) => {
                self.printed.push(unit);
                self.product.dispatch(OutputAction::Print(unit));
            }
            OutputAction::PrintString(text) => {
                self.printed.extend_from_slice(&text);
                self.product.dispatch(OutputAction::PrintString(text));
            }
            other => self.product.dispatch(other),
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        self.product.begin_dcs(action)
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        self.product.dcs_put(code_unit)
    }
}

pub struct MacroExecutingProduct {
    machine: StateMachine<OutputStateMachineEngine<MacroExecutionDispatch>>,
}

impl MacroExecutingProduct {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        Self {
            machine: StateMachine::new(OutputStateMachineEngine::new(MacroExecutionDispatch::new(
                geometry,
            ))),
        }
    }

    #[must_use]
    pub const fn machine(&self) -> &StateMachine<OutputStateMachineEngine<MacroExecutionDispatch>> {
        &self.machine
    }

    pub fn machine_mut(
        &mut self,
    ) -> &mut StateMachine<OutputStateMachineEngine<MacroExecutionDispatch>> {
        &mut self.machine
    }

    pub fn process_str(&mut self, text: &str) {
        let units = text.encode_utf16().collect::<Vec<_>>();
        self.process_sequence(&units, InvocationContext::default());
    }

    fn process_sequence(&mut self, sequence: &[u16], context: InvocationContext) {
        let previous_context = self.machine.engine().dispatch().invocation_context();
        self.machine
            .engine_mut()
            .dispatch_mut()
            .set_invocation_context(context);

        for &unit in sequence {
            self.machine.process_code_unit(unit);
            while let Some(pending) = self.machine.engine_mut().dispatch_mut().take_pending() {
                self.process_sequence(&pending.sequence, pending.context);
                self.machine
                    .engine_mut()
                    .dispatch_mut()
                    .set_invocation_context(context);
            }
        }

        self.machine
            .engine_mut()
            .dispatch_mut()
            .set_invocation_context(previous_context);
    }
}
