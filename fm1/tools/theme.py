"""Apply the D.S.B. theme to the pinned Foma test panel."""
import sys
from pathlib import Path

page = Path(sys.argv[1])
html = page.read_text()
html = html.replace("<title>FM-1 firmware</title>", "<title>D.S.B. for FM-1 | Firmware test bench</title>")
html = html.replace("</head>", '  <link rel="stylesheet" href="dsb.css" />\n  </head>')
html = html.replace(
    '<strong id="fwname">FM-1</strong>',
    '<img src="dsb-logo.png" alt="Drums Sample Bass" class="dsb-logo" width="61" height="24" />'
    '<strong id="fwname" hidden>D.S.B.</strong>',
)
html = html.replace('>Emulator</a>', '>Test bench</a>')
html = html.replace('>Source</a>', '>Foma source</a>')
html = html.replace('        <div class="device" id="device">', '''        <div class="dsb-heading">
          <h1>Drums. Sample. Bass.</h1>
          <p>FM-1 firmware test bench</p>
        </div>
        <div class="dsb-map" aria-label="Performance controls">
          <span><b>HOME</b> Keys</span>
          <span><b>FX</b> Chops</span>
          <span><b>SEQ</b> Drums</span>
          <span><b>GLO</b> Mixer</span>
          <span><b>PLAY</b> Transport</span>
          <span><b>REC</b> Resample</span>
          <span><b>ENV</b> Warm Cab</span>
        </div>
        <div class="device" id="device">''')
page.write_text(html)
