//! Portable throttled-callback scheduling semantics from `til::throttled_func`.
//!
//! The native implementation uses Windows thread-pool timers. Rust owns the deterministic
//! scheduling policy while the timer mechanism is expressed with safe standard-library threads.

use std::sync::{Arc, Mutex, PoisonError};
use std::thread;
use std::time::Duration;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ThrottledOptions {
    pub delay: Duration,
    pub debounce: bool,
    pub leading: bool,
    pub trailing: bool,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ThrottledError {
    NeitherLeadingNorTrailing,
    NonPositiveDelay,
}

pub struct Throttled<T> {
    inner: Arc<Inner<T>>,
}

impl<T> Clone for Throttled<T> {
    fn clone(&self) -> Self {
        Self {
            inner: Arc::clone(&self.inner),
        }
    }
}

struct Inner<T> {
    options: ThrottledOptions,
    callback: Box<dyn Fn(T) + Send + Sync + 'static>,
    state: Mutex<State<T>>,
}

struct State<T> {
    pending: Option<T>,
    timer_running: bool,
    timer_generation: u64,
}

impl<T: Send + 'static> Throttled<T> {
    /// Creates a throttled callback with the same leading/trailing/debounce policy as TIL.
    ///
    /// # Errors
    ///
    /// Returns an error when neither edge is enabled or when the delay is zero.
    pub fn new(
        options: ThrottledOptions,
        callback: impl Fn(T) + Send + Sync + 'static,
    ) -> Result<Self, ThrottledError> {
        if !options.leading && !options.trailing {
            return Err(ThrottledError::NeitherLeadingNorTrailing);
        }
        if options.delay.is_zero() {
            return Err(ThrottledError::NonPositiveDelay);
        }

        Ok(Self {
            inner: Arc::new(Inner {
                options,
                callback: Box::new(callback),
                state: Mutex::new(State {
                    pending: None,
                    timer_running: false,
                    timer_generation: 0,
                }),
            }),
        })
    }

    /// Schedules or immediately invokes the callback according to the configured edge policy.
    pub fn call(&self, argument: T) {
        let mut immediate = None;
        let mut scheduled_generation = None;

        {
            let mut state = self
                .inner
                .state
                .lock()
                .unwrap_or_else(PoisonError::into_inner);
            let timer_was_running = state.timer_running;
            state.timer_running = true;

            if !timer_was_running && self.inner.options.leading {
                immediate = Some(argument);
            } else if self.inner.options.trailing {
                state.pending = Some(argument);
            }

            if !timer_was_running || self.inner.options.debounce {
                state.timer_generation = state.timer_generation.wrapping_add(1);
                scheduled_generation = Some(state.timer_generation);
            }
        }

        if let Some(argument) = immediate {
            (self.inner.callback)(argument);
        }

        if let Some(generation) = scheduled_generation {
            let inner = Arc::clone(&self.inner);
            thread::spawn(move || {
                thread::sleep(inner.options.delay);
                run_trailing(&inner, generation);
            });
        }
    }
}

fn run_trailing<T>(inner: &Arc<Inner<T>>, generation: u64) {
    let pending = {
        let mut state = inner.state.lock().unwrap_or_else(PoisonError::into_inner);
        if generation != state.timer_generation {
            return;
        }

        state.timer_running = false;
        state.pending.take()
    };

    if let Some(argument) = pending {
        (inner.callback)(argument);
    }
}

#[cfg(test)]
mod tests {
    use super::{Throttled, ThrottledOptions};
    use std::sync::{Arc, Mutex, PoisonError, mpsc};
    use std::time::Duration;

    #[test]
    fn microsoft_til_throttled_func_basic() {
        let (sender, receiver) = mpsc::channel();
        let holder = Arc::new(Mutex::new(None::<Throttled<bool>>));
        let callback_holder = Arc::clone(&holder);

        let throttled = Throttled::new(
            ThrottledOptions {
                delay: Duration::from_millis(10),
                debounce: false,
                leading: false,
                trailing: true,
            },
            move |reschedule| {
                sender.send(()).unwrap();
                if reschedule {
                    let nested = callback_holder
                        .lock()
                        .unwrap_or_else(PoisonError::into_inner)
                        .as_ref()
                        .unwrap()
                        .clone();
                    nested.call(false);
                }
            },
        )
        .unwrap();

        *holder.lock().unwrap_or_else(PoisonError::into_inner) = Some(throttled.clone());
        throttled.call(true);

        receiver.recv_timeout(Duration::from_secs(1)).unwrap();
        receiver.recv_timeout(Duration::from_secs(1)).unwrap();
    }
}
