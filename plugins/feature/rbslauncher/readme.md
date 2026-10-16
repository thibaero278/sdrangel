<h1>RBS Launcher</h1>

<h2>Introduction</h2>

RBS Launcher opens the RBS window: a simplified, SDR++ style interface that drives SDRangel for the SDRplay RSP1B.
A mode (FT8 20 m, APRS, ADS-B...) is started with one click: it selects the RSP1B, applies the device settings of the mode,
adds the decoder channel, starts the device and shows the spectrum, the waterfall and the decodes.

<h2>Layouts</h2>

- **Tout-en-un**: as SDR++, a fixed width menu on the left (controls and info), the spectrum and waterfall taking
  all the rest of the window, and the decodes under them. Drag the line between them to share the height.
- **Onglets**: as the SDRangel workspaces, one tab per panel (spectrum and waterfall, decodes, controls, info),
  each one using the whole window.

The window remembers the layout used last and its size. It opens full screen the first time.

While RBS is open the SDRangel window is hidden: "Réglages avancés dans SDRangel" shows it again.
Closing RBS closes SDRangel, which saves its configuration.

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

Optional keys:

    "spectrum": { "fftSize": 8192, "linesPerSecond": 6, "max": -90, "min": -130 },
    "frequencies": [
        { "band": "40 m HF", "frequency": 7074000, "note": "jour et nuit", "standard": true },
        { "band": "20 m HF", "frequency": 14074000, "note": "monde entier", "standard": true }
    ]

`spectrum` gives the default resolution (up to 8192: wider waterfalls stay black on many graphics chips), waterfall
speed and scale of the mode; changes made in RBS are remembered per mode. `frequencies` fills the "Bande" selector of
the top bar: bands on the left, their frequencies on the right. The frequency chosen last is used the next time the
mode starts.

RBS keeps its own settings apart from SDRangel's (QSettings application "RBS"): SDRangel's save removes any other group.

<h2>Decodes</h2>

- FT8 decodes are read from the FT8 demodulator log (`logMessages` must be 1).
- APRS packets and ADS-B aircraft are collected from the message pipes, as the MCP server does.

Distances need your position in Preferences > My Position. Messages for your callsign are highlighted when the station
name in Preferences is your callsign.
