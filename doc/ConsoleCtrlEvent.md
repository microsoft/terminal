# Console Control Events

## Generation

conhost requests that user32 injects a thread into the attached console application.
See ntuser's exitwin.c for `CreateCtrlThread`.

## Timeouts

_Sourced from ntuser's **exitwin.c**, **user.h**_

| Event                  | Circumstances                   | Timeout                                                     |
|------------------------|---------------------------------|-------------------------------------------------------------|
| `CTRL_CLOSE_EVENT`     | _any_                           | system parameter `SPI_GETHUNGAPPTIMEOUT`, 5000ms            |
| `CTRL_LOGOFF_EVENT`    | `CONSOLE_QUICK_RESOLVE_FLAG`[1] | registry key `CriticalAppShutdownTimeout` or 500ms          |
| `CTRL_LOGOFF_EVENT`    | _none of the above_             | system parameter `SPI_GETWAITTOKILLTIMEOUT`, 5000ms         |
| `CTRL_SHUTDOWN_EVENT`  | **service process**             | system parameter `SPI_GETWAITTOKILLSERVICETIMEOUT`, 20000ms |
| `CTRL_SHUTDOWN_EVENT`  | `CONSOLE_QUICK_RESOLVE_FLAG`[1] | registry key `CriticalAppShutdownTimeout` or 500ms          |
| `CTRL_SHUTDOWN_EVENT`  | _none of the above_             | system parameter `SPI_GETWAITTOKILLTIMEOUT`, 5000ms         |
| `CTRL_C`, `CTRL_BREAK` | _any_                           | **no timeout**                                              |

_[1]: nobody sets `CONSOLE_QUICK_RESOLVE_FLAG`._
