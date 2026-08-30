pub const MAX_RETRIES_FOR_RENDER_ENGINE: u32 = 5;
pub const RENDER_BACKOFF_BASE_MILLIS: u32 = 100;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct RenderAttempt {
    pub attempt: u32,
    pub backoff_millis: u32,
}

pub fn render_attempts() -> impl Iterator<Item = RenderAttempt> {
    (0..=MAX_RETRIES_FOR_RENDER_ENGINE).map(|attempt| RenderAttempt {
        attempt,
        backoff_millis: if attempt == 0 {
            0
        } else {
            RENDER_BACKOFF_BASE_MILLIS.saturating_mul(1_u32 << (attempt - 1))
        },
    })
}

#[cfg(test)]
mod tests {
    use super::{RenderAttempt, render_attempts};

    #[test]
    fn first_attempt_is_immediate_and_five_retries_follow() {
        let actual = render_attempts().collect::<Vec<_>>();
        assert_eq!(
            actual,
            vec![
                RenderAttempt {
                    attempt: 0,
                    backoff_millis: 0,
                },
                RenderAttempt {
                    attempt: 1,
                    backoff_millis: 100,
                },
                RenderAttempt {
                    attempt: 2,
                    backoff_millis: 200,
                },
                RenderAttempt {
                    attempt: 3,
                    backoff_millis: 400,
                },
                RenderAttempt {
                    attempt: 4,
                    backoff_millis: 800,
                },
                RenderAttempt {
                    attempt: 5,
                    backoff_millis: 1_600,
                },
            ]
        );
    }
}
