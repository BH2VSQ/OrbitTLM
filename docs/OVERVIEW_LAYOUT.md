# Overview telemetry layout

The Overview page uses one continuous, full-width telemetry table instead of four separate cards.

- Four parameter/value pairs are packed into each row to reduce vertical height.
- The parameter columns resize with the available viewport width; value columns share the remaining width.
- Long values and names remain available in item tooltips while the visible table avoids a horizontal scrollbar.
- A vertical scrollbar appears only if the available window height cannot display all fields.
- Satellite-specific field mapping is still performed by the existing BY04/ASRTU-1 card builders; those source tables are hidden implementation details, not visible UI sections.
