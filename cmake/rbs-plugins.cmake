# RBS: initial cache (cmake -C) building only the plugins RBS Launcher uses.
# Used by scripts/build-rbs.cmd. The others are not compiled, so they are also missing from the
# SDRangel window. To get one back: set its option to ON here and rebuild with "build-rbs.cmd clean".
#
# Always built (no option): fileinput, testsource, localinput, audioinput, kiwisdr, remotetcpinput,
# sigmffileinput (if libsigmf), the sample sinks and the test MIMO devices.

cmake_policy(SET CMP0057 NEW) # if(... IN_LIST ...): an initial cache script has no project policies

# --- Devices: only the RSP1B (SDRplay API v3) ---
foreach(option
    ENABLE_AARONIARTSA ENABLE_AIRSPY ENABLE_AIRSPYHF ENABLE_BLADERF ENABLE_FUNCUBE ENABLE_HACKRF
    ENABLE_IIO ENABLE_LIMESUITE ENABLE_METIS ENABLE_MIRISDR ENABLE_PERSEUS ENABLE_RTLSDR
    ENABLE_SOAPYSDR ENABLE_XTRX ENABLE_USRP ENABLE_FOBOS)
    set(${option} OFF CACHE BOOL "RBS: not used" FORCE)
endforeach()
set(ENABLE_SDRPLAY ON CACHE BOOL "RBS: RSP1B" FORCE)

# --- No transmit or MIMO channels ---
set(ENABLE_CHANNELTX OFF CACHE BOOL "RBS: not used" FORCE)
set(ENABLE_CHANNELMIMO OFF CACHE BOOL "RBS: not used" FORCE)

# --- Receive channels: decoders of the modes and listening demodulators ---
set(RBS_CHANNELRX
    DEMODFT8      # FT8
    DEMODPACKET   # APRS
    DEMODADSB     # ADS-B
    DEMODSSB      # CW, USB / LSB listening
    DEMODNFM      # NFM listening
    DEMODAM       # AM listening
    DEMODWFM      # WFM listening
    REMOTETCPSINK # not used, but external/CMakeLists.txt builds FLAC only with it, and remotetcpinput needs FLAC
)
set(ENABLE_CHANNELRX ON CACHE BOOL "RBS" FORCE)
foreach(option
    DEMODADSB DEMODAM DEMODDATV DEMODDVB RADIOCLOCK RADIOASTRONOMY DEMODRADIOSONDE DEMODDRONEID
    FREQTRACKER DEMODATV DEMODPAGER DEMODDAB DEMODDRM UDPSINK DEMODAIS DEMODNFM DEMODM17 FILESINK
    DEMODFREEDV DEMODCHIRPCHAT DEMODMESHTASTIC DEMODMESHCORE REMOTESINK REMOTETCPSINK DEMODSSB
    CHANALYZER SIGMFFILESINK DEMODBFM DEMODWFM NOISEFIGURE DEMODVOR LOCALSINK DEMODPACKET DEMODAPT
    DEMODWEFAX DEMODDSD DEMODFT8 DEMODNAVTEX DEMODRTTY DEMODPSK31 DEMODILS DEMODSELCAL DEMODDSC
    HEATMAP FREQSCANNER ENDOFTRAIN CHANNELPOWER WDSPRX DEMODINMARSAT)
    if(option IN_LIST RBS_CHANNELRX)
        set(ENABLE_CHANNELRX_${option} ON CACHE BOOL "RBS" FORCE)
    else()
        set(ENABLE_CHANNELRX_${option} OFF CACHE BOOL "RBS: not used" FORCE)
    endif()
endforeach()

# --- Features: only RBS Launcher (it compiles the MCP data feed source itself) ---
set(ENABLE_FEATURE ON CACHE BOOL "RBS" FORCE)
foreach(option
    LIMERFE VORLOCALIZER JOGDIALCONTROLLER MAP SIMPLEPTT RADIOSONDE ANTENNATOOLS SATELLITETRACKER
    APRS DEMODANALYZER AMBE AIS AFC STARTRACKER RIGCTLSERVER PERTESTER GS232CONTROLLER REMOTECONTROL
    SKYMAP SID MORSEDECODER DENOISER FREQDISPLAY MCPSERVER)
    set(ENABLE_FEATURE_${option} OFF CACHE BOOL "RBS: not used" FORCE)
endforeach()
set(ENABLE_FEATURE_RBSLAUNCHER ON CACHE BOOL "RBS" FORCE)
