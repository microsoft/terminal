# Refactor proposal

## Goals:
1) reduce duplicate code
2) remove static functions
3) improve readability
4) improve reliability
5) improve code-coverage for testing

## Approach:
1) reduce duplicate code
    - `Move()` should rely on `MoveEndpointByUnit()`
2) remove static functions
    - all helper functions should not be static
3) improve readability
    - use `COORD` system internally
    - import/export Endpoints
4) improve reliability
    - rely more heavily on the `Viewport` and `TextBuffer` functions
5) improve code-coverage for testing
    - having reusable code means that we are testing more of our code better

## Targets (Code):
```c++
IFACEMETHODIMP Move(_In_ TextUnit unit,
                    _In_ int count,
                    _Out_ int* pRetVal) override;
IFACEMETHODIMP MoveEndpointByUnit(_In_ TextPatternRangeEndpoint endpoint,
                                  _In_ TextUnit unit,
                                  _In_ int count,
                                  _Out_ int* pRetVal) override;
IFACEMETHODIMP MoveEndpointByRange(_In_ TextPatternRangeEndpoint endpoint,
                                   _In_ ITextRangeProvider* pTargetRange,
                                   _In_ TextPatternRangeEndpoint targetEndpoint) override;
```

## Timeline
| Estimate | Task | Notes |
| -- | -- | -- |
| 3 | switch to `COORD` | remove helper functions |
|  |                    | convert to/from COORD at f(n) boundary |
|  |                    | update MoveState |
| 2 | remove `degenerate` |  |
| X | Y | |

## Horrible, Terrible, and Terrifying things I've found
- CompareEndpoints clamped. No documentation on why (or a need to do so)
- `GetText()` is something we shouldn't be doing manually.
- `Move()` can be reduced to `Expand...()` --> `MoveEndpointByUnit(start)` --> `Expand()`
- `MoveEndpointByRange()` just reduced to "set my endpoint to target endpoint"
- `MoveEndpointByUnitCharacter()` is literally just `Viewport::MoveInBounds()`