<h1>RBS Launcher</h1>

<h2>Introduction</h2>

RBS Launcher opens the RBS window: a simplified, SDR++ style interface that drives SDRangel for the SDRplay RSP1B.
A mode (FT8 20 m, APRS, ADS-B...) is started with one click: it selects the RSP1B, applies the device settings of the mode,
adds the decoder channel, starts the device and shows the spectrum, the waterfall and the decodes.

<h2>Layouts</h2>

- **Tout-en-un**: all the panels (controls, spectrum and waterfall, decodes, info) in one window.
- **Fenêtres**: each panel in its own window, to place on one or two screens or to keep only the part you need.

The window remembers the layout used last and the arrangement of each layout. The "Panneaux" menu shows or hides each panel.

<h2>Modes</h2>

Each mode is a JSON file ("fiche de mode"). The built-in ones are FT8 20 m, APRS and ADS-B. JSON files placed in the
modes folder (button "Ouvrir le dossier des fiches de modes") add modes, or replace a built-in one with the same `id`:

    {
        "id": "ft8_40m",
        "order": 4,
        "name": "FT8 40 m",
        "group": "HF",
        "band": "40 m HF",
        "view": "ft8",
        "demod": "USB",
        "antenna": "MLA-30+",
        "device": { "centerFrequency": 7060000, "devSampleRate": 2000000, "log2Decim": 3, "fcPos": 2,
                    "ifAGC": 1, "lnaIndex": 4, "biasTee": 0, "amNotch": 1, "fmNotch": 1, "dabNotch": 0 },
        "channel": { "type": "FT8Demod", "inputFrequencyOffset": 14000, "settings": { "logMessages": 1 } }
    }

`device` holds SDRplayV3 device settings and `channel` the SDRangel channel type with its settings. `view` selects the
decode table: `ft8`, `aprs` or `adsb`.

<h2>Decodes</h2>

- FT8 decodes are read from the FT8 demodulator log (`logMessages` must be 1).
- APRS packets and ADS-B aircraft are collected from the message pipes, as the MCP server does.

Distances need your position in Preferences > My Position. Messages for your callsign are highlighted when the station
name in Preferences is your callsign.
