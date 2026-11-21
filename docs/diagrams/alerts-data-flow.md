# Alert data flow

```mermaid
flowchart TD
    Config[Build-time JSON configuration] --> Display[Initialize RGB display and 10-line LVGL draw buffer]
    Display --> Startup[Open Kyiv and Kyiv oblast]
    Startup --> Mode{Network mode}
    Mode -->|Demo| Demo[Show fictional alert data]
    Mode -->|No Wi-Fi configuration| Offline[Show configuration missing]
    Mode -->|Live| Init[Log internal heap and initialize Wi-Fi driver]
    Init --> Wifi[Connect Wi-Fi and obtain an IP address]
    Wifi --> Poll[Poll Tryvoha every 60 seconds]
    Poll --> HTTPS[Fetch HTTPS with TLS allocations in PSRAM]
    HTTPS --> Parse[Validate complete snapshot]
    Poll -->|Two consecutive failures| Reserve[Poll UA Siren reserve]
    Reserve --> HTTPS
    Reserve -->|Periodic probes| Primary[Validate primary recovery twice]
    Primary --> Poll
    Parse -->|Valid| Model[Replace typed snapshot and log provider, source timestamp and record count]
    Model --> Signal{Previously clear Kyiv city or district now under alert}
    Signal -->|Yes| Buzzer[Pulse GPIO6 high once for 180 ms]
    Parse -->|Invalid or unavailable| Retain[Retain last valid snapshot]
    Retain --> Fresh[Evaluate monotonic age]
    Fresh -->|Expired| Stale[Display stale overlay]
    Model --> Scope[Resolve city, district and oblast scope]
    Catalog[Verified provider hierarchy and geometry manifest] --> Scope
    Scope --> Community[Community alerts affect their parent district partially]
    Community --> Colors
    Scope --> Colors[Red, yellow, green or gray per district]
    Colors --> Home[Kyiv city and seven oblast districts]
    Home -->|Ukraine tab| National[All Ukraine district map]
    National -->|Kyiv tab| Home
```

Kyiv city has no parent oblast in the model, so an oblast-wide alert cannot
implicitly change the city's independent status. The reserve provider only
confirms positively mapped active alerts; omission remains unknown until a
complete inactive-region contract is established.

The first known status of a zone establishes its baseline silently. Unknown
updates retain the last known zone status. Repeated active statuses and changes
in alert severity do not trigger a new pulse.
